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
##  @file UdpLink.hpp

File Description:
##  Viewer side of the encrypted UDP video: hello, frames put together in any order, nacks, acks, keep alive
\**************************************************************/

#ifndef UDPLINK_H
    #define UDPLINK_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "../net/Datagram.hpp"  // xdesk::net::DatagramCrypto, FrameAssembler
    #include <QHostAddress>         // QHostAddress
    #include <QJsonObject>          // QJsonObject
    #include <QUdpSocket>           // QUdpSocket
    #include <QByteArray>           // QByteArray
    #include <QObject>              // QObject
    #include <cstdint>              // std::uint16_t, std::uint32_t, std::int64_t
    #include <QTimer>               // QTimer

    //----------------------------------------------------------------//
    /* DEFINE */

    #define LINK_HELLO_EVERY 250 // ms between two hellos until the host answers
    #define LINK_HELLO_GIVE_UP 2500 // ms of hellos without answer: the video stays on tcp
    #define LINK_PING_EVERY 1000 // ms between keep alives (NAT mappings)
    #define LINK_SILENCE 5000 // ms without an authentic datagram of the host before the video goes back to tcp

namespace xdesk::client { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class UdpLink: public QObject {
    Q_OBJECT

    private:
        QUdpSocket _socket;
        QTimer _timer; // hellos, nacks, keep alive (5 ms)
        net::DatagramCrypto _crypto;
        net::FrameAssembler _assembler;
        QHostAddress _host;
        std::uint16_t _port = 0;
        bool _active = false;
        bool _started = false;
        std::int64_t _since = 0; // ms of the first hello
        std::int64_t _hello = 0; // ms of the last hello
        std::int64_t _ping = 0; // ms of the last keep alive
        std::int64_t _seen = 0; // ms of the last authentic datagram of the host
        int _rtt = 50; // ms (latency of the session)

        // ---------- Pre-Function -------- //
        void onReadyRead_(void);
        void tick_(void);
        void send_(const net::Datagram type, const QByteArray& payload);

    public:
        // ---------- Pre-Function -------- //
        bool start(const QHostAddress& host, const QJsonObject& offer); // false: no usable offer
        void stop(void);
        void ack(const std::uint32_t id);

        // ------------ Function ---------- //
        /* setter */
        _cold inline void setRtt(const int rtt) {this->_rtt = std::max(1, rtt);};

        /* getter */
        _cold _nodiscard inline bool isActive(void) const {return this->_active;};

        // ---------- Constructor --------- //
        explicit UdpLink(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~UdpLink() override = default;

    signals:
        void frame(const QByteArray& message); // a Binary::Frame put together
        void activeChanged(const bool active); // false also when the hellos got no answer
        void gaveUp(void); // a frame lost for good: a complete one is needed
};

} // namespace end
#endif /* UDPLINK_H */
