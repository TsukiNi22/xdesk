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
##  @file Stream.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/net/Protocol.hpp"
#include "xdesk/host/Stream.hpp"
#include <QDateTime>
#include <QImage>
#include <QHash>
#include <cmath>

xdesk::host::Stream::Stream(QObject* parent)
: QObject(parent)
{
    this->_timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&this->_timer, &QTimer::timeout, this, &Stream::tick_);
}

xdesk::host::Stream::~Stream()
{
    this->stop();
}

void xdesk::host::Stream::start(void)
{
    this->_capture = platform::create_screen_capture();
    if (!this->_capture) {
        emit this->failed("No screen capture backend available on this system");
        return;
    }
    this->_size = this->_capture->size();
    this->_backend = this->_capture->name();
    this->_timer.start(4);
    emit this->ready(this->_size, this->_backend);
}

void xdesk::host::Stream::stop(void)
{
    this->_timer.stop();
    this->_clients.clear();
    if (this->_capture) this->_capture->stop();
    this->_capture.reset();
}

void xdesk::host::Stream::addClient(const QString& session, const int quality, const double scale, const int fps)
{
    StreamClient client;

    client.quality = quality;
    client.scale = scale;
    client.fps = fps;
    this->_clients.insert(session, client);
}

void xdesk::host::Stream::removeClient(const QString& session)
{
    this->_clients.remove(session);
}

void xdesk::host::Stream::ack(const QString& session, const std::uint32_t id)
{
    QHash<QString, StreamClient>::iterator it = this->_clients.find(session);

    (void)id;
    if (it != this->_clients.end() && it->inFlight > 0) it->inFlight--;
}

void xdesk::host::Stream::setQuality(const QString& session, const int quality, const double scale, const int fps)
{
    QHash<QString, StreamClient>::iterator it = this->_clients.find(session);

    if (it == this->_clients.end()) return;
    if (std::abs(it->scale - scale) > 0.001) it->encoder.reset();
    it->quality = quality;
    it->scale = scale;
    it->fps = fps;
}

void xdesk::host::Stream::refresh(const QString& session)
{
    QHash<QString, StreamClient>::iterator it = this->_clients.find(session);

    if (it != this->_clients.end()) it->encoder.reset();
}

void xdesk::host::Stream::tick_(void)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QHash<int, QImage> scaled; // scale (per mille) -> frame
    QImage frame;
    bool anyReady = false;

    if (!this->_capture || this->_clients.isEmpty()) return;

    // Who can take a frame now (pacing + frames in flight)
    for (StreamClient& client: this->_clients) {
        if (client.inFlight >= XDESK_FRAMES_IN_FLIGHT && now - client.lastFrame > 1500) client.inFlight = 0; // ack lost: resume
        if (client.inFlight < XDESK_FRAMES_IN_FLIGHT && now - client.lastFrame >= 1000 / std::max(1, client.fps)) anyReady = true;
    }
    if (!anyReady) return;
    frame = this->_capture->grab();
    if (frame.isNull()) return;

    // Resolution changed: every client gets a complete frame
    if (frame.size() != this->_size) {
        this->_size = frame.size();
        for (StreamClient& client: this->_clients)
            client.encoder.reset();
        emit this->resized(this->_size);
    }

    // Encode for every ready client (its own dirty tiles)
    for (QHash<QString, StreamClient>::iterator it = this->_clients.begin(); it != this->_clients.end(); ++it) {
        StreamClient& client = it.value();
        const int key = static_cast<int>(std::lround(client.scale * 1000));
        QByteArray message;

        if (client.inFlight >= XDESK_FRAMES_IN_FLIGHT || now - client.lastFrame < 1000 / std::max(1, client.fps)) continue;
        if (!scaled.contains(key))
            scaled.insert(key, key >= 1000 ? frame : frame.scaled(std::max(1, static_cast<int>(frame.width() * client.scale)), std::max(1, static_cast<int>(frame.height() * client.scale)), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        client.lastFrame = now;
        message = client.encoder.encode(scaled.value(key), client.quality);
        if (message.isEmpty()) continue;
        client.inFlight++;
        emit this->frame(it.key(), message);
    }

    // Cursor (screen coordinates, scaled by the host per session)
    const QPoint cursor = this->_capture->cursor();
    if (cursor != this->_cursor) {
        this->_cursor = cursor;
        emit this->cursor(cursor);
    }
}
