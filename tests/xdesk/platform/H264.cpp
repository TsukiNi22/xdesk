/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 07/11/2026 by @author Tsukini

File Name:
##  @file H264.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/H264.hpp"
#include <gtest/gtest.h>
#include <QElapsedTimer>
#include <iostream>
#include <cstdlib>
#include <QImage>

// Mean absolute error of the rgb of two images of the same size
static double mean_error(const QImage& a, const QImage& b)
{
    double total = 0.0;

    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const QRgb p = a.pixel(x, y);
            const QRgb q = b.pixel(x, y);
            total += std::abs(qRed(p) - qRed(q)) + std::abs(qGreen(p) - qGreen(q)) + std::abs(qBlue(p) - qBlue(q));
        }
    return total / (3.0 * a.width() * a.height());
}

// Moving picture: soft gradients and blocks of colors shifted by step (odd size: padded to even by the encoder)
static QImage moving_frame(const int step)
{
    QImage frame(641, 361, QImage::Format_RGB32);

    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            frame.setPixel(x, y, qRgb((x + step * 4) & 255, (y * 2) & 255, ((x + y) / 2 + step * 8) & 255));
    for (int i = 0; i < 6; ++i)
        for (int y = 40 + i * 50; y < 70 + i * 50; ++y)
            for (int x = 30 + i * 60 + step * 3; x < 90 + i * 60 + step * 3 && x < frame.width(); ++x)
                frame.setPixel(x, y, qRgb(40 * i, 255 - 40 * i, 128));
    return frame;
}

TEST(H264, EncodedFramesDecodeCloseToTheSource)
{
    xdesk::platform::H264Encoder encoder;
    xdesk::platform::H264Decoder decoder;
    qsizetype keySize = 0;
    qsizetype deltaSize = 0;

    if (!xdesk::platform::h264_available()) GTEST_SKIP() << xdesk::platform::h264_library().toStdString();
    std::cout << "library: " << xdesk::platform::h264_library().toStdString() << std::endl;
    for (int step = 0; step < 6; ++step) {
        const QImage frame = moving_frame(step);
        bool key = false;
        const QByteArray unit = encoder.encode(frame, 70, &key);
        ASSERT_FALSE(unit.isEmpty());
        EXPECT_EQ(key, step == 0); // an IDR first, then only predicted frames
        if (step == 0) keySize = unit.size();
        else deltaSize = std::max(deltaSize, unit.size());
        const QImage decoded = decoder.decode(unit, frame.width(), frame.height());
        ASSERT_FALSE(decoded.isNull());
        EXPECT_EQ(decoded.size(), frame.size()); // cropped back to the odd size
        EXPECT_LT(mean_error(decoded, frame), 6.0);
    }
    EXPECT_LT(deltaSize, keySize);

    // Asked again: an IDR a new decoder can start from
    xdesk::platform::H264Decoder fresh;
    bool key = false;
    encoder.forceKey();
    const QByteArray unit = encoder.encode(moving_frame(7), 70, &key);
    EXPECT_TRUE(key);
    EXPECT_FALSE(fresh.decode(unit, 641, 361).isNull());
}

TEST(H264, LowerQualityIsLighter)
{
    xdesk::platform::H264Encoder sharp;
    xdesk::platform::H264Encoder light;
    bool key = false;

    if (!xdesk::platform::h264_available()) GTEST_SKIP() << xdesk::platform::h264_library().toStdString();
    EXPECT_LT(light.encode(moving_frame(0), 20, &key).size(), sharp.encode(moving_frame(0), 95, &key).size());
}

// Not a check: time and bytes of 1080p frames (run: unit_tests --gtest_also_run_disabled_tests --gtest_filter='*Benchmark*')
TEST(H264, DISABLED_BenchmarkEncode)
{
    xdesk::platform::H264Encoder encoder;
    QImage frame(1920, 1080, QImage::Format_RGB32);
    qint64 total = 0;
    qsizetype bytes = 0;

    if (!xdesk::platform::h264_available()) GTEST_SKIP() << xdesk::platform::h264_library().toStdString();
    for (int step = 0; step < 31; ++step) {
        QElapsedTimer timer;
        bool key = false;
        for (int y = 0; y < 1080; ++y)
            for (int x = 0; x < 1920; ++x)
                frame.setPixel(x, y, qRgb((x + step * 6) & 255, (y + step * 2) & 255, ((x ^ y) + step) & 255));
        timer.start();
        const qsizetype size = encoder.encode(frame, 70, &key).size();
        if (step == 0) continue; // the IDR
        total += timer.nsecsElapsed();
        bytes += size;
    }
    std::cout << "1080p predicted frame: " << total / 30 / 1000000.0 << " ms " << bytes / 30 / 1024 << " KiB" << std::endl;
}
