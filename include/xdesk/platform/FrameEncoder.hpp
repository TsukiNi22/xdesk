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
##  @file FrameEncoder.hpp

File Description:
##  Dirty tiles encoder: only the changed 64x64 tiles of a frame, jpeg, solid color, palette, copy (scroll) or one H.264 frame (large move)
\**************************************************************/

#ifndef FRAMEENCODER_H
    #define FRAMEENCODER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include "IVideoEncoder.hpp" // xdesk::platform::IVideoEncoder, create_video_encoder
    #include <QByteArray>       // QByteArray
    #include <functional>       // std::function
    #include <QVector>          // QVector
    #include <cstdint>          // std::uint32_t, std::uint64_t
    #include <QImage>           // QImage
    #include <memory>           // std::shared_ptr
    #include <QList>            // QList

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

_hot void parallel_for(const int count, const std::function<void(const int)>& fn); // fn(0) ... fn(count - 1) on the encoding threads and the caller, returns once all are done

//----------------------------------------------------------------//
/* CLASS */

// One encoder per session (each one knows what its client already has)
class FrameEncoder {
    private:
        QVector<std::uint64_t> _hashes; // hash of every tile sent (row major)
        QVector<std::uint8_t> _quality; // jpeg quality each tile was sent with (0 = solid / not sent)
        int _cols = 0; // tiles per row
        int _rows = 0; // tiles per column
        int _width = 0; // frame size of the hashes
        int _height = 0;
        std::uint32_t _id = 0; // frame id
        bool _palette = false; // the client decodes the palette tiles (lossless text / ui)
        bool _copy = false; // the client applies the copy tiles (scroll): only on a path that keeps the order of the frames
        bool _video = false; // a large move is sent as one H.264 frame (the client decodes it, its link is the bottleneck)
        bool _ordered = false; // copy tiles or video frames sent since the last reset: they need the frames in order
        QImage _previous; // last frame encoded (source of the copy tiles), kept only while _copy
        std::shared_ptr<IVideoEncoder> _encoder; // created at the first video frame (shared: the clients are copied into their table once)
        int _codecs = VIDEO_H264; // codecs the client decodes (VIDEO_* bits)
        QString _prefer = "auto"; // codec asked by the host settings (auto, h264, hevc, av1)
        bool _hardware = true; // a gpu encoder may be used
        int _videoFailures = 0; // encoders that failed in a row (3: tiles only until a reset)
        qsizetype _encoderPixels = 0; // frame size the encoder was chosen for (another size: chosen again)

        // ---------- Pre-Function -------- //
        _hot void findCopies_(const QImage& image, const QVector<std::uint64_t>& hashes, QVector<int>& offsets) const; // offsets[tile] = rows its content moved down since the previous frame (0 = not found)
        _hot _nodiscard QList<QByteArray> copyTiles_(const QVector<int>& offsets); // copy tiles of the found offsets (runs of a row), their quality taken from their sources
        _hot _nodiscard QByteArray encodeVideo_(const QImage& image, const QVector<std::uint64_t>& hashes, const int quality); // Binary::Video message (empty = failed: tiles), every tile then known as lossy

    public:
        // ---------- Pre-Function -------- //
        _hot _nodiscard QByteArray encode(const QImage& frame, const int quality); // Binary::Frame message, empty = nothing changed (moving parts lighter, sent again sharp once still)
        _cold void reset(void); // next frame is complete (new client, resize, quality change)
        _cold void setCodecs(const int codecs, const QString& prefer, const bool hardware); // what the client decodes, the settings of the host (a new encoder at the next video frame)
        _hot _nodiscard static QByteArray encodeJpeg(const QImage& image, const int quality); // libjpeg-turbo
        _hot _nodiscard static QImage decodeJpeg(const QByteArray& data); // libjpeg-turbo, RGB32 (null on error)

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline std::uint32_t lastId(void) const {return this->_id;};
        _cold _nodiscard inline bool orderBound(void) const      {return this->_ordered;};
        _cold _nodiscard inline QString videoEncoder(void) const {return this->_encoder ? this->_encoder->name() : QString();}; // name of the encoder in use ("" = none yet)
        /* setter */
        _cold inline void setPalette(const bool enabled) {this->_palette = enabled;};
        _cold inline void setCopy(const bool enabled)    {this->_copy = enabled; if (!enabled) this->_previous = QImage();};
        _cold inline void setVideo(const bool enabled)   {this->_video = enabled;};

        // ---------- Constructor --------- //
        FrameEncoder() = default;

        // ----------- Destructor --------- //
        ~FrameEncoder() = default;
};

} // namespace end
#endif /* FRAMEENCODER_H */
