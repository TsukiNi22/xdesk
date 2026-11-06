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
##  @file Stream.hpp

File Description:
##  Capture thread of the host: grabs the screen at the asked rate and encodes it for each ready session
\**************************************************************/

#ifndef STREAM_H
    #define STREAM_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>                      // _hot, _cold, _nodiscard
    #include "../platform/IScreenCapture.hpp"       // xdesk::platform::IScreenCapture
    #include "../platform/FrameEncoder.hpp"         // xdesk::platform::FrameEncoder
    #include "FlowControl.hpp"                      // xdesk::host::FlowControl
    #include <QByteArray>                           // QByteArray
    #include <algorithm>                            // std::max
    #include <QObject>                              // QObject
    #include <QString>                              // QString
    #include <cstdint>                              // std::uint32_t
    #include <QTimer>                               // QTimer
    #include <QPoint>                               // QPoint
    #include <memory>                               // std::unique_ptr
    #include <QSize>                                // QSize
    #include <QHash>                                // QHash

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct StreamClient {
    platform::FrameEncoder encoder; // what this client already has
    FlowControl flow; // frames in flight and quality, from the bandwidth and the ping of this client
    int quality = 70; // jpeg quality
    double scale = 1.0; // frame scale asked by the client
    double effective = 0.0; // scale applied: the asked one, capped by the max height of the host (0 = not computed yet)
    int fps = 30; // cap
    bool paused = false; // the client does not look at the screen (terminal tab, hidden page): no frame
    qint64 lastFrame = 0; // ms of the last frame sent
    qint64 idle = 0; // cacheKey of a captured frame that had nothing new for this client (skipped while the capture gives it again)
};

//----------------------------------------------------------------//
/* CLASS */

// Lives in its own QThread: every public slot is called through queued connections / QMetaObject::invokeMethod
class Stream: public QObject {
    Q_OBJECT

    private:
        std::unique_ptr<platform::IScreenCapture> _capture; // created in the thread
        QHash<QString, StreamClient> _clients; // session id -> state
        QTimer* _timer = nullptr; // 4 ms tick: frame pacing (created in the thread)
        QTimer* _idle = nullptr; // closes the capture a while after the last viewer left
        QPoint _cursor = {-1, -1}; // last cursor sent
        QSize _size; // captured screen size
        QString _backend = ""; // name of the capture backend
        int _maxHeight = 0; // frames at most this tall, the width follows (0 = the screen size)

        // ---------- Pre-Function -------- //
        void tick_(void);
        void openCapture_(void); // first viewer: the capture starts only now (the Wayland portal asks the user here)
        void openIfWatched_(void); // the capture opened when a client looks at the screen (not paused)
        void closeCapture_(void); // no viewer anymore: the screen is released

    public slots:
        // ---------- Pre-Function -------- //
        void start(void); // prepare the timers (the capture waits for the first viewer)
        void stop(void);
        void addClient(const QString& session, const int quality, const double scale, const int fps);
        void removeClient(const QString& session);
        void ack(const QString& session, const std::uint32_t id);
        void setQuality(const QString& session, const int quality, const double scale, const int fps);
        void refresh(const QString& session); // next frame complete
        void setPaused(const QString& session, const bool paused);

        // ------------ Function ---------- //
        inline void setMaxHeight(const int height) {this->_maxHeight = std::max(0, height);}; // 0 = the screen size

    public:
        // ---------- Constructor --------- //
        explicit Stream(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Stream() override;

    signals:
        void ready(const QSize& size, const QString& backend);
        void closed(void); // capture released (no viewer)
        void failed(const QString& error);
        void frame(const QString& session, const QByteArray& message); // Binary::Frame for this session
        void cursor(const QPoint& position); // Binary::Cursor for every session
        void resized(const QSize& size);
        void scaled(const QString& session, const double scale); // scale applied to the frames of this session (input / cursor mapping)
};

} // namespace end
#endif /* STREAM_H */
