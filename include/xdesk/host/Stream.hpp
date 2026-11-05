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
    #include <QByteArray>                           // QByteArray
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
    int inFlight = 0; // frames sent without ack
    int quality = 70; // jpeg quality
    double scale = 1.0; // frame scale
    int fps = 30; // cap
    qint64 lastFrame = 0; // ms of the last frame sent
};

//----------------------------------------------------------------//
/* CLASS */

// Lives in its own QThread: every public slot is called through queued connections / QMetaObject::invokeMethod
class Stream: public QObject {
    Q_OBJECT

    private:
        std::unique_ptr<platform::IScreenCapture> _capture; // created in the thread
        QHash<QString, StreamClient> _clients; // session id -> state
        QTimer _timer; // 4 ms tick: frame pacing
        QPoint _cursor = {-1, -1}; // last cursor sent
        QSize _size; // captured screen size
        QString _backend = ""; // name of the capture backend

        // ---------- Pre-Function -------- //
        void tick_(void);

    public slots:
        void start(void); // create the capture (emits ready / failed)
        void stop(void);
        void addClient(const QString& session, const int quality, const double scale, const int fps);
        void removeClient(const QString& session);
        void ack(const QString& session, const std::uint32_t id);
        void setQuality(const QString& session, const int quality, const double scale, const int fps);
        void refresh(const QString& session); // next frame complete

    public:
        // ---------- Constructor --------- //
        explicit Stream(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Stream() override;

    signals:
        void ready(const QSize& size, const QString& backend);
        void failed(const QString& error);
        void frame(const QString& session, const QByteArray& message); // Binary::Frame for this session
        void cursor(const QPoint& position); // Binary::Cursor for every session
        void resized(const QSize& size);
};

} // namespace end
#endif /* STREAM_H */
