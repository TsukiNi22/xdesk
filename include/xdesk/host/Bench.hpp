/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/11/2026 by @author Tsukini

File Name:
##  @file Bench.hpp

File Description:
##  xdesk bench: every way of sending the screen measured on this computer, then played on simulated networks
\**************************************************************/

#ifndef BENCH_H
    #define BENCH_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <QString>          // QString
    #include <QList>            // QList

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// A network of the simulation
struct BenchNetwork {
    QString name;
    double mbits = 0.0; // bandwidth
    double rtt = 0.0; // ping (ms)
    double loss = 0.0; // share of the packets lost (0 - 1)
};

// One way of sending one scene, measured on this computer
struct BenchResult {
    QString scene; // typing, scroll, drag, video
    QString mode; // tiles, tiles+palette+copy, h264_nvenc, openh264...
    double encodeMs = 0.0; // mean time to encode a frame (wall clock, every core)
    double p95Ms = 0.0;
    double bytes = 0.0; // mean bytes per frame
    double psnr = 0.0; // dB of what the viewer shows (0 = not measured)
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard QList<BenchNetwork> bench_networks(void); // lan, wifi, fiber wan, 4g, degraded, very bad
_cold _nodiscard double bench_fps(const BenchResult& result, const BenchNetwork& network, const int cap = 60); // frames per second this way reaches on this network
_cold _nodiscard double bench_latency(const BenchResult& result, const BenchNetwork& network); // ms from the capture to the screen of the viewer
_cold _nodiscard QString bench_file(void); // <data>/bench.json: the measures of the last xdesk bench on this computer
// The video encoder of the last bench giving the most frames on this link (its large moves: drag + video scenes), "" = no bench / none fits
_cold _nodiscard QString bench_best_encoder(const int height, const double mbits, const double rtt, const int codecs);
// Runs the bench (heights: "1080", "2160", "1080,2160", "all"; frames per scene), prints the tables, saves them (bench_file), 0 = done
_cold int run_bench(int& argc, char** argv, const QString& heights, const int frames = 60);

} // namespace end
#endif /* BENCH_H */
