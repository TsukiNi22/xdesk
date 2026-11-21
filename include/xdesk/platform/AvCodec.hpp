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
##  @file AvCodec.hpp

File Description:
##  Hardware video encoders (NVENC, Quick Sync, VA-API) and software decoders of libavcodec, loaded at run time
\**************************************************************/

#ifndef AVCODEC_H
    #define AVCODEC_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>   // _hot, _cold, _nodiscard
    #include "IVideoEncoder.hpp" // xdesk::platform::IVideoEncoder, VideoCodec
    #include <QStringList>       // QStringList
    #include <QByteArray>        // QByteArray
    #include <QString>           // QString
    #include <QImage>            // QImage

//----------------------------------------------------------------//
/* CLASS */

struct AVCodecContext; // libavcodec (only in the .cpp)
struct AVBufferRef;
struct AVPacket;
struct AVFrame;

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard bool av_available(void); // libavcodec 62 + libavutil 60 (FFmpeg 8) loaded
_cold _nodiscard QString av_library(void); // the libraries, or why they are missing
_cold _nodiscard QStringList av_candidates(const VideoCodec codec); // hardware encoders tried for a codec, best first
_cold _nodiscard QStringList av_encoders(const VideoCodec codec); // those that open and give a frame at once on this computer (probed once, thread safe)
_cold _nodiscard bool av_decodes(const VideoCodec codec); // a software decoder of libavcodec exists for it

//----------------------------------------------------------------//
/* CLASS */

/*
 * A hardware encoder of libavcodec: constant quantizer (the flow control paces the frames), no B frame, no delay,
 * BT.709 limited range, the size rounded up to even (drawn cropped), an IDR only when asked
 *  nvenc: yuv420p from the cpu (preset p1, ultra low latency) / qsv: nv12 from the cpu / vaapi: nv12 uploaded to the gpu
*/
class AvEncoder: public IVideoEncoder {
    private:
        AVCodecContext* _ctx = nullptr;
        AVFrame* _frame = nullptr; // planes from the cpu
        AVFrame* _gpu = nullptr; // vaapi surface
        AVBufferRef* _device = nullptr; // vaapi device
        AVPacket* _packet = nullptr;
        QString _name = ""; // "h264_nvenc"...
        VideoCodec _codec = VideoCodec::H264;
        int _width = 0; // encoded size (even)
        int _height = 0;
        int _qp = 0; // quantizer of the open encoder (codec scale)
        int _bitrate = 0; // bits / s asked (0 = constant quantizer)
        int _rate = 0; // bits / s of the open encoder (0 = constant quantizer)
        long long _rateAt = 0; // ms of the last change of _rate
        long long _pts = 0;
        bool _key = true; // next frame is a key frame
        bool _broken = false; // failed to open: never tried again

        // ---------- Pre-Function -------- //
        _cold bool open_(const int width, const int height, const int qp);
        _cold void close_(void) noexcept;

    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard QByteArray encode(const QImage& frame, const int quality, bool* key) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline VideoCodec codec(void) const override {return this->_codec;};
        _cold _nodiscard inline QString name(void) const override     {return this->_name;};
        _cold _nodiscard inline bool hardware(void) const override    {return true;};
        _cold _nodiscard inline bool broken(void) const               {return this->_broken;};

        /* setter */
        _cold inline void forceKey(void) override {this->_key = true;};
        _cold inline void setBitrate(const int bits) override {this->_bitrate = bits > 0 ? bits : 0;};

        // ------------ Operator ---------- //
        AvEncoder& operator=(const AvEncoder&) = delete;

        // ---------- Constructor --------- //
        AvEncoder(const QString& name, const VideoCodec codec): _name{name}, _codec{codec} {};

        AvEncoder(const AvEncoder&) = delete;

        // ----------- Destructor --------- //
        ~AvEncoder() noexcept override {this->close_();};
};

// Software decoder of libavcodec (h264, hevc, av1): the bench and the viewers for the codecs openh264 cannot decode
class AvDecoder {
    private:
        AVCodecContext* _ctx = nullptr;
        AVFrame* _frame = nullptr;
        AVPacket* _packet = nullptr;
        VideoCodec _codec = VideoCodec::H264;

        // ---------- Pre-Function -------- //
        _cold bool open_(const VideoCodec codec);

    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard QImage decode(const QByteArray& data, const VideoCodec codec, const int width, const int height); // RGB32 cropped to width x height, null = nothing to show
        _cold void reset(void) noexcept; // the next data starts a new stream

        // ------------ Operator ---------- //
        AvDecoder& operator=(const AvDecoder&) = delete;

        // ---------- Constructor --------- //
        AvDecoder() = default;

        AvDecoder(const AvDecoder&) = delete;

        // ----------- Destructor --------- //
        ~AvDecoder() noexcept {this->reset();};
};

} // namespace end
#endif /* AVCODEC_H */
