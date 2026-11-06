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
##  @file LanePolicy.hpp

File Description:
##  How many sockets carry the video of a web viewer: more while the path holds the frames back and they help, one when it keeps up
\**************************************************************/

#ifndef LANEPOLICY_H
    #define LANEPOLICY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <cstdint>          // std::int64_t

    //----------------------------------------------------------------//
    /* DEFINE */

    #define LANE_MAX 4 // sockets of the video of a viewer at most (its main one included)
    #define LANE_WINDOW 2000 // ms of one measure (delivered bytes, mean pressure)
    #define LANE_WARMUP 1000 // ms after a change not judged (the new socket connects, the window adapts)
    #define LANE_TRIAL 5000 // ms a change is tried before it is judged
    #define LANE_PRESSED 0.25 // pressure from which one more socket is tried (2 windows in a row)
    #define LANE_RELAXED 0.05 // pressure under which a socket is given back (LANE_RELAXED_FOR ms)
    #define LANE_RELAXED_FOR 10000
    #define LANE_GAIN 1.10 // a socket more is kept when it delivers 10 % more...
    #define LANE_EASED 0.15 // ... or holds the frames back 0.15 less
    #define LANE_BACKOFF 15000 // ms before a socket that did not help is tried again (doubled each time)
    #define LANE_BACKOFF_MAX 120000
    #define LANE_HOLD_REMOVE 60000 // ms before giving back a socket again when it was needed

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class LaneTrial {
    None,
    Add, // one socket more, kept if the path delivers more / holds back less
    Remove, // one socket less, put back if the path holds back more
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Several TCP connections only help when one connection is the limit (its window after losses on a far path), not when
 * the link itself is full: each change is a trial, kept only if it measurably helps, and a socket is given back once
 * the path keeps up again. Pressure = share of the frame slots held back by the flow control (full window, lowered
 * quality), reported by the stream every second
*/
class LanePolicy {
    private:
        int _lanes = 1;
        int _max = LANE_MAX;
        bool _fixed = false; // count forced (XDESK_LANES): no trial
        std::int64_t _windowAt = 0; // ms the current window started (0 = not started)
        std::int64_t _windowBytes = 0;
        double _windowPressure = 0.0;
        int _windowSamples = 0;
        double _goodput = 0.0; // bytes per ms of the last window
        double _pressure = 0.0; // mean of the last window
        int _pressed = 0; // windows in a row over LANE_PRESSED
        std::int64_t _relaxedSince = 0; // ms the pressure went under LANE_RELAXED (0 = not relaxed)
        LaneTrial _trial = LaneTrial::None;
        std::int64_t _trialAt = 0; // ms the change was made
        std::int64_t _trialBytes = 0; // delivered after the warmup
        double _trialPressure = 0.0;
        int _trialSamples = 0;
        double _baseGoodput = 0.0; // before the change
        double _basePressure = 0.0;
        int _failures = 0; // tries in a row that did not help
        std::int64_t _holdAdd = 0; // ms before which no socket is added
        std::int64_t _holdRemove = 0; // ms before which no socket is given back

        // ---------- Pre-Function -------- //
        void start_(const LaneTrial trial, const std::int64_t now);
        void judge_(const std::int64_t now);

    public:
        // ---------- Pre-Function -------- //
        void delivered(const std::int64_t bytes, const std::int64_t now); // a frame acked
        void pressure(const double pressure, const std::int64_t now); // the last second of the stream
        _hot int update(const std::int64_t now); // sockets wanted now
        void setMax(const int max); // sockets that can be used (1 = only the main one)
        void setFixed(const int lanes); // this count whatever the path (0 = adaptive again)
        void reset(void);

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int lanes(void) const           {return this->_lanes;};
        _cold _nodiscard inline double goodput(void) const      {return this->_goodput;};
        _cold _nodiscard inline double lastPressure(void) const {return this->_pressure;};
        _cold _nodiscard inline LaneTrial trial(void) const     {return this->_trial;};
};

} // namespace end
#endif /* LANEPOLICY_H */
