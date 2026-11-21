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
##  @file H264.hpp

File Description:
##  H.264 video of the moving screen: openh264 (loaded at run time) encoder and decoder, BGRX <-> I420 BT.709
\**************************************************************/

#ifndef H264_H
    #define H264_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include "IVideoEncoder.hpp" // xdesk::platform::IVideoEncoder
    #include <QByteArray>       // QByteArray
    #include <QString>          // QString
    #include <QImage>           // QImage

//----------------------------------------------------------------//
/* CLASS */

class ISVCEncoder; // openh264 (wels/codec_api.h, only in the .cpp)
class ISVCDecoder;

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard bool h264_available(void); // openh264 2.6+ loaded: the video can be encoded / decoded
_cold _nodiscard QString h264_library(void); // path of the library, or why it is missing
// BGRX -> 4:2:0 BT.709 limited range (u = nullptr: nv12, the chroma interleaved in v), the frame repeated up to width x height (even)
_hot void bgrx_to_yuv420(const QImage& frame, const int width, const int height, unsigned char* y, const int yStride, unsigned char* u, const int uStride, unsigned char* v, const int vStride);
_hot void yuv420_to_bgrx(const unsigned char* const planes[3], const int yStride, const int uStride, const int vStride, QImage& image); // I420 BT.709 limited range -> RGB32 (the size of the image)

//----------------------------------------------------------------//
/* CLASS */

// H.264 of one client while its screen moves: screen content real time mode, no B frame, one access unit per frame
class H264Encoder: public IVideoEncoder {
    private:
        ISVCEncoder* _encoder = nullptr;
        QByteArray _yuv; // I420 planes of the frame (even size)
        int _width = 0; // encoded size (the frame rounded up to even)
        int _height = 0;
        int _qp = 0; // constant quantizer of the open encoder
        long long _stamp = 0; // ms of the next frame
        bool _key = true; // next frame is an IDR

        // ---------- Pre-Function -------- //
        _cold bool open_(const int width, const int height, const int qp);
        _cold void close_(void) noexcept;

    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard QByteArray encode(const QImage& frame, const int quality, bool* key) override; // annex B access unit (empty = failed), key = an IDR

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline VideoCodec codec(void) const override {return VideoCodec::H264;};
        _cold _nodiscard inline QString name(void) const override     {return "openh264";};
        _cold _nodiscard inline bool hardware(void) const override    {return false;};

        /* setter */
        _cold inline void forceKey(void) override {this->_key = true;}; // the next frame starts a new stream (new decoder on the client)

        // ------------ Operator ---------- //
        H264Encoder& operator=(const H264Encoder&) = delete;

        // ---------- Constructor --------- //
        H264Encoder() = default;

        H264Encoder(const H264Encoder&) = delete;

        // ----------- Destructor --------- //
        ~H264Encoder() noexcept override {this->close_();};
};

// Decoder of the video of a host (viewer)
class H264Decoder {
    private:
        ISVCDecoder* _decoder = nullptr;

        // ---------- Pre-Function -------- //
        _cold bool open_(void);

    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard QImage decode(const QByteArray& data, const int width, const int height); // RGB32 frame cropped to width x height, null = nothing to show (an error: wait for a key frame)
        _cold void reset(void) noexcept; // the next data starts a new stream

        // ------------ Operator ---------- //
        H264Decoder& operator=(const H264Decoder&) = delete;

        // ---------- Constructor --------- //
        H264Decoder() = default;

        H264Decoder(const H264Decoder&) = delete;

        // ----------- Destructor --------- //
        ~H264Decoder() noexcept {this->reset();};
};

} // namespace end
#endif /* H264_H */
