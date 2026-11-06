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
#include <QThreadPool>
#include <QSemaphore>
#include <functional>
#include <algorithm>
#include <jpeglib.h>
#include <QThread>
#include <csetjmp>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <cstdio>
#include <QRect>
#include <QList>

//----------------------------------------------------------------//
/* DEFINE */

#define MAX_RUN_TILES 16 // dirty tiles of one row merged in one jpeg (less headers, better compression)
#define MOTION_SHARE 4 // more than 1 / 4 of the tiles changed = motion (scroll, video, drag): sent lighter
#define STILL_SHARE 32 // less than 1 / 32 changed = still: the light tiles of the motion are sent again sharp
#define MAX_WORKERS 8 // encoding threads (rows of tiles in parallel)
#define PALETTE_MAX 64 // colors of a palette tile at most (text, ui; a photo or a gradient has more: jpeg)
#define PALETTE_SHARE 3 // a palette tile is kept when its bytes are under 1 / 3 of its pixels (else jpeg is lighter)

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

// Lossless tile of a few colors: palette + one index per pixel (zlib), false = too many colors or not lighter
_hot static bool palette_tile(const unsigned char* bits, const int stride, const int x, const int y, const int w, const int h, QByteArray* out)
{
    std::uint32_t colors[PALETTE_MAX];
    std::uint32_t table[256]; // open addressing: color + 1 (0 = empty) -> its index in tableIndex
    std::uint8_t tableIndex[256];
    QByteArray indices(static_cast<qsizetype>(w) * h, Qt::Uninitialized);
    char* index = indices.data();
    std::uint32_t last = 0xFFFFFFFFu;
    char lastIndex = 0;
    int count = 0;

    std::memset(table, 0, sizeof(table));
    for (int row = 0; row < h; ++row) {
        const std::uint32_t* line = reinterpret_cast<const std::uint32_t*>(bits + static_cast<std::size_t>(y + row) * static_cast<std::size_t>(stride)) + x;
        for (int col = 0; col < w; ++col) {
            const std::uint32_t color = line[col] & 0x00FFFFFFu;
            if (color != last) {
                unsigned cell = (color * 0x9E3779B1u) >> 24;
                while (table[cell] != 0 && table[cell] != color + 1)
                    cell = (cell + 1) & 255u;
                if (table[cell] == 0) {
                    if (count == PALETTE_MAX) return false; // a photo, a gradient: jpeg
                    table[cell] = color + 1;
                    tableIndex[cell] = static_cast<std::uint8_t>(count);
                    colors[count++] = color;
                }
                last = color;
                lastIndex = static_cast<char>(tableIndex[cell]);
            }
            *index++ = lastIndex;
        }
    }
    const QByteArray packed = qCompress(indices, 1).mid(4); // fastest zlib level (the indices are very repetitive anyway), stream only
    if ((packed.size() + count * 3) * PALETTE_SHARE > static_cast<qsizetype>(w) * h) return false;
    xdesk::net::ByteWriter tile(xdesk::net::Binary::Frame, static_cast<std::size_t>(packed.size()) + count * 3 + 20);
    tile.u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y)).u16(static_cast<std::uint16_t>(w)).u16(static_cast<std::uint16_t>(h));
    tile.u8(static_cast<std::uint8_t>(xdesk::net::TileEncoding::Palette)).u8(static_cast<std::uint8_t>(count));
    for (int i = 0; i < count; ++i)
        tile.u8(static_cast<std::uint8_t>(colors[i] >> 16)).u8(static_cast<std::uint8_t>(colors[i] >> 8)).u8(static_cast<std::uint8_t>(colors[i]));
    tile.u32(static_cast<std::uint32_t>(packed.size()));
    tile.bytes(packed);
    *out = tile.data().mid(1);
    return true;
}

void xdesk::platform::parallel_for(const int count, const std::function<void(const int)>& fn)
{
    static QThreadPool pool; // own pool: never waits behind the tasks of the global one
    const int workers = std::min({count, MAX_WORKERS, std::max(1, QThread::idealThreadCount())});
    std::atomic<int> next = 0;
    QSemaphore done;
    const std::function<void(void)> work = [&next, &done, &fn, count](void) {
        for (int i = next++; i < count; i = next++)
            fn(i);
        done.release();
    };

    if (workers <= 1) {
        for (int i = 0; i < count; ++i)
            fn(i);
        return;
    }
    pool.setMaxThreadCount(MAX_WORKERS);
    for (int w = 1; w < workers; ++w)
        pool.start(work);
    work();
    done.acquire(workers);
}

//----------------------------------------------------------------//
/* CLASS */

void xdesk::platform::FrameEncoder::reset(void)
{
    std::fill(this->_hashes.begin(), this->_hashes.end(), 0);
    std::fill(this->_quality.begin(), this->_quality.end(), 0);
}

QByteArray xdesk::platform::FrameEncoder::encode(const QImage& frame, const int quality)
{
    QImage image = frame;
    QList<QByteArray> tiles;
    QVector<std::uint64_t> hashes;
    QVector<int> want; // jpeg quality to send each tile with (0 = nothing to send)
    QVector<QList<QByteArray>> rows;
    int changed = 0;
    int moved = 0;

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
        this->_quality = QVector<std::uint8_t>(static_cast<qsizetype>(this->_cols) * this->_rows, 0);
    }

    const unsigned char* bits = image.constBits();
    const int stride = static_cast<int>(image.bytesPerLine());
    const qsizetype count = static_cast<qsizetype>(this->_cols) * this->_rows;
    const int quiet = std::clamp(quality, 1, 100);
    hashes = QVector<std::uint64_t>(count, 0);
    want = QVector<int>(count, 0);
    rows = QVector<QList<QByteArray>>(this->_rows);

    // Hash every tile (rows in parallel)
    parallel_for(this->_rows, [&](const int ty) {
        for (int tx = 0; tx < this->_cols; ++tx) {
            const int x = tx * XDESK_TILE_SIZE;
            const int y = ty * XDESK_TILE_SIZE;
            hashes[static_cast<qsizetype>(ty) * this->_cols + tx] = hash_tile(bits, stride, x, y, std::min(XDESK_TILE_SIZE, this->_width - x), std::min(XDESK_TILE_SIZE, this->_height - y));
        }
    });

    // Motion (many known tiles changed): lighter jpeg, still (almost nothing changed): the light tiles sent again sharp
    for (qsizetype i = 0; i < count; ++i) {
        if (hashes[i] == this->_hashes[i]) continue;
        changed++;
        if (this->_hashes[i] != 0) moved++;
    }
    const int motion = moved * MOTION_SHARE > count ? std::max(20, quiet * 3 / 5) : quiet;
    const bool still = static_cast<qsizetype>(changed) * STILL_SHARE < count;
    for (qsizetype i = 0; i < count; ++i) {
        if (hashes[i] != this->_hashes[i]) want[i] = this->_hashes[i] != 0 ? motion : quiet; // a new tile (first frame, reset): sharp at once
        else if (still && this->_quality[i] != 0 && this->_quality[i] < quiet) want[i] = quiet;
    }

    // Encode each row (in parallel): solid tiles at once, runs of tiles of the same quality in one jpeg
    parallel_for(this->_rows, [&](const int ty) {
        QList<QByteArray>& out = rows[ty];
        int tx = 0;

        for (int i = 0; i < this->_cols; ++i) {
            const qsizetype index = static_cast<qsizetype>(ty) * this->_cols + i;
            const int x = i * XDESK_TILE_SIZE;
            const int y = ty * XDESK_TILE_SIZE;
            std::uint32_t color = 0;

            QByteArray palette;

            if (want[index] == 0 || hashes[index] == this->_hashes[index]) continue; // nothing / a sharper copy of an unchanged tile
            if (!solid_tile(bits, stride, x, y, std::min(XDESK_TILE_SIZE, this->_width - x), std::min(XDESK_TILE_SIZE, this->_height - y), &color)) {
                // Text / ui: exact and lighter than a jpeg
                if (!this->_palette || !palette_tile(bits, stride, x, y, std::min(XDESK_TILE_SIZE, this->_width - x), std::min(XDESK_TILE_SIZE, this->_height - y), &palette)) continue;
                out.append(palette);
                want[index] = 0;
                this->_quality[index] = 0; // exact
                continue;
            }
            net::ByteWriter tile(net::Binary::Frame, 16);
            tile.u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y));
            tile.u16(static_cast<std::uint16_t>(std::min(XDESK_TILE_SIZE, this->_width - x))).u16(static_cast<std::uint16_t>(std::min(XDESK_TILE_SIZE, this->_height - y)));
            tile.u8(static_cast<std::uint8_t>(net::TileEncoding::Solid));
            tile.u8(static_cast<std::uint8_t>(color >> 16)).u8(static_cast<std::uint8_t>(color >> 8)).u8(static_cast<std::uint8_t>(color));
            out.append(tile.data().mid(1));
            want[index] = 0;
            this->_quality[index] = 0; // exact
        }
        while (tx < this->_cols) {
            const qsizetype first = static_cast<qsizetype>(ty) * this->_cols + tx;
            const int q = want[first];
            int end = tx;
            unsigned char* jpeg = nullptr;
            unsigned long jpegSize = 0;

            if (q == 0) {++tx; continue;}
            while (end < this->_cols && end - tx < MAX_RUN_TILES && want[static_cast<qsizetype>(ty) * this->_cols + end] == q)
                ++end;
            const int x = tx * XDESK_TILE_SIZE;
            const int y = ty * XDESK_TILE_SIZE;
            const int w = std::min(end * XDESK_TILE_SIZE, this->_width) - x;
            const int h = std::min(XDESK_TILE_SIZE, this->_height - y);
            const bool sent = jpeg_encode_rect(bits, stride, x, y, w, h, q, &jpeg, &jpegSize);
            if (sent) {
                net::ByteWriter tile(net::Binary::Frame, jpegSize + 16);
                tile.u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y)).u16(static_cast<std::uint16_t>(w)).u16(static_cast<std::uint16_t>(h));
                tile.u8(static_cast<std::uint8_t>(net::TileEncoding::Jpeg));
                tile.u32(static_cast<std::uint32_t>(jpegSize));
                tile.bytes(QByteArray(reinterpret_cast<const char*>(jpeg), static_cast<qsizetype>(jpegSize)));
                out.append(tile.data().mid(1));
            }
            for (int i = tx; i < end; ++i) {
                const qsizetype index = static_cast<qsizetype>(ty) * this->_cols + i;
                if (sent) this->_quality[index] = static_cast<std::uint8_t>(q);
                else hashes[index] = 0; // not sent: the next frame retries it
            }
            if (jpeg) std::free(jpeg);
            tx = end;
        }
    });
    this->_hashes = hashes;
    for (const QList<QByteArray>& row: rows)
        tiles.append(row);
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
