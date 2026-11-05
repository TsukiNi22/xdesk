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
##  @file WebSocket.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/Crypto.hpp"
#include "xdesk/net/WebSocket.hpp"
#include "xdesk/net/Protocol.hpp"
#include <QCryptographicHash>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QByteArray>
#include <QString>
#include <cstdint>
#include <QTimer>

// RFC 6455 opcodes
static constexpr int OP_CONTINUATION = 0x0;
static constexpr int OP_TEXT = 0x1;
static constexpr int OP_BINARY = 0x2;
static constexpr int OP_CLOSE = 0x8;
static constexpr int OP_PING = 0x9;
static constexpr int OP_PONG = 0xA;
static const QByteArray WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

xdesk::net::WebSocket::WebSocket(QSslSocket* socket, const bool server, const QByteArray& pending, QObject* parent)
: QObject(parent), _socket{socket}, _server{server}, _open{server}, _buffer{pending}, _maxMessage{XDESK_MAX_MESSAGE}
{
    this->_socket->setParent(this);
    this->_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1); // no Nagle: input latency
    QObject::connect(this->_socket, &QSslSocket::readyRead, this, &WebSocket::onReadyRead_);
    QObject::connect(this->_socket, &QSslSocket::disconnected, this, [this]() {
        const bool wasOpen = this->_open;
        this->_open = false;
        if (wasOpen || this->_server) emit this->closed();
        else emit this->failed("Connection closed during the handshake");
    });
    if (server && !this->_buffer.isEmpty()) QTimer::singleShot(0, this, &WebSocket::parseFrames_);
}

xdesk::net::WebSocket::~WebSocket()
{
    if (this->_socket) this->_socket->disconnect(this);
}

xdesk::net::WebSocket* xdesk::net::WebSocket::connectTo(const QString& host, const quint16 port, const QString& path, const QSslConfiguration& tls, QObject* parent)
{
    QSslSocket* socket = new QSslSocket();
    WebSocket* ws = new WebSocket(socket, false, {}, parent);
    QSslConfiguration config = tls;

    // The certificate is self-signed: pinned by the Client (fingerprint), nothing secret is sent before
    config.setPeerVerifyMode(QSslSocket::QueryPeer);
    socket->setSslConfiguration(config);
    ws->_path = path;
    QObject::connect(socket, &QSslSocket::sslErrors, socket, [socket](const QList<QSslError>&) {socket->ignoreSslErrors();});
    QObject::connect(socket, &QSslSocket::errorOccurred, ws, [ws, socket](QAbstractSocket::SocketError) {
        if (!ws->_open) emit ws->failed(socket->errorString());
    });
    QObject::connect(socket, &QSslSocket::encrypted, ws, [ws, socket, host, port]() {
        QByteArray request;

        ws->_clientKey = QString::fromLatin1(xdesk::security::random_bytes(16).toBase64());
        request += "GET " + ws->_path.toUtf8() + " HTTP/1.1\r\n";
        request += "Host: " + host.toUtf8() + ":" + QByteArray::number(port) + "\r\n";
        request += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: " + ws->_clientKey.toLatin1() + "\r\n";
        request += "Sec-WebSocket-Version: 13\r\nUser-Agent: xdesk/" XDESK_VERSION "\r\n\r\n";
        socket->write(request);
    });
    socket->connectToHostEncrypted(host, port);
    return ws;
}

bool xdesk::net::WebSocket::parseHandshakeResponse_(void)
{
    const qsizetype end = this->_buffer.indexOf("\r\n\r\n");
    const QByteArray expected = QCryptographicHash::hash(this->_clientKey.toLatin1() + WS_GUID, QCryptographicHash::Sha1).toBase64();
    QList<QByteArray> lines;
    bool accepted = false;

    if (end < 0) {
        if (this->_buffer.size() > 16384) {emit this->failed("Handshake response too big"); this->abort();}
        return false;
    }
    lines = this->_buffer.left(end).split('\n');
    this->_buffer.remove(0, end + 4);
    if (lines.isEmpty() || !lines.front().startsWith("HTTP/1.1 101")) {
        emit this->failed("Websocket upgrade refused: " + QString::fromUtf8(lines.value(0)).trimmed());
        this->abort();
        return false;
    }
    for (const QByteArray& line: lines)
        if (line.toLower().startsWith("sec-websocket-accept:")) accepted = line.mid(21).trimmed() == expected;
    if (!accepted) {emit this->failed("Invalid Sec-WebSocket-Accept"); this->abort(); return false;}
    this->_open = true;
    emit this->opened();
    return true;
}

void xdesk::net::WebSocket::onReadyRead_(void)
{
    this->_buffer.append(this->_socket->readAll());
    if (!this->_open && !this->_server && !this->parseHandshakeResponse_()) return;
    this->parseFrames_();
}

void xdesk::net::WebSocket::parseFrames_(void)
{
    while (this->_buffer.size() >= 2) {
        const std::uint8_t b0 = static_cast<std::uint8_t>(this->_buffer[0]);
        const std::uint8_t b1 = static_cast<std::uint8_t>(this->_buffer[1]);
        const bool fin = b0 & 0x80;
        const int opcode = b0 & 0x0F;
        const bool masked = b1 & 0x80;
        quint64 length = b1 & 0x7F;
        qsizetype header = 2;
        QByteArray payload;

        // Clients must mask, servers must not (RFC 6455 5.1)
        if (masked != this->_server) {this->close(1002, "Bad masking"); return;}
        if (length == 126) {
            if (this->_buffer.size() < 4) return;
            length = (static_cast<quint64>(static_cast<std::uint8_t>(this->_buffer[2])) << 8) | static_cast<std::uint8_t>(this->_buffer[3]);
            header = 4;
        } else if (length == 127) {
            if (this->_buffer.size() < 10) return;
            length = 0;
            for (int i = 0; i < 8; ++i)
                length = (length << 8) | static_cast<std::uint8_t>(this->_buffer[2 + i]);
            header = 10;
        }
        if (length > static_cast<quint64>(this->_maxMessage)) {this->close(1009, "Message too big"); return;}
        if (masked) header += 4;
        if (static_cast<quint64>(this->_buffer.size()) < static_cast<quint64>(header) + length) return;
        payload = this->_buffer.mid(header, static_cast<qsizetype>(length));
        if (masked) {
            const QByteArray mask = this->_buffer.mid(header - 4, 4);
            char* data = payload.data();
            for (qsizetype i = 0; i < payload.size(); ++i)
                data[i] = static_cast<char>(data[i] ^ mask[i % 4]);
        }
        this->_buffer.remove(0, header + static_cast<qsizetype>(length));

        switch (opcode) {
            case OP_PING: this->sendFrame_(OP_PONG, payload); break;
            case OP_PONG: break;
            case OP_CLOSE:
                if (!this->_closing) this->sendFrame_(OP_CLOSE, payload.left(2));
                this->_closing = true;
                this->_socket->disconnectFromHost();
                return;
            case OP_TEXT:
            case OP_BINARY:
            case OP_CONTINUATION:
                if (opcode != OP_CONTINUATION) {this->_fragments.clear(); this->_fragmentOpcode = opcode;}
                this->_fragments.append(payload);
                if (this->_fragments.size() > this->_maxMessage) {this->close(1009, "Message too big"); return;}
                if (!fin) break;
                if (this->_fragmentOpcode == OP_TEXT) {
                    if (this->_fragments.size() > XDESK_MAX_CONTROL) {this->close(1009, "Control message too big"); return;}
                    emit this->textReceived(QString::fromUtf8(this->_fragments));
                } else emit this->binaryReceived(this->_fragments);
                this->_fragments.clear();
                break;
            default: this->close(1002, "Unknown opcode"); return;
        }
    }
}

void xdesk::net::WebSocket::sendFrame_(const int opcode, const QByteArray& payload)
{
    QByteArray frame;
    const quint64 length = static_cast<quint64>(payload.size());
    const bool mask = !this->_server;

    if (!this->_socket || this->_socket->state() != QAbstractSocket::ConnectedState) return;
    frame.reserve(payload.size() + 14);
    frame.append(static_cast<char>(0x80 | opcode));
    if (length < 126) frame.append(static_cast<char>((mask ? 0x80 : 0) | length));
    else if (length <= 0xFFFF) {
        frame.append(static_cast<char>((mask ? 0x80 : 0) | 126));
        frame.append(static_cast<char>(length >> 8));
        frame.append(static_cast<char>(length));
    } else {
        frame.append(static_cast<char>((mask ? 0x80 : 0) | 127));
        for (int i = 7; i >= 0; --i)
            frame.append(static_cast<char>(length >> (i * 8)));
    }
    if (mask) {
        const QByteArray key = xdesk::security::random_bytes(4);
        QByteArray masked = payload;
        char* data = masked.data();
        frame.append(key);
        for (qsizetype i = 0; i < masked.size(); ++i)
            data[i] = static_cast<char>(data[i] ^ key[i % 4]);
        frame.append(masked);
    } else frame.append(payload);
    this->_socket->write(frame);
}

void xdesk::net::WebSocket::sendText(const QString& text)
{
    if (this->isOpen()) this->sendFrame_(OP_TEXT, text.toUtf8());
}

void xdesk::net::WebSocket::sendBinary(const QByteArray& data)
{
    if (this->isOpen()) this->sendFrame_(OP_BINARY, data);
}

void xdesk::net::WebSocket::close(const int code, const QString& reason)
{
    QByteArray payload;

    if (this->_closing || !this->_socket) return;
    payload.append(static_cast<char>(code >> 8));
    payload.append(static_cast<char>(code));
    payload.append(reason.toUtf8().left(120));
    this->sendFrame_(OP_CLOSE, payload);
    this->_closing = true;
    this->_socket->flush();
    QTimer::singleShot(1500, this->_socket, [socket = this->_socket]() {socket->abort();}); // peer that never answers
    this->_socket->disconnectFromHost();
}

void xdesk::net::WebSocket::abort(void)
{
    this->_closing = true;
    if (this->_socket) this->_socket->abort();
}

QHostAddress xdesk::net::WebSocket::peerAddress(void) const
{
    return this->_socket ? this->_socket->peerAddress() : QHostAddress();
}

qint64 xdesk::net::WebSocket::pendingBytes(void) const
{
    return this->_socket ? this->_socket->bytesToWrite() + this->_socket->encryptedBytesToWrite() : 0;
}

QSslSocket* xdesk::net::WebSocket::socket(void) const
{
    return this->_socket;
}
