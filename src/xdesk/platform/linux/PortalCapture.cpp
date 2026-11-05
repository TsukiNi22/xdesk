/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 05/11/2026 by @author Tsukini

File Name:
##  @file PortalCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/linux/PortalCapture.hpp"
#include "xdesk/security/Crypto.hpp"
#include "xdesk/core/Config.hpp"
#include <QDBusUnixFileDescriptor>
#include <condition_variable>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusInterface>
#include <QDBusArgument>
#include <QVariantMap>
#include <QDBusReply>
#include <QEventLoop>
#include <QFileInfo>
#include <unistd.h>
#include <QObject>
#include <cstring>
#include <QTimer>
#include <chrono>
#include <QFile>
#include <mutex>
#include <QUrl>
#include <QDir>
#ifdef XDESK_HAS_PIPEWIRE
    #include <spa/param/video/format-utils.h>
    #include <spa/param/video/type-info.h>
    #include <spa/param/buffers.h>
    #include <pipewire/pipewire.h>
    #include <spa/debug/types.h>
#endif

//----------------------------------------------------------------//
/* DEFINE */

#define PORTAL_SERVICE "org.freedesktop.portal.Desktop"
#define PORTAL_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SCREENCAST "org.freedesktop.portal.ScreenCast"
#define PORTAL_SCREENSHOT "org.freedesktop.portal.Screenshot"
#define PORTAL_REQUEST "org.freedesktop.portal.Request"
#define PORTAL_SESSION "org.freedesktop.portal.Session"
#define USER_TIMEOUT 120000 // ms to answer the "share your screen" dialog
#define FIRST_FRAME_TIMEOUT 5000 // ms
#define FRAME_WAIT 8 // ms waited in grab for a fresh frame
#define SCREENSHOT_INTERVAL 1000 // ms between two screenshots (fallback)

//----------------------------------------------------------------//
/* CLASS */

// Waits the Response signal of one portal request (lives in the capture thread)
class PortalRequest: public QObject {
    Q_OBJECT

    public:
        unsigned int response = 2; // 0 = success, 1 = cancelled, 2 = other
        QVariantMap results;
        bool done = false;
        QEventLoop loop;

    public slots:
        void onResponse(const uint code, const QVariantMap& values) {this->response = code; this->results = values; this->done = true; this->loop.quit();};
};

#ifdef XDESK_HAS_PIPEWIRE
    // PipeWire objects and the latest frame (written by the PipeWire thread, read by the capture thread)
    struct xdesk::platform::PipeWireState {
        pw_thread_loop* loop = nullptr;
        pw_context* context = nullptr;
        pw_core* core = nullptr;
        pw_stream* stream = nullptr;
        spa_hook streamListener;
        spa_video_info_raw format;
        std::mutex lock;
        std::condition_variable fresh;
        QImage latest;
        bool hasFresh = false;
        bool failed = false;
        QString error = "";
    };
#else
    struct xdesk::platform::PipeWireState {
        /* Nothing */
    };
#endif

//----------------------------------------------------------------//
/* TOOLS */

// Call a portal method returning a Request, wait its Response (false + error on failure / cancel / timeout)
_cold static bool portal_request(const QString& interface, const QString& method, QList<QVariant> args, QVariantMap options, QVariantMap& results, QString& error, const int timeout)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString token = "xdesk" + xdesk::security::random_id(6);
    QString sender = bus.baseService().mid(1);
    PortalRequest request;
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, interface, method);

    sender.replace('.', '_');
    const QString path = QString(PORTAL_PATH) + "/request/" + sender + "/" + token;
    if (!bus.connect(PORTAL_SERVICE, path, PORTAL_REQUEST, "Response", &request, SLOT(onResponse(uint,QVariantMap)))) {
        error = "cannot subscribe to the portal response";
        return false;
    }
    options.insert("handle_token", token);
    args.append(options);
    call.setArguments(args);
    const QDBusMessage reply = bus.call(call, QDBus::Block, 10000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        error = interface + "." + method + ": " + reply.errorMessage();
        (void)bus.disconnect(PORTAL_SERVICE, path, PORTAL_REQUEST, "Response", &request, SLOT(onResponse(uint,QVariantMap)));
        return false;
    }
    if (!request.done) {
        QTimer::singleShot(timeout, &request.loop, &QEventLoop::quit);
        request.loop.exec();
    }
    (void)bus.disconnect(PORTAL_SERVICE, path, PORTAL_REQUEST, "Response", &request, SLOT(onResponse(uint,QVariantMap)));
    if (!request.done) {error = method + ": no answer from the portal"; return false;}
    if (request.response == 1) {error = method + ": cancelled by the user"; return false;}
    if (request.response != 0) {error = method + ": refused by the portal"; return false;}
    results = request.results;
    return true;
}

_cold static QVariant portal_property(const QString& interface, const QString& name)
{
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, "org.freedesktop.DBus.Properties", "Get");
    call.setArguments({interface, name});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(call, QDBus::Block, 5000);

    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return {};
    return reply.arguments().first().value<QDBusVariant>().variant();
}

_cold static QString token_path(void)
{
    return QDir(xdesk::core::Config::dataDir()).filePath("portal.token");
}

#ifdef XDESK_HAS_PIPEWIRE
    static void on_state_changed(void* data, enum pw_stream_state, enum pw_stream_state state, const char* error)
    {
        xdesk::platform::PipeWireState* pw = static_cast<xdesk::platform::PipeWireState*>(data);

        if (state != PW_STREAM_STATE_ERROR && state != PW_STREAM_STATE_UNCONNECTED) return;
        std::lock_guard lock(pw->lock);
        pw->failed = true;
        pw->error = error ? QString::fromUtf8(error) : QString("PipeWire stream disconnected");
        pw->fresh.notify_all();
    }

    static void on_param_changed(void* data, uint32_t id, const struct spa_pod* param)
    {
        xdesk::platform::PipeWireState* pw = static_cast<xdesk::platform::PipeWireState*>(data);
        uint32_t mediaType = 0;
        uint32_t mediaSubtype = 0;
        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const spa_pod* params[1];

        if (!param || id != SPA_PARAM_Format) return;
        if (spa_format_parse(param, &mediaType, &mediaSubtype) < 0) return;
        if (mediaType != SPA_MEDIA_TYPE_video || mediaSubtype != SPA_MEDIA_SUBTYPE_raw) return;
        if (spa_format_video_raw_parse(param, &pw->format) < 0) return;

        // Shared memory buffers only (no dmabuf: no GPU import needed)
        params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
            SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 8),
            SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
            SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int((1 << SPA_DATA_MemPtr) | (1 << SPA_DATA_MemFd))));
        (void)pw_stream_update_params(pw->stream, params, 1);
    }

    static void on_process(void* data)
    {
        xdesk::platform::PipeWireState* pw = static_cast<xdesk::platform::PipeWireState*>(data);
        pw_buffer* buffer = nullptr;
        pw_buffer* next = nullptr;

        // Only the newest buffer matters: older ones are given back at once
        while ((next = pw_stream_dequeue_buffer(pw->stream)) != nullptr) {
            if (buffer) (void)pw_stream_queue_buffer(pw->stream, buffer);
            buffer = next;
        }
        if (!buffer) return;
        const spa_data& plane = buffer->buffer->datas[0];
        const int width = static_cast<int>(pw->format.size.width);
        const int height = static_cast<int>(pw->format.size.height);
        if (plane.data && plane.chunk && plane.chunk->size > 0 && !(plane.chunk->flags & SPA_CHUNK_FLAG_CORRUPTED) && width > 0 && height > 0) {
            const bool bgr = pw->format.format == SPA_VIDEO_FORMAT_BGRx || pw->format.format == SPA_VIDEO_FORMAT_BGRA;
            const int stride = plane.chunk->stride > 0 ? plane.chunk->stride : width * 4;
            const uint8_t* source = static_cast<const uint8_t*>(plane.data) + plane.chunk->offset;
            QImage image(width, height, bgr ? QImage::Format_RGB32 : QImage::Format_RGBX8888);
            const std::size_t line = static_cast<std::size_t>(width) * 4;

            if (static_cast<std::size_t>(stride) * static_cast<std::size_t>(height - 1) + line <= plane.maxsize) {
                for (int y = 0; y < height; ++y)
                    std::memcpy(image.scanLine(y), source + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride), line);
                std::lock_guard lock(pw->lock);
                pw->latest = image;
                pw->hasFresh = true;
                pw->fresh.notify_all();
            }
        }
        (void)pw_stream_queue_buffer(pw->stream, buffer);
    }

    static const pw_stream_events STREAM_EVENTS = {
        .version = PW_VERSION_STREAM_EVENTS,
        .state_changed = on_state_changed,
        .param_changed = on_param_changed,
        .process = on_process,
    };
#endif

//----------------------------------------------------------------//
/* CLASS */

xdesk::platform::PortalCapture::PortalCapture(void)
{
    /* Nothing */
}

xdesk::platform::PortalCapture::~PortalCapture()
{
    this->stop();
}

bool xdesk::platform::PortalCapture::start(void)
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        this->_error = "no D-Bus session bus";
        return false;
    }
    #ifdef XDESK_HAS_PIPEWIRE
        if (this->startScreenCast_()) return true;
        if (this->_error.contains("cancelled")) return false; // the user said no: never fall back silently
    #endif
    return this->startScreenshot_();
}

bool xdesk::platform::PortalCapture::startScreenCast_(void)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const uint version = portal_property(PORTAL_SCREENCAST, "version").toUInt();
    const uint cursorModes = portal_property(PORTAL_SCREENCAST, "AvailableCursorModes").toUInt();
    QVariantMap results;
    QVariantMap options;
    QFile tokenFile(token_path());

    if (version == 0) {this->_error = "ScreenCast portal missing (xdg-desktop-portal)"; return false;}

    // Session
    options.insert("session_handle_token", "xdesk" + security::random_id(6));
    if (!portal_request(PORTAL_SCREENCAST, "CreateSession", {}, options, results, this->_error, 10000)) return false;
    this->_session = results.value("session_handle").toString();
    if (this->_session.isEmpty()) {this->_error = "CreateSession: no session handle"; return false;}

    // Sources: one monitor, cursor in the frames, approval remembered
    options.clear();
    options.insert("types", static_cast<uint>(1));
    options.insert("multiple", false);
    options.insert("cursor_mode", static_cast<uint>((cursorModes & 2) ? 2 : 1));
    if (version >= 4) {
        options.insert("persist_mode", static_cast<uint>(2));
        if (tokenFile.open(QIODevice::ReadOnly)) {
            const QString token = QString::fromUtf8(tokenFile.readAll()).trimmed();
            if (!token.isEmpty()) options.insert("restore_token", token);
            tokenFile.close();
        }
    }
    if (!portal_request(PORTAL_SCREENCAST, "SelectSources", {QVariant::fromValue(QDBusObjectPath(this->_session))}, options, results, this->_error, 10000)) {
        this->closeSession_();
        return false;
    }

    // Start: the dialog of the compositor (skipped with a valid restore token)
    options.clear();
    if (!portal_request(PORTAL_SCREENCAST, "Start", {QVariant::fromValue(QDBusObjectPath(this->_session)), QString("")}, options, results, this->_error, USER_TIMEOUT)) {
        this->closeSession_();
        return false;
    }
    if (results.contains("restore_token") && tokenFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        (void)tokenFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        (void)tokenFile.write(results.value("restore_token").toString().toUtf8());
        tokenFile.close();
    }
    const QDBusArgument streams = results.value("streams").value<QDBusArgument>();
    streams.beginArray();
    while (!streams.atEnd()) {
        uint node = 0;
        QVariantMap properties;
        streams.beginStructure();
        streams >> node >> properties;
        streams.endStructure();
        if (this->_node == 0) this->_node = node;
    }
    streams.endArray();
    if (this->_node == 0) {this->_error = "Start: no stream"; this->closeSession_(); return false;}

    // PipeWire remote of the session
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, PORTAL_SCREENCAST, "OpenPipeWireRemote");
    call.setArguments({QVariant::fromValue(QDBusObjectPath(this->_session)), QVariantMap()});
    const QDBusReply<QDBusUnixFileDescriptor> remote = bus.call(call, QDBus::Block, 10000);
    if (!remote.isValid() || !remote.value().isValid()) {
        this->_error = "OpenPipeWireRemote: " + remote.error().message();
        this->closeSession_();
        return false;
    }
    if (!this->startPipeWire_(::dup(remote.value().fileDescriptor()))) {
        this->closeSession_();
        return false;
    }
    return true;
}

bool xdesk::platform::PortalCapture::startPipeWire_(const int fd)
{
    #ifdef XDESK_HAS_PIPEWIRE
        static std::once_flag initialized;
        uint8_t buffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        const spa_pod* params[1];
        spa_rectangle defaultSize = SPA_RECTANGLE(1920, 1080);
        spa_rectangle minSize = SPA_RECTANGLE(1, 1);
        spa_rectangle maxSize = SPA_RECTANGLE(16384, 16384);
        spa_fraction defaultRate = SPA_FRACTION(60, 1);
        spa_fraction minRate = SPA_FRACTION(0, 1);
        spa_fraction maxRate = SPA_FRACTION(240, 1);

        if (fd < 0) {this->_error = "invalid PipeWire fd"; return false;}
        std::call_once(initialized, [](void) {pw_init(nullptr, nullptr);});
        this->_pw = std::make_unique<PipeWireState>();
        PipeWireState* pw = this->_pw.get();
        pw->loop = pw_thread_loop_new("xdesk-capture", nullptr);
        if (!pw->loop) {::close(fd); this->_error = "pw_thread_loop_new failed"; return false;}
        pw->context = pw_context_new(pw_thread_loop_get_loop(pw->loop), nullptr, 0);
        if (!pw->context || pw_thread_loop_start(pw->loop) < 0) {::close(fd); this->_error = "PipeWire context failed"; return false;}

        pw_thread_loop_lock(pw->loop);
        pw->core = pw_context_connect_fd(pw->context, fd, nullptr, 0); // takes the fd
        if (!pw->core) {
            pw_thread_loop_unlock(pw->loop);
            this->_error = "PipeWire connection refused";
            return false;
        }
        pw->stream = pw_stream_new(pw->core, "xdesk", pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Screen", nullptr));
        pw_stream_add_listener(pw->stream, &pw->streamListener, &STREAM_EVENTS, pw);
        params[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
            SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
            SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(5, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_RGBx, SPA_VIDEO_FORMAT_RGBA),
            SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&defaultSize, &minSize, &maxSize),
            SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&defaultRate, &minRate, &maxRate)));
        const int connected = pw_stream_connect(pw->stream, PW_DIRECTION_INPUT, this->_node, static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS), params, 1);
        pw_thread_loop_unlock(pw->loop);
        if (connected < 0) {this->_error = "pw_stream_connect failed"; return false;}

        // First frame (gives the size)
        std::unique_lock lock(pw->lock);
        if (!pw->fresh.wait_for(lock, std::chrono::milliseconds(FIRST_FRAME_TIMEOUT), [pw](void) {return pw->hasFresh || pw->failed;}) || pw->failed) {
            this->_error = pw->failed ? pw->error : QString("no frame from PipeWire");
            return false;
        }
        this->_size = pw->latest.size();
        this->_screenshot = false;
        return true;
    #else
        if (fd >= 0) ::close(fd);
        this->_error = "PipeWire support not compiled in";
        return false;
    #endif
}

bool xdesk::platform::PortalCapture::startScreenshot_(void)
{
    const QString previous = this->_error;

    this->_screenshot = true;
    this->_last = this->screenshot_();
    if (this->_last.isNull()) {
        this->_error = previous.isEmpty() ? this->_error : previous + " / " + this->_error;
        return false;
    }
    this->_size = this->_last.size();
    return true;
}

QImage xdesk::platform::PortalCapture::screenshot_(void)
{
    QVariantMap results;
    QVariantMap options;

    options.insert("interactive", false);
    this->_lastShot.start();
    if (!portal_request(PORTAL_SCREENSHOT, "Screenshot", {QString("")}, options, results, this->_error, 10000)) return {};
    const QString file = QUrl(results.value("uri").toString()).toLocalFile();
    QImage image(file);
    (void)QFile::remove(file);
    if (image.isNull()) {this->_error = "Screenshot: unreadable file " + file; return {};}
    return image.convertToFormat(QImage::Format_RGB32);
}

QImage xdesk::platform::PortalCapture::grab(void)
{
    if (this->_screenshot) {
        if (!this->_lastShot.isValid() || this->_lastShot.elapsed() >= SCREENSHOT_INTERVAL) {
            const QImage image = this->screenshot_();
            if (!image.isNull()) {this->_last = image; this->_size = image.size();}
        }
        return this->_last;
    }
    #ifdef XDESK_HAS_PIPEWIRE
        if (!this->_pw) return {};
        PipeWireState* pw = this->_pw.get();
        QImage image;
        {
            std::unique_lock lock(pw->lock);
            if (!pw->hasFresh && !pw->failed)
                (void)pw->fresh.wait_for(lock, std::chrono::milliseconds(FRAME_WAIT), [pw](void) {return pw->hasFresh || pw->failed;});
            if (pw->failed) {this->_error = pw->error; return {};}
            pw->hasFresh = false;
            image = pw->latest; // the same frame again when nothing changed (the encoder skips it)
        }
        if (image.format() != QImage::Format_RGB32) image = image.convertToFormat(QImage::Format_RGB32);
        this->_size = image.size();
        return image;
    #else
        return {};
    #endif
}

void xdesk::platform::PortalCapture::stop(void)
{
    #ifdef XDESK_HAS_PIPEWIRE
        if (this->_pw) {
            PipeWireState* pw = this->_pw.get();
            if (pw->loop) pw_thread_loop_stop(pw->loop);
            if (pw->stream) pw_stream_destroy(pw->stream);
            if (pw->core) pw_core_disconnect(pw->core);
            if (pw->context) pw_context_destroy(pw->context);
            if (pw->loop) pw_thread_loop_destroy(pw->loop);
            this->_pw.reset();
        }
    #endif
    this->closeSession_();
}

void xdesk::platform::PortalCapture::closeSession_(void)
{
    if (this->_session.isEmpty()) return;
    QDBusMessage call = QDBusMessage::createMethodCall(PORTAL_SERVICE, this->_session, PORTAL_SESSION, "Close");
    (void)QDBusConnection::sessionBus().call(call, QDBus::NoBlock);
    this->_session.clear();
    this->_node = 0;
}

#include "PortalCapture.moc"
