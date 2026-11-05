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
##  @file Server.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xdesk/core/Config.hpp"
#include "xdesk/net/Server.hpp"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QHostAddress>
#include <QSslSocket>
#include <QFileInfo>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QFile>

// Answer headers of every response
static const QByteArray SECURITY_HEADERS =
    "Content-Security-Policy: default-src 'self'; img-src 'self' blob: data:; connect-src 'self'; style-src 'self'; script-src 'self'; worker-src 'self' blob:; frame-ancestors 'none'; base-uri 'none'; form-action 'none'\r\n"
    "X-Content-Type-Options: nosniff\r\n"
    "Referrer-Policy: no-referrer\r\n"
    "Cache-Control: no-store\r\n"
    "Cross-Origin-Opener-Policy: same-origin\r\n"
    "Cross-Origin-Resource-Policy: same-origin\r\n"
    "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n";

static QByteArray content_type(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "html") return "text/html; charset=utf-8";
    if (ext == "js" || ext == "mjs") return "text/javascript; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "png") return "image/png";
    if (ext == "json" || ext == "webmanifest") return "application/manifest+json";
    if (ext == "apk") return "application/vnd.android.package-archive";
    if (ext == "txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

xdesk::net::Server::Server(QObject* parent)
: QObject(parent)
{
    this->_server.setHandshakeTimeout(8000);
    QObject::connect(&this->_server, &QSslServer::pendingConnectionAvailable, this, &Server::onConnection_);
    QObject::connect(&this->_server, &QSslServer::errorOccurred, this, [](QSslSocket* socket, QAbstractSocket::SocketError) {socket->deleteLater();});
    QObject::connect(&this->_server, &QSslServer::handshakeInterruptedOnError, this, [](QSslSocket* socket, const QSslError&) {socket->abort();});
}

void xdesk::net::Server::listen(const QString& address, const quint16 port, const QSslConfiguration& tls)
{
    QSslConfiguration config = tls;

    config.setPeerVerifyMode(QSslSocket::VerifyNone); // clients authenticate inside the websocket
    this->_server.setSslConfiguration(config);
    this->_server.setMaxPendingConnections(64);
    if (!this->_server.listen(QHostAddress(address), port)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Network, "Cannot listen on " + address.toStdString() + ":" + std::to_string(port) + ": " + this->_server.errorString().toStdString());
}

void xdesk::net::Server::stop(void)
{
    this->_server.close();
}

void xdesk::net::Server::setInfo(const std::function<QJsonObject(void)>& info)
{
    this->_info = info;
}

void xdesk::net::Server::setFilter(const std::function<bool(const QHostAddress&)>& filter)
{
    this->_filter = filter;
}

quint16 xdesk::net::Server::port(void) const
{
    return this->_server.serverPort();
}

void xdesk::net::Server::onConnection_(void)
{
    while (this->_server.hasPendingConnections()) {
        QSslSocket* socket = qobject_cast<QSslSocket*>(this->_server.nextPendingConnection());
        QPointer<QSslSocket> guard = socket;

        if (!socket) continue;
        if (this->_filter && !this->_filter(socket->peerAddress())) {socket->abort(); socket->deleteLater(); continue;}
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        socket->setProperty("xdesk_buffer", QByteArray());
        QObject::connect(socket, &QSslSocket::readyRead, this, [this, socket]() {this->onRequest_(socket);});
        QObject::connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);

        // Slow clients: 10 s to send the request
        QTimer::singleShot(10000, socket, [guard]() {
            if (guard && guard->property("xdesk_upgraded").toBool() == false) guard->abort();
        });
    }
}

void xdesk::net::Server::answer_(QSslSocket* socket, const int status, const QByteArray& type, const QByteArray& body, const bool close)
{
    static const QHash<int, QByteArray> REASONS = {{200, "OK"}, {400, "Bad Request"}, {403, "Forbidden"}, {404, "Not Found"}, {405, "Method Not Allowed"}, {431, "Request Header Fields Too Large"}};
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + " " + REASONS.value(status, "Error") + "\r\n";

    response += SECURITY_HEADERS;
    response += "Content-Type: " + type + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += close ? "Connection: close\r\n\r\n" : "Connection: keep-alive\r\n\r\n";
    if (!socket->property("xdesk_head").toBool()) response += body;
    socket->write(response);
    if (close) socket->disconnectFromHost();
}

void xdesk::net::Server::onRequest_(QSslSocket* socket)
{
    QByteArray buffer = socket->property("xdesk_buffer").toByteArray() + socket->readAll();
    const qsizetype end = buffer.indexOf("\r\n\r\n");
    QList<QByteArray> lines;
    QList<QByteArray> request;
    QHash<QByteArray, QByteArray> headers;
    QString path;
    QByteArray rest;

    if (end < 0) {
        if (buffer.size() > 16384) this->answer_(socket, 431, "text/plain", "Too large");
        else socket->setProperty("xdesk_buffer", buffer);
        return;
    }
    rest = buffer.mid(end + 4);
    lines = buffer.left(end).split('\n');
    request = lines.front().trimmed().split(' ');
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const qsizetype colon = lines[i].indexOf(':');
        if (colon > 0) headers[lines[i].left(colon).trimmed().toLower()] = lines[i].mid(colon + 1).trimmed();
    }
    socket->setProperty("xdesk_buffer", QByteArray());
    if (request.size() != 3 || (request[0] != "GET" && request[0] != "HEAD")) {this->answer_(socket, 405, "text/plain", "GET only"); return;}
    socket->setProperty("xdesk_head", request[0] == "HEAD");
    path = QString::fromUtf8(QByteArray::fromPercentEncoding(request[1].split('?').front()));
    if (path.contains("..") || path.contains('\\') || !path.startsWith('/')) {this->answer_(socket, 400, "text/plain", "Bad path"); return;}

    // Websocket upgrade: the socket leaves the http server
    if (path == "/ws") {
        const QByteArray key = headers.value("sec-websocket-key");
        if (key.isEmpty() || !headers.value("upgrade").toLower().contains("websocket") || headers.value("sec-websocket-version") != "13") {
            this->answer_(socket, 400, "text/plain", "Websocket upgrade expected");
            return;
        }
        const QByteArray accept = QCryptographicHash::hash(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", QCryptographicHash::Sha1).toBase64();
        socket->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n");
        socket->setProperty("xdesk_upgraded", true);
        if (socket->peerAddress().isLoopback() && headers.contains("cf-connecting-ip")) socket->setProperty("xdesk_forwarded", QString::fromLatin1(headers.value("cf-connecting-ip")));
        socket->disconnect(this);
        QObject::disconnect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
        emit this->newSocket(new WebSocket(socket, true, rest), QString::fromUtf8(headers.value("user-agent")));
        return;
    }

    // Api
    if (path == "/api/info") {
        this->answer_(socket, 200, "application/json", QJsonDocument(this->_info ? this->_info() : QJsonObject()).toJson(QJsonDocument::Compact), false);
        return;
    }
    if (path == "/app/version" || path == "/app/xdesk.apk") {
        QFile file(path == "/app/version" ? ":/android/version.txt" : ":/android/xdesk.apk");
        if (!file.exists()) file.setFileName(core::Config::dataDir() + (path == "/app/version" ? "/version.txt" : "/xdesk.apk"));
        if (!file.open(QIODevice::ReadOnly)) {this->answer_(socket, 404, "text/plain", "No android application in this build"); return;}
        this->answer_(socket, 200, content_type(file.fileName()), file.readAll());
        return;
    }

    // Web client
    if (path == "/") path = "/index.html";
    QFile file(":/web" + path);
    if (!file.open(QIODevice::ReadOnly)) {this->answer_(socket, 404, "text/plain", "Not found"); return;}
    this->answer_(socket, 200, content_type(path), file.readAll(), false);
}
