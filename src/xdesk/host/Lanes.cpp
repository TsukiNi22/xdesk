/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 06/11/2026 by @author Tsukini

File Name:
##  @file Lanes.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/Crypto.hpp"
#include "xdesk/net/Datagram.hpp"
#include "xdesk/net/Protocol.hpp"
#include "xdesk/host/Lanes.hpp"
#include <QJsonDocument>
#include <QDateTime>
#include <algorithm>
#include <QtEndian>
#include <QtGlobal>

xdesk::host::Lanes::Lanes(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_timer, &QTimer::timeout, this, &Lanes::onTick_);
    this->_timer.start(250);
}

xdesk::host::Lanes::~Lanes()
{
    const QList<QString> sessions = this->_sets.keys();

    for (const QString& session: sessions)
        this->remove(session);
}

QJsonObject xdesk::host::Lanes::offer(const QString& session)
{
    LaneSet set;

    this->remove(session);
    set.token = security::random_bytes(32);
    if (qEnvironmentVariableIntValue("XDESK_LANES") > 0) set.policy.setFixed(qEnvironmentVariableIntValue("XDESK_LANES"));
    this->_sets.insert(session, set);
    return {{"token", QString::fromLatin1(set.token.toBase64())}, {"max", LANE_MAX}};
}

void xdesk::host::Lanes::remove(const QString& session)
{
    QHash<QString, LaneSet>::iterator it = this->_sets.find(session);

    if (it == this->_sets.end()) return;
    const QList<QPointer<net::WebSocket>> sockets = it->sockets;
    this->_sets.erase(it);
    for (const QPointer<net::WebSocket>& ws: sockets)
        if (ws) {ws->disconnect(this); ws->close(1000); ws->deleteLater();}
}

void xdesk::host::Lanes::join(net::WebSocket* ws)
{
    const QPointer<net::WebSocket> guarded(ws);

    ws->setParent(this);
    this->_pending.removeAll(nullptr);
    if (this->_pending.size() >= LANE_PENDING_MAX) {ws->abort(); ws->deleteLater(); return;}
    this->_pending.append(guarded);
    QObject::connect(ws, &net::WebSocket::textReceived, this, [this, ws](const QString& text) {this->onJoin_(ws, text);});
    QObject::connect(ws, &net::WebSocket::closed, this, [this, ws]() {
        this->_pending.removeAll(ws);
        ws->deleteLater();
    });
    QTimer::singleShot(LANE_JOIN_TIMEOUT, this, [this, guarded]() {
        if (guarded && this->_pending.contains(guarded)) guarded->abort(); // its closed signal deletes it
    });
}

void xdesk::host::Lanes::onJoin_(net::WebSocket* ws, const QString& text)
{
    const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString session = message.value("session").toString();
    const QByteArray token = QByteArray::fromBase64(message.value("token").toString().toLatin1());
    const int index = message.value("index").toInt(0);
    QHash<QString, LaneSet>::iterator it = this->_sets.find(session);

    ws->disconnect(this); // one message only
    this->_pending.removeAll(ws);
    QObject::connect(ws, &net::WebSocket::closed, ws, &QObject::deleteLater);

    // A lane of a known session, with its token, at a place the viewer was told to fill
    if (message.value("t").toString() != "lane" || it == this->_sets.end() || token.size() != 32 || !security::constant_equal(token, it->token) || index < 1 || index >= it->announced) {
        ws->close(1008, "Unknown lane");
        return;
    }
    while (it->sockets.size() < index)
        it->sockets.append(nullptr);
    if (it->sockets[index - 1] && it->sockets[index - 1]->isOpen()) {ws->close(1008, "Lane already open"); return;}
    it->sockets[index - 1] = ws;
    QObject::connect(ws, &net::WebSocket::binaryReceived, this, [this, session](const QByteArray& data) {
        // Acks only
        if (data.size() != 5 || static_cast<std::uint8_t>(data[0]) != static_cast<std::uint8_t>(net::Binary::FrameAck)) return;
        const std::uint32_t id = qFromBigEndian<std::uint32_t>(data.constData() + 1);
        emit this->frameAcked(session, id);
        this->acked(session, id);
    });
    QObject::connect(ws, &net::WebSocket::closed, this, [this, session, ws]() {this->onLaneClosed_(session, ws);});
    emit this->changed(session);
}

void xdesk::host::Lanes::onLaneClosed_(const QString& session, net::WebSocket* ws)
{
    QHash<QString, LaneSet>::iterator it = this->_sets.find(session);
    int lane = -1;
    bool dropped = false;

    if (it == this->_sets.end()) return;
    for (int i = 0; i < it->sockets.size(); ++i)
        if (it->sockets[i] == ws) {lane = i + 1; it->sockets[i] = nullptr;}
    if (lane < 0) return;
    for (QHash<std::uint32_t, QPair<std::int64_t, int>>::iterator f = it->flight.begin(); f != it->flight.end();) {
        if (f->second == lane) {f = it->flight.erase(f); dropped = true;}
        else ++f;
    }
    emit this->changed(session);
    if (dropped) emit this->lost(session);
}

void xdesk::host::Lanes::retire_(LaneSet& set, const int keep)
{
    for (int i = std::max(0, keep - 1); i < set.sockets.size(); ++i) {
        const QPointer<net::WebSocket> ws = set.sockets[i];
        set.sockets[i] = nullptr; // no new frame on it
        if (!ws) continue;
        QTimer::singleShot(LANE_RETIRE, this, [ws]() {if (ws) ws->close(1000);}); // its last acks still arrive
    }
}

bool xdesk::host::Lanes::sendFrame(const QString& session, const QByteArray& message, const qint64 mainPending)
{
    QHash<QString, LaneSet>::iterator it = this->_sets.find(session);
    const std::uint32_t id = net::frame_id(message);
    qint64 least = -1;
    int best = 0;

    if (it == this->_sets.end()) return false;

    // The least loaded socket (bytes still in its buffers): a frame never waits behind a slow one; equal ones in turn
    const int count = std::min(static_cast<int>(it->sockets.size()), it->announced - 1) + 1;
    for (int k = 0; k < count; ++k) {
        const int lane = (it->turn + k) % count;
        const QPointer<net::WebSocket> ws = lane == 0 ? nullptr : it->sockets[lane - 1];
        const qint64 pending = lane == 0 ? mainPending : (ws && ws->isOpen() ? ws->pendingBytes() : -1);
        if (pending < 0 || (least >= 0 && pending >= least)) continue;
        least = pending;
        best = lane;
    }
    it->turn = (best + 1) % count;
    if (id != 0) {
        if (it->flight.size() >= LANE_FLIGHT_KEPT) it->flight.clear(); // acks lost long ago
        it->flight.insert(id, qMakePair(static_cast<std::int64_t>(message.size()), best));
    }
    if (best == 0) return false;
    it->sockets[best - 1]->sendBinary(message);
    return true;
}

void xdesk::host::Lanes::acked(const QString& session, const std::uint32_t id)
{
    QHash<QString, LaneSet>::iterator it = this->_sets.find(session);

    if (it == this->_sets.end()) return;
    const QPair<std::int64_t, int> frame = it->flight.take(id);
    if (frame.first > 0) it->policy.delivered(frame.first, QDateTime::currentMSecsSinceEpoch());
}

void xdesk::host::Lanes::pressure(const QString& session, const double pressure)
{
    QHash<QString, LaneSet>::iterator it = this->_sets.find(session);

    if (it != this->_sets.end()) it->policy.pressure(pressure, QDateTime::currentMSecsSinceEpoch());
}

void xdesk::host::Lanes::setMax(const QString& session, const int max)
{
    QHash<QString, LaneSet>::iterator it = this->_sets.find(session);

    if (it != this->_sets.end()) it->policy.setMax(max);
}

QString xdesk::host::Lanes::describe(const QString& session) const
{
    const QHash<QString, LaneSet>::const_iterator it = this->_sets.constFind(session);
    int open = 1;

    if (it == this->_sets.constEnd()) return "";
    for (int i = 0; i < std::min(static_cast<int>(it->sockets.size()), it->announced - 1); ++i)
        open += it->sockets[i] && it->sockets[i]->isOpen();
    return open > 1 ? "tcp x" + QString::number(open) : QString("");
}

void xdesk::host::Lanes::onTick_(void)
{
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();

    for (QHash<QString, LaneSet>::iterator it = this->_sets.begin(); it != this->_sets.end(); ++it) {
        const int lanes = it->policy.update(now);
        if (lanes == it->announced) continue;
        if (lanes < it->announced) this->retire_(it.value(), lanes);
        it->announced = lanes;
        emit this->wanted(it.key(), lanes);
        emit this->changed(it.key());
    }
}
