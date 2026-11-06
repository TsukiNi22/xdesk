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

//----------------------------------------------------------------//
/* DEFINE */

#define OPEN_DELAY 400 // ms waited after a new client before the capture starts (its pause message comes first)

xdesk::host::Stream::Stream(QObject* parent)
: QObject(parent)
{
    /* Nothing */
}

xdesk::host::Stream::~Stream()
{
    this->stop();
}

void xdesk::host::Stream::start(void)
{
    // Timers of the stream thread: created here so they belong to it
    this->_timer = new QTimer(this);
    this->_timer->setTimerType(Qt::PreciseTimer);
    QObject::connect(this->_timer, &QTimer::timeout, this, &Stream::tick_);
    this->_idle = new QTimer(this);
    this->_idle->setSingleShot(true);
    QObject::connect(this->_idle, &QTimer::timeout, this, &Stream::closeCapture_);
}

void xdesk::host::Stream::stop(void)
{
    this->_clients.clear();
    if (this->_idle) this->_idle->stop();
    this->closeCapture_();
}

void xdesk::host::Stream::openCapture_(void)
{
    QString error = "";

    if (this->_capture) return;
    this->_capture = platform::create_screen_capture(error);
    if (!this->_capture) {
        emit this->failed(error.isEmpty() ? QString("no screen capture backend on this system") : error);
        return;
    }
    this->_size = this->_capture->size();
    this->_backend = this->_capture->name();
    this->_cursor = {-1, -1};
    if (this->_timer) this->_timer->start(4);
    emit this->ready(this->_size, this->_backend);
}

void xdesk::host::Stream::closeCapture_(void)
{
    if (!this->_capture || !this->_clients.isEmpty()) return;
    if (this->_timer) this->_timer->stop();
    this->_capture->stop();
    this->_capture.reset();
    emit this->closed();
}

void xdesk::host::Stream::addClient(const QString& session, const int quality, const double scale, const int fps)
{
    StreamClient client;

    client.quality = quality;
    client.scale = scale;
    client.fps = fps;
    this->_clients.insert(session, client);
    if (this->_idle) this->_idle->stop();
    if (!this->_capture) QTimer::singleShot(OPEN_DELAY, this, &Stream::openIfWatched_); // a client opening on a terminal pauses at once: no capture for it
    else if (this->_timer && !this->_timer->isActive()) this->_timer->start(4);
}

void xdesk::host::Stream::openIfWatched_(void)
{
    if (this->_capture) return;
    for (const StreamClient& client: this->_clients)
        if (!client.paused) {this->openCapture_(); return;}
}

void xdesk::host::Stream::removeClient(const QString& session)
{
    this->_clients.remove(session);
    if (this->_clients.isEmpty() && this->_idle) this->_idle->start(15000); // a quick reconnect keeps the portal session
}

void xdesk::host::Stream::ack(const QString& session, const std::uint32_t id)
{
    QHash<QString, StreamClient>::iterator it = this->_clients.find(session);

    if (it != this->_clients.end()) it->flow.acked(id, QDateTime::currentMSecsSinceEpoch());
}

void xdesk::host::Stream::setQuality(const QString& session, const int quality, const double scale, const int fps)
{
    QHash<QString, StreamClient>::iterator it = this->_clients.find(session);

    if (it == this->_clients.end()) return;
    it->quality = quality;
    it->scale = scale;
    it->effective = 0.0; // recomputed (and announced) with the next frame
    it->idle = 0; // a higher quality sends the still tiles again
    it->fps = fps;
}

void xdesk::host::Stream::setPaused(const QString& session, const bool paused)
{
    QHash<QString, StreamClient>::iterator it = this->_clients.find(session);

    if (it == this->_clients.end()) return;
    it->paused = paused;
    it->flow.reset(); // the acks of the frames before the pause may never come
    if (!paused) this->openIfWatched_(); // the first look at the screen starts the capture
}

void xdesk::host::Stream::refresh(const QString& session)
{
    QHash<QString, StreamClient>::iterator it = this->_clients.find(session);

    if (it == this->_clients.end()) return;
    it->encoder.reset();
    it->idle = 0;
}

void xdesk::host::Stream::tick_(void)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QHash<int, QImage> scaled; // scale (per mille) -> frame
    QImage frame;
    bool anyReady = false;

    if (!this->_capture || this->_clients.isEmpty()) return;

    // Who can take a frame now (pacing + frames in flight), and how much the path holds each one back
    for (QHash<QString, StreamClient>::iterator it = this->_clients.begin(); it != this->_clients.end(); ++it) {
        StreamClient& client = it.value();
        const bool due = !client.paused && now - client.lastFrame >= 1000 / std::max(1, client.fps);
        client.flow.expire(now); // acks lost: resume
        if (due && client.flow.canSend(client.fps)) anyReady = true;
        else if (due && client.lastTick > 0) client.blocked += std::min<qint64>(now - client.lastTick, 50);
        client.lastTick = now;
        if (client.second == 0) client.second = now;
        if (now - client.second < 1000) continue;
        const double lowered = client.paused ? 0.0 : std::max(0, client.quality - client.flow.ceiling()) / 100.0;
        emit this->pressure(it.key(), std::min(1.0, static_cast<double>(client.blocked) / static_cast<double>(now - client.second) + lowered));
        client.blocked = 0;
        client.second = now;
    }
    if (!anyReady) return;
    frame = this->_capture->grab();
    if (frame.isNull()) return;

    // Resolution changed: every client gets a complete frame
    if (frame.size() != this->_size) {
        this->_size = frame.size();
        for (StreamClient& client: this->_clients) {
            client.encoder.reset();
            client.idle = 0;
        }
        emit this->resized(this->_size);
    }

    // Encode for every ready client (its own dirty tiles)
    for (QHash<QString, StreamClient>::iterator it = this->_clients.begin(); it != this->_clients.end(); ++it) {
        StreamClient& client = it.value();
        const double cap = this->_maxHeight > 0 && frame.height() > this->_maxHeight ? static_cast<double>(this->_maxHeight) / frame.height() : 1.0;
        const double effective = std::min(client.scale, cap);
        const int key = static_cast<int>(std::lround(effective * 1000));
        QByteArray message;

        const qint64 interval = 1000 / std::max(1, client.fps);
        if (client.paused || !client.flow.canSend(client.fps) || now - client.lastFrame < interval) continue;

        // Another size: a complete frame, and the host maps the input with the new scale
        if (std::abs(client.effective - effective) > 0.0005) {
            if (client.effective > 0.0) client.encoder.reset();
            client.effective = effective;
            client.idle = 0;
            emit this->scaled(it.key(), effective);
        }
        if (client.idle == frame.cacheKey()) continue; // the same capture as the last empty encode: no scaling, no hashing
        if (!scaled.contains(key))
            scaled.insert(key, key >= 1000 ? frame : frame.scaled(std::max(1, static_cast<int>(frame.width() * effective)), std::max(1, static_cast<int>(frame.height() * effective)), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        client.lastFrame = now - client.lastFrame < interval * 2 ? client.lastFrame + interval : now; // on the slots of the rate (the 4 ms tick would make 30 fps 27)
        message = client.encoder.encode(scaled.value(key), client.flow.quality(client.quality));
        client.idle = message.isEmpty() ? frame.cacheKey() : 0;
        if (message.isEmpty()) continue;
        client.flow.sent(client.encoder.lastId(), message.size(), now);
        emit this->frame(it.key(), message);
    }

    // Cursor (screen coordinates, scaled by the host per session)
    const QPoint cursor = this->_capture->cursor();
    if (cursor != this->_cursor) {
        this->_cursor = cursor;
        emit this->cursor(cursor);
    }
}
