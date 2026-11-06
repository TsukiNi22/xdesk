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
##  @file UdpServer.hpp

File Description:
##  Video of the native viewers over encrypted UDP: pieces, parity when the path loses, pieces sent again on a nack
\**************************************************************/

#ifndef UDPSERVER_H
    #define UDPSERVER_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "../net/Datagram.hpp"  // xdesk::net::DatagramCrypto, Datagram
    #include <QHostAddress>         // QHostAddress
    #include <QJsonObject>          // QJsonObject
    #include <QUdpSocket>           // QUdpSocket
    #include <QByteArray>           // QByteArray
    #include <QObject>              // QObject
    #include <QString>              // QString
    #include <cstdint>              // std::uint16_t, std::uint32_t, std::int64_t
    #include <QTimer>               // QTimer
    #include <QHash>                // QHash
    #include <QList>                // QList

    //----------------------------------------------------------------//
    /* DEFINE */

    #define UDP_SILENCE 4000 // ms without an authentic datagram of a viewer before its video goes back to tcp
    #define UDP_KEEP 3000 // ms a frame is kept for the nacks
    #define UDP_FEC_ON 0.005 // share of the pieces asked again from which the parity is sent
    #define UDP_FEC_OFF 0.001 // share under which it stops (after UDP_FEC_CALM ms)
    #define UDP_FEC_CALM 10000 // ms of a clean path before the parity stops

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// A frame sent and kept for the nacks
struct UdpFrame {
    QList<QByteArray> payloads; // data pieces then parity ones
    int count = 0; // data pieces
    std::int64_t sent = 0; // ms of the first send
    std::int64_t last = 0; // ms of the last (re)send
    bool acked = false;
};

// A native viewer reached over UDP
struct UdpPeer {
    QString session;
    net::DatagramCrypto crypto;
    QHostAddress address; // learnt from its first authentic datagram only
    std::uint16_t port = 0;
    bool active = false;
    std::int64_t seen = 0; // ms of its last authentic datagram
    QHash<std::uint32_t, UdpFrame> frames; // id -> pieces kept
    double rtt = 50.0; // smoothed ms (send -> ack)
    double sentPieces = 0.0; // decaying counts of the last seconds
    double askedPieces = 0.0;
    bool fec = false;
    std::int64_t clean = 0; // ms since the path loses less than UDP_FEC_OFF
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * One QUdpSocket on the port of the server. A viewer gets {port, connection, keys} in its welcome (inside TLS), proves
 * the keys with an authentic Hello from its address, then gets its frames here instead of on its websocket.
 * Nothing is ever sent to an address that did not send an authentic datagram (no reflection, no amplification).
*/
class UdpServer: public QObject {
    Q_OBJECT

    private:
        QUdpSocket _socket;
        QTimer _timer; // probes, silence, parity decision
        QHash<std::uint32_t, UdpPeer> _peers; // connection -> viewer
        QHash<QString, std::uint32_t> _sessions; // session -> connection

        // ---------- Pre-Function -------- //
        void onReadyRead_(void);
        void tick_(void);
        void send_(UdpPeer& peer, const net::Datagram type, const QByteArray& payload);
        void setActive_(UdpPeer& peer, const bool active);

    public:
        // ---------- Pre-Function -------- //
        bool listen(const QHostAddress& address, const std::uint16_t port); // false: the port is taken (the video stays on tcp)
        void stop(void);
        _cold _nodiscard QJsonObject offer(const QString& session); // keys of a new viewer (sent in its welcome)
        void remove(const QString& session);
        _hot bool sendFrame(const QString& session, const QByteArray& message); // false: not reached over udp (send it on tcp)
        _cold _nodiscard QString describe(const QString& session) const; // "udp", "udp + parity" or "" (tcp)

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline bool isListening(void) const {return this->_socket.state() == QAbstractSocket::BoundState;};

        // ---------- Constructor --------- //
        explicit UdpServer(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~UdpServer() override = default;

    signals:
        void acked(const QString& session, const std::uint32_t id); // a frame arrived complete
        void activeChanged(const QString& session, const bool active); // its video moved to udp / back to tcp
};

} // namespace end
#endif /* UDPSERVER_H */
