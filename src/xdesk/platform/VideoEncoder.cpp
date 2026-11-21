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
##  @file VideoEncoder.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/IVideoEncoder.hpp"
#include "xdesk/platform/AvCodec.hpp"
#include "xdesk/platform/H264.hpp"
#include <QStringList>

//----------------------------------------------------------------//
/* DEFINE */

#define SOFTWARE_MAX_PIXELS (1920 * 1200) // larger frames stay in tiles without a gpu encoder (openh264 would hold the stream back)
#define HARDWARE_MAX_PIXELS (4096 * 2304) // 4k (and a little more): what the gpu encoders take at 60 fps

//----------------------------------------------------------------//
/* FUNCTION */

QString xdesk::platform::video_codec_name(const VideoCodec codec)
{
    switch (codec) {
        case VideoCodec::Hevc: return "hevc";
        case VideoCodec::Av1: return "av1";
        default: return "h264";
    }
}

int xdesk::platform::video_codec_bit(const VideoCodec codec)
{
    switch (codec) {
        case VideoCodec::Hevc: return VIDEO_HEVC;
        case VideoCodec::Av1: return VIDEO_AV1;
        default: return VIDEO_H264;
    }
}

QString xdesk::platform::video_encoders(void)
{
    QStringList names;

    for (const VideoCodec codec: {VideoCodec::Av1, VideoCodec::Hevc, VideoCodec::H264})
        names << av_encoders(codec);
    if (h264_available()) names << "openh264";
    return names.isEmpty() ? QString("none") : names.join(", ");
}

std::shared_ptr<xdesk::platform::IVideoEncoder> xdesk::platform::create_video_encoder(const int codecs, const QString& prefer, const bool hardware, const int width, const int height)
{
    QList<VideoCodec> order = {VideoCodec::Av1, VideoCodec::Hevc, VideoCodec::H264}; // the smallest frames first at the same quality
    const qsizetype pixels = static_cast<qsizetype>(width) * height;

    if (prefer == "h264") order = {VideoCodec::H264, VideoCodec::Hevc, VideoCodec::Av1};
    else if (prefer == "hevc") order = {VideoCodec::Hevc, VideoCodec::Av1, VideoCodec::H264};
    if (hardware && pixels <= HARDWARE_MAX_PIXELS) {
        for (const VideoCodec codec: order) {
            if (!(codecs & video_codec_bit(codec))) continue;
            const QStringList names = av_encoders(codec);
            if (!names.isEmpty()) return std::make_shared<AvEncoder>(names.first(), codec);
        }
    }
    if ((codecs & VIDEO_H264) && h264_available() && pixels <= SOFTWARE_MAX_PIXELS) return std::make_shared<H264Encoder>();
    return nullptr;
}
