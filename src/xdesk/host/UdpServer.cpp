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
##  @file UdpServer.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/Crypto.hpp"
#include "xdesk/host/UdpServer.hpp"
#include <QNetworkDatagram>
#include <QDateTime>
#include <algorithm>
#include <QtEndian>

xdesk::host::UdpServer::UdpServer(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_socket, &QUdpSocket::readyRead, this, &UdpServer::onReadyRead_);
    QObject::connect(&this->_timer, &QTimer::timeout, this, &UdpServer::tick_);
}

bool xdesk::host::UdpServer::listen(const QHostAddress& address, const std::uint16_t port)
{
    this->stop();
    if (!this->_socket.bind(address, port)) return false;
    this->_socket.setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 4 << 20); // a big frame leaves at once
    this->_socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 1 << 20);
    this->_timer.start(10);
    return true;
}

void xdesk::host::UdpServer::stop(void)
{
    QList<QString> sessions = this->_sessions.keys();

    for (const QString& session: sessions)
        this->remove(session);
    this->_timer.stop();
    this->_socket.close();
}

QJsonObject xdesk::host::UdpServer::offer(const QString& session)
{
    const QByteArray down = security::random_bytes(32); // host -> viewer
    const QByteArray up = security::random_bytes(32); // viewer -> host
    std::uint32_t connection = 0;
    UdpPeer peer;

    if (!this->isListening()) return {};
    this->remove(session);
    while (connection == 0 || this->_peers.contains(connection))
        connection = qFromBigEndian<std::uint32_t>(security::random_bytes(4).constData());
    peer.session = session;
    peer.crypto = net::DatagramCrypto(connection, down, up, true);
    this->_peers.insert(connection, peer);
    this->_sessions.insert(session, connection);
    return {{"port", static_cast<int>(this->_socket.localPort())}, {"connection", static_cast<qint64>(connection)}, {"down", QString::fromLatin1(down.toBase64())}, {"up", QString::fromLatin1(up.toBase64())}};
}

void xdesk::host::UdpServer::remove(const QString& session)
{
    const std::uint32_t connection = this->_sessions.take(session);

    if (connection) this->_peers.remove(connection);
}

QString xdesk::host::UdpServer::describe(const QString& session) const
{
    const UdpPeer peer = this->_peers.value(this->_sessions.value(session));

    if (!peer.active) return "";
    return peer.fec ? "udp + parity" : "udp";
}

void xdesk::host::UdpServer::setActive_(UdpPeer& peer, const bool active)
{
    if (peer.active == active) return;
    peer.active = active;
    if (!active) peer.frames.clear();
    emit this->activeChanged(peer.session, active);
}

void xdesk::host::UdpServer::send_(UdpPeer& peer, const net::Datagram type, const QByteArray& payload)
{
    const QByteArray packet = peer.crypto.seal(type, payload);

    if (!packet.isEmpty() && peer.port != 0) this->_socket.writeDatagram(packet, peer.address, peer.port);
}

bool xdesk::host::UdpServer::sendFrame(const QString& session, const QByteArray& message)
{
    QHash<std::uint32_t, UdpPeer>::iterator it = this->_peers.find(this->_sessions.value(session));
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();
    const std::uint32_t id = net::frame_id(message);
    UdpFrame frame;

    if (it == this->_peers.end() || !it->active || id == 0) return false;
    frame.payloads = net::split_frame(id, message, it->fec);
    frame.count = static_cast<int>((message.size() + XDESK_UDP_CHUNK - 1) / XDESK_UDP_CHUNK);
    frame.sent = now;
    frame.last = now;
    if (frame.payloads.isEmpty()) return false;
    for (const QByteArray& payload: frame.payloads)
        this->send_(it.value(), net::Datagram::Data, payload);
    it->sentPieces += frame.count;
    it->frames.insert(id, frame);
    return true;
}

void xdesk::host::UdpServer::onReadyRead_(void)
{
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();

    while (this->_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = this->_socket.receiveDatagram(2048);
        const QByteArray packet = datagram.data();
        QHash<std::uint32_t, UdpPeer>::iterator it = this->_peers.find(net::DatagramCrypto::connectionOf(packet));
        net::Datagram type = net::Datagram::Hello;
        QByteArray payload;

        // Unknown connection, forged, damaged or replayed: dropped without an answer
        if (it == this->_peers.end() || !it->crypto.open(packet, type, payload)) continue;
        UdpPeer& peer = it.value();
        peer.seen = now;
        peer.address = datagram.senderAddress(); // an authentic datagram: the viewer is there (NAT mappings change)
        peer.port = static_cast<std::uint16_t>(datagram.senderPort());
        switch (type) {
            case net::Datagram::Hello:
                this->send_(peer, net::Datagram::HelloAck, {});
                this->setActive_(peer, true);
                break;
            case net::Datagram::Ping:
                this->send_(peer, net::Datagram::Pong, payload.left(8));
                break;
            case net::Datagram::Ack: {
                if (payload.size() < 4) break;
                const std::uint32_t id = qFromBigEndian<std::uint32_t>(payload.constData());
                QHash<std::uint32_t, UdpFrame>::iterator frame = peer.frames.find(id);
                if (frame == peer.frames.end() || frame->acked) break;
                frame->acked = true;
                peer.rtt = peer.rtt * 0.875 + static_cast<double>(now - frame->sent) * 0.125;
                emit this->acked(peer.session, id);
                break;
            }
            case net::Datagram::Nack: {
                if (payload.size() < 6) break;
                const std::uint32_t id = qFromBigEndian<std::uint32_t>(payload.constData());
                const int n = qFromBigEndian<std::uint16_t>(payload.constData() + 4);
                QHash<std::uint32_t, UdpFrame>::iterator frame = peer.frames.find(id);
                if (frame == peer.frames.end() || frame->acked || payload.size() < 6 + 2 * n) break;
                for (int i = 0; i < n; ++i) {
                    const int index = qFromBigEndian<std::uint16_t>(payload.constData() + 6 + 2 * i);
                    if (index < frame->count) this->send_(peer, net::Datagram::Data, frame->payloads[index]);
                }
                frame->last = now;
                peer.askedPieces += n;
                break;
            }
            default: break;
        }
    }
}

void xdesk::host::UdpServer::tick_(void)
{
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();

    for (UdpPeer& peer: this->_peers) {
        if (!peer.active) continue;

        // Silent viewer: back to tcp
        if (now - peer.seen > UDP_SILENCE) {
            this->setActive_(peer, false);
            continue;
        }

        // Frames without ack: the last piece again (the viewer then asks the others), old ones forgotten
        const std::int64_t probe = std::max<std::int64_t>(30, static_cast<std::int64_t>(peer.rtt * 2.0));
        QList<std::uint32_t> old;
        for (QHash<std::uint32_t, UdpFrame>::iterator it = peer.frames.begin(); it != peer.frames.end(); ++it) {
            if (now - it->sent > UDP_KEEP) {old.append(it.key()); continue;}
            if (it->acked || now - it->last < probe) continue;
            this->send_(peer, net::Datagram::Data, it->payloads[it->count - 1]);
            it->last = now;
        }
        for (const std::uint32_t id: old)
            peer.frames.remove(id);

        // Parity: on while the path loses, off after a clean while (counts decay over ~2 s)
        const double loss = peer.sentPieces > 50.0 ? peer.askedPieces / peer.sentPieces : 0.0;
        if (loss >= UDP_FEC_ON) {peer.fec = true; peer.clean = now;}
        else if (loss > UDP_FEC_OFF) peer.clean = now;
        else if (peer.fec && now - peer.clean > UDP_FEC_CALM) peer.fec = false;
        peer.sentPieces *= 0.995;
        peer.askedPieces *= 0.995;
    }
}
