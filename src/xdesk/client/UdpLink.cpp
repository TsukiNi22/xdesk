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
##  @file UdpLink.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/client/UdpLink.hpp"
#include <QNetworkDatagram>
#include <QDateTime>
#include <QtEndian>

xdesk::client::UdpLink::UdpLink(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_socket, &QUdpSocket::readyRead, this, &UdpLink::onReadyRead_);
    QObject::connect(&this->_timer, &QTimer::timeout, this, &UdpLink::tick_);
}

bool xdesk::client::UdpLink::start(const QHostAddress& host, const QJsonObject& offer)
{
    const QByteArray down = QByteArray::fromBase64(offer.value("down").toString().toLatin1());
    const QByteArray up = QByteArray::fromBase64(offer.value("up").toString().toLatin1());
    const std::uint32_t connection = static_cast<std::uint32_t>(offer.value("connection").toInteger());
    const int port = offer.value("port").toInt();

    this->stop();
    if (host.isNull() || down.size() != 32 || up.size() != 32 || connection == 0 || port <= 0 || port > 65535) return false;
    if (!this->_socket.bind(host.protocol() == QAbstractSocket::IPv6Protocol ? QHostAddress::AnyIPv6 : QHostAddress::AnyIPv4, 0)) return false;
    this->_socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 4 << 20); // a big frame arrives at once
    this->_crypto = net::DatagramCrypto(connection, up, down, false);
    this->_host = host;
    this->_port = static_cast<std::uint16_t>(port);
    this->_started = true;
    this->_since = QDateTime::currentMSecsSinceEpoch();
    this->_hello = 0;
    this->_timer.start(5);
    this->tick_();
    return true;
}

void xdesk::client::UdpLink::stop(void)
{
    const bool was = this->_active;

    this->_timer.stop();
    this->_socket.close();
    this->_assembler.clear();
    this->_active = false;
    this->_started = false;
    if (was) emit this->activeChanged(false);
}

void xdesk::client::UdpLink::send_(const net::Datagram type, const QByteArray& payload)
{
    const QByteArray packet = this->_crypto.seal(type, payload);

    if (!packet.isEmpty()) this->_socket.writeDatagram(packet, this->_host, this->_port);
}

void xdesk::client::UdpLink::ack(const std::uint32_t id)
{
    QByteArray payload(4, '\0');

    qToBigEndian(id, payload.data());
    this->send_(net::Datagram::Ack, payload);
}

void xdesk::client::UdpLink::onReadyRead_(void)
{
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();

    while (this->_socket.hasPendingDatagrams()) {
        const QNetworkDatagram datagram = this->_socket.receiveDatagram(2048);
        net::Datagram type = net::Datagram::Hello;
        QByteArray payload;
        QByteArray frame;
        std::uint32_t id = 0;

        // Only the host, with the keys of this session (anything else dropped)
        if (!datagram.senderAddress().isEqual(this->_host, QHostAddress::TolerantConversion) || !this->_crypto.open(datagram.data(), type, payload)) continue;
        this->_seen = now;
        if (!this->_active) {
            this->_active = true; // a HelloAck, or data when the ack was lost
            emit this->activeChanged(true);
        }
        if (type != net::Datagram::Data) continue;
        switch (this->_assembler.add(payload, now, frame, id)) {
            case 1: this->ack(id); emit this->frame(frame); break;
            case 2: this->ack(id); break; // its ack was lost
            default: break;
        }
    }
}

void xdesk::client::UdpLink::tick_(void)
{
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();

    if (!this->_started) return;

    // Hellos until the host answers, then keep alives
    if (!this->_active) {
        if (now - this->_since > LINK_HELLO_GIVE_UP) {this->stop(); emit this->activeChanged(false); return;} // no answer: tcp
        if (now - this->_hello >= LINK_HELLO_EVERY) {this->send_(net::Datagram::Hello, {}); this->_hello = now;}
        return;
    }
    if (now - this->_seen > LINK_SILENCE) {this->stop(); return;} // the host went silent: tcp
    if (now - this->_ping >= LINK_PING_EVERY) {
        QByteArray stamp(8, '\0');
        qToBigEndian(static_cast<std::uint64_t>(now), stamp.data());
        this->send_(net::Datagram::Ping, stamp);
        this->_ping = now;
    }

    // Missing pieces asked again, frames lost for good given up
    for (const QPair<std::uint32_t, QList<std::uint16_t>>& due: this->_assembler.missing(now, this->_rtt)) {
        QByteArray payload(6, '\0');
        qToBigEndian(due.first, payload.data());
        qToBigEndian(static_cast<std::uint16_t>(due.second.size()), payload.data() + 4);
        for (const std::uint16_t index: due.second) {
            char bytes[2];
            qToBigEndian(index, bytes);
            payload.append(bytes, 2);
        }
        this->send_(net::Datagram::Nack, payload);
    }
    if (!this->_assembler.expired(now).isEmpty()) emit this->gaveUp();
}
