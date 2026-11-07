/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 07/11/2026 by @author Tsukini

File Name:
##  @file H264.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/FrameEncoder.hpp"
#include "xdesk/platform/H264.hpp"
#include <QCoreApplication>
#include <wels/codec_api.h>
#include <QStringList>
#include <algorithm>
#include <QLibrary>
#include <QThread>
#include <cstdint>
#include <cstring>
#include <QDir>

//----------------------------------------------------------------//
/* DEFINE */

#define H264_MAJOR 2 // the structures of third_party/openh264 (2.6): the same major, this minor at least
#define H264_MINOR 6
#define H264_SONAME 8 // libopenh264.so.8 (2.6)
#define H264_THREADS 4 // encoding threads (one slice each) at most
#define H264_QP_STEP 6 // quantizer change that reopens the encoder (an IDR: not at every move of the quality)
#define H264_FPS 60.0f // frame rate the encoder is told (the flow control paces the real one)

//----------------------------------------------------------------//
/* TYPEDEF */

using CreateEncoder = int (*)(ISVCEncoder**);
using DestroyEncoder = void (*)(ISVCEncoder*);
using CreateDecoder = long (*)(ISVCDecoder**);
using DestroyDecoder = void (*)(ISVCDecoder*);
using GetVersion = void (*)(OpenH264Version*);

//----------------------------------------------------------------//
/* STRUCT */

// Entry points of openh264 (loaded once, never unloaded)
struct OpenH264 {
    CreateEncoder createEncoder = nullptr;
    DestroyEncoder destroyEncoder = nullptr;
    CreateDecoder createDecoder = nullptr;
    DestroyDecoder destroyDecoder = nullptr;
    QString path = "openh264 not found"; // the library, or why it is missing
};

//----------------------------------------------------------------//
/* TOOLS */

// XDESK_OPENH264 = a path, else libopenh264.so.8 (Linux) / the openh264*.dll of Cisco next to xdesk.exe (Windows)
_cold static OpenH264 load_openh264(void)
{
    OpenH264 lib;
    QStringList candidates;

    if (!qEnvironmentVariableIsEmpty("XDESK_OPENH264")) candidates << qEnvironmentVariable("XDESK_OPENH264");
    #ifdef _WIN32
        const QDir folder(QCoreApplication::applicationDirPath());
        for (const QString& name: folder.entryList({"openh264*.dll"}, QDir::Files, QDir::Name | QDir::Reversed))
            candidates << folder.filePath(name);
    #else
        candidates << "libopenh264.so." + QString::number(H264_SONAME);
    #endif
    for (const QString& candidate: candidates) {
        QLibrary library(candidate);
        OpenH264Version version = {0, 0, 0, 0};

        if (!library.load()) {lib.path = "openh264 not loaded: " + library.errorString(); continue;}
        const GetVersion getVersion = reinterpret_cast<GetVersion>(library.resolve("WelsGetCodecVersionEx"));
        if (getVersion) getVersion(&version);
        if (version.uMajor != H264_MAJOR || version.uMinor < H264_MINOR) {
            lib.path = QString("openh264 %1.%2 (%3) is too old: %4.%5 or newer needed").arg(version.uMajor).arg(version.uMinor).arg(candidate).arg(H264_MAJOR).arg(H264_MINOR);
            library.unload();
            continue;
        }
        lib.createEncoder = reinterpret_cast<CreateEncoder>(library.resolve("WelsCreateSVCEncoder"));
        lib.destroyEncoder = reinterpret_cast<DestroyEncoder>(library.resolve("WelsDestroySVCEncoder"));
        lib.createDecoder = reinterpret_cast<CreateDecoder>(library.resolve("WelsCreateDecoder"));
        lib.destroyDecoder = reinterpret_cast<DestroyDecoder>(library.resolve("WelsDestroyDecoder"));
        if (lib.createEncoder && lib.destroyEncoder && lib.createDecoder && lib.destroyDecoder) {
            lib.path = QString("%1 (%2.%3.%4)").arg(library.fileName()).arg(version.uMajor).arg(version.uMinor).arg(version.uRevision);
            return lib;
        }
        lib = OpenH264();
        lib.path = "openh264 incomplete: " + candidate;
    }
    return lib;
}

_cold static const OpenH264& openh264(void)
{
    static const OpenH264 lib = load_openh264();

    return lib;
}

// BGRX -> I420 BT.709 limited range (pairs of rows in parallel), the last column / row repeated up to the even size
_hot static void bgrx_to_i420(const QImage& frame, const int width, const int height, unsigned char* planes)
{
    unsigned char* yPlane = planes;
    unsigned char* uPlane = planes + static_cast<std::size_t>(width) * height;
    unsigned char* vPlane = uPlane + static_cast<std::size_t>(width / 2) * (height / 2);
    const int lastX = frame.width() - 1;
    const int lastY = frame.height() - 1;

    xdesk::platform::parallel_for(height / 2, [&](const int pair) {
        const std::uint32_t* rows[2] = {
            reinterpret_cast<const std::uint32_t*>(frame.constScanLine(std::min(pair * 2, lastY))),
            reinterpret_cast<const std::uint32_t*>(frame.constScanLine(std::min(pair * 2 + 1, lastY)))
        };
        unsigned char* yRows[2] = {yPlane + static_cast<std::size_t>(pair * 2) * width, yPlane + static_cast<std::size_t>(pair * 2 + 1) * width};
        unsigned char* u = uPlane + static_cast<std::size_t>(pair) * (width / 2);
        unsigned char* v = vPlane + static_cast<std::size_t>(pair) * (width / 2);

        for (int i = 0; i < width / 2; ++i) {
            int red = 0;
            int green = 0;
            int blue = 0;
            for (int k = 0; k < 4; ++k) {
                const std::uint32_t pixel = rows[k / 2][std::min(i * 2 + k % 2, lastX)];
                const int r = static_cast<int>((pixel >> 16) & 0xFF);
                const int g = static_cast<int>((pixel >> 8) & 0xFF);
                const int b = static_cast<int>(pixel & 0xFF);
                yRows[k / 2][i * 2 + k % 2] = static_cast<unsigned char>(((47 * r + 157 * g + 16 * b + 128) >> 8) + 16);
                red += r;
                green += g;
                blue += b;
            }
            red = (red + 2) >> 2;
            green = (green + 2) >> 2;
            blue = (blue + 2) >> 2;
            u[i] = static_cast<unsigned char>(((-26 * red - 87 * green + 112 * blue + 128) >> 8) + 128);
            v[i] = static_cast<unsigned char>(((112 * red - 102 * green - 10 * blue + 128) >> 8) + 128);
        }
    });
}

// I420 BT.709 limited range -> RGB32 (rows in parallel)
_hot static void i420_to_bgrx(unsigned char* const planes[3], const int yStride, const int uvStride, QImage& image)
{
    const int width = image.width();
    uchar* bits = image.bits();
    const qsizetype stride = image.bytesPerLine();

    xdesk::platform::parallel_for(image.height(), [&](const int row) {
        const unsigned char* y = planes[0] + static_cast<std::size_t>(row) * yStride;
        const unsigned char* u = planes[1] + static_cast<std::size_t>(row / 2) * uvStride;
        const unsigned char* v = planes[2] + static_cast<std::size_t>(row / 2) * uvStride;
        std::uint32_t* out = reinterpret_cast<std::uint32_t*>(bits + static_cast<std::size_t>(row) * stride);

        for (int x = 0; x < width; ++x) {
            const int c = (y[x] - 16) * 298 + 128;
            const int d = u[x / 2] - 128;
            const int e = v[x / 2] - 128;
            const std::uint32_t red = static_cast<std::uint32_t>(std::clamp((c + 459 * e) >> 8, 0, 255));
            const std::uint32_t green = static_cast<std::uint32_t>(std::clamp((c - 55 * d - 136 * e) >> 8, 0, 255));
            const std::uint32_t blue = static_cast<std::uint32_t>(std::clamp((c + 541 * d) >> 8, 0, 255));
            out[x] = 0xFF000000u | (red << 16) | (green << 8) | blue;
        }
    });
}

bool xdesk::platform::h264_available(void)
{
    return openh264().createEncoder != nullptr;
}

QString xdesk::platform::h264_library(void)
{
    return openh264().path;
}

//----------------------------------------------------------------//
/* CLASS */

bool xdesk::platform::H264Encoder::open_(const int width, const int height, const int qp)
{
    const OpenH264& lib = openh264();
    const int threads = std::clamp(QThread::idealThreadCount(), 1, H264_THREADS);
    SEncParamExt param;
    int format = videoFormatI420;
    int trace = WELS_LOG_QUIET;

    if (!lib.createEncoder || lib.createEncoder(&this->_encoder) != 0 || !this->_encoder) {this->_encoder = nullptr; return false;}
    std::memset(&param, 0, sizeof(param));
    (void)this->_encoder->SetOption(ENCODER_OPTION_TRACE_LEVEL, &trace);
    (void)this->_encoder->GetDefaultParams(&param);
    param.iUsageType = SCREEN_CONTENT_REAL_TIME;
    param.iPicWidth = width;
    param.iPicHeight = height;
    param.iRCMode = RC_OFF_MODE; // constant quantizer: the flow control paces the frames
    param.iTargetBitrate = 50000000;
    param.iMaxBitrate = UNSPECIFIED_BIT_RATE;
    param.fMaxFrameRate = H264_FPS;
    param.iTemporalLayerNum = 1;
    param.iSpatialLayerNum = 1;
    param.uiIntraPeriod = 0; // an IDR only when asked
    param.iNumRefFrame = 1;
    param.eSpsPpsIdStrategy = CONSTANT_ID;
    param.iEntropyCodingModeFlag = 0; // CAVLC: constrained baseline, every decoder
    param.bEnableFrameSkip = false;
    param.iComplexityMode = LOW_COMPLEXITY;
    param.iMultipleThreadIdc = static_cast<unsigned short>(threads);
    param.bEnableDenoise = false;
    param.bEnableSceneChangeDetect = false; // no unasked IDR
    param.bEnableLongTermReference = false;
    SSpatialLayerConfig& layer = param.sSpatialLayers[0];
    layer.iVideoWidth = width;
    layer.iVideoHeight = height;
    layer.fFrameRate = H264_FPS;
    layer.iSpatialBitrate = param.iTargetBitrate;
    layer.iMaxSpatialBitrate = UNSPECIFIED_BIT_RATE;
    layer.uiProfileIdc = PRO_BASELINE;
    layer.uiLevelIdc = LEVEL_UNKNOWN;
    layer.iDLayerQp = qp;
    layer.sSliceArgument.uiSliceMode = threads > 1 ? SM_FIXEDSLCNUM_SLICE : SM_SINGLE_SLICE;
    layer.sSliceArgument.uiSliceNum = static_cast<unsigned int>(threads);
    layer.bVideoSignalTypePresent = true; // BT.709 limited range, like the conversion
    layer.uiVideoFormat = VF_UNDEF;
    layer.bFullRange = false;
    layer.bColorDescriptionPresent = true;
    layer.uiColorPrimaries = CP_BT709;
    layer.uiTransferCharacteristics = TRC_BT709;
    layer.uiColorMatrix = CM_BT709;
    if (this->_encoder->InitializeExt(&param) != cmResultSuccess) {this->close_(); return false;}
    (void)this->_encoder->SetOption(ENCODER_OPTION_DATAFORMAT, &format);
    this->_width = width;
    this->_height = height;
    this->_qp = qp;
    this->_key = true;
    return true;
}

void xdesk::platform::H264Encoder::close_(void) noexcept
{
    if (!this->_encoder) return;
    (void)this->_encoder->Uninitialize();
    openh264().destroyEncoder(this->_encoder);
    this->_encoder = nullptr;
    this->_width = 0;
    this->_height = 0;
}

QByteArray xdesk::platform::H264Encoder::encode(const QImage& frame, const int quality, bool* key)
{
    QImage image = frame;
    const int width = (frame.width() + 1) & ~1;
    const int height = (frame.height() + 1) & ~1;
    const int qp = std::clamp(50 - quality * 3 / 10, 18, 42); // jpeg quality 70 -> 29, 90 -> 23, 30 -> 41
    SSourcePicture picture;
    SFrameBSInfo info;
    QByteArray unit;

    *key = false;
    if (image.isNull() || width < 16 || height < 16) return {};
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32)
        image = image.convertToFormat(QImage::Format_RGB32);
    if (!this->_encoder || width != this->_width || height != this->_height || std::abs(qp - this->_qp) >= H264_QP_STEP) {
        this->close_();
        if (!this->open_(width, height, qp)) return {};
    }

    // Planes, then one access unit (an IDR when asked)
    this->_yuv.resize(static_cast<qsizetype>(width) * height * 3 / 2);
    bgrx_to_i420(image, width, height, reinterpret_cast<unsigned char*>(this->_yuv.data()));
    std::memset(&picture, 0, sizeof(picture));
    std::memset(&info, 0, sizeof(info));
    picture.iColorFormat = videoFormatI420;
    picture.iPicWidth = width;
    picture.iPicHeight = height;
    picture.iStride[0] = width;
    picture.iStride[1] = width / 2;
    picture.iStride[2] = width / 2;
    picture.pData[0] = reinterpret_cast<unsigned char*>(this->_yuv.data());
    picture.pData[1] = picture.pData[0] + static_cast<std::size_t>(width) * height;
    picture.pData[2] = picture.pData[1] + static_cast<std::size_t>(width / 2) * (height / 2);
    picture.uiTimeStamp = this->_stamp;
    this->_stamp += 16;
    if (this->_key) (void)this->_encoder->ForceIntraFrame(true);
    if (this->_encoder->EncodeFrame(&picture, &info) != cmResultSuccess) {this->close_(); return {};}
    if (info.eFrameType == videoFrameTypeSkip || info.eFrameType == videoFrameTypeInvalid) return {};
    for (int l = 0; l < info.iLayerNum; ++l) {
        const SLayerBSInfo& layerInfo = info.sLayerInfo[l];
        qsizetype size = 0;
        for (int n = 0; n < layerInfo.iNalCount; ++n)
            size += layerInfo.pNalLengthInByte[n];
        unit.append(reinterpret_cast<const char*>(layerInfo.pBsBuf), size);
    }
    *key = info.eFrameType == videoFrameTypeIDR;
    if (*key) this->_key = false;
    return unit;
}

bool xdesk::platform::H264Decoder::open_(void)
{
    const OpenH264& lib = openh264();
    SDecodingParam param;
    int trace = WELS_LOG_QUIET;

    if (!lib.createDecoder || lib.createDecoder(&this->_decoder) != 0 || !this->_decoder) {this->_decoder = nullptr; return false;}
    std::memset(&param, 0, sizeof(param));
    param.uiTargetDqLayer = 0xFF;
    param.eEcActiveIdc = ERROR_CON_DISABLE; // a broken frame is dropped, not guessed
    param.sVideoProperty.size = sizeof(param.sVideoProperty);
    param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    (void)this->_decoder->SetOption(DECODER_OPTION_TRACE_LEVEL, &trace);
    if (this->_decoder->Initialize(&param) != 0) {this->reset(); return false;}
    return true;
}

void xdesk::platform::H264Decoder::reset(void) noexcept
{
    if (!this->_decoder) return;
    (void)this->_decoder->Uninitialize();
    openh264().destroyDecoder(this->_decoder);
    this->_decoder = nullptr;
}

QImage xdesk::platform::H264Decoder::decode(const QByteArray& data, const int width, const int height)
{
    unsigned char* planes[3] = {nullptr, nullptr, nullptr};
    SBufferInfo info;

    if (data.isEmpty() || (!this->_decoder && !this->open_())) return {};
    std::memset(&info, 0, sizeof(info));
    if (this->_decoder->DecodeFrameNoDelay(reinterpret_cast<const unsigned char*>(data.constData()), static_cast<int>(data.size()), planes, &info) != dsErrorFree) return {};
    if (info.iBufferStatus != 1 || !planes[0] || !planes[1] || !planes[2]) return {};
    const SSysMEMBuffer& buffer = info.UsrData.sSystemBuffer;
    QImage image(std::min(width, buffer.iWidth), std::min(height, buffer.iHeight), QImage::Format_RGB32);
    if (image.isNull()) return {};
    i420_to_bgrx(planes, buffer.iStride[0], buffer.iStride[1], image);
    return image;
}
