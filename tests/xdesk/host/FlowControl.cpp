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
##  Frames in flight and quality against simulated links (ping, bandwidth, queue)
\**************************************************************/

#include "xdesk/host/FlowControl.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <deque>

// Result of a simulated stream
struct SimResult {
    double fps = 0.0; // frames received per second (last half of the run)
    double latency = 0.0; // ms from the send to the arrival (average, last half)
    int quality = 0; // last jpeg quality used
};

/*
 * Host ticking every 4 ms at 30 fps, frames of `size` bytes at quality 70 (scaled with the quality),
 * a link of `bandwidth` bytes per ms with a FIFO queue and `delay` ms each way, acks sent on arrival
 * fixed = true: the old behavior (2 frames in flight, quality untouched)
*/
static SimResult simulate(const double bandwidth, const int delay, const int size, const bool fixed)
{
    xdesk::host::FlowControl flow;
    std::deque<std::pair<std::int64_t, std::uint32_t>> acks; // <ms the ack reaches the host, id>
    std::int64_t linkFree = 0;
    std::int64_t lastFrame = -1000;
    std::uint32_t id = 0;
    int inFlight = 0;
    int received = 0;
    double latency = 0.0;
    SimResult result;

    for (std::int64_t now = 0; now < 20000; now += 4) {
        while (!acks.empty() && acks.front().first <= now) {
            flow.acked(acks.front().second, now);
            acks.pop_front();
            inFlight--;
        }
        flow.expire(now);
        const bool open = fixed ? inFlight < 2 : flow.canSend(30);
        if (!open || now - lastFrame < 1000 / 30) continue;
        const int quality = fixed ? 70 : flow.quality(70);
        const std::int64_t bytes = static_cast<std::int64_t>(size) * quality / 70;
        const std::int64_t start = std::max(now, linkFree);
        linkFree = start + static_cast<std::int64_t>(static_cast<double>(bytes) / bandwidth);
        const std::int64_t arrival = linkFree + delay;
        flow.sent(++id, bytes, now);
        inFlight++;
        lastFrame = now - lastFrame < 2000 / 30 ? lastFrame + 1000 / 30 : now; // paced on the slots like the stream
        acks.emplace_back(arrival + delay, id);
        result.quality = quality;
        if (now >= 10000) {
            received++;
            latency += static_cast<double>(arrival - now);
        }
    }
    result.fps = received / 10.0;
    result.latency = received ? latency / received : 0.0;
    ::testing::Test::RecordProperty((fixed ? "fixed_" : "flow_") + std::to_string(static_cast<int>(bandwidth)) + "_" + std::to_string(delay), std::to_string(result.fps) + " fps, " + std::to_string(static_cast<int>(result.latency)) + " ms, q" + std::to_string(result.quality));
    return result;
}

TEST(FlowControl, FarViewerOnAFastLinkGetsTheFullFrameRate)
{
    // 120 ms of ping, 20 Mbit/s, 60 KB frames: 2 frames per ping capped it near 14 fps
    const SimResult before = simulate(2500.0, 60, 60000, true);
    const SimResult after = simulate(2500.0, 60, 60000, false);

    EXPECT_LT(before.fps, 16.0);
    EXPECT_GT(after.fps, 27.0);
    EXPECT_LT(after.latency, 200.0); // no queue built
    EXPECT_EQ(after.quality, 70); // nothing lowered
}

TEST(FlowControl, SlowLinkLowersTheQualityInsteadOfQueuing)
{
    // 4 Mbit/s for 60 KB frames: 8 fps at most at quality 70
    const SimResult before = simulate(500.0, 40, 60000, true);
    const SimResult after = simulate(500.0, 40, 60000, false);

    EXPECT_LT(after.quality, 70);
    EXPECT_LT(after.latency, 400.0);
    EXPECT_GT(after.fps, before.fps * 2.5); // lighter frames: many more of them
    EXPECT_LT(after.latency, before.latency * 1.15); // for about the same lag
}

TEST(FlowControl, LanKeepsTheSmallWindowAndTheQuality)
{
    const SimResult after = simulate(12500.0, 1, 200000, false);

    EXPECT_GT(after.fps, 29.0);
    EXPECT_EQ(after.quality, 70);
    EXPECT_LT(after.latency, 40.0);
}

TEST(FlowControl, LostAcksAreGivenUp)
{
    xdesk::host::FlowControl flow;

    EXPECT_EQ(flow.quality(70), 70);
    flow.sent(1, 1000, 0);
    flow.sent(2, 1000, 10);
    EXPECT_FALSE(flow.canSend(30));
    flow.expire(2000);
    EXPECT_TRUE(flow.canSend(30));
    EXPECT_EQ(flow.inFlight(), 0);
    EXPECT_LT(flow.quality(70), 70); // a stall: lighter frames
}

/*
 * Automatic resolution: frames of `size` bytes at quality 70 and full resolution (bytes follow the quality and the
 * square of the scale), a link of `slow` bytes per ms for 20 s then `fast` for 40 s; returns <lowest scale, last scale>
*/
static std::pair<double, double> simulate_scale(const double slow, const double fast, const int size, const bool automatic)
{
    xdesk::host::FlowControl flow;
    std::deque<std::pair<std::int64_t, std::uint32_t>> acks;
    std::int64_t linkFree = 0;
    std::int64_t lastFrame = -1000;
    std::uint32_t id = 0;
    double lowest = 1.0;

    flow.setAutoScale(automatic);
    for (std::int64_t now = 0; now < 60000; now += 4) {
        const double bandwidth = now < 20000 ? slow : fast;
        while (!acks.empty() && acks.front().first <= now) {
            flow.acked(acks.front().second, now);
            acks.pop_front();
        }
        flow.expire(now);
        if (!flow.canSend(30) || now - lastFrame < 1000 / 30) continue;
        const double scale = flow.scale();
        const std::int64_t bytes = static_cast<std::int64_t>(size * flow.quality(70) / 70.0 * scale * scale);
        const std::int64_t start = std::max(now, linkFree);
        linkFree = start + static_cast<std::int64_t>(static_cast<double>(bytes) / bandwidth);
        flow.sent(++id, bytes, now);
        lastFrame = now - lastFrame < 2000 / 30 ? lastFrame + 1000 / 30 : now;
        acks.emplace_back(linkFree + 2 * 20, id);
        lowest = std::min(lowest, scale);
    }
    return {lowest, flow.scale()};
}

TEST(FlowControl, TooSlowLinkLowersTheResolutionThenRaisesIt)
{
    // 50 kB frames on 20 kB/s: even the lowest quality queues -> smaller frames; then 5 MB/s: back to the asked resolution
    const std::pair<double, double> result = simulate_scale(20.0, 5000.0, 50000, true);

    EXPECT_LT(result.first, 0.6);
    EXPECT_DOUBLE_EQ(result.second, 1.0);
}

TEST(FlowControl, AutomaticResolutionCanBeTurnedOff)
{
    const std::pair<double, double> result = simulate_scale(20.0, 5000.0, 50000, false);

    EXPECT_DOUBLE_EQ(result.first, 1.0);
}

TEST(FlowControl, FastLinkKeepsTheAskedResolution)
{
    const std::pair<double, double> result = simulate_scale(5000.0, 5000.0, 50000, true);

    EXPECT_DOUBLE_EQ(result.first, 1.0);
}
