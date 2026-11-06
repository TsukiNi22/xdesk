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
#include <gtest/gtest.h>
#include <QPainter>
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
        else {solid++; (void)r.u8(); (void)r.u8(); (void)r.u8();}
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
