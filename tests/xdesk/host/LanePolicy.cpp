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
##  @file LanePolicy.cpp

File Description:
##  Sockets of the video against simulated paths (one connection the limit, the link the limit, recovery)
\**************************************************************/

#include "xdesk/host/LanePolicy.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <string>

// Result of a simulated viewer
struct LaneRun {
    int lanes = 1; // at the end
    int most = 1; // highest count used
    int adds = 0; // sockets added (tries included)
    double pressure = 0.0; // mean of the last 20 s
};

/*
 * A viewer asking `demand` bytes per ms, each connection carrying at most `perLane` (its window on a lossy far path),
 * the link at most `link`: what is not delivered holds the frames back (pressure = share not delivered)
*/
static LaneRun simulate(xdesk::host::LanePolicy& policy, const double demand, const double perLane, const double link, const int seconds, const std::int64_t start = 1)
{
    LaneRun run;
    int lanes = policy.lanes();
    double pressure = 0.0;
    int samples = 0;

    for (std::int64_t t = start; t < start + seconds * 1000LL; t += 50) {
        const double delivered = std::min({demand, link, perLane * lanes});
        policy.delivered(static_cast<std::int64_t>(delivered * 50), t);
        if (t % 1000 < 50) {
            const double p = 1.0 - delivered / demand;
            policy.pressure(p, t);
            if (t >= start + (seconds - 20) * 1000LL) {pressure += p; samples++;}
        }
        if (t % 250 < 50) {
            const int now = policy.update(t);
            if (now > lanes) run.adds++;
            lanes = now;
            run.most = std::max(run.most, lanes);
        }
    }
    run.lanes = lanes;
    run.pressure = samples > 0 ? pressure / samples : 0.0;
    return run;
}

TEST(LanePolicy, OneSocketWhileThePathKeepsUp)
{
    xdesk::host::LanePolicy policy;
    const LaneRun run = simulate(policy, 400.0, 1000.0, 5000.0, 60);

    EXPECT_EQ(run.lanes, 1);
    EXPECT_EQ(run.adds, 0);
    EXPECT_LT(run.pressure, 0.01);
}

TEST(LanePolicy, MoreSocketsWhenOneConnectionIsTheLimit)
{
    xdesk::host::LanePolicy policy;
    const LaneRun run = simulate(policy, 600.0, 200.0, 5000.0, 90); // a lossy far path: 1/3 per connection

    ::testing::Test::RecordProperty("lanes", run.lanes);
    ::testing::Test::RecordProperty("pressure", std::to_string(run.pressure));
    EXPECT_GE(run.lanes, 3);
    EXPECT_LT(run.pressure, 0.05);
}

TEST(LanePolicy, NoExtraSocketWhenTheLinkIsFull)
{
    xdesk::host::LanePolicy policy;
    const LaneRun run = simulate(policy, 600.0, 300.0, 300.0, 120); // the link is the limit: more sockets cannot help

    EXPECT_EQ(run.lanes, 1);
    EXPECT_LE(run.adds, 4); // tried, given back, tried again less and less often
}

TEST(LanePolicy, SocketsGivenBackOnceThePathRecovers)
{
    xdesk::host::LanePolicy policy;
    const LaneRun bad = simulate(policy, 600.0, 200.0, 5000.0, 90);
    const LaneRun good = simulate(policy, 150.0, 200.0, 5000.0, 90, 90001);

    EXPECT_GE(bad.lanes, 3);
    EXPECT_EQ(good.lanes, 1);
}

TEST(LanePolicy, MaxCapsTheSockets)
{
    xdesk::host::LanePolicy policy;
    LaneRun run;

    policy.setMax(2);
    run = simulate(policy, 600.0, 100.0, 5000.0, 90);
    EXPECT_EQ(run.most, 2);
    policy.setMax(1);
    EXPECT_EQ(policy.lanes(), 1);
}
