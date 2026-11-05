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
##  @file Server.hpp

File Description:
##  HTTPS server of the host: web client, android app, api, websocket upgrade
\**************************************************************/

#ifndef SERVER_H
    #define SERVER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>   // _cold, _nodiscard
    #include "WebSocket.hpp"     // xdesk::net::WebSocket
    #include <QSslConfiguration> // QSslConfiguration
    #include <QJsonObject>       // QJsonObject
    #include <QSslServer>        // QSslServer
    #include <QByteArray>        // QByteArray
    #include <functional>        // std::function
    #include <QObject>           // QObject
    #include <QString>           // QString

namespace xdesk::net { // namespace start
//----------------------------------------------------------------//
/* CLASS */

/*
 * QSslServer reading the name asked by the client (SNI of the ClientHello) before the handshake:
 *  - the named name (the tailnet name with its tailscale certificate) -> the named configuration
 *  - anything else (an address, no SNI: the native clients) -> the default one (self-signed, pinned)
 * A socket is pending once encrypted (like QSslServer)
*/
class TlsServer final: public QSslServer {
    private:
        QString _name = ""; // "" = one certificate for everybody
        QSslConfiguration _named;

        // ---------- Pre-Function -------- //
        void onHello_(QSslSocket* socket); // ClientHello complete: certificate chosen, handshake started

    protected:
        // ---------- Pre-Function -------- //
        void incomingConnection(qintptr descriptor) override;

    public:
        // ---------- Pre-Function -------- //
        void setNamed(const QString& name, const QSslConfiguration& config); // "" = removed
        _cold _nodiscard static QString serverName(const QByteArray& hello, bool& complete); // SNI of a ClientHello (lower case, "" = none), complete = false while the record is not all there

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const QString& named(void) const {return this->_name;};

        // ---------- Constructor --------- //
        explicit TlsServer(QObject* parent = nullptr): QSslServer(parent) { /* Nothing */ };

        // ----------- Destructor --------- //
        ~TlsServer() override = default;
};

/*
 * Routes (GET only):
 *  /                -> web client (qrc :/web/index.html), the android app banner shown on phones
 *  /<file>          -> qrc :/web/<file>
 *  /app/xdesk.apk   -> android application (qrc :/android/xdesk.apk, or <data>/xdesk.apk)
 *  /api/info        -> {"name", "version", "apk": version, "fingerprint", "pq": true}
 *  /ws              -> websocket upgrade (newSocket)
 * Every answer has strict security headers (CSP self, no-store, nosniff, frame-ancestors none)
*/
class Server: public QObject {
    Q_OBJECT

    private:
        TlsServer _server;
        std::function<QJsonObject(void)> _info; // /api/info content
        std::function<bool(const QHostAddress&)> _filter; // false = connection dropped (banned address)

        // ---------- Pre-Function -------- //
        void onConnection_(void);
        void onRequest_(QSslSocket* socket);
        void answer_(QSslSocket* socket, const int status, const QByteArray& type, const QByteArray& body, const bool close = true);

    public:
        // ---------- Pre-Function -------- //
        void listen(const QString& address, const quint16 port, const QSslConfiguration& tls); // throws Network
        void stop(void);
        void setInfo(const std::function<QJsonObject(void)>& info);
        void setFilter(const std::function<bool(const QHostAddress&)>& filter);
        _cold _nodiscard quint16 port(void) const;

        // ------------ Function ---------- //
        /* setter */
        _cold inline void setNamedCertificate(const QString& name, const QSslConfiguration& tls) {this->_server.setNamed(name, tls);}; // served to the clients asking this name ("" = none)

        // ---------- Constructor --------- //
        explicit Server(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Server() override = default;

    signals:
        void newSocket(xdesk::net::WebSocket* socket, const QString& userAgent); // owned by the receiver
};

} // namespace end
#endif /* SERVER_H */
