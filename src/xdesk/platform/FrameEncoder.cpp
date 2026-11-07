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
#include <unordered_map>
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
#include <vector>
#include <cstdio>
#include <QHash>
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
#define COPY_MIN_MOVED 4 // known tiles changed before a scroll is looked for
#define COPY_MIN_TILES 2 // changed tiles of a column before its lines are compared
#define COPY_MIN_VOTES 12 // lines of a column found at the same offset before it is trusted

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

// Hash of each line of a tile column, 0 = a line of one color (found anywhere: no clue of a move)
_hot static void line_hashes(const unsigned char* bits, const int stride, const int x, const int w, const int height, std::uint64_t* out)
{
    for (int y = 0; y < height; ++y) {
        const std::uint32_t* line = reinterpret_cast<const std::uint32_t*>(bits + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride)) + x;
        const std::uint32_t first = line[0] & 0x00FFFFFFu;
        std::uint64_t hash = 0x9E3779B97F4A7C15ull;
        std::uint32_t differ = 0;

        for (int col = 0; col < w; ++col) {
            const std::uint32_t color = line[col] & 0x00FFFFFFu;
            differ |= color ^ first;
            hash = (hash ^ color) * 0xC4CEB9FE1A85EC53ull;
        }
        out[y] = differ == 0 ? 0 : (hash ^ (hash >> 29)) | 1;
    }
}

// Rows a tile column moved down between two frames (negative: up), 0 = none: vote of the lines met once before
_hot static int column_offset(const std::uint64_t* before, const std::uint64_t* after, const int height)
{
    std::unordered_map<std::uint64_t, int> lines; // hash -> its line in the previous frame (-1 = several lines)
    std::vector<int> votes(static_cast<std::size_t>(height) * 2 + 1, 0);
    int best = 0;

    lines.reserve(static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        if (before[y] == 0) continue;
        const auto &[it, inserted] = lines.try_emplace(before[y], y);
        if (!inserted) it->second = -1;
    }
    for (int y = 0; y < height; ++y) {
        if (after[y] == 0) continue;
        auto it = lines.find(after[y]);
        if (it == lines.end() || it->second < 0 || it->second == y) continue;
        ++votes[static_cast<std::size_t>(y - it->second + height)];
    }
    for (int i = 0; i < static_cast<int>(votes.size()); ++i)
        if (votes[static_cast<std::size_t>(i)] > votes[static_cast<std::size_t>(best)]) best = i;
    return votes[static_cast<std::size_t>(best)] >= COPY_MIN_VOTES ? best - height : 0;
}

// true when the rectangle of a frame is the one of another frame sy - y rows higher
_hot static bool same_rect(const QImage& now, const QImage& before, const int x, const int y, const int sy, const int w, const int h)
{
    for (int row = 0; row < h; ++row)
        if (std::memcmp(now.constScanLine(y + row) + static_cast<std::size_t>(x) * 4, before.constScanLine(sy + row) + static_cast<std::size_t>(x) * 4, static_cast<std::size_t>(w) * 4) != 0) return false;
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
    this->_copied = false;
}

void xdesk::platform::FrameEncoder::findCopies_(const QImage& image, const QVector<std::uint64_t>& hashes, QVector<int>& offsets) const
{
    QVector<int> columns(this->_cols, 0);
    QHash<int, int> seen; // offset -> columns
    int common = 0;

    // Offset of each column that changed enough (line hashes of both frames, columns in parallel)
    parallel_for(this->_cols, [&](const int tx) {
        const int x = tx * XDESK_TILE_SIZE;
        const int w = std::min(XDESK_TILE_SIZE, this->_width - x);
        int changed = 0;

        for (int ty = 0; ty < this->_rows; ++ty) {
            const qsizetype index = static_cast<qsizetype>(ty) * this->_cols + tx;
            if (hashes[index] != this->_hashes[index] && this->_hashes[index] != 0) changed++;
        }
        if (changed < COPY_MIN_TILES) return;
        std::vector<std::uint64_t> before(static_cast<std::size_t>(this->_height));
        std::vector<std::uint64_t> after(static_cast<std::size_t>(this->_height));
        line_hashes(this->_previous.constBits(), static_cast<int>(this->_previous.bytesPerLine()), x, w, this->_height, before.data());
        line_hashes(image.constBits(), static_cast<int>(image.bytesPerLine()), x, w, this->_height, after.data());
        columns[tx] = column_offset(before.data(), after.data(), this->_height);
    });

    // The offset most columns found also tries the columns that found none (few distinct lines)
    for (const int offset: columns)
        if (offset != 0) seen[offset]++;
    for (QHash<int, int>::const_iterator it = seen.cbegin(); it != seen.cend(); ++it)
        if (common == 0 || it.value() > seen.value(common)) common = it.key();
    if (seen.isEmpty()) return;

    // Each changed tile found exactly at one of them in the previous frame, from tiles the client has
    parallel_for(this->_rows, [&](const int ty) {
        for (int tx = 0; tx < this->_cols; ++tx) {
            const qsizetype index = static_cast<qsizetype>(ty) * this->_cols + tx;
            const int x = tx * XDESK_TILE_SIZE;
            const int y = ty * XDESK_TILE_SIZE;
            const int w = std::min(XDESK_TILE_SIZE, this->_width - x);
            const int h = std::min(XDESK_TILE_SIZE, this->_height - y);

            if (hashes[index] == this->_hashes[index]) continue;
            for (const int offset: {columns[tx], common}) {
                const int sy = y - offset;
                bool known = offset != 0 && sy >= 0 && sy + h <= this->_height;

                // The client has every source tile
                for (int sty = std::max(0, sy / XDESK_TILE_SIZE); known && sty <= (sy + h - 1) / XDESK_TILE_SIZE; ++sty)
                    known = this->_hashes[static_cast<qsizetype>(sty) * this->_cols + tx] != 0;
                if (!known || !same_rect(image, this->_previous, x, y, sy, w, h)) continue;
                offsets[index] = offset;
                break;
            }
        }
    });
}

QList<QByteArray> xdesk::platform::FrameEncoder::copyTiles_(const QVector<int>& offsets)
{
    QList<QByteArray> tiles;
    QVector<std::uint8_t> quality = this->_quality;

    for (int ty = 0; ty < this->_rows; ++ty) {
        int tx = 0;

        while (tx < this->_cols) {
            const int offset = offsets[static_cast<qsizetype>(ty) * this->_cols + tx];
            int end = tx;

            if (offset == 0) {++tx; continue;}
            while (end < this->_cols && offsets[static_cast<qsizetype>(ty) * this->_cols + end] == offset)
                ++end;
            const int x = tx * XDESK_TILE_SIZE;
            const int y = ty * XDESK_TILE_SIZE;
            const int w = std::min(end * XDESK_TILE_SIZE, this->_width) - x;
            const int h = std::min(XDESK_TILE_SIZE, this->_height - y);
            net::ByteWriter tile(net::Binary::Frame, 16);
            tile.u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y)).u16(static_cast<std::uint16_t>(w)).u16(static_cast<std::uint16_t>(h));
            tile.u8(static_cast<std::uint8_t>(net::TileEncoding::Copy));
            tile.u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y - offset));
            tiles.append(tile.data().mid(1));

            // A copy is as sharp as the worst of its sources (sent again sharp once still)
            for (int i = tx; i < end; ++i) {
                const int sy = y - offset;
                int worst = 101; // 101 = exact
                for (int sty = sy / XDESK_TILE_SIZE; sty <= (sy + h - 1) / XDESK_TILE_SIZE; ++sty) {
                    const int q = this->_quality[static_cast<qsizetype>(sty) * this->_cols + i];
                    worst = std::min(worst, q == 0 ? 101 : q);
                }
                quality[static_cast<qsizetype>(ty) * this->_cols + i] = static_cast<std::uint8_t>(worst > 100 ? 0 : worst);
            }
            tx = end;
        }
    }
    this->_quality = quality;
    if (!tiles.isEmpty()) this->_copied = true;
    return tiles;
}

QByteArray xdesk::platform::FrameEncoder::encode(const QImage& frame, const int quality)
{
    QImage image = frame;
    QList<QByteArray> tiles;
    QVector<std::uint64_t> hashes;
    QVector<int> want; // jpeg quality to send each tile with (0 = nothing to send)
    QVector<QList<QByteArray>> rows;
    QVector<int> offsets; // per tile: rows its content moved (copy tile), 0 = sent as itself
    QList<QByteArray> copies;
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

    // Scroll: the tiles found higher / lower in the previous frame are copied by the client from what it has
    offsets = QVector<int>(count, 0);
    if (this->_copy && this->_previous.size() == image.size()) {
        for (qsizetype i = 0; i < count; ++i)
            if (hashes[i] != this->_hashes[i] && this->_hashes[i] != 0) moved++;
        if (moved >= COPY_MIN_MOVED) this->findCopies_(image, hashes, offsets);
        moved = 0;
    }
    copies = this->copyTiles_(offsets);

    // Motion (many known tiles changed): lighter jpeg, still (almost nothing changed): the light tiles sent again sharp
    for (qsizetype i = 0; i < count; ++i) {
        if (hashes[i] == this->_hashes[i] || offsets[i] != 0) continue;
        changed++;
        if (this->_hashes[i] != 0) moved++;
    }
    const int motion = moved * MOTION_SHARE > count ? std::max(20, quiet * 3 / 5) : quiet;
    const bool still = static_cast<qsizetype>(changed) * STILL_SHARE < count;
    for (qsizetype i = 0; i < count; ++i) {
        if (offsets[i] != 0) continue; // copied
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
    if (this->_copy) this->_previous = image;
    tiles = copies; // first: the client copies from the frame as it was before this message
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
