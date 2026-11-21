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
##  @file AvCodec.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/FrameEncoder.hpp"
#include "xdesk/platform/AvCodec.hpp"
#include "xdesk/platform/H264.hpp"
#include <QCoreApplication>
#include <algorithm>
#include <QElapsedTimer>
#include <QLibrary>
#include <cstring>
#include <QMutex>
#include <cerrno>
#include <QHash>
#include <QDir>
extern "C" {
    #include <libavutil/hwcontext.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/frame.h>
    #include <libavutil/opt.h>
}

//----------------------------------------------------------------//
/* DEFINE */

#define AV_CODEC_MAJOR 62 // the headers of third_party/ffmpeg (FFmpeg 8): the same majors at run time
#define AV_UTIL_MAJOR 60
#define AV_QP_STEP 6 // quantizer change that reopens the encoder (an IDR: not at every move of the quality)
#define AV_FPS 60 // frame rate the encoder is told (the flow control paces the real one)
#define AV_PROBE_WIDTH 640 // size of the probe frame
#define AV_PROBE_HEIGHT 360

//----------------------------------------------------------------//
/* STRUCT */

// Entry points of libavcodec / libavutil (loaded once, never unloaded)
struct AvLib {
    unsigned (*codecVersion)(void) = nullptr;
    unsigned (*utilVersion)(void) = nullptr;
    const AVCodec* (*findEncoder)(const char*) = nullptr;
    const AVCodec* (*findDecoder)(const char*) = nullptr;
    AVCodecContext* (*allocContext)(const AVCodec*) = nullptr;
    void (*freeContext)(AVCodecContext**) = nullptr;
    int (*open)(AVCodecContext*, const AVCodec*, AVDictionary**) = nullptr;
    int (*sendFrame)(AVCodecContext*, const AVFrame*) = nullptr;
    int (*receivePacket)(AVCodecContext*, AVPacket*) = nullptr;
    int (*sendPacket)(AVCodecContext*, const AVPacket*) = nullptr;
    int (*receiveFrame)(AVCodecContext*, AVFrame*) = nullptr;
    AVPacket* (*packetAlloc)(void) = nullptr;
    void (*packetFree)(AVPacket**) = nullptr;
    void (*packetUnref)(AVPacket*) = nullptr;
    AVFrame* (*frameAlloc)(void) = nullptr;
    void (*frameFree)(AVFrame**) = nullptr;
    int (*frameBuffer)(AVFrame*, int) = nullptr;
    int (*frameWritable)(AVFrame*) = nullptr;
    int (*optSet)(void*, const char*, const char*, int) = nullptr;
    int (*optSetInt)(void*, const char*, int64_t, int) = nullptr;
    int (*deviceCreate)(AVBufferRef**, enum AVHWDeviceType, const char*, AVDictionary*, int) = nullptr;
    AVBufferRef* (*framesAlloc)(AVBufferRef*) = nullptr;
    int (*framesInit)(AVBufferRef*) = nullptr;
    int (*hwBuffer)(AVBufferRef*, AVFrame*, int) = nullptr;
    int (*transfer)(AVFrame*, const AVFrame*, int) = nullptr;
    AVBufferRef* (*bufferRef)(const AVBufferRef*) = nullptr;
    void (*bufferUnref)(AVBufferRef**) = nullptr;
    void (*logLevel)(int) = nullptr;
    QString path = "libavcodec not found"; // the libraries, or why they are missing
};

//----------------------------------------------------------------//
/* TOOLS */

template<typename T>
_cold static void resolve(QLibrary& library, T& fn, const char* name)
{
    fn = reinterpret_cast<T>(library.resolve(name));
}

// XDESK_AVCODEC_DIR = a folder, else the system libraries (Linux: libavcodec.so.62, Windows: avcodec-62.dll next to xdesk.exe)
_cold static AvLib load_av(void)
{
    AvLib lib;
    const QString folder = qEnvironmentVariable("XDESK_AVCODEC_DIR");
    #ifdef _WIN32
        const QString base = folder.isEmpty() ? QCoreApplication::applicationDirPath() : folder;
        QLibrary util(QDir(base).filePath("avutil-" + QString::number(AV_UTIL_MAJOR)));
        QLibrary codec(QDir(base).filePath("avcodec-" + QString::number(AV_CODEC_MAJOR)));
    #else
        QLibrary util(folder.isEmpty() ? QString("avutil") : QDir(folder).filePath("libavutil"), AV_UTIL_MAJOR);
        QLibrary codec(folder.isEmpty() ? QString("avcodec") : QDir(folder).filePath("libavcodec"), AV_CODEC_MAJOR);
    #endif

    if (qEnvironmentVariableIsEmpty("LIBVA_MESSAGING_LEVEL")) qputenv("LIBVA_MESSAGING_LEVEL", "0"); // the va driver says hello at every open otherwise
    if (!util.load() || !codec.load()) {
        lib.path = "FFmpeg 8 libraries not loaded (libavcodec " + QString::number(AV_CODEC_MAJOR) + ", libavutil " + QString::number(AV_UTIL_MAJOR) + "): " + (util.isLoaded() ? codec.errorString() : util.errorString());
        return lib;
    }
    resolve(codec, lib.codecVersion, "avcodec_version");
    resolve(util, lib.utilVersion, "avutil_version");
    if (!lib.codecVersion || !lib.utilVersion || lib.codecVersion() >> 16 != AV_CODEC_MAJOR || lib.utilVersion() >> 16 != AV_UTIL_MAJOR) {
        lib = AvLib();
        lib.path = "libavcodec / libavutil of another FFmpeg major (" + QString::number(AV_CODEC_MAJOR) + " / " + QString::number(AV_UTIL_MAJOR) + " needed)";
        return lib;
    }
    resolve(codec, lib.findEncoder, "avcodec_find_encoder_by_name");
    resolve(codec, lib.findDecoder, "avcodec_find_decoder_by_name");
    resolve(codec, lib.allocContext, "avcodec_alloc_context3");
    resolve(codec, lib.freeContext, "avcodec_free_context");
    resolve(codec, lib.open, "avcodec_open2");
    resolve(codec, lib.sendFrame, "avcodec_send_frame");
    resolve(codec, lib.receivePacket, "avcodec_receive_packet");
    resolve(codec, lib.sendPacket, "avcodec_send_packet");
    resolve(codec, lib.receiveFrame, "avcodec_receive_frame");
    resolve(codec, lib.packetAlloc, "av_packet_alloc");
    resolve(codec, lib.packetFree, "av_packet_free");
    resolve(codec, lib.packetUnref, "av_packet_unref");
    resolve(util, lib.frameAlloc, "av_frame_alloc");
    resolve(util, lib.frameFree, "av_frame_free");
    resolve(util, lib.frameBuffer, "av_frame_get_buffer");
    resolve(util, lib.frameWritable, "av_frame_make_writable");
    resolve(util, lib.optSet, "av_opt_set");
    resolve(util, lib.optSetInt, "av_opt_set_int");
    resolve(util, lib.deviceCreate, "av_hwdevice_ctx_create");
    resolve(util, lib.framesAlloc, "av_hwframe_ctx_alloc");
    resolve(util, lib.framesInit, "av_hwframe_ctx_init");
    resolve(util, lib.hwBuffer, "av_hwframe_get_buffer");
    resolve(util, lib.transfer, "av_hwframe_transfer_data");
    resolve(util, lib.bufferRef, "av_buffer_ref");
    resolve(util, lib.bufferUnref, "av_buffer_unref");
    resolve(util, lib.logLevel, "av_log_set_level");
    if (!lib.findEncoder || !lib.findDecoder || !lib.allocContext || !lib.freeContext || !lib.open || !lib.sendFrame || !lib.receivePacket
        || !lib.sendPacket || !lib.receiveFrame || !lib.packetAlloc || !lib.packetFree || !lib.packetUnref || !lib.frameAlloc || !lib.frameFree
        || !lib.frameBuffer || !lib.frameWritable || !lib.optSet || !lib.optSetInt || !lib.deviceCreate || !lib.framesAlloc || !lib.framesInit
        || !lib.hwBuffer || !lib.transfer || !lib.bufferRef || !lib.bufferUnref || !lib.logLevel) {
        lib = AvLib();
        lib.path = "libavcodec incomplete: " + codec.fileName();
        return lib;
    }
    lib.logLevel(AV_LOG_QUIET); // the encoders that fail to open on this gpu are expected (probe)
    lib.path = QString("%1 (%2.%3), %4").arg(codec.fileName()).arg(lib.codecVersion() >> 16).arg((lib.codecVersion() >> 8) & 0xFF).arg(util.fileName());
    return lib;
}

_cold static const AvLib& av(void)
{
    static const AvLib lib = load_av();

    return lib;
}

// Monotonic ms (pace of the bitrate changes)
_cold static long long monotonic_ms(void)
{
    static QElapsedTimer clock;

    if (!clock.isValid()) clock.start();
    return clock.elapsed();
}

// Rate control of a link (CBR, about two frames of buffer: the frames never wait behind each other)
_cold static void set_rate(AVCodecContext* ctx, const int bits)
{
    ctx->bit_rate = bits;
    ctx->rc_max_rate = bits;
    ctx->rc_buffer_size = std::max(bits / 15, 64000);
    ctx->rc_initial_buffer_occupancy = ctx->rc_buffer_size;
}

// Encoder family of a libavcodec name
_cold static QString backend_of(const QString& name)
{
    return name.section('_', -1);
}

// Quantizer on the scale of the codec (h264 / hevc 0 - 51, av1 0 - 255) from a jpeg like quality
_cold static int qp_of(const xdesk::platform::VideoCodec codec, const int quality)
{
    const int qp = std::clamp(50 - quality * 3 / 10, 18, 42); // the scale of openh264: 70 -> 29, 90 -> 23, 30 -> 41

    return codec == xdesk::platform::VideoCodec::Av1 ? qp * 5 : qp;
}

//----------------------------------------------------------------//
/* FUNCTION */

bool xdesk::platform::av_available(void)
{
    return av().findEncoder != nullptr;
}

QString xdesk::platform::av_library(void)
{
    return av().path;
}

QStringList xdesk::platform::av_candidates(const VideoCodec codec)
{
    const QString prefix = video_codec_name(codec);

    return {prefix + "_nvenc", prefix + "_qsv", prefix + "_vaapi"};
}

QStringList xdesk::platform::av_encoders(const VideoCodec codec)
{
    static QMutex lock;
    static QHash<int, QStringList> probed;
    const QMutexLocker guard(&lock);
    QImage frame(AV_PROBE_WIDTH, AV_PROBE_HEIGHT, QImage::Format_RGB32);
    QStringList found;

    if (probed.contains(static_cast<int>(codec))) return probed.value(static_cast<int>(codec));
    if (av_available()) {
        frame.fill(QColor(40, 90, 160));
        for (const QString& name: av_candidates(codec)) {
            AvEncoder encoder(name, codec);
            bool key = false;
            if (!av().findEncoder(name.toUtf8().constData())) continue;
            if (!encoder.encode(frame, 70, &key).isEmpty() && key) found << name; // opens on this gpu, one key frame out at once
        }
    }
    probed.insert(static_cast<int>(codec), found);
    return found;
}

bool xdesk::platform::av_decodes(const VideoCodec codec)
{
    if (!av_available()) return false;
    if (codec == VideoCodec::Av1) return av().findDecoder("libdav1d") || av().findDecoder("av1");
    return av().findDecoder(video_codec_name(codec).toUtf8().constData()) != nullptr;
}

//----------------------------------------------------------------//
/* CLASS */

bool xdesk::platform::AvEncoder::open_(const int width, const int height, const int qp)
{
    const AvLib& lib = av();
    const AVCodec* codec = lib.findEncoder ? lib.findEncoder(this->_name.toUtf8().constData()) : nullptr;
    const QString backend = backend_of(this->_name);
    const bool av1 = this->_codec == VideoCodec::Av1;

    if (!codec) return false;
    this->_ctx = lib.allocContext(codec);
    this->_packet = lib.packetAlloc();
    this->_frame = lib.frameAlloc();
    if (!this->_ctx || !this->_packet || !this->_frame) {this->close_(); return false;}
    this->_ctx->width = width;
    this->_ctx->height = height;
    this->_ctx->time_base = AVRational{1, AV_FPS};
    this->_ctx->framerate = AVRational{AV_FPS, 1};
    this->_ctx->gop_size = 1 << 20; // an IDR only when asked
    this->_ctx->max_b_frames = 0;
    this->_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    this->_ctx->colorspace = AVCOL_SPC_BT709;
    this->_ctx->color_primaries = AVCOL_PRI_BT709;
    this->_ctx->color_trc = AVCOL_TRC_BT709;
    this->_ctx->color_range = AVCOL_RANGE_MPEG;
    this->_frame->width = width;
    this->_frame->height = height;
    if (backend == "nvenc") {
        this->_ctx->pix_fmt = AV_PIX_FMT_BGR0; // converted by the gpu (BT.709 of the VUI)
        (void)lib.optSet(this->_ctx->priv_data, "preset", "p1", 0);
        (void)lib.optSet(this->_ctx->priv_data, "tune", "ull", 0);
        if (this->_bitrate > 0) {
            (void)lib.optSet(this->_ctx->priv_data, "rc", "cbr", 0);
            set_rate(this->_ctx, this->_bitrate);
        } else {
            (void)lib.optSet(this->_ctx->priv_data, "rc", "constqp", 0);
            (void)lib.optSetInt(this->_ctx->priv_data, "qp", qp, 0);
        }
        (void)lib.optSetInt(this->_ctx->priv_data, "zerolatency", 1, 0);
        (void)lib.optSetInt(this->_ctx->priv_data, "delay", 0, 0);
        (void)lib.optSetInt(this->_ctx->priv_data, "forced-idr", 1, 0);
    } else if (backend == "qsv") {
        this->_ctx->pix_fmt = AV_PIX_FMT_NV12;
        if (this->_bitrate > 0) {
            set_rate(this->_ctx, this->_bitrate); // bit_rate == rc_max_rate: CBR
            (void)lib.optSetInt(this->_ctx->priv_data, "low_delay_brc", 1, 0);
        } else {
            this->_ctx->flags |= AV_CODEC_FLAG_QSCALE;
            this->_ctx->global_quality = qp * FF_QP2LAMBDA;
        }
        (void)lib.optSet(this->_ctx->priv_data, "preset", "veryfast", 0);
        (void)lib.optSetInt(this->_ctx->priv_data, "async_depth", 1, 0);
        (void)lib.optSetInt(this->_ctx->priv_data, "forced_idr", 1, 0);
        (void)lib.optSetInt(this->_ctx->priv_data, "look_ahead", 0, 0);
    } else if (backend == "vaapi") {
        AVBufferRef* frames = nullptr;
        if (lib.deviceCreate(&this->_device, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0) < 0) {this->close_(); return false;}
        frames = lib.framesAlloc(this->_device);
        if (!frames) {this->close_(); return false;}
        AVHWFramesContext* pool = reinterpret_cast<AVHWFramesContext*>(frames->data);
        pool->format = AV_PIX_FMT_VAAPI;
        pool->sw_format = AV_PIX_FMT_NV12;
        pool->width = width;
        pool->height = height;
        pool->initial_pool_size = 4;
        if (lib.framesInit(frames) < 0) {lib.bufferUnref(&frames); this->close_(); return false;}
        this->_ctx->hw_frames_ctx = frames;
        this->_ctx->pix_fmt = AV_PIX_FMT_VAAPI;
        if (this->_bitrate > 0) {
            set_rate(this->_ctx, this->_bitrate);
            (void)lib.optSet(this->_ctx->priv_data, "rc_mode", "CBR", 0);
        } else {
            this->_ctx->global_quality = qp;
            (void)lib.optSet(this->_ctx->priv_data, "rc_mode", "CQP", 0);
        }
        (void)lib.optSetInt(this->_ctx->priv_data, "async_depth", 1, 0);
        this->_gpu = lib.frameAlloc();
        if (!this->_gpu) {this->close_(); return false;}
    } else {
        this->close_();
        return false;
    }
    if (!av1 && this->_codec == VideoCodec::Hevc) (void)lib.optSet(this->_ctx->priv_data, "profile", "main", 0);
    // H.264: constrained baseline like openh264 (every decoder, openh264 of the native viewers included)
    if (this->_codec == VideoCodec::H264) (void)lib.optSet(this->_ctx->priv_data, "profile", backend == "vaapi" ? "constrained_baseline" : "baseline", 0);
    this->_frame->format = backend == "nvenc" ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_NV12;
    if (lib.open(this->_ctx, codec, nullptr) < 0 || lib.frameBuffer(this->_frame, 0) < 0) {this->close_(); return false;}
    this->_width = width;
    this->_height = height;
    this->_qp = qp;
    this->_rate = this->_bitrate;
    this->_rateAt = monotonic_ms();
    this->_key = true;
    return true;
}

void xdesk::platform::AvEncoder::close_(void) noexcept
{
    const AvLib& lib = av();

    if (this->_ctx) lib.freeContext(&this->_ctx);
    if (this->_frame) lib.frameFree(&this->_frame);
    if (this->_gpu) lib.frameFree(&this->_gpu);
    if (this->_packet) lib.packetFree(&this->_packet);
    if (this->_device) lib.bufferUnref(&this->_device);
    this->_width = 0;
    this->_height = 0;
}

QByteArray xdesk::platform::AvEncoder::encode(const QImage& frame, const int quality, bool* key)
{
    const AvLib& lib = av();
    QImage image = frame;
    const int width = (frame.width() + 1) & ~1;
    const int height = (frame.height() + 1) & ~1;
    const int qp = qp_of(this->_codec, quality);
    const int step = this->_codec == VideoCodec::Av1 ? AV_QP_STEP * 5 : AV_QP_STEP;
    AVFrame* input = nullptr;
    QByteArray unit;
    int result = 0;

    *key = false;
    if (this->_broken || image.isNull() || width < 16 || height < 16) return {};
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32)
        image = image.convertToFormat(QImage::Format_RGB32);
    // Reopened (a key frame) for another size, another mode (bitrate / quantizer, at most every VIDEO_RATE_EVERY), a far
    // quantizer, or a new bitrate on vaapi; nvenc / qsv take a new bitrate live (FFmpeg reconfigures them)
    const bool paced = monotonic_ms() - this->_rateAt >= VIDEO_RATE_EVERY;
    const bool newRate = this->_rate > 0 && this->_bitrate > 0 && std::abs(this->_bitrate - this->_rate) > this->_rate * VIDEO_RATE_STEP && paced;
    const bool vaapi = backend_of(this->_name) == "vaapi";
    if (!this->_ctx || width != this->_width || height != this->_height || ((this->_bitrate > 0) != (this->_rate > 0) && paced) || (this->_rate == 0 && std::abs(qp - this->_qp) >= step) || (newRate && vaapi)) {
        this->close_();
        if (!this->open_(width, height, qp)) {this->_broken = true; return {};}
    } else if (newRate) {
        set_rate(this->_ctx, this->_bitrate);
        this->_rate = this->_bitrate;
        this->_rateAt = monotonic_ms();
    }

    // Planes from the cpu (uploaded to the gpu for vaapi), then one packet at once
    if (lib.frameWritable(this->_frame) < 0) return {};
    if (this->_frame->format == AV_PIX_FMT_BGR0) {
        // Rows copied as they are (the last ones / column repeated up to the even size)
        const int rowBytes = image.width() * 4;
        parallel_for(height, [&](const int row) {
            unsigned char* out = this->_frame->data[0] + static_cast<std::size_t>(row) * this->_frame->linesize[0];
            const uchar* in = image.constScanLine(std::min(row, image.height() - 1));
            std::memcpy(out, in, static_cast<std::size_t>(rowBytes));
            if (width > image.width()) std::memcpy(out + rowBytes, in + rowBytes - 4, 4);
        });
    } else if (this->_frame->format == AV_PIX_FMT_YUV420P)
        bgrx_to_yuv420(image, width, height, this->_frame->data[0], this->_frame->linesize[0], this->_frame->data[1], this->_frame->linesize[1], this->_frame->data[2], this->_frame->linesize[2]);
    else
        bgrx_to_yuv420(image, width, height, this->_frame->data[0], this->_frame->linesize[0], nullptr, 0, this->_frame->data[1], this->_frame->linesize[1]);
    input = this->_frame;
    if (this->_gpu) {
        if (lib.hwBuffer(this->_ctx->hw_frames_ctx, this->_gpu, 0) < 0 || lib.transfer(this->_gpu, this->_frame, 0) < 0) {this->close_(); return {};}
        input = this->_gpu;
    }
    input->pts = this->_pts++;
    input->pict_type = this->_key ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;
    if (this->_key) input->flags |= AV_FRAME_FLAG_KEY;
    else input->flags &= ~AV_FRAME_FLAG_KEY;
    result = lib.sendFrame(this->_ctx, input);
    if (this->_gpu) {
        // The surface goes back to the pool once encoded (a reference is kept by the encoder meanwhile)
        AVFrame* used = this->_gpu;
        lib.frameFree(&used);
        this->_gpu = lib.frameAlloc();
    }
    if (result < 0) {this->close_(); return {};}
    while ((result = lib.receivePacket(this->_ctx, this->_packet)) >= 0) {
        unit.append(reinterpret_cast<const char*>(this->_packet->data), this->_packet->size);
        if (this->_packet->flags & AV_PKT_FLAG_KEY) *key = true;
        lib.packetUnref(this->_packet);
    }
    // No packet out for this frame: an encoder that holds frames would show the previous one (never used)
    if (unit.isEmpty() && result == AVERROR(EAGAIN)) {this->close_(); this->_broken = true; return {};}
    if (unit.isEmpty()) {this->close_(); return {};}
    if (*key) this->_key = false;
    return unit;
}

bool xdesk::platform::AvDecoder::open_(const VideoCodec codec)
{
    const AvLib& lib = av();
    const AVCodec* decoder = nullptr;

    if (!av_available()) return false;
    if (codec == VideoCodec::Av1) decoder = lib.findDecoder("libdav1d") ? lib.findDecoder("libdav1d") : lib.findDecoder("av1");
    else decoder = lib.findDecoder(video_codec_name(codec).toUtf8().constData());
    if (!decoder) return false;
    this->_ctx = lib.allocContext(decoder);
    this->_frame = lib.frameAlloc();
    this->_packet = lib.packetAlloc();
    if (!this->_ctx || !this->_frame || !this->_packet) {this->reset(); return false;}
    this->_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    this->_ctx->thread_count = 0; // automatic (frame threads would delay: low delay keeps slices only)
    this->_ctx->thread_type = FF_THREAD_SLICE;
    if (lib.open(this->_ctx, decoder, nullptr) < 0) {this->reset(); return false;}
    this->_codec = codec;
    return true;
}

void xdesk::platform::AvDecoder::reset(void) noexcept
{
    const AvLib& lib = av();

    if (this->_ctx) lib.freeContext(&this->_ctx);
    if (this->_frame) lib.frameFree(&this->_frame);
    if (this->_packet) lib.packetFree(&this->_packet);
}

QImage xdesk::platform::AvDecoder::decode(const QByteArray& data, const VideoCodec codec, const int width, const int height)
{
    const AvLib& lib = av();
    QImage image;

    if (data.isEmpty()) return {};
    if (this->_ctx && this->_codec != codec) this->reset();
    if (!this->_ctx && !this->open_(codec)) return {};
    this->_packet->data = reinterpret_cast<uint8_t*>(const_cast<char*>(data.constData()));
    this->_packet->size = static_cast<int>(data.size());
    if (lib.sendPacket(this->_ctx, this->_packet) < 0) {this->_packet->data = nullptr; this->_packet->size = 0; return {};}
    this->_packet->data = nullptr;
    this->_packet->size = 0;
    while (lib.receiveFrame(this->_ctx, this->_frame) >= 0) {
        if (this->_frame->format != AV_PIX_FMT_YUV420P && this->_frame->format != AV_PIX_FMT_YUVJ420P) continue; // 4:2:0 8 bits only (what the hosts send)
        image = QImage(std::min(width, this->_frame->width), std::min(height, this->_frame->height), QImage::Format_RGB32);
        if (image.isNull()) continue;
        const unsigned char* const planes[3] = {this->_frame->data[0], this->_frame->data[1], this->_frame->data[2]};
        yuv420_to_bgrx(planes, this->_frame->linesize[0], this->_frame->linesize[1], this->_frame->linesize[2], image);
    }
    return image;
}
