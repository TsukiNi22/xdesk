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
##  @file Bench.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/FrameEncoder.hpp"
#include "xdesk/platform/AvCodec.hpp"
#include "xdesk/platform/H264.hpp"
#include "xdesk/host/Compress.hpp"
#include "xdesk/host/Bench.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStringList>
#include <algorithm>
#include <QVector>
#include <cstdint>
#include <clocale>
#include <cstdio>
#include <memory>
#include <cmath>

//----------------------------------------------------------------//
/* DEFINE */

#define BENCH_QUALITY 70 // quality of every mode (the default of the hosts)
#define BENCH_PSNR_EVERY 10 // frames between two quality measures
#define BENCH_GOOD_PSNR 28.0 // dB under which a way is too blurry to be chosen (1 pixel text: jpeg 70 is about 30)
#define BENCH_PARITY 0.875 // share of the udp packets that carry data while the path loses (xor parity 1 / 8)
#define BENCH_DECODE_MS 3.0 // ms a viewer takes to draw a frame (decoder / canvas)

using xdesk::host::BenchResult;
using xdesk::host::BenchNetwork;

//----------------------------------------------------------------//
/* TOOLS */

_cold static std::uint32_t mix(std::uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352dU;
    v ^= v >> 15;
    v *= 0x846ca68bU;
    v ^= v >> 16;
    return v;
}

// Lines of pseudo glyphs (7 x 12 strokes in cells of 9 x 18) on an editor background: text, as a compressor sees it
_cold static void draw_text(QImage& image, const int top, const int height, const std::uint32_t seed, const int scroll = 0)
{
    static const std::uint32_t INK[4] = {0xFFD4D4D4, 0xFF569CD6, 0xFFCE9178, 0xFF6A9955};

    for (int y = std::max(0, top); y < std::min(image.height(), top + height); ++y) {
        std::uint32_t* row = reinterpret_cast<std::uint32_t*>(image.scanLine(y));
        const int page = y - top + scroll; // line of the page shown on this row
        const int line = page / 18;
        const int inside = page % 18 - 3;
        const int length = static_cast<int>(mix(seed + static_cast<std::uint32_t>(line) * 7919U) % 140U);
        for (int x = 0; x < image.width(); ++x) {
            const int col = x / 9;
            const int gx = x % 9;
            row[x] = 0xFF1E1E1E;
            if (inside < 0 || inside >= 12 || gx >= 7 || col >= length || col < 2) continue;
            const std::uint32_t glyph = mix(seed ^ (static_cast<std::uint32_t>(line) * 131U + static_cast<std::uint32_t>(col) * 977U));
            if ((glyph % 7U) == 0) continue; // a space
            if ((glyph >> (static_cast<std::uint32_t>(inside * 7 + gx) % 29U)) & 1U)
                row[x] = INK[(glyph >> 8) % 4];
        }
    }
}

// Picture like content (a photo, a video): smooth waves + grain, moving with t
_cold static void draw_picture(QImage& image, const QRect& area, const int t)
{
    QVector<int> wx(area.width());
    QVector<int> wy(area.height());

    for (int x = 0; x < area.width(); ++x)
        wx[x] = static_cast<int>(60.0 * std::sin((x + t * 7) * 0.011) + 30.0 * std::sin((x - t * 3) * 0.037));
    for (int y = 0; y < area.height(); ++y)
        wy[y] = static_cast<int>(60.0 * std::cos((y - t * 5) * 0.013) + 25.0 * std::sin((y + t * 2) * 0.041));
    xdesk::platform::parallel_for(area.height(), [&](const int y) {
        if (area.top() + y < 0 || area.top() + y >= image.height()) return;
        std::uint32_t* row = reinterpret_cast<std::uint32_t*>(image.scanLine(area.top() + y));
        for (int x = 0; x < area.width(); ++x) {
            const int px = area.left() + x;
            if (px < 0 || px >= image.width()) continue;
            const int grain = static_cast<int>(mix(static_cast<std::uint32_t>((x + t * 13) * 2654435761U) ^ static_cast<std::uint32_t>(y * 40503 + t)) % 13U) - 6;
            const int v = wx[x] + wy[y];
            const int r = std::clamp(128 + v + grain, 0, 255);
            const int g = std::clamp(110 + v / 2 - wy[y] / 3 + grain, 0, 255);
            const int b = std::clamp(140 - v / 2 + wx[x] / 3 + grain, 0, 255);
            row[px] = 0xFF000000U | (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
        }
    });
}

// Frame i of a scene: typing (a few letters), scroll (the page moves 24 lines of pixels), drag (a window moves), video (full screen)
_cold static QImage scene_frame(const QString& scene, const int width, const int height, const int i)
{
    QImage image(width, height, QImage::Format_RGB32);

    if (scene == "video") {
        draw_picture(image, image.rect(), i);
        return image;
    }
    draw_text(image, 0, height, 42U, scene == "scroll" ? i * 24 : 0);
    if (scene == "typing") {
        // The letters typed so far on the last line of the page
        const int y = (height / 18 - 2) * 18 + 3;
        for (int k = 0; k <= i; ++k)
            for (int dy = 0; dy < 12; ++dy)
                for (int dx = 0; dx < 7; ++dx)
                    if ((mix(static_cast<std::uint32_t>(k) * 31U) >> ((dy * 7 + dx) % 29)) & 1U) image.setPixel(18 + (k % 120) * 9 + dx, y + dy, 0xFFFFFFFF);
    } else if (scene == "drag") {
        const int w = width / 2;
        const int h = height / 2;
        draw_picture(image, QRect(width / 8 + i * 12, height / 6 + i * 4, w, h), 0); // the same picture, moved
    }
    return image;
}

// Quality of what the viewer shows (RGB, every 4th pixel)
_cold static double psnr(const QImage& source, const QImage& shown)
{
    double error = 0.0;
    qint64 count = 0;

    if (shown.isNull() || shown.size() != source.size()) return 0.0;
    for (int y = 0; y < source.height(); y += 4) {
        const std::uint32_t* a = reinterpret_cast<const std::uint32_t*>(source.constScanLine(y));
        const std::uint32_t* b = reinterpret_cast<const std::uint32_t*>(shown.constScanLine(y));
        for (int x = 0; x < source.width(); x += 4) {
            for (int shift = 0; shift <= 16; shift += 8) {
                const double d = static_cast<double>(static_cast<int>((a[x] >> shift) & 0xFF) - static_cast<int>((b[x] >> shift) & 0xFF));
                error += d * d;
            }
            count += 3;
        }
    }
    if (count == 0) return 0.0;
    error /= static_cast<double>(count);
    return error <= 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / error);
}

_cold static BenchResult summarize(const QString& scene, const QString& mode, QVector<double>& times, const double bytes, const QVector<double>& quality)
{
    BenchResult r;

    r.scene = scene;
    r.mode = mode;
    if (times.size() > 1) times.removeFirst(); // the first frame opens the encoder (and is complete): measured apart by nobody, a cost paid once
    std::sort(times.begin(), times.end());
    for (const double t: times)
        r.encodeMs += t;
    r.encodeMs /= std::max<qsizetype>(1, times.size());
    r.p95Ms = times.isEmpty() ? 0.0 : times[std::min<qsizetype>(times.size() - 1, times.size() * 95 / 100)];
    r.bytes = bytes / std::max<qsizetype>(1, times.size());
    for (const double q: quality)
        r.psnr += q;
    r.psnr = quality.isEmpty() ? 0.0 : r.psnr / static_cast<double>(quality.size());
    return r;
}

// Tiles (what a LAN viewer gets), or tiles + palette + copy (a far viewer without video)
_cold static BenchResult bench_tiles(const QString& scene, const int width, const int height, const int frames, const bool far)
{
    xdesk::platform::FrameEncoder encoder;
    QVector<double> times;
    QVector<double> quality;
    double bytes = 0.0;
    QElapsedTimer timer;

    encoder.setPalette(far);
    encoder.setCopy(far);
    encoder.setVideo(false);
    for (int i = 0; i < frames; ++i) {
        const QImage frame = scene_frame(scene, width, height, i);
        timer.start();
        const QByteArray message = encoder.encode(frame, BENCH_QUALITY);
        times << static_cast<double>(timer.nsecsElapsed()) / 1e6;
        bytes += static_cast<double>(message.size());
        // The tiles of a frame are jpeg of this quality (palette ones: lossless), close to one jpeg of the whole frame
        if (i % BENCH_PSNR_EVERY == 0) quality << psnr(frame, xdesk::platform::FrameEncoder::decodeJpeg(xdesk::platform::FrameEncoder::encodeJpeg(frame, BENCH_QUALITY)));
    }
    return summarize(scene, far ? "tiles+palette+copy" : "tiles", times, bytes, quality);
}

// One video encoder on every frame, decoded back for its quality
_cold static BenchResult bench_video(const QString& scene, const int width, const int height, const int frames, std::shared_ptr<xdesk::platform::IVideoEncoder> encoder)
{
    xdesk::platform::AvDecoder decoder;
    xdesk::platform::H264Decoder fallback;
    QVector<double> times;
    QVector<double> quality;
    double bytes = 0.0;
    QElapsedTimer timer;
    const bool avDecode = xdesk::platform::av_decodes(encoder->codec());

    encoder->forceKey();
    for (int i = 0; i < frames; ++i) {
        const QImage frame = scene_frame(scene, width, height, i);
        bool key = false;
        timer.start();
        const QByteArray unit = encoder->encode(frame, BENCH_QUALITY, &key);
        times << static_cast<double>(timer.nsecsElapsed()) / 1e6;
        if (unit.isEmpty()) return BenchResult{scene, encoder->name() + " (failed)", 0.0, 0.0, 0.0, 0.0};
        bytes += static_cast<double>(unit.size());
        const QImage shown = avDecode ? decoder.decode(unit, encoder->codec(), width, height) : fallback.decode(unit, width, height);
        if (i % BENCH_PSNR_EVERY == 0) quality << psnr(frame, shown);
    }
    return summarize(scene, encoder->name(), times, bytes, quality);
}

// Log like text (a build, a "cat" of a file): what the terminals send
_cold static QByteArray terminal_text(const int size)
{
    static const char* WORDS[] = {"error", "warning", "build", "src/xdesk/host/Stream.cpp", "[100%]", "Linking", "CXX", "object", "=", "0x7f3a", "-->", "ok", "test", "passed", "\x1b[32m", "\x1b[0m", "frame", "tile", "session"}; // xstyle: ignore LU-ANSI (colours of a real build log: data, not output)
    QByteArray out;
    std::uint32_t k = 1;

    while (out.size() < size) {
        const int words = 4 + static_cast<int>(mix(k++) % 9U);
        for (int w = 0; w < words; ++w) {
            out += WORDS[mix(k++) % (sizeof(WORDS) / sizeof(WORDS[0]))];
            out += mix(k) % 5U == 0 ? QByteArray::number(mix(k++) % 100000U) : QByteArray(" ");
        }
        out += "\r\n";
    }
    return out.left(size);
}

_cold static void print_compression(void)
{
    xdesk::platform::FrameEncoder encoder;
    QByteArray tiles;

    encoder.setPalette(true);
    for (int i = 0; i < 30; ++i)
        tiles += encoder.encode(scene_frame("typing", 1920, 1080, i), BENCH_QUALITY);
    std::printf("\nCompression of what is not jpeg / video (zlib in every client; lz4 / zstd measured for comparison)\n");
    std::printf("  %-22s %-6s %8s %8s %10s %10s %14s\n", "payload", "codec", "bytes", "ratio", "comp MB/s", "dec MB/s", "pays below");
    for (const std::pair<const char*, QByteArray>& payload: {std::pair<const char*, QByteArray>{"terminal output", terminal_text(256 * 1024)}, {"tiles (typing, 1080p)", tiles}}) {
        for (const QString& codec: xdesk::host::compress_codecs()) {
            QElapsedTimer timer;
            QByteArray packed;
            QByteArray back;
            double compressMs = 0.0;
            double decompressMs = 0.0;
            for (int round = 0; round < 5; ++round) {
                timer.start();
                packed = xdesk::host::compress_with(codec, payload.second);
                compressMs += static_cast<double>(timer.nsecsElapsed()) / 1e6;
                timer.start();
                back = xdesk::host::decompress_with(codec, packed, static_cast<int>(payload.second.size()));
                decompressMs += static_cast<double>(timer.nsecsElapsed()) / 1e6;
            }
            compressMs /= 5.0;
            decompressMs /= 5.0;
            if (packed.isEmpty() || back != payload.second) {std::printf("  %-22s %-6s %8s\n", payload.first, qPrintable(codec), "failed"); continue;}
            const double mb = static_cast<double>(payload.second.size()) / 1e6;
            const double saved = static_cast<double>(payload.second.size() - packed.size()) * 8.0 / 1e6; // Mbit saved
            const double breakEven = saved / ((compressMs + decompressMs) / 1000.0); // Mbit/s under which the time saved on the link is larger
            std::printf("  %-22s %-6s %8lld %7.2fx %10.0f %10.0f %10.0f Mb/s\n", payload.first, qPrintable(codec), static_cast<long long>(payload.second.size()),
                static_cast<double>(payload.second.size()) / std::max<qsizetype>(1, packed.size()), mb / (compressMs / 1000.0), mb / (decompressMs / 1000.0), breakEven);
        }
    }
}

//----------------------------------------------------------------//
/* FUNCTION */

QList<xdesk::host::BenchNetwork> xdesk::host::bench_networks(void)
{
    return {
        {"LAN (1 Gb/s, 1 ms)", 1000.0, 1.0, 0.0},
        {"Wi-Fi (150 Mb/s, 6 ms, 0.5 %)", 150.0, 6.0, 0.005},
        {"Fiber WAN (100 Mb/s, 25 ms)", 100.0, 25.0, 0.001},
        {"4G (25 Mb/s, 60 ms, 1 %)", 25.0, 60.0, 0.01},
        {"Degraded (6 Mb/s, 120 ms, 3 %)", 6.0, 120.0, 0.03},
        {"Very bad (2 Mb/s, 250 ms, 5 %)", 2.0, 250.0, 0.05},
    };
}

double xdesk::host::bench_fps(const BenchResult& result, const BenchNetwork& network, const int cap)
{
    const double goodput = network.mbits * 1e6 / 8.0 * (1.0 - network.loss) * (network.loss > 0.0 ? BENCH_PARITY : 1.0); // bytes per second
    const double cpu = result.encodeMs > 0.0 ? 1000.0 / result.encodeMs : cap;
    const double link = result.bytes > 0.0 ? goodput / result.bytes : cap;

    return std::max(0.0, std::min({static_cast<double>(cap), cpu, link}));
}

double xdesk::host::bench_latency(const BenchResult& result, const BenchNetwork& network)
{
    const double goodput = network.mbits * 1e6 / 8.0 * (1.0 - network.loss) * (network.loss > 0.0 ? BENCH_PARITY : 1.0);

    return result.encodeMs + result.bytes / goodput * 1000.0 + network.rtt / 2.0 + BENCH_DECODE_MS;
}

int xdesk::host::run_bench(int& argc, char** argv, const QString& heights, const int frames)
{
    QCoreApplication app(argc, argv);
    QList<int> sizes;

    (void)std::setlocale(LC_NUMERIC, "C"); // 1.57, not 1,57
    const QStringList scenes = {"typing", "scroll", "drag", "video"};

    for (const QString& h: (heights.isEmpty() || heights == "all" ? QString("1080,2160") : heights).split(',', Qt::SkipEmptyParts))
        if (h.toInt() >= 144) sizes << h.toInt();
    if (sizes.isEmpty()) sizes = {1080, 2160};
    std::printf("xdesk bench: synthetic scenes, quality %d, %d frames each, every core of this computer\n", BENCH_QUALITY, frames);
    std::printf("  openh264: %s\n  libavcodec: %s\n", qPrintable(platform::h264_library()), qPrintable(platform::av_library()));
    std::printf("  video encoders: %s\n", qPrintable(platform::video_encoders()));
    for (const int height: sizes) {
        const int width = height * 16 / 9;
        QList<BenchResult> results;

        std::printf("\n=== %dx%d ===\n", width, height);
        {
            // Color conversion of the video encoders (cpu, every core): a part of their time
            const QImage frame = scene_frame("video", width, height, 0);
            QByteArray planes(static_cast<qsizetype>(width) * height * 3 / 2, Qt::Uninitialized);
            unsigned char* y = reinterpret_cast<unsigned char*>(planes.data());
            QElapsedTimer timer;
            timer.start();
            for (int i = 0; i < 10; ++i)
                platform::bgrx_to_yuv420(frame, width, height, y, width, nullptr, 0, y + static_cast<std::size_t>(width) * height, width);
            std::printf("  bgrx -> nv12 (cpu): %.2f ms per frame\n", static_cast<double>(timer.nsecsElapsed()) / 1e7);
        }
        std::printf("  %-8s %-20s %9s %9s %10s %8s\n", "scene", "mode", "enc ms", "p95 ms", "KB/frame", "PSNR");
        for (const QString& scene: scenes) {
            QList<std::shared_ptr<platform::IVideoEncoder>> encoders;
            if (platform::h264_available() && static_cast<qint64>(width) * height <= 1920LL * 1200LL) encoders << std::make_shared<platform::H264Encoder>();
            for (const platform::VideoCodec codec: {platform::VideoCodec::H264, platform::VideoCodec::Hevc, platform::VideoCodec::Av1})
                for (const QString& name: platform::av_encoders(codec))
                    encoders << std::make_shared<platform::AvEncoder>(name, codec);
            results << bench_tiles(scene, width, height, frames, false) << bench_tiles(scene, width, height, frames, true);
            for (const std::shared_ptr<platform::IVideoEncoder>& encoder: encoders)
                results << bench_video(scene, width, height, frames, encoder);
            for (const BenchResult& r: results)
                if (r.scene == scene)
                    std::printf("  %-8s %-20s %9.2f %9.2f %10.1f %8.1f\n", qPrintable(r.scene), qPrintable(r.mode), r.encodeMs, r.p95Ms, r.bytes / 1024.0, r.psnr);
            std::fflush(stdout);
        }

        // The best way of each scene on each network: the most frames per second at a good quality, then the lowest lag
        std::printf("\n  Best way per network (fps capped at 60, PSNR >= %.0f dB):\n", BENCH_GOOD_PSNR);
        std::printf("  %-32s %-8s %-20s %6s %9s %8s\n", "network", "scene", "best", "fps", "lag ms", "PSNR");
        for (const BenchNetwork& network: bench_networks()) {
            for (const QString& scene: scenes) {
                const BenchResult* best = nullptr;
                for (const BenchResult& r: results) {
                    if (r.scene != scene || r.encodeMs <= 0.0 || r.psnr < BENCH_GOOD_PSNR) continue;
                    if (!best) {best = &r; continue;}
                    const double a = bench_fps(r, network);
                    const double b = bench_fps(*best, network);
                    if (a > b * 1.05 || (a > b * 0.95 && bench_latency(r, network) < bench_latency(*best, network))) best = &r;
                }
                if (best) std::printf("  %-32s %-8s %-20s %6.1f %9.1f %8.1f\n", qPrintable(network.name), qPrintable(scene), qPrintable(best->mode), bench_fps(*best, network), bench_latency(*best, network), best->psnr);
            }
        }
        std::fflush(stdout);
    }
    print_compression();
    return 0;
}
