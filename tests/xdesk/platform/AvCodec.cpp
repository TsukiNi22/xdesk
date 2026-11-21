/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/11/2026 by @author Tsukini

File Name:
##  @file AvCodec.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/FrameEncoder.hpp"
#include "xdesk/platform/AvCodec.hpp"
#include "xdesk/host/Compress.hpp"
#include "xdesk/net/Protocol.hpp"
#include <gtest/gtest.h>
#include <QPainter>
#include <cstdlib>
#include <QImage>

// A screen with a moving picture over it: the large move the video is made for
static QImage moving(const int width, const int height, const int shift)
{
    QImage image(width, height, QImage::Format_RGB32);

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, qRgb((x + shift * 9) % 256, (y * 2 + shift * 5) % 256, (x + y + shift * 3) % 256));
    return image;
}

static double mean_error(const QImage& a, const QImage& b)
{
    double total = 0.0;

    for (int y = 0; y < a.height(); y += 2)
        for (int x = 0; x < a.width(); x += 2) {
            const QRgb p = a.pixel(x, y);
            const QRgb q = b.pixel(x, y);
            total += std::abs(qRed(p) - qRed(q)) + std::abs(qGreen(p) - qGreen(q)) + std::abs(qBlue(p) - qBlue(q));
        }
    return total / (3.0 * (a.width() / 2) * (a.height() / 2));
}

// Every codec a gpu of this computer encodes: a large move goes as one Video message of that codec, decoded back close to the frame
TEST(AvCodec, VideoMessageOfEveryHardwareCodec)
{
    int tested = 0;

    if (!xdesk::platform::av_available()) GTEST_SKIP() << "FFmpeg 8 libraries missing: " << xdesk::platform::av_library().toStdString();
    for (const xdesk::platform::VideoCodec codec: {xdesk::platform::VideoCodec::H264, xdesk::platform::VideoCodec::Hevc, xdesk::platform::VideoCodec::Av1}) {
        if (xdesk::platform::av_encoders(codec).isEmpty() || !xdesk::platform::av_decodes(codec)) continue;
        xdesk::platform::FrameEncoder encoder;
        xdesk::platform::AvDecoder decoder;
        encoder.setCodecs(xdesk::platform::video_codec_bit(codec), xdesk::platform::video_codec_name(codec), true);
        encoder.setVideo(true);
        ASSERT_FALSE(encoder.encode(moving(640, 360, 0), 70).isEmpty()); // the complete first frame (tiles)
        const QImage frame = moving(640, 360, 7);
        const QByteArray message = encoder.encode(frame, 70);
        ASSERT_GT(message.size(), 10);
        ASSERT_EQ(static_cast<std::uint8_t>(message[0]), static_cast<std::uint8_t>(xdesk::net::Binary::Video)) << xdesk::platform::video_codec_name(codec).toStdString();
        EXPECT_EQ((static_cast<std::uint8_t>(message[9]) >> 1) & 3, static_cast<int>(codec));
        EXPECT_EQ(static_cast<std::uint8_t>(message[9]) & 1, 1); // a new encoder starts with a key frame
        const QImage shown = decoder.decode(message.mid(10), codec, 640, 360);
        ASSERT_FALSE(shown.isNull()) << encoder.videoEncoder().toStdString();
        EXPECT_LT(mean_error(frame, shown), 12.0) << encoder.videoEncoder().toStdString();
        ++tested;
    }
    if (tested == 0) GTEST_SKIP() << "no gpu encoder on this computer";
}

// Without any video codec the large moves stay in tiles
TEST(AvCodec, NoCodecKeepsTiles)
{
    xdesk::platform::FrameEncoder encoder;

    encoder.setCodecs(0, "auto", true);
    encoder.setVideo(true);
    ASSERT_FALSE(encoder.encode(moving(320, 192, 0), 70).isEmpty());
    const QByteArray message = encoder.encode(moving(320, 192, 7), 70);
    ASSERT_FALSE(message.isEmpty());
    EXPECT_EQ(static_cast<std::uint8_t>(message[0]), static_cast<std::uint8_t>(xdesk::net::Binary::Frame));
}

// Every compressor gives back what it got (zlib always, lz4 / zstd when their library is there)
TEST(Compress, RoundTrip)
{
    QByteArray text;

    for (int i = 0; i < 2000; ++i)
        text += "line " + QByteArray::number(i) + " of a build log: [ok] src/xdesk/host/Stream.cpp\r\n";
    ASSERT_TRUE(xdesk::host::compress_codecs().contains("zlib"));
    for (const QString& codec: xdesk::host::compress_codecs()) {
        const QByteArray packed = xdesk::host::compress_with(codec, text);
        ASSERT_FALSE(packed.isEmpty()) << codec.toStdString();
        EXPECT_LT(packed.size(), text.size() / 2) << codec.toStdString();
        EXPECT_EQ(xdesk::host::decompress_with(codec, packed, static_cast<int>(text.size())), text) << codec.toStdString();
    }
    EXPECT_TRUE(xdesk::host::decompress_with("zlib", "not zlib", 100).isEmpty());
}
