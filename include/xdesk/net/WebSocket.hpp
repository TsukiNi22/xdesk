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
##  @file WebSocket.hpp

File Description:
##  Minimal RFC 6455 websocket over a TLS socket (server and client side)
\**************************************************************/

#ifndef WEBSOCKET_H
    #define WEBSOCKET_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QSslConfiguration>// QSslConfiguration
    #include <QHostAddress>     // QHostAddress
    #include <QByteArray>       // QByteArray
    #include <QSslSocket>       // QSslSocket
    #include <QObject>          // QObject
    #include <QString>          // QString

namespace xdesk::net { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class WebSocket: public QObject {
    Q_OBJECT

    private:
        QSslSocket* _socket = nullptr; // owned (child)
        bool _server = true; // server side: frames received are masked, sent unmasked
        bool _open = false; // handshake done
        bool _closing = false; // close frame sent
        QByteArray _buffer; // received bytes not parsed yet
        QByteArray _fragments; // payload of a fragmented message
        int _fragmentOpcode = 0; // opcode of the fragmented message
        QString _clientKey; // client side: Sec-WebSocket-Key sent
        QString _path; // client side: path asked
        qint64 _maxMessage = 0; // limit (XDESK_MAX_MESSAGE)

        // ---------- Pre-Function -------- //
        void onReadyRead_(void);
        void parseFrames_(void);
        void sendFrame_(const int opcode, const QByteArray& payload);
        _cold _nodiscard bool parseHandshakeResponse_(void); // client side

    public:
        // ---------- Pre-Function -------- //
        void sendText(const QString& text);
        _hot void sendBinary(const QByteArray& data);
        void close(const int code = 1000, const QString& reason = "");
        void abort(void); // drop without close frame
        _cold _nodiscard QHostAddress peerAddress(void) const;
        _hot _nodiscard qint64 pendingBytes(void) const; // not written to the network yet (back pressure)
        _cold _nodiscard QSslSocket* socket(void) const;

        /* client */
        _cold static WebSocket* connectTo(const QString& host, const quint16 port, const QString& path, const QSslConfiguration& tls, QObject* parent = nullptr); // emits opened / failed

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline bool isOpen(void) const {return this->_open && !this->_closing;};

        // ---------- Constructor --------- //
        WebSocket(QSslSocket* socket, const bool server, const QByteArray& pending = {}, QObject* parent = nullptr); // server: upgrade already answered

        // ----------- Destructor --------- //
        ~WebSocket() override;

    signals:
        void opened(void); // client: handshake done
        void failed(const QString& error); // client: connection / tls / handshake error
        void textReceived(const QString& text);
        void binaryReceived(const QByteArray& data);
        void closed(void); // once, whatever the reason
};

} // namespace end
#endif /* WEBSOCKET_H */
