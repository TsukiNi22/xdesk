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
##  @file IVideoEncoder.hpp

File Description:
##  Video encoder of the moving screen (openh264 or a hardware encoder of libavcodec), the codecs a client decodes
\**************************************************************/

#ifndef IVIDEOENCODER_H
    #define IVIDEOENCODER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <cstdint>          // std::uint8_t
    #include <QString>          // QString
    #include <QImage>           // QImage
    #include <memory>           // std::shared_ptr

    //----------------------------------------------------------------//
    /* DEFINE */

    #define VIDEO_H264 1 // bits of the codecs a client decodes ("h264", "hevc", "av1" in the "enc" of its hello)
    #define VIDEO_HEVC 2
    #define VIDEO_AV1 4

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* ENUM */

// Codec of a Binary::Video frame (bits 1-2 of its flags)
enum class VideoCodec: std::uint8_t {
    H264 = 0, // annex B access unit
    Hevc = 1, // annex B access unit (Main)
    Av1 = 2, // temporal unit of OBUs (low overhead format, a sequence header in the key frames)
};

//----------------------------------------------------------------//
/* CLASS */

// One stream of one client: every encode is one frame out at once (no B frame, no delay), an IDR when asked
class IVideoEncoder {
    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard virtual QByteArray encode(const QImage& frame, const int quality, bool* key) = 0; // the frame (empty = failed: tiles), key = a key frame
        _cold virtual void forceKey(void) = 0; // the next frame starts a new stream (new decoder on the client)
        _cold _nodiscard virtual VideoCodec codec(void) const = 0;
        _cold _nodiscard virtual QString name(void) const = 0; // "openh264", "h264_nvenc", "hevc_vaapi"...
        _cold _nodiscard virtual bool hardware(void) const = 0; // the gpu encodes (4k at 60 fps, almost no cpu)

        // ----------- Destructor --------- //
        virtual ~IVideoEncoder() = default;
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard QString video_codec_name(const VideoCodec codec); // "h264", "hevc", "av1"
_cold _nodiscard int video_codec_bit(const VideoCodec codec); // VIDEO_H264...
_cold _nodiscard QString video_encoders(void); // what this host can use: "av1_nvenc, h264_nvenc, openh264" (probed once)
// The best encoder for a client (codecs = its VIDEO_* bits, prefer = "auto" | "h264" | "hevc" | "av1", hardware = the gpu
// may encode): a hardware one of the preferred codec first, then the other codecs (av1, hevc, h264), then openh264 when the
// frame is small enough for the cpu; nullptr = tiles only
_cold _nodiscard std::shared_ptr<IVideoEncoder> create_video_encoder(const int codecs, const QString& prefer, const bool hardware, const int width, const int height);

} // namespace end
#endif /* IVIDEOENCODER_H */
