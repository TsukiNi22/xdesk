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
##  @file FlowControl.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/host/FlowControl.hpp"
#include <algorithm>
#include <cmath>

void xdesk::host::FlowControl::reset(void)
{
    this->_flight.clear();
    this->_rates.clear();
    this->_rtts.clear();
    this->_delivered = 0;
    this->_deliveredAt = 0;
    this->_bytes = 0;
    this->_queue = 0.0;
}

void xdesk::host::FlowControl::sent(const std::uint32_t id, const std::int64_t bytes, const std::int64_t now)
{
    // After an idle time the rate is measured from this send (not from the last ack, long ago)
    if (this->_flight.isEmpty()) this->_deliveredAt = now;
    this->_flight.append(FlowFrame{id, now, bytes, this->_delivered, this->_deliveredAt});
    this->_bytes += bytes;
}

void xdesk::host::FlowControl::acked(const std::uint32_t id, const std::int64_t now)
{
    FlowFrame last;
    qsizetype index = 0;

    // Selective: over udp the frames complete in any order (a late one never holds the next ones)
    while (index < this->_flight.size() && this->_flight[index].id != id)
        ++index;
    if (index == this->_flight.size()) return;
    last = this->_flight.takeAt(index);
    this->_bytes -= last.bytes;
    this->_delivered += last.bytes;
    this->_deliveredAt = now;

    // Samples: delivery rate since the frame was sent, ping
    const double interval = static_cast<double>(std::max<std::int64_t>(1, now - last.deliveredAt));
    const double rtt = static_cast<double>(std::max<std::int64_t>(1, now - last.at));
    double recent = rtt;

    this->_rates.append(qMakePair(now, static_cast<double>(this->_delivered - last.delivered) / interval));
    this->_rtts.append(qMakePair(now, rtt));
    while (!this->_rates.isEmpty() && now - this->_rates.first().first > FLOW_RATE_WINDOW)
        this->_rates.removeFirst();
    while (this->_rtts.size() > 1 && now - this->_rtts.first().first > FLOW_RTT_WINDOW)
        this->_rtts.removeFirst();

    // Standing queue: the lowest ping of the last moments above the lowest of all (a lost packet, jitter or a big frame
    // makes one ack late, a queue makes all of them late)
    for (const QPair<std::int64_t, double>& sample: this->_rtts)
        if (now - sample.first <= FLOW_QUEUE_WINDOW) recent = std::min(recent, sample.second);
    this->_queue = std::max(0.0, recent - this->minRtt());

    // Quality: down while a queue grows, up again once it is gone
    if (this->_queue > FLOW_QUEUE_HIGH && now - this->_changed >= FLOW_DOWN_EVERY) {
        this->_quality = std::max(FLOW_QUALITY_FLOOR, std::min(this->_quality, this->_asked) - 10);
        this->_changed = now;
    } else if (this->_queue < FLOW_QUEUE_LOW && this->_quality < 100 && now - this->_changed >= FLOW_UP_EVERY) {
        this->_quality = this->_quality + 5 >= this->_asked ? 100 : this->_quality + 5;
        this->_changed = now;
    }
}

void xdesk::host::FlowControl::expire(const std::int64_t now)
{
    if (this->_flight.isEmpty() || now - this->_flight.first().at <= FLOW_ACK_TIMEOUT) return;
    this->_flight.clear();
    this->_bytes = 0;
    this->_quality = std::max(FLOW_QUALITY_FLOOR, std::min(this->_quality, this->_asked) - 10); // a stall: lighter frames
    this->_changed = now;
}

bool xdesk::host::FlowControl::canSend(const int fps) const
{
    const qsizetype count = this->_flight.size();
    const double rtt = this->minRtt();

    if (count < FLOW_MIN_WINDOW) return true;
    if (count >= FLOW_MAX_WINDOW || rtt <= 0.0) return false;
    // No queue: the frames the frame rate needs during one ping
    if (this->_queue < FLOW_QUEUE_LOW && count < static_cast<qsizetype>(std::ceil(fps * rtt / 1000.0)) + 1) return true;
    // A queue: only what the path holds (its bandwidth during one ping)
    return static_cast<double>(this->_bytes) < this->bandwidth() * rtt;
}

int xdesk::host::FlowControl::quality(const int asked)
{
    this->_asked = asked;
    return std::max(1, std::min(asked, this->_quality));
}

double xdesk::host::FlowControl::bandwidth(void) const
{
    double best = 0.0;

    for (const QPair<std::int64_t, double>& sample: this->_rates)
        best = std::max(best, sample.second);
    return best;
}

double xdesk::host::FlowControl::minRtt(void) const
{
    double best = 0.0;

    for (const QPair<std::int64_t, double>& sample: this->_rtts)
        if (best == 0.0 || sample.second < best) best = sample.second;
    return best;
}
