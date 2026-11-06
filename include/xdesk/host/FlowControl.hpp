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
##  @file FlowControl.hpp

File Description:
##  Frames in flight and image quality of one viewer, sized from the bandwidth and the ping measured by the acks
\**************************************************************/

#ifndef FLOWCONTROL_H
    #define FLOWCONTROL_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <algorithm>        // std::min
    #include <cstdint>          // std::uint32_t, std::int64_t
    #include <QList>            // QList
    #include <QPair>            // QPair

    //----------------------------------------------------------------//
    /* DEFINE */

    #define FLOW_MIN_WINDOW 2 // frames always allowed in flight (the window of a LAN)
    #define FLOW_MAX_WINDOW 8 // frames in flight at most (a far viewer on a fast link)
    #define FLOW_RATE_WINDOW 2500 // ms of delivery rate samples kept (max filter)
    #define FLOW_RTT_WINDOW 10000 // ms of ping samples kept (min filter)
    #define FLOW_ACK_TIMEOUT 1500 // ms without ack before the frames in flight are given up
    #define FLOW_QUEUE_HIGH 40.0 // ms of queuing (smoothed) from which the quality goes down
    #define FLOW_QUEUE_LOW 15.0 // ms of queuing under which the quality may go up again
    #define FLOW_QUALITY_FLOOR 25 // lowest jpeg quality chosen by the flow control
    #define FLOW_DOWN_EVERY 400 // ms between two steps down of the quality
    #define FLOW_UP_EVERY 1500 // ms between two steps up of the quality

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// A frame sent and not acked yet
struct FlowFrame {
    std::uint32_t id = 0;
    std::int64_t at = 0; // ms it was sent
    std::int64_t bytes = 0;
    std::int64_t delivered = 0; // bytes acked when it was sent
    std::int64_t deliveredAt = 0; // ms of that count
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Window (at least 2 frames, at most 8):
 *  - no queue: the frames the asked rate needs during one ping (min of the last acks), so a far viewer gets its frame
 *    rate instead of 2 frames per ping
 *  - a queue: what the path holds, its bandwidth (bytes acked over the time they took, max of the last samples) x ping
 * The time an ack takes beyond the ping is a queue: the jpeg quality steps down while it grows, up once it is gone.
*/
class FlowControl {
    private:
        QList<FlowFrame> _flight; // oldest first
        QList<QPair<std::int64_t, double>> _rates; // <ms, bytes per ms>
        QList<QPair<std::int64_t, double>> _rtts; // <ms, ping ms>
        std::int64_t _delivered = 0; // bytes acked since the reset
        std::int64_t _deliveredAt = 0; // ms of the last ack (or of the send that ended an idle time)
        std::int64_t _bytes = 0; // bytes in flight
        double _size = 0.0; // usual size of a frame (smoothed bytes)
        double _queue = 0.0; // smoothed queuing delay (ms)
        int _quality = 100; // ceiling of the jpeg quality
        int _asked = 100; // quality asked by the viewer (last one seen)
        std::int64_t _changed = 0; // ms of the last quality step

    public:
        // ---------- Pre-Function -------- //
        void reset(void); // everything measured again (frames in flight forgotten)
        void sent(const std::uint32_t id, const std::int64_t bytes, const std::int64_t now);
        void acked(const std::uint32_t id, const std::int64_t now); // acks the frames up to id (in order)
        void expire(const std::int64_t now); // frames without ack for too long: given up (lost acks, a sleeping viewer)
        _hot _nodiscard bool canSend(const int fps) const; // one more frame fits in the window (fps: the rate asked)
        _hot _nodiscard int quality(const int asked); // jpeg quality to use: the asked one, lowered while the path is congested
        _cold _nodiscard double bandwidth(void) const; // bytes per ms (0 = not measured yet)
        _cold _nodiscard double minRtt(void) const; // ms (0 = not measured yet)

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int inFlight(void) const {return static_cast<int>(this->_flight.size());};
        _cold _nodiscard inline double queue(void) const {return this->_queue;};
        _cold _nodiscard inline int ceiling(void) const  {return this->_quality;};
};

} // namespace end
#endif /* FLOWCONTROL_H */
