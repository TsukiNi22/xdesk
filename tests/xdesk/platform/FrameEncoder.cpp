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
#include "xdesk/platform/H264.hpp"
#include "xdesk/net/Protocol.hpp"
#include <gtest/gtest.h>
#include <QElapsedTimer>
#include <functional>
#include <QPainter>
#include <iostream>
#include <cstring>
#include <QImage>

// Tiles of a Frame message: <count, jpeg tiles, solid tiles, covered pixels>
static std::tuple<int, int, int, long> tiles_of(const QByteArray& message)
{
    xdesk::net::ByteReader r(message);
    int jpeg = 0;
    int solid = 0;
    int count = 0;
    long area = 0;

    (void)r.u32();
    (void)r.u16();
    (void)r.u16();
    count = r.u16();
    for (int i = 0; i < count && r.ok(); ++i) {
        (void)r.u16();
        (void)r.u16();
        const long w = r.u16();
        const long h = r.u16();
        area += w * h;
        const std::uint8_t encoding = r.u8();
        if (encoding == 1) {jpeg++; (void)r.bytes(static_cast<qsizetype>(r.u32()));}
        else if (encoding == 3) {
            const int colors = r.u8();
            (void)r.bytes(colors * 3);
            (void)r.bytes(static_cast<qsizetype>(r.u32()));
        } else {solid++; (void)r.u8(); (void)r.u8(); (void)r.u8();}
    }
    EXPECT_TRUE(r.ok());
    return {count, jpeg, solid, area};
}

static QImage test_frame(void)
{
    QImage frame(640, 360, QImage::Format_RGB32);
    QPainter painter(&frame);

    frame.fill(QColor(20, 30, 40));
    painter.fillRect(QRect(100, 50, 200, 120), QColor(200, 10, 10));
    for (int i = 0; i < 40; ++i)
        painter.drawLine(i * 15, 200, 640 - i * 7, 359);
    return frame;
}

TEST(FrameEncoder, FirstFrameIsCompleteThenOnlyChanges)
{
    xdesk::platform::FrameEncoder encoder;
    QImage frame = test_frame();
    const long area = 640L * 360L;

    const QByteArray first = encoder.encode(frame, 70);
    ASSERT_FALSE(first.isEmpty());
    EXPECT_EQ(static_cast<std::uint8_t>(first[0]), 0x10);
    EXPECT_EQ(std::get<3>(tiles_of(first)), area); // the whole frame is covered
    EXPECT_GT(std::get<2>(tiles_of(first)), 0); // flat background -> solid tiles

    // Same frame: nothing to send
    EXPECT_TRUE(encoder.encode(frame, 70).isEmpty());

    // One pixel changed: one tile
    frame.setPixel(5, 5, qRgb(255, 255, 255));
    EXPECT_EQ(std::get<3>(tiles_of(encoder.encode(frame, 70))), 64L * 64L); // one tile

    // Reset: complete again
    encoder.reset();
    EXPECT_EQ(std::get<3>(tiles_of(encoder.encode(frame, 70))), area);
}

TEST(FrameEncoder, JpegRoundTrip)
{
    const QImage frame = test_frame().copy(0, 0, 64, 64);
    const QByteArray jpeg = xdesk::platform::FrameEncoder::encodeJpeg(frame, 90);
    const QImage decoded = xdesk::platform::FrameEncoder::decodeJpeg(jpeg);

    ASSERT_FALSE(jpeg.isEmpty());
    ASSERT_EQ(decoded.size(), frame.size());
    EXPECT_NEAR(qRed(decoded.pixel(10, 10)), qRed(frame.pixel(10, 10)), 12);
    EXPECT_TRUE(xdesk::platform::FrameEncoder::decodeJpeg("not a jpeg").isNull());
}

TEST(FrameEncoder, ResizeSendsACompleteFrame)
{
    xdesk::platform::FrameEncoder encoder;

    (void)encoder.encode(test_frame(), 70);
    const QImage smaller = test_frame().scaled(320, 180);
    EXPECT_EQ(std::get<3>(tiles_of(encoder.encode(smaller, 70))), 320L * 180L);
}

TEST(FrameEncoder, MotionIsLighterThenSentAgainSharp)
{
    xdesk::platform::FrameEncoder encoder;
    QImage frame = test_frame();
    QImage moved;
    long sharp = 0;
    long light = 0;

    // First frame: sharp (nothing to send again)
    for (int y = 0; y < 360; ++y)
        for (int x = 0; x < 640; ++x)
            frame.setPixel(x, y, qRgb((x * 7 + y) % 256, (x + y * 3) % 256, (x * y) % 256)); // noise: no solid tile
    sharp = encoder.encode(frame, 90).size();
    EXPECT_TRUE(encoder.encode(frame, 90).isEmpty());

    // Everything moved (scrolled by 3 pixels): lighter than the sharp frame
    moved = frame.copy(3, 0, 640, 360);
    light = encoder.encode(moved, 90).size();
    EXPECT_GT(light, 0);
    EXPECT_LT(light, sharp);

    // Still now: the same tiles sent again sharp, once
    const QByteArray again = encoder.encode(moved, 90);
    EXPECT_EQ(std::get<3>(tiles_of(again)), 640L * 360L);
    EXPECT_GT(again.size(), light);
    EXPECT_TRUE(encoder.encode(moved, 90).isEmpty());
}

// Glyph like strokes (anti aliased: 4 levels between the ink and the paper) in a cell of 8 x 14 pixels
static void glyph(QImage& frame, const int x, const int y, const unsigned seed, const QRgb ink, const QRgb paper)
{
    for (int dy = 2; dy < 12; ++dy)
        for (int dx = 1; dx < 7; ++dx) {
            const unsigned bit = (seed * 2654435761u >> ((dx * 5 + dy * 3) % 29)) & 7u;
            if (bit > 3 || x + dx >= frame.width() || y + dy >= frame.height() || y + dy < 0) continue;
            const int level = static_cast<int>(bit); // 0 = ink ... 3 = almost paper
            const std::function<int(const int, const int)> mix = [level](const int a, const int b) {return (a * (4 - level) + b * level) / 4;};
            frame.setPixel(x + dx, y + dy, qRgb(mix(qRed(ink), qRed(paper)), mix(qGreen(ink), qGreen(paper)), mix(qBlue(ink), qBlue(paper))));
        }
}

// Desktop like frame: a light editor and a dark terminal full of text, a photo like gradient
static QImage desktop_frame(const int scroll)
{
    QImage frame(1920, 1080, QImage::Format_RGB32);
    const QRgb paper = qRgb(250, 250, 250);
    const QRgb dark = qRgb(12, 12, 12);

    frame.fill(QColor(36, 40, 48));
    for (int y = 40; y < 1020; ++y)
        for (int x = 40; x < 1140; ++x)
            frame.setPixel(x, y, paper);
    for (int line = 0; line < 69; ++line)
        for (int col = 0; col < 130; ++col)
            if ((line * 7 + col * 3) % 11 != 0) glyph(frame, 50 + col * 8, 44 + line * 14 - scroll, static_cast<unsigned>((line + scroll / 14) * 131 + col), qRgb(30, 30, 30), paper);
    for (int y = 40; y < 520; ++y)
        for (int x = 1180; x < 1880; ++x)
            frame.setPixel(x, y, dark);
    for (int line = 0; line < 33; ++line)
        for (int col = 0; col < 85; ++col)
            glyph(frame, 1188 + col * 8, 44 + line * 14, static_cast<unsigned>(line * 97 + col), qRgb(120, 220, 120), dark);
    for (int y = 560; y < 1040; ++y)
        for (int x = 1180; x < 1880; ++x)
            frame.setPixel(x, y, qRgb((x * 7 + y) & 255, (y * 3) & 255, (x + y * 5) & 255));
    return frame;
}

// Not a check: time and bytes of a complete desktop frame and of a scroll, without / with the palette tiles
// (run: unit_tests --gtest_also_run_disabled_tests --gtest_filter='*Benchmark*')
TEST(FrameEncoder, DISABLED_BenchmarkPalette)
{
    for (const bool palette: {false, true}) {
        qint64 fullNs = 0;
        qint64 scrollNs = 0;
        qsizetype fullBytes = 0;
        qsizetype scrollBytes = 0;
        const QImage first = desktop_frame(0);
        const QImage next = desktop_frame(16);
        for (int run = 0; run < 10; ++run) {
            xdesk::platform::FrameEncoder encoder;
            QElapsedTimer timer;
            encoder.setPalette(palette);
            timer.start();
            fullBytes = encoder.encode(first, 70).size();
            fullNs += timer.nsecsElapsed();
            timer.restart();
            scrollBytes = encoder.encode(next, 70).size();
            scrollNs += timer.nsecsElapsed();
        }
        std::cout << (palette ? "palette" : "jpeg   ") << "  full " << fullNs / 10000000.0 << " ms " << fullBytes / 1024 << " KiB   scroll " << scrollNs / 10000000.0 << " ms " << scrollBytes / 1024 << " KiB" << std::endl;
    }
}

TEST(FrameEncoder, PaletteTilesAreLosslessAndOnlyWhenAsked)
{
    const QImage frame = desktop_frame(0);
    xdesk::platform::FrameEncoder plain;
    xdesk::platform::FrameEncoder palette;
    const QByteArray withoutPalette = plain.encode(frame, 70);
    QByteArray withPalette;
    int paletteTiles = 0;
    int checked = 0;

    palette.setPalette(true);
    withPalette = palette.encode(frame, 70);
    // Without the capability: never a palette tile
    {
        xdesk::net::ByteReader r(withoutPalette);
        (void)r.u32(); (void)r.u16(); (void)r.u16();
        const int count = r.u16();
        for (int i = 0; i < count && r.ok(); ++i) {
            (void)r.u16(); (void)r.u16(); (void)r.u16(); (void)r.u16();
            const std::uint8_t encoding = r.u8();
            EXPECT_NE(encoding, 3);
            if (encoding == 1) (void)r.bytes(static_cast<qsizetype>(r.u32()));
            else {(void)r.u8(); (void)r.u8(); (void)r.u8();}
        }
    }
    // With it: palette tiles decode to the exact pixels
    xdesk::net::ByteReader r(withPalette);
    (void)r.u32(); (void)r.u16(); (void)r.u16();
    const int count = r.u16();
    for (int i = 0; i < count && r.ok(); ++i) {
        const int x = r.u16();
        const int y = r.u16();
        const int w = r.u16();
        const int h = r.u16();
        const std::uint8_t encoding = r.u8();
        if (encoding == 1) {(void)r.bytes(static_cast<qsizetype>(r.u32())); continue;}
        if (encoding != 3) {(void)r.u8(); (void)r.u8(); (void)r.u8(); continue;}
        paletteTiles++;
        const int colors = r.u8();
        const QByteArray table = r.bytes(colors * 3);
        const QByteArray packed = r.bytes(static_cast<qsizetype>(r.u32()));
        const qsizetype pixels = static_cast<qsizetype>(w) * h;
        QByteArray sized(4, Qt::Uninitialized);
        sized[0] = static_cast<char>((pixels >> 24) & 0xFF); sized[1] = static_cast<char>((pixels >> 16) & 0xFF);
        sized[2] = static_cast<char>((pixels >> 8) & 0xFF); sized[3] = static_cast<char>(pixels & 0xFF);
        const QByteArray indices = qUncompress(sized + packed);
        ASSERT_EQ(indices.size(), pixels);
        for (int py = 0; py < h; ++py)
            for (int px = 0; px < w; ++px) {
                const int index = static_cast<unsigned char>(indices[py * w + px]);
                ASSERT_LT(index, colors);
                const QRgb expected = frame.pixel(x + px, y + py);
                const QRgb got = qRgb(static_cast<unsigned char>(table[index * 3]), static_cast<unsigned char>(table[index * 3 + 1]), static_cast<unsigned char>(table[index * 3 + 2]));
                ASSERT_EQ(got & 0xFFFFFF, expected & 0xFFFFFF);
                checked++;
            }
    }
    EXPECT_TRUE(r.ok());
    EXPECT_GT(paletteTiles, 20); // the text windows
    EXPECT_GT(checked, 0);
}

// Desktop frame whose editor is scrolled by scroll rows: a pure shift of its text (what a scroll copies)
static QImage scrolled_frame(const int scroll)
{
    static QImage document;
    QImage frame = desktop_frame(0);

    if (document.isNull()) {
        document = QImage(1100, 2400, QImage::Format_RGB32);
        document.fill(QColor(250, 250, 250));
        for (int line = 0; line < 170; ++line)
            for (int col = 0; col < 130; ++col)
                if ((line * 7 + col * 3) % 11 != 0) glyph(document, 10 + col * 8, 4 + line * 14, static_cast<unsigned>(line * 131 + col), qRgb(30, 30, 30), qRgb(250, 250, 250));
    }
    for (int y = 0; y < 980; ++y)
        std::memcpy(frame.scanLine(40 + y) + 40 * 4, document.constScanLine(y + scroll), 1100 * 4);
    return frame;
}

// What a client draws from a Frame message (every encoding), returns its copy tiles
static int apply_frame(QImage& canvas, const QByteArray& message)
{
    xdesk::net::ByteReader r(message);
    const QImage before = canvas.copy(); // the copy tiles read the frame as it was before the message
    int copies = 0;

    (void)r.u32();
    const int width = r.u16();
    const int height = r.u16();
    const int count = r.u16();
    if (canvas.size() != QSize(width, height)) {
        canvas = QImage(width, height, QImage::Format_RGB32);
        canvas.fill(Qt::black);
    }
    for (int i = 0; i < count && r.ok(); ++i) {
        const int x = r.u16();
        const int y = r.u16();
        const int w = r.u16();
        const int h = r.u16();
        const std::uint8_t encoding = r.u8();
        QImage tile(w, h, QImage::Format_RGB32);
        if (encoding == 1) {
            tile = xdesk::platform::FrameEncoder::decodeJpeg(r.bytes(static_cast<qsizetype>(r.u32())));
        } else if (encoding == 2) {
            const int red = r.u8();
            const int green = r.u8();
            const int blue = r.u8();
            tile.fill(qRgb(red, green, blue));
        } else if (encoding == 3) {
            const int colors = r.u8();
            const QByteArray table = r.bytes(colors * 3);
            const QByteArray packed = r.bytes(static_cast<qsizetype>(r.u32()));
            const qsizetype pixels = static_cast<qsizetype>(w) * h;
            QByteArray sized(4, Qt::Uninitialized);
            sized[0] = static_cast<char>((pixels >> 24) & 0xFF); sized[1] = static_cast<char>((pixels >> 16) & 0xFF);
            sized[2] = static_cast<char>((pixels >> 8) & 0xFF); sized[3] = static_cast<char>(pixels & 0xFF);
            const QByteArray indices = qUncompress(sized + packed);
            EXPECT_EQ(indices.size(), pixels);
            for (qsizetype p = 0; p < indices.size(); ++p) {
                const int index = static_cast<unsigned char>(indices[p]) * 3;
                tile.setPixel(static_cast<int>(p % w), static_cast<int>(p / w), qRgb(static_cast<unsigned char>(table[index]), static_cast<unsigned char>(table[index + 1]), static_cast<unsigned char>(table[index + 2])));
            }
        } else if (encoding == 4) {
            const int sx = r.u16();
            const int sy = r.u16();
            EXPECT_EQ(copies, i); // the copies come first
            EXPECT_TRUE(QRect(0, 0, width, height).contains(QRect(sx, sy, w, h)));
            tile = before.copy(sx, sy, w, h);
            copies++;
        } else {
            ADD_FAILURE() << "unknown tile encoding " << static_cast<int>(encoding);
            break;
        }
        for (int row = 0; row < h && !tile.isNull(); ++row)
            std::memcpy(canvas.scanLine(y + row) + static_cast<qsizetype>(x) * 4, tile.constScanLine(row), static_cast<std::size_t>(w) * 4);
    }
    EXPECT_TRUE(r.ok());
    return copies;
}

TEST(FrameEncoder, ScrollIsCopiedWhenAskedAndExact)
{
    const QImage first = scrolled_frame(0);
    const QImage next = scrolled_frame(37);
    xdesk::platform::FrameEncoder plain;
    xdesk::platform::FrameEncoder copying;
    QImage plainCanvas;
    QImage canvas;
    long wrong = 0;

    plain.setPalette(true);
    copying.setPalette(true);
    copying.setCopy(true);
    (void)apply_frame(plainCanvas, plain.encode(first, 70));
    EXPECT_EQ(apply_frame(canvas, copying.encode(first, 70)), 0); // nothing to copy from yet
    const QByteArray without = plain.encode(next, 70);
    const QByteArray with = copying.encode(next, 70);
    EXPECT_EQ(apply_frame(plainCanvas, without), 0); // never asked: never a copy
    EXPECT_GT(apply_frame(canvas, with), 10);
    EXPECT_TRUE(copying.orderBound());
    EXPECT_LT(with.size() * 4, without.size()); // the scroll costs only the revealed lines
    std::cout << "scroll of 37 rows: " << without.size() / 1024 << " KiB without the copies, " << with.size() / 1024 << " KiB with" << std::endl;

    // The editor (lossless palette tiles and copies of them) is exactly the new frame
    for (int y = 40; y < 1020; ++y)
        for (int x = 40; x < 1140; ++x)
            wrong += (canvas.pixel(x, y) & 0xFFFFFF) != (next.pixel(x, y) & 0xFFFFFF);
    EXPECT_EQ(wrong, 0);

    // Still afterwards: nothing more, and a reset forgets the sources
    EXPECT_TRUE(copying.encode(next, 70).isEmpty());
    copying.reset();
    EXPECT_FALSE(copying.orderBound());
    EXPECT_EQ(apply_frame(canvas, copying.encode(scrolled_frame(80), 70)), 0);
}

// Not a check: time and bytes of a scroll without / with the copy tiles (run like the palette benchmark)
TEST(FrameEncoder, DISABLED_BenchmarkScroll)
{
    const QImage first = scrolled_frame(0);
    const QImage next = scrolled_frame(37);

    for (const bool copy: {false, true}) {
        qint64 scrollNs = 0;
        qsizetype scrollBytes = 0;
        for (int run = 0; run < 10; ++run) {
            xdesk::platform::FrameEncoder encoder;
            QElapsedTimer timer;
            encoder.setPalette(true);
            encoder.setCopy(copy);
            (void)encoder.encode(first, 70);
            timer.start();
            scrollBytes = encoder.encode(next, 70).size();
            scrollNs += timer.nsecsElapsed();
        }
        std::cout << (copy ? "copy " : "plain") << "  scroll " << scrollNs / 10000000.0 << " ms " << scrollBytes / 1024 << " KiB" << std::endl;
    }
}

// Whole frame moving (a video, a dragged window): every pixel changes with step
static QImage video_frame(const int step)
{
    QImage frame(640, 360, QImage::Format_RGB32);

    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            frame.setPixel(x, y, qRgb((x + step * 5) & 255, (y * 2 + step) & 255, ((x + y) / 2 + step * 7) & 255));
    return frame;
}

TEST(FrameEncoder, LargeMovesGoAsVideoWhenAskedThenTilesSharpOnceStill)
{
    xdesk::platform::FrameEncoder encoder;
    xdesk::platform::FrameEncoder tiles;
    xdesk::platform::H264Decoder decoder;
    const std::function<std::uint8_t(const QByteArray&)> type = [](const QByteArray& message) {return message.isEmpty() ? 0 : static_cast<std::uint8_t>(message[0]);};
    const std::function<bool(const QByteArray&)> key = [](const QByteArray& message) {return message.size() > 9 && (message[9] & 1) != 0;};

    if (!xdesk::platform::h264_available()) GTEST_SKIP() << xdesk::platform::h264_library().toStdString();
    encoder.setVideo(true);
    EXPECT_EQ(type(encoder.encode(video_frame(0), 70)), static_cast<std::uint8_t>(xdesk::net::Binary::Frame)); // the first frame: sharp tiles
    EXPECT_EQ(type(tiles.encode(video_frame(0), 70)), static_cast<std::uint8_t>(xdesk::net::Binary::Frame));
    for (int step = 1; step < 4; ++step) {
        const QByteArray message = encoder.encode(video_frame(step), 70);
        ASSERT_EQ(type(message), static_cast<std::uint8_t>(xdesk::net::Binary::Video));
        EXPECT_EQ(key(message), step == 1); // a key frame first, then predicted ones
        EXPECT_FALSE(decoder.decode(message.mid(10), 640, 360).isNull());
        EXPECT_EQ(type(tiles.encode(video_frame(step), 70)), static_cast<std::uint8_t>(xdesk::net::Binary::Frame)); // never asked: tiles
    }
    EXPECT_TRUE(encoder.orderBound());

    // Still: the whole screen sent again sharp in tiles, then nothing
    const QByteArray sharp = encoder.encode(video_frame(3), 70);
    ASSERT_EQ(type(sharp), static_cast<std::uint8_t>(xdesk::net::Binary::Frame));
    EXPECT_EQ(std::get<3>(tiles_of(sharp)), 640L * 360L);
    EXPECT_TRUE(encoder.encode(video_frame(3), 70).isEmpty());

    // After a reset: sharp tiles, then a key frame for a new decoder
    encoder.reset();
    EXPECT_FALSE(encoder.orderBound());
    EXPECT_EQ(type(encoder.encode(video_frame(4), 70)), static_cast<std::uint8_t>(xdesk::net::Binary::Frame));
    const QByteArray again = encoder.encode(video_frame(5), 70);
    ASSERT_EQ(type(again), static_cast<std::uint8_t>(xdesk::net::Binary::Video));
    EXPECT_TRUE(key(again));
}
