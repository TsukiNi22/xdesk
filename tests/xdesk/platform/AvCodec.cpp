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
#include "xdesk/platform/H264.hpp"
#include "xdesk/host/Compress.hpp"
#include "xdesk/host/Bench.hpp"
#include "xdesk/net/Protocol.hpp"
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QPainter>
#include <QThread>
#include <iostream>
#include <memory>
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

// The encoder of the bench: the fastest one while the link has room, the smallest frames once it is the bottleneck
TEST(Bench, BestEncoderFollowsTheLink)
{
    QFile file(xdesk::host::bench_file());
    const QJsonArray results = {
        QJsonObject{{"height", 2160}, {"scene", "video"}, {"mode", "hevc_nvenc"}, {"encodeMs", 17.0}, {"bytes", 38000.0}, {"psnr", 35.0}},
        QJsonObject{{"height", 2160}, {"scene", "drag"}, {"mode", "hevc_nvenc"}, {"encodeMs", 17.0}, {"bytes", 14000.0}, {"psnr", 34.0}},
        QJsonObject{{"height", 2160}, {"scene", "video"}, {"mode", "hevc_vaapi"}, {"encodeMs", 24.0}, {"bytes", 22000.0}, {"psnr", 35.0}},
        QJsonObject{{"height", 2160}, {"scene", "drag"}, {"mode", "hevc_vaapi"}, {"encodeMs", 24.0}, {"bytes", 8000.0}, {"psnr", 34.0}},
        QJsonObject{{"height", 2160}, {"scene", "video"}, {"mode", "tiles"}, {"encodeMs", 9.0}, {"bytes", 380000.0}, {"psnr", 37.0}},
        QJsonObject{{"height", 2160}, {"scene", "video"}, {"mode", "h264_nvenc"}, {"encodeMs", 17.0}, {"bytes", 34000.0}, {"psnr", 35.0}},
    };

    ASSERT_TRUE(QDir().mkpath(QFileInfo(file).absolutePath()));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(QJsonObject{{"version", 1}, {"results", results}}).toJson());
    file.close();
    EXPECT_EQ(xdesk::host::bench_best_encoder(2160, 500.0, 5.0, VIDEO_HEVC | VIDEO_H264), "hevc_nvenc"); // room on the link: the fastest
    EXPECT_EQ(xdesk::host::bench_best_encoder(2160, 4.0, 120.0, VIDEO_HEVC | VIDEO_H264), "hevc_vaapi"); // 4 Mb/s: the smallest frames
    EXPECT_EQ(xdesk::host::bench_best_encoder(2160, 500.0, 5.0, VIDEO_H264), "h264_nvenc"); // only what the viewer decodes
    EXPECT_EQ(xdesk::host::bench_best_encoder(2160, 500.0, 5.0, VIDEO_AV1), ""); // nothing measured for it: the default order
}

// Sized for the link (CBR): the frames of a moving picture stay near bitrate / frame rate, far under the constant quantizer
TEST(AvCodec, BitrateSizesTheFrames)
{
    QList<std::shared_ptr<xdesk::platform::IVideoEncoder>> encoders;

    for (const xdesk::platform::VideoCodec codec: {xdesk::platform::VideoCodec::H264, xdesk::platform::VideoCodec::Hevc}) // the gpu ones (openh264 stays at a quantizer)
        for (const QString& name: xdesk::platform::av_encoders(codec))
            encoders << std::make_shared<xdesk::platform::AvEncoder>(name, codec);
    if (encoders.isEmpty()) GTEST_SKIP() << "no video encoder on this computer";
    for (const std::shared_ptr<xdesk::platform::IVideoEncoder>& encoder: encoders) {
        double free = 0.0;
        double sized = 0.0;
        bool key = false;
        for (int i = 0; i < 30; ++i)
            free += encoder->encode(moving(640, 360, i * 3), 90, &key).size();
        encoder->setBitrate(500000); // 500 kbit/s: about 1 KB per frame at 60 fps (the mode change reopens the encoder at the next frame paced)
        QThread::msleep(2100);
        for (int i = 30; i < 110; ++i) {
            const qsizetype bytes = encoder->encode(moving(640, 360, i * 3), 90, &key).size();
            if (i >= 50) sized += static_cast<double>(bytes); // after the key frame of the switch and the convergence of the rate control
        }
        std::cout << encoder->name().toStdString() << ": quality 90 " << free / 30.0 << " B/frame, 500 kbit/s " << sized / 60.0 << " B/frame" << std::endl;
        EXPECT_LT(sized / 60.0, free / 30.0 / 2.0) << encoder->name().toStdString(); // at least twice lighter than quality 90
        EXPECT_LT(sized / 60.0, 500000.0 / 8.0 / 60.0 * 3.0) << encoder->name().toStdString(); // near the budget of a frame
    }
}

// An update the link cannot carry in two frame intervals is sent lighter at once
TEST(FrameEncoder, LargeUpdateFitsTheLink)
{
    xdesk::platform::FrameEncoder free;
    xdesk::platform::FrameEncoder slow;

    slow.setLink(4000, 0); // 4 KB per frame interval (about 2 Mbit/s at 60 fps)
    const qsizetype big = free.encode(moving(640, 384, 0), 70).size();
    const qsizetype light = slow.encode(moving(640, 384, 0), 70).size();
    EXPECT_LT(light, big * 3 / 4); // a smooth picture: its jpeg headers weigh, the quality less
    EXPECT_GT(light, 0);
}
