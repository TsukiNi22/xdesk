/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 05/11/2026 by @author Tsukini

File Name:
##  @file FrameEncoder.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/FrameEncoder.hpp"
#include "xdesk/net/Protocol.hpp"
#include <algorithm>
#include <jpeglib.h>
#include <csetjmp>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <QRect>
#include <QList>

//----------------------------------------------------------------//
/* DEFINE */

#define MAX_RUN_TILES 16 // dirty tiles of one row merged in one jpeg (less headers, better compression)

//----------------------------------------------------------------//
/* STRUCT */

// libjpeg error manager: longjmp instead of exit()
struct JpegError {
    jpeg_error_mgr manager;
    std::jmp_buf jump;
};

// Decode request: only POD between setjmp and longjmp (no destructor skipped)
struct JpegDecodeJob {
    const unsigned char* data = nullptr;
    unsigned long size = 0;
    unsigned char* (*allocate)(void* context, const int width, const int height, int* stride) = nullptr; // returns the pixels
    void* context = nullptr;
};

//----------------------------------------------------------------//
/* TOOLS */

static void jpeg_error_exit(j_common_ptr info)
{
    JpegError* error = reinterpret_cast<JpegError*>(info->err);
    std::longjmp(error->jump, 1);
}

static void jpeg_silent(j_common_ptr)
{
    /* Nothing */
}

// Encode a rectangle of a RGB32 image (BGRX in memory), the output is malloc'ed by libjpeg (free() it)
_hot static bool jpeg_encode_rect(const unsigned char* bits, const int stride, const int x, const int y, const int w, const int h, const int quality, unsigned char** out, unsigned long* outSize)
{
    jpeg_compress_struct cinfo;
    JpegError error;

    cinfo.err = jpeg_std_error(&error.manager);
    error.manager.error_exit = jpeg_error_exit;
    error.manager.output_message = jpeg_silent;
    if (setjmp(error.jump)) {
        jpeg_destroy_compress(&cinfo);
        if (*out) std::free(*out);
        *out = nullptr;
        return false;
    }
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, out, outSize);
    cinfo.image_width = static_cast<JDIMENSION>(w);
    cinfo.image_height = static_cast<JDIMENSION>(h);
    cinfo.input_components = 4;
    cinfo.in_color_space = JCS_EXT_BGRX; // QImage::Format_RGB32 on little endian: no conversion
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, std::clamp(quality, 1, 100), TRUE);
    cinfo.dct_method = JDCT_IFAST; // latency over the last bit of quality
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = const_cast<JSAMPROW>(bits + static_cast<std::size_t>(y + static_cast<int>(cinfo.next_scanline)) * static_cast<std::size_t>(stride) + static_cast<std::size_t>(x) * 4);
        (void)jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return true;
}

_hot static bool jpeg_decode(const JpegDecodeJob* job)
{
    jpeg_decompress_struct cinfo;
    JpegError error;
    unsigned char* pixels = nullptr;
    int stride = 0;

    cinfo.err = jpeg_std_error(&error.manager);
    error.manager.error_exit = jpeg_error_exit;
    error.manager.output_message = jpeg_silent;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, job->data, job->size);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    cinfo.out_color_space = JCS_EXT_BGRX;
    cinfo.dct_method = JDCT_IFAST;
    jpeg_start_decompress(&cinfo);
    pixels = job->allocate(job->context, static_cast<int>(cinfo.output_width), static_cast<int>(cinfo.output_height), &stride);
    if (!pixels) {
        jpeg_abort_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = pixels + static_cast<std::size_t>(cinfo.output_scanline) * static_cast<std::size_t>(stride);
        (void)jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

static unsigned char* allocate_image(void* context, const int width, const int height, int* stride)
{
    QImage* image = static_cast<QImage*>(context);

    *image = QImage(width, height, QImage::Format_RGB32);
    if (image->isNull()) return nullptr;
    *stride = static_cast<int>(image->bytesPerLine());
    return image->bits();
}

// Fast 64 bit hash of a tile (alpha ignored), 0 is reserved for "unknown"
_hot static std::uint64_t hash_tile(const unsigned char* bits, const int stride, const int x, const int y, const int w, const int h)
{
    std::uint64_t hash = 0x9E3779B97F4A7C15ull ^ (static_cast<std::uint64_t>(w) << 32) ^ static_cast<std::uint64_t>(h);

    for (int row = 0; row < h; ++row) {
        const std::uint32_t* line = reinterpret_cast<const std::uint32_t*>(bits + static_cast<std::size_t>(y + row) * static_cast<std::size_t>(stride)) + x;
        int col = 0;
        for (; col + 1 < w; col += 2) {
            std::uint64_t v = (static_cast<std::uint64_t>(line[col] & 0x00FFFFFFu) << 32) | (line[col + 1] & 0x00FFFFFFu);
            v *= 0xFF51AFD7ED558CCDull;
            v ^= v >> 33;
            hash = (hash ^ v) * 0xC4CEB9FE1A85EC53ull;
        }
        if (col < w)
            hash = (hash ^ (line[col] & 0x00FFFFFFu)) * 0xC4CEB9FE1A85EC53ull;
        hash ^= hash >> 29;
    }
    return hash ? hash : 1;
}

// true + color when every pixel of the tile has the same rgb
_hot static bool solid_tile(const unsigned char* bits, const int stride, const int x, const int y, const int w, const int h, std::uint32_t* color)
{
    const std::uint32_t first = *(reinterpret_cast<const std::uint32_t*>(bits + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride)) + x) & 0x00FFFFFFu;

    for (int row = 0; row < h; ++row) {
        const std::uint32_t* line = reinterpret_cast<const std::uint32_t*>(bits + static_cast<std::size_t>(y + row) * static_cast<std::size_t>(stride)) + x;
        for (int col = 0; col < w; ++col)
            if ((line[col] & 0x00FFFFFFu) != first) return false;
    }
    *color = first;
    return true;
}

//----------------------------------------------------------------//
/* CLASS */

void xdesk::platform::FrameEncoder::reset(void)
{
    std::fill(this->_hashes.begin(), this->_hashes.end(), 0);
}

QByteArray xdesk::platform::FrameEncoder::encode(const QImage& frame, const int quality)
{
    QImage image = frame;
    QList<QByteArray> tiles;
    QVector<char> dirty;

    if (image.isNull() || image.width() <= 0 || image.height() <= 0) return {};
    if (image.width() > 0xFFFF || image.height() > 0xFFFF) return {};
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32)
        image = image.convertToFormat(QImage::Format_RGB32);

    // Size change: everything is sent again
    if (image.width() != this->_width || image.height() != this->_height) {
        this->_width = image.width();
        this->_height = image.height();
        this->_cols = (this->_width + XDESK_TILE_SIZE - 1) / XDESK_TILE_SIZE;
        this->_rows = (this->_height + XDESK_TILE_SIZE - 1) / XDESK_TILE_SIZE;
        this->_hashes = QVector<std::uint64_t>(static_cast<qsizetype>(this->_cols) * this->_rows, 0);
    }

    const unsigned char* bits = image.constBits();
    const int stride = static_cast<int>(image.bytesPerLine());
    dirty = QVector<char>(static_cast<qsizetype>(this->_cols) * this->_rows, 0);

    // Find the changed tiles (solid ones are written at once, the others merged by row below)
    for (int ty = 0; ty < this->_rows; ++ty) {
        for (int tx = 0; tx < this->_cols; ++tx) {
            const int x = tx * XDESK_TILE_SIZE;
            const int y = ty * XDESK_TILE_SIZE;
            const int w = std::min(XDESK_TILE_SIZE, this->_width - x);
            const int h = std::min(XDESK_TILE_SIZE, this->_height - y);
            const qsizetype index = static_cast<qsizetype>(ty) * this->_cols + tx;
            const std::uint64_t hash = hash_tile(bits, stride, x, y, w, h);
            std::uint32_t color = 0;

            if (this->_hashes[index] == hash) continue;
            this->_hashes[index] = hash;
            if (solid_tile(bits, stride, x, y, w, h, &color)) {
                net::ByteWriter tile(net::Binary::Frame, 16);
                tile.u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y)).u16(static_cast<std::uint16_t>(w)).u16(static_cast<std::uint16_t>(h));
                tile.u8(static_cast<std::uint8_t>(net::TileEncoding::Solid));
                tile.u8(static_cast<std::uint8_t>(color >> 16)).u8(static_cast<std::uint8_t>(color >> 8)).u8(static_cast<std::uint8_t>(color));
                tiles.append(tile.data().mid(1));
            } else {
                dirty[index] = 1;
            }
        }
    }

    // Runs of dirty tiles on one row: one jpeg each
    for (int ty = 0; ty < this->_rows; ++ty) {
        int tx = 0;
        while (tx < this->_cols) {
            if (!dirty[static_cast<qsizetype>(ty) * this->_cols + tx]) {++tx; continue;}
            int end = tx;
            while (end < this->_cols && end - tx < MAX_RUN_TILES && dirty[static_cast<qsizetype>(ty) * this->_cols + end])
                ++end;
            const int x = tx * XDESK_TILE_SIZE;
            const int y = ty * XDESK_TILE_SIZE;
            const int w = std::min(end * XDESK_TILE_SIZE, this->_width) - x;
            const int h = std::min(XDESK_TILE_SIZE, this->_height - y);
            unsigned char* out = nullptr;
            unsigned long outSize = 0;

            if (jpeg_encode_rect(bits, stride, x, y, w, h, quality, &out, &outSize)) {
                net::ByteWriter tile(net::Binary::Frame, outSize + 16);
                tile.u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y)).u16(static_cast<std::uint16_t>(w)).u16(static_cast<std::uint16_t>(h));
                tile.u8(static_cast<std::uint8_t>(net::TileEncoding::Jpeg));
                tile.u32(static_cast<std::uint32_t>(outSize));
                tile.bytes(QByteArray(reinterpret_cast<const char*>(out), static_cast<qsizetype>(outSize)));
                tiles.append(tile.data().mid(1));
            } else {
                // Not sent: forget the hashes so that the next frame retries them
                for (int i = tx; i < end; ++i)
                    this->_hashes[static_cast<qsizetype>(ty) * this->_cols + i] = 0;
            }
            if (out) std::free(out);
            tx = end;
        }
    }
    if (tiles.isEmpty()) return {};

    // Message: header + tiles (count fits: 4K = 60 x 34 tiles)
    qsizetype total = 12;
    for (const QByteArray& tile: tiles)
        total += tile.size();
    net::ByteWriter message(net::Binary::Frame, static_cast<std::size_t>(total));
    message.u32(++this->_id).u16(static_cast<std::uint16_t>(this->_width)).u16(static_cast<std::uint16_t>(this->_height));
    message.u16(static_cast<std::uint16_t>(tiles.size()));
    for (const QByteArray& tile: tiles)
        message.bytes(tile);
    return message.data();
}

QByteArray xdesk::platform::FrameEncoder::encodeJpeg(const QImage& image, const int quality)
{
    QImage source = image;
    unsigned char* out = nullptr;
    unsigned long outSize = 0;
    QByteArray result;

    if (source.isNull()) return {};
    if (source.format() != QImage::Format_RGB32 && source.format() != QImage::Format_ARGB32)
        source = source.convertToFormat(QImage::Format_RGB32);
    if (jpeg_encode_rect(source.constBits(), static_cast<int>(source.bytesPerLine()), 0, 0, source.width(), source.height(), quality, &out, &outSize))
        result = QByteArray(reinterpret_cast<const char*>(out), static_cast<qsizetype>(outSize));
    if (out) std::free(out);
    return result;
}

QImage xdesk::platform::FrameEncoder::decodeJpeg(const QByteArray& data)
{
    QImage image;
    JpegDecodeJob job;

    if (data.isEmpty()) return {};
    job.data = reinterpret_cast<const unsigned char*>(data.constData());
    job.size = static_cast<unsigned long>(data.size());
    job.allocate = allocate_image;
    job.context = &image;
    if (!jpeg_decode(&job)) return {};
    return image;
}
