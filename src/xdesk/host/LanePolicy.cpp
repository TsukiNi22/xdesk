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
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/host/LanePolicy.hpp"
#include <algorithm>

void xdesk::host::LanePolicy::reset(void)
{
    const int max = this->_max;
    const bool fixed = this->_fixed;

    *this = LanePolicy();
    this->_max = max;
    this->_fixed = fixed;
    if (fixed) this->_lanes = max;
}

void xdesk::host::LanePolicy::setMax(const int max)
{
    if (this->_fixed) return;
    this->_max = std::clamp(max, 1, LANE_MAX);
    if (this->_lanes > this->_max) {
        this->_lanes = this->_max;
        this->_trial = LaneTrial::None;
    }
}

void xdesk::host::LanePolicy::setFixed(const int lanes)
{
    this->_fixed = lanes > 0;
    this->_trial = LaneTrial::None;
    this->_max = this->_fixed ? std::clamp(lanes, 1, LANE_MAX) : LANE_MAX;
    this->_lanes = this->_fixed ? this->_max : 1;
}

void xdesk::host::LanePolicy::delivered(const std::int64_t bytes, const std::int64_t now)
{
    this->_windowBytes += bytes;
    if (this->_trial != LaneTrial::None && now - this->_trialAt >= LANE_WARMUP) this->_trialBytes += bytes;
}

void xdesk::host::LanePolicy::pressure(const double pressure, const std::int64_t now)
{
    const double p = std::clamp(pressure, 0.0, 1.0);

    this->_windowPressure += p;
    this->_windowSamples++;
    if (this->_trial != LaneTrial::None && now - this->_trialAt >= LANE_WARMUP) {
        this->_trialPressure += p;
        this->_trialSamples++;
    }
}

void xdesk::host::LanePolicy::start_(const LaneTrial trial, const std::int64_t now)
{
    this->_trial = trial;
    this->_trialAt = now;
    this->_trialBytes = 0;
    this->_trialPressure = 0.0;
    this->_trialSamples = 0;
    this->_baseGoodput = this->_goodput;
    this->_basePressure = this->_pressure;
    this->_lanes += trial == LaneTrial::Add ? 1 : -1;
}

void xdesk::host::LanePolicy::judge_(const std::int64_t now)
{
    const double span = static_cast<double>(std::max<std::int64_t>(1, now - this->_trialAt - LANE_WARMUP));
    const double goodput = static_cast<double>(this->_trialBytes) / span;
    const double pressure = this->_trialSamples > 0 ? this->_trialPressure / this->_trialSamples : this->_basePressure;

    if (this->_trial == LaneTrial::Add) {
        const bool helped = goodput >= this->_baseGoodput * LANE_GAIN || pressure <= this->_basePressure - LANE_EASED;
        if (helped) this->_failures = 0;
        else {
            // The link itself is the limit (or the socket did not connect): back to fewer, tried again later
            this->_lanes--;
            this->_holdAdd = now + std::min<std::int64_t>(LANE_BACKOFF_MAX, static_cast<std::int64_t>(LANE_BACKOFF) << std::min(this->_failures, 4));
            this->_failures++;
        }
    } else if (pressure >= LANE_PRESSED || pressure > this->_basePressure + LANE_EASED) {
        // It was needed
        this->_lanes++;
        this->_holdRemove = now + LANE_HOLD_REMOVE;
    }
    this->_trial = LaneTrial::None;
    this->_pressed = 0;
    this->_relaxedSince = 0;
    this->_windowAt = now;
    this->_windowBytes = 0;
    this->_windowPressure = 0.0;
    this->_windowSamples = 0;
}

int xdesk::host::LanePolicy::update(const std::int64_t now)
{
    if (this->_fixed) return this->_lanes;
    if (this->_windowAt == 0) {this->_windowAt = now; return this->_lanes;}
    if (this->_trial != LaneTrial::None) {
        if (now - this->_trialAt >= LANE_TRIAL) this->judge_(now);
        return this->_lanes;
    }
    if (now - this->_windowAt < LANE_WINDOW) return this->_lanes;

    // Close the window
    this->_goodput = static_cast<double>(this->_windowBytes) / static_cast<double>(now - this->_windowAt);
    this->_pressure = this->_windowSamples > 0 ? this->_windowPressure / this->_windowSamples : 0.0;
    this->_windowAt = now;
    this->_windowBytes = 0;
    this->_windowPressure = 0.0;
    this->_windowSamples = 0;
    this->_pressed = this->_pressure >= LANE_PRESSED ? this->_pressed + 1 : 0;
    if (this->_pressure >= LANE_RELAXED) this->_relaxedSince = 0;
    else if (this->_relaxedSince == 0) this->_relaxedSince = now;

    // Held back twice in a row: one socket more; kept up for a while: one less
    if (this->_pressed >= 2 && this->_lanes < this->_max && now >= this->_holdAdd) this->start_(LaneTrial::Add, now);
    else if (this->_lanes > 1 && this->_relaxedSince != 0 && now - this->_relaxedSince >= LANE_RELAXED_FOR && now >= this->_holdRemove) this->start_(LaneTrial::Remove, now);
    return this->_lanes;
}
