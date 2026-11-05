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
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include "WebSocket.hpp"    // xdesk::net::WebSocket
    #include <QJsonObject>      // QJsonObject
    #include <QSslServer>       // QSslServer
    #include <QByteArray>       // QByteArray
    #include <functional>       // std::function
    #include <QObject>          // QObject
    #include <QString>          // QString

namespace xdesk::net { // namespace start
//----------------------------------------------------------------//
/* CLASS */

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
        QSslServer _server;
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

        // ---------- Constructor --------- //
        explicit Server(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Server() override = default;

    signals:
        void newSocket(xdesk::net::WebSocket* socket, const QString& userAgent); // owned by the receiver
};

} // namespace end
#endif /* SERVER_H */
