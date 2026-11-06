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
##  @file Lanes.hpp

File Description:
##  Extra websockets of the video of the web viewers: joined with a token of the session, frames on the least loaded one, opened and given back by the lane policy
\**************************************************************/

#ifndef LANES_H
    #define LANES_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "../net/WebSocket.hpp" // xdesk::net::WebSocket
    #include "LanePolicy.hpp"       // xdesk::host::LanePolicy
    #include <QJsonObject>          // QJsonObject
    #include <QByteArray>           // QByteArray
    #include <QPointer>             // QPointer
    #include <QObject>              // QObject
    #include <QString>              // QString
    #include <cstdint>              // std::uint32_t, std::int64_t
    #include <QTimer>               // QTimer
    #include <QHash>                // QHash
    #include <QPair>                // QPair
    #include <QList>                // QList

    //----------------------------------------------------------------//
    /* DEFINE */

    #define LANE_JOIN_TIMEOUT 5000 // ms a socket opened on /lane has to name its session
    #define LANE_PENDING_MAX 32 // sockets waiting for their first message at most (others dropped)
    #define LANE_RETIRE 3000 // ms a socket no longer wanted keeps its frames in flight before it is closed
    #define LANE_FLIGHT_KEPT 512 // frames remembered for the delivered bytes

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// The extra sockets of one viewer
struct LaneSet {
    QByteArray token; // 32 random bytes given in the welcome (inside the authenticated TLS session)
    QList<QPointer<net::WebSocket>> sockets; // lane i + 1 (null = not connected)
    LanePolicy policy;
    QHash<std::uint32_t, QPair<std::int64_t, int>> flight; // frame -> <bytes, lane (0 = main socket)>
    int announced = 1; // sockets the viewer was told to keep (its main one included)
    int turn = 0; // next socket among the equally loaded ones (all kept warm)
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * A lane socket only carries frames (host -> viewer) and their acks (viewer -> host): no input, no control, no
 * terminal. It is refused unless its first message names an active web session with that session's token
 * XDESK_LANES=1..4 forces the count (diagnosis of a known path), else the lane policy decides
*/
class Lanes: public QObject {
    Q_OBJECT

    private:
        QHash<QString, LaneSet> _sets; // session -> its lanes
        QList<QPointer<net::WebSocket>> _pending; // opened, not joined yet
        QTimer _timer; // policy of every set (250 ms)

        // ---------- Pre-Function -------- //
        void onTick_(void);
        void onJoin_(net::WebSocket* ws, const QString& text);
        void onLaneClosed_(const QString& session, net::WebSocket* ws);
        void retire_(LaneSet& set, const int keep); // lanes from keep on stop taking frames, closed a bit later

    public:
        // ---------- Pre-Function -------- //
        _cold _nodiscard QJsonObject offer(const QString& session); // {token, max} for the welcome of a web viewer
        void remove(const QString& session);
        void join(net::WebSocket* ws); // a socket of /lane (owned from now on)
        _hot bool sendFrame(const QString& session, const QByteArray& message, const qint64 mainPending); // false: send it on the main socket
        _hot void acked(const QString& session, const std::uint32_t id); // any path
        void pressure(const QString& session, const double pressure);
        void setMax(const QString& session, const int max); // 1 = only the main socket (the video went over udp)
        _cold _nodiscard QString describe(const QString& session) const; // "tcp x3", "" with one socket

        // ---------- Constructor --------- //
        explicit Lanes(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Lanes() override;

    signals:
        void wanted(const QString& session, const int lanes); // to tell the viewer ({"t":"lanes","n":lanes})
        void frameAcked(const QString& session, const std::uint32_t id); // an ack received on a lane
        void lost(const QString& session); // a lane dropped frames in flight: a complete frame is needed
        void changed(const QString& session); // its sockets changed
};

} // namespace end
#endif /* LANES_H */
