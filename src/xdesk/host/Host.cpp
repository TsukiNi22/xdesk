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
##  @file Host.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xdesk/security/Crypto.hpp"
#include "xdesk/host/Host.hpp"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QClipboard>
#include <QHostInfo>
#include <QDateTime>
#include <QFile>
#include <QDir>

// Human readable input of the activity feed
static QString mouse_button_name(const std::uint8_t button)
{
    static const char* NAMES[] = {"left", "middle", "right", "back", "forward"};

    return button < 5 ? NAMES[button] : "button " + QString::number(button);
}

xdesk::host::Host::Host(core::Config& config, QObject* parent)
: QObject(parent), _config{config}, _log{core::Config::logDir()}, _cert{core::Config::dataDir()}, _registry{core::Config::dataDir()}
{
    QObject::connect(&this->_code, &security::PairingCode::changed, this, [this](const QString&) {emit this->stateChanged();});
    QObject::connect(&this->_registry, &security::DeviceRegistry::changed, this, [this]() {
        this->_guard.endTrustedWrite();
        emit this->stateChanged();
    });
    QObject::connect(&this->_guard, &security::IntegrityGuard::tamper, this, &Host::onTamper_);
    QObject::connect(&this->_exposure, &net::Exposure::changed, this, &Host::stateChanged);
    QObject::connect(&this->_server, &net::Server::newSocket, this, &Host::onSocket_);
}

xdesk::host::Host::~Host()
{
    if (this->_running) this->stop();
}

void xdesk::host::Host::start(void)
{
    const core::Settings& s = this->_config.settings();
    const QString data = core::Config::dataDir();
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();

    // Identity, devices, code
    this->_cert.load();
    this->_registry.load();
    if (this->_registry.wasTampered())
        this->_log.alert("tamper", "", "devices.json was not written by xdesk: every trusted device was reset (bans kept)");
    this->_code.configure(s.codeLength, s.codeLifetime);

    // Tamper protection (the devices of an unclean stop with control / terminal are banned)
    this->_guard.start({core::Config::executablePath(), this->_config.path(), data + "/devices.json", data + "/registry.key", data + "/cert.pem", data + "/key.pem"},
        {core::Config::configDir(), data}, data + "/running.json");
    for (const QString& entry: this->_guard.previousCrashSessions()) {
        const QString device = entry.section('|', 0, 0);
        const QString address = entry.section('|', 1, 1);
        if (!s.banOnTamper) break;
        this->_guard.beginTrustedWrite();
        if (!device.isEmpty()) this->_registry.ban(device, "xdesk was stopped while this device had control / a terminal");
        if (!address.isEmpty()) this->_registry.banAddress(address, "xdesk was stopped while this address had control / a terminal");
        this->_log.alert("tamper", "", "Unclean stop during a remote session: banned " + entry);
    }
    this->_guard.writeMarker({});
    this->_guard.harden();

    // Server
    tls.setLocalCertificate(this->_cert.certificate());
    tls.setPrivateKey(this->_cert.key());
    security::configure_tls(tls);
    this->_server.setFilter([this](const QHostAddress& address) {
        QString ip = address.toString();
        if (ip.startsWith("::ffff:")) ip = ip.mid(7);
        return !this->_registry.addressBan(ip).has_value();
    });
    this->_server.setInfo([this]() {
        QFile version(":/android/version.txt");
        return QJsonObject{
            {"name", QHostInfo::localHostName()}, {"version", XDESK_VERSION}, {"pq", true},
            {"apk", version.open(QIODevice::ReadOnly) ? QString::fromUtf8(version.readAll()).trimmed() : QString()},
            {"fingerprint", security::fingerprint_text(this->_cert.fingerprint())}, {"web", this->_config.settings().allowWeb},
        };
    });
    this->_server.listen(s.bind, s.port, tls);
    this->_exposure.start(s, this->_server.port(), QHostInfo::localHostName(), security::fingerprint_short(this->_cert.fingerprint()));

    // Capture thread
    this->_stream = new Stream();
    this->_stream->moveToThread(&this->_streamThread);
    QObject::connect(&this->_streamThread, &QThread::started, this->_stream, &Stream::start);
    QObject::connect(&this->_streamThread, &QThread::finished, this->_stream, &QObject::deleteLater);
    QObject::connect(this->_stream, &Stream::ready, this, [this](const QSize& size, const QString& backend) {
        this->_screen = size;
        this->_captureBackend = backend;
        this->_injector = platform::create_input_injector(size);
        this->_log.info("server", "", "Capture " + backend + " " + QString::number(size.width()) + "x" + QString::number(size.height()) + ", input " + this->inputBackend());
        emit this->stateChanged();
    });
    QObject::connect(this->_stream, &Stream::failed, this, [this](const QString& error) {
        this->_captureBackend = "none (" + error + ")";
        this->_log.warn("server", "", "Screen capture unavailable: " + error);
        emit this->stateChanged();
    });
    QObject::connect(this->_stream, &Stream::frame, this, [this](const QString& id, const QByteArray& message) {
        Session* s = this->_sessions.value(id);
        if (s && s->state() == State::Active) s->sendBinary(message, true);
    });
    QObject::connect(this->_stream, &Stream::cursor, this, [this](const QPoint& position) {
        for (Session* s: this->_sessions) {
            const double scale = this->_scales.value(s->id(), 1.0);
            net::ByteWriter w(net::Binary::Cursor, 8);
            if (s->state() != State::Active || !s->permissions().view) continue;
            w.i16(static_cast<std::int16_t>(position.x() * scale)).i16(static_cast<std::int16_t>(position.y() * scale)).u8(position.x() >= 0 ? 1 : 0);
            s->sendBinary(w.data());
        }
    });
    QObject::connect(this->_stream, &Stream::resized, this, [this](const QSize& size) {
        this->_screen = size;
        this->_injector = platform::create_input_injector(size);
        for (Session* s: this->_sessions)
            if (s->state() == State::Active) s->sendJson({{"t", "screen"}, {"w", size.width()}, {"h", size.height()}});
        emit this->stateChanged();
    });
    this->_streamThread.start();
    this->_running = true;
    this->_log.info("server", "", "Sharing started on " + s.bind + ":" + QString::number(this->_server.port()) + " (" + core::exposure_name(s.exposure) + ", " + security::tls_summary() + ")");
    emit this->stateChanged();
}

void xdesk::host::Host::stop(void)
{
    QEventLoop loop;

    if (!this->_running) return;
    this->_running = false;
    this->kickAll("Host stopped sharing");
    this->_exposure.stop();
    this->_server.stop();

    // The capture may wait for the ui thread (Qt grab): keep processing events while the thread stops
    if (this->_stream) QMetaObject::invokeMethod(this->_stream, &Stream::stop, Qt::QueuedConnection);
    QObject::connect(&this->_streamThread, &QThread::finished, &loop, &QEventLoop::quit);
    this->_streamThread.quit();
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    if (this->_streamThread.isRunning()) loop.exec();
    this->_stream = nullptr;
    if (this->_injector) this->_injector->releaseAll();
    this->_guard.writeMarker({});
    this->_log.info("server", "", "Sharing stopped");
}

xdesk::host::Permissions xdesk::host::Host::defaultPermissions_(const Session* session) const
{
    const core::Settings& s = this->_config.settings();
    Permissions p;

    (void)session;
    p.view = true;
    p.control = s.allowControl && !s.viewOnlyDefault;
    p.terminal = s.allowTerminal;
    p.clipboard = false;
    return p;
}

void xdesk::host::Host::onSocket_(net::WebSocket* ws, const QString& userAgent)
{
    Session* session = nullptr;
    int alive = 0;

    for (Session* s: this->_sessions)
        alive += s->state() != State::Closed;
    session = new Session(ws, userAgent, this);
    if (alive >= this->_config.settings().maxSessions) {
        session->deny("Too many sessions on this host");
        session->deleteLater();
        return;
    }
    this->_sessions.insert(session->id(), session);
    QObject::connect(session, &Session::hello, this, &Host::onHello_);
    QObject::connect(session, &Session::auth, this, &Host::onAuth_);
    QObject::connect(session, &Session::control, this, &Host::onControl_);
    QObject::connect(session, &Session::input, this, &Host::onInput_);
    QObject::connect(session, &Session::closed, this, &Host::onClosed_);
    QObject::connect(session, &Session::changed, this, &Host::sessionChanged);
    QObject::connect(session, &Session::activity, this, [this](Session* s, const QString& description) {emit this->activity(s->id(), description);});
    this->_log.info("session", session->id(), "Connection from " + session->address(), {{"address", session->address()}, {"userAgent", userAgent}});
    emit this->sessionAdded(session);
}

void xdesk::host::Host::onHello_(Session* session, const QJsonObject& message)
{
    const QString device = message.value("device").toString();
    const std::optional<security::Device> known = this->_registry.find(device);
    const bool web = session->kind() == net::ClientKind::Web || session->kind() == net::ClientKind::Apk;

    if (web && !this->_config.settings().allowWeb) {session->deny("Web clients are disabled on this host"); return;}
    if (known.has_value() && known->banned) {
        this->_log.warn("auth", session->id(), "Banned device tried to connect: " + known->name, {{"device", device}});
        session->deny("This device is banned from this host");
        return;
    }
    if (known.has_value()) session->setDeviceId(device);
    session->sendChallenge(QHostInfo::localHostName(), this->_code.salt(), known.has_value() && known->trusted && !known->secret.isEmpty());
    emit this->sessionChanged(session);
}

void xdesk::host::Host::authFailed_(Session* session, const QString& reason)
{
    const int failures = ++this->_authFailures[session->address()];
    const int max = this->_config.settings().maxAuthFailures;

    this->_log.warn("auth", session->id(), "Authentication failed (" + reason + ") from " + session->address(), {{"failures", failures}});
    if (failures >= max) {
        this->_registry.banAddress(session->address(), "Too many failed authentications", 3600);
        this->_authFailures.remove(session->address());
        this->_code.rotate(); // the code under attack is dropped
        this->_log.alert("auth", session->id(), session->address() + " banned for 1 h after " + QString::number(failures) + " failures");
    }
    session->deny(reason);
}

void xdesk::host::Host::onAuth_(Session* session, const QJsonObject& message)
{
    const QString method = message.value("method").toString();
    const core::Approval approval = this->_config.settings().approval;
    const QByteArray transcript = security::auth_transcript(session->serverNonce(), session->clientNonce(), this->_cert.fingerprint(), "client");
    const QByteArray serverTranscript = security::auth_transcript(session->serverNonce(), session->clientNonce(), this->_cert.fingerprint(), "server");
    const QByteArray proof = QByteArray::fromHex(message.value("proof").toString().toLatin1());
    bool trustedDevice = false;

    if (method == "code") {
        if (!this->_code.checkPlain(message.value("code").toString())) {this->authFailed_(session, "Wrong or expired code"); return;}
        this->_code.rotate(); // one time code
    } else if (method == "proof") {
        if (!this->_code.checkProof(transcript, proof)) {this->authFailed_(session, "Wrong or expired code"); return;}
        this->_pendingProof[session->id()] = QString::fromLatin1(this->_code.serverProof(serverTranscript).toHex());
        this->_code.rotate();
    } else if (method == "device") {
        const std::optional<security::Device> d = this->_registry.find(session->deviceId());
        if (!d.has_value() || !d->trusted || d->secret.isEmpty() || !security::constant_equal(security::hmac_sha256(d->secret, transcript), proof)) {
            this->authFailed_(session, "Unknown or untrusted device: use a code");
            return;
        }
        this->_pendingProof[session->id()] = QString::fromLatin1(security::hmac_sha256(d->secret, serverTranscript).toHex());
        trustedDevice = true;
    } else {this->authFailed_(session, "Unknown authentication method"); return;}
    this->_authFailures.remove(session->address());
    this->_remember[session->id()] = message.value("remember").toBool(false);
    this->_log.info("auth", session->id(), "Authenticated (" + method + ") " + session->name() + " from " + session->address());

    // Local confirmation
    if (approval == core::Approval::Always || (approval == core::Approval::Untrusted && !trustedDevice)) {
        session->setState(State::Pending);
        this->_log.info("auth", session->id(), "Waiting for the local approval");
        emit this->approvalRequested(session);
        QTimer::singleShot(60000, session, [this, session]() {
            if (session->state() == State::Pending) {
                this->_log.info("auth", session->id(), "Approval timeout: refused");
                session->deny("The host did not answer");
            }
        });
        return;
    }
    this->admit_(session, this->_remember.value(session->id()));
}

void xdesk::host::Host::approve(const QString& id, const bool accepted, const bool trust)
{
    Session* session = this->_sessions.value(id);

    if (!session || session->state() != State::Pending) return;
    if (!accepted) {
        this->_log.info("auth", id, "Refused by the local user");
        session->deny("Refused by the host");
        return;
    }
    this->_log.info("auth", id, trust ? "Accepted and trusted by the local user" : "Accepted by the local user");
    this->admit_(session, trust || this->_remember.value(id));
    if (trust && !session->deviceId().isEmpty()) this->_registry.setTrusted(session->deviceId(), true);
}

void xdesk::host::Host::admit_(Session* session, const bool remember)
{
    const core::Settings& s = this->_config.settings();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::optional<security::Device> known = this->_registry.find(session->deviceId());
    security::Device device = known.value_or(security::Device{});
    QJsonObject extra;

    // Device record (every admitted client gets one: bans follow the device)
    if (!known.has_value()) {
        device.id = security::random_id(12);
        device.since = now;
    }
    device.name = session->name();
    device.kind = net::client_kind_name(session->kind());
    device.os = session->os();
    device.address = session->address();
    device.seen = now;
    device.sessions++;
    if (remember && device.secret.isEmpty()) {
        device.secret = security::random_bytes(32);
        device.trusted = device.trusted || s.approval == core::Approval::Never;
        extra["device"] = QJsonObject{{"id", device.id}, {"secret", QString::fromLatin1(device.secret.toHex())}};
    } else if (!known.has_value()) extra["device"] = QJsonObject{{"id", device.id}};
    (void)this->_registry.upsert(device);
    session->setDeviceId(device.id);

    // Welcome
    extra["host"] = QHostInfo::localHostName();
    extra["screen"] = QJsonObject{{"w", this->_screen.width()}, {"h", this->_screen.height()}};
    extra["capture"] = this->_captureBackend;
    if (this->_pendingProof.contains(session->id())) extra["proof"] = this->_pendingProof.take(session->id());
    session->accept(this->defaultPermissions_(session), extra);
    this->_scales[session->id()] = s.scale;
    if (this->_stream)
        QMetaObject::invokeMethod(this->_stream, "addClient", Qt::QueuedConnection, Q_ARG(QString, session->id()), Q_ARG(int, s.quality), Q_ARG(double, s.scale), Q_ARG(int, s.fps));
    this->_log.info("session", session->id(), "Session started: " + session->name() + " (" + net::client_kind_name(session->kind()) + ", " + session->address() + ")", session->describe());
    this->updateMarker_();
    emit this->sessionChanged(session);
}

void xdesk::host::Host::onControl_(Session* session, const QJsonObject& message)
{
    const QString type = message.value("t").toString();
    const Permissions& perms = session->permissions();

    if (type == "quality") {
        const int quality = std::clamp(message.value("quality").toInt(70), 1, 100);
        const double scale = std::clamp(message.value("scale").toDouble(1.0), 0.25, 1.0);
        const int fps = std::clamp(message.value("fps").toInt(30), 1, 60);
        this->_scales[session->id()] = scale;
        if (this->_stream)
            QMetaObject::invokeMethod(this->_stream, "setQuality", Qt::QueuedConnection, Q_ARG(QString, session->id()), Q_ARG(int, quality), Q_ARG(double, scale), Q_ARG(int, fps));
    } else if (type == "mode") {
        const QString mode = message.value("mode").toString() == "control" && perms.control ? "control" : "view";
        if (mode != "control" && session->mode() == "control" && this->_injector) this->_injector->releaseAll();
        session->setMode(mode);
        this->_log.info("session", session->id(), "Mode " + mode);
        this->updateMarker_();
    } else if (type == "refresh") {
        if (this->_stream) QMetaObject::invokeMethod(this->_stream, "refresh", Qt::QueuedConnection, Q_ARG(QString, session->id()));
    } else if (type == "term_open") {
        this->openTerminal_(session, static_cast<std::uint8_t>(message.value("id").toInt()), message.value("cols").toInt(80), message.value("rows").toInt(24));
    } else if (type == "term_resize") {
        for (Terminal& t: this->_terminals)
            if (t.session == session->id() && t.id == message.value("id").toInt()) t.pty->resize(std::clamp(message.value("cols").toInt(80), 2, 1000), std::clamp(message.value("rows").toInt(24), 2, 1000));
    } else if (type == "term_close") {
        this->closeTerminal_(session->id(), static_cast<std::uint8_t>(message.value("id").toInt()));
    } else if (type == "clip") {
        if (!perms.clipboard || !qobject_cast<QGuiApplication*>(QCoreApplication::instance())) return;
        QGuiApplication::clipboard()->setText(message.value("text").toString().left(1 << 20));
        session->setActivity("clipboard set (" + QString::number(message.value("text").toString().size()) + " chars)");
        this->_log.info("input", session->id(), "Clipboard set by the session");
    }
}

void xdesk::host::Host::onInput_(Session* session, const QByteArray& message)
{
    const net::Binary type = static_cast<net::Binary>(static_cast<std::uint8_t>(message[0]));
    const double scale = this->_scales.value(session->id(), 1.0);
    net::ByteReader r(message);
    const bool canControl = session->permissions().control && session->mode() == "control" && this->_injector && !this->_panelFocused;

    switch (type) {
        case net::Binary::FrameAck: {
            const std::uint32_t id = r.u32();
            if (r.ok() && this->_stream) QMetaObject::invokeMethod(this->_stream, "ack", Qt::QueuedConnection, Q_ARG(QString, session->id()), Q_ARG(std::uint32_t, id));
            return;
        }
        case net::Binary::TermData: {
            const std::uint8_t id = r.u8();
            const QByteArray data = r.rest();
            for (Terminal& t: this->_terminals)
                if (r.ok() && t.session == session->id() && t.id == id) {this->terminalInput_(t, data); return;}
            return;
        }
        default: break;
    }
    if (!canControl) return; // view only, panel focused, or no injector: ignored
    switch (type) {
        case net::Binary::MouseMove: {
            const int x = static_cast<int>(r.u16() / scale);
            const int y = static_cast<int>(r.u16() / scale);
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (!r.ok()) return;
            this->_injector->mouseMove(x, y);
            if (now - this->_lastMoveLog > 250) {this->_lastMoveLog = now; session->setActivity("mouse " + QString::number(x) + "," + QString::number(y));}
            break;
        }
        case net::Binary::MouseButton: {
            const std::uint8_t button = r.u8();
            const bool down = r.u8() != 0;
            if (!r.ok() || button > 4) return;
            this->_injector->mouseButton(static_cast<net::MouseButton>(button), down);
            session->setActivity(mouse_button_name(button) + (down ? " press" : " release"));
            if (down) this->_log.info("input", session->id(), "Click " + mouse_button_name(button));
            break;
        }
        case net::Binary::MouseWheel: {
            const int dx = r.i16();
            const int dy = r.i16();
            if (!r.ok()) return;
            this->_injector->mouseWheel(dx, dy);
            session->setActivity("wheel " + QString::number(dx) + "," + QString::number(dy));
            break;
        }
        case net::Binary::Key: {
            const bool down = r.u8() != 0;
            const QString code = QString::fromLatin1(r.bytes(r.u8()));
            if (!r.ok() || code.isEmpty() || code.size() > 32) return;
            this->_injector->key(code, down);
            session->setActivity("key " + code + (down ? " down" : " up"));
            if (down) this->_log.info("input", session->id(), "Key " + code);
            break;
        }
        case net::Binary::Text: {
            const QString text = QString::fromUtf8(r.rest()).left(4096);
            this->_injector->text(text);
            session->setActivity("text \"" + text.left(40) + "\"");
            this->_log.info("input", session->id(), "Text typed (" + QString::number(text.size()) + " chars)", {{"text", text}});
            break;
        }
        default: break;
    }
}

void xdesk::host::Host::openTerminal_(Session* session, const std::uint8_t id, const int cols, const int rows)
{
    const core::Settings& s = this->_config.settings();
    QMap<QString, QString> env;
    Terminal terminal;
    int count = 0;

    if (!session->permissions().terminal || !s.allowTerminal) {session->sendJson({{"t", "term_exit"}, {"id", id}, {"code", -1}, {"reason", "Terminal not allowed"}}); return;}
    for (const Terminal& t: this->_terminals) {
        if (t.session != session->id()) continue;
        count++;
        if (t.id == id) return;
    }
    if (count >= 4) {session->sendJson({{"t", "term_exit"}, {"id", id}, {"code", -1}, {"reason", "4 terminals at most"}}); return;}
    env["XDESK_SESSION"] = session->id();
    env["TERM"] = "xterm-256color";
    terminal.pty = platform::create_terminal(this);
    terminal.session = session->id();
    terminal.id = id;
    terminal.transcript = core::Config::logDir() + "/terminal-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "-" + session->id() + "-" + QString::number(id) + ".log";
    if (!terminal.pty->start(s.shell.isEmpty() ? platform::default_shell() : s.shell, std::clamp(cols, 2, 1000), std::clamp(rows, 2, 1000), env)) {
        session->sendJson({{"t", "term_exit"}, {"id", id}, {"code", -1}, {"reason", terminal.pty->error()}});
        this->_log.warn("terminal", session->id(), "Terminal failed: " + terminal.pty->error());
        terminal.pty->deleteLater();
        return;
    }
    this->_guard.addTerminal(session->id(), terminal.pty->pid());
    QObject::connect(terminal.pty, &platform::ITerminal::output, this, [this, sid = session->id(), id, path = terminal.transcript](const QByteArray& data) {
        Session* s = this->_sessions.value(sid);
        net::ByteWriter w(net::Binary::TermData, static_cast<std::size_t>(data.size()) + 2);
        QFile file(path);
        if (s) s->sendBinary(w.u8(id).bytes(data).data());
        if (file.open(QIODevice::Append)) file.write(data);
        emit this->terminalOutput(sid, id, data);
    });
    QObject::connect(terminal.pty, &platform::ITerminal::finished, this, [this, sid = session->id(), id](const int code) {
        Session* s = this->_sessions.value(sid);
        if (s) s->sendJson({{"t", "term_exit"}, {"id", id}, {"code", code}});
        this->closeTerminal_(sid, id);
    });
    this->_terminals.append(terminal);
    session->setActivity("terminal " + QString::number(id) + " opened");
    this->_log.info("terminal", session->id(), "Terminal " + QString::number(id) + " opened (pid " + QString::number(terminal.pty->pid()) + ")", {{"transcript", terminal.transcript}});
    this->updateMarker_();
}

void xdesk::host::Host::terminalInput_(Terminal& terminal, const QByteArray& data)
{
    Session* session = this->_sessions.value(terminal.session);
    QByteArray forward;
    QFile file(terminal.transcript);

    if (!session || !session->permissions().terminal) return;

    // Line tracking: the command is checked BEFORE its Enter reaches the shell
    for (const char c: data) {
        if (c == '\r' || c == '\n') {
            const QString line = QString::fromUtf8(terminal.line);
            terminal.line.clear();
            if (!line.trimmed().isEmpty()) {
                this->_log.info("terminal", terminal.session, "$ " + line.left(500), {{"terminal", terminal.id}});
                session->setActivity("$ " + line.left(80));
            }
            if (this->_guard.checkCommand(terminal.session, line)) return; // banned, nothing forwarded
        } else if (c == 0x7F || c == 0x08) {
            if (!terminal.line.isEmpty()) terminal.line.chop(1);
        } else if (c == 0x03 || c == 0x15) terminal.line.clear(); // ctrl+c / ctrl+u
        else if (static_cast<unsigned char>(c) >= 0x20) terminal.line.append(c);
        forward.append(c);
    }
    if (file.open(QIODevice::Append)) file.write("\n[input] " + data.toHex(' ') + "\n");
    terminal.pty->write(forward);
    emit this->terminalInput(terminal.session, terminal.id, forward);
}

void xdesk::host::Host::closeTerminal_(const QString& session, const std::uint8_t id)
{
    for (qsizetype i = 0; i < this->_terminals.size(); ++i) {
        Terminal& t = this->_terminals[i];
        if (t.session != session || t.id != id) continue;
        this->_guard.removeTerminal(session, t.pty->pid());
        t.pty->disconnect(this);
        t.pty->terminate();
        t.pty->deleteLater();
        this->_log.info("terminal", session, "Terminal " + QString::number(id) + " closed");
        this->_terminals.removeAt(i);
        break;
    }
    this->updateMarker_();
}

void xdesk::host::Host::closeTerminals_(const QString& session)
{
    QList<std::uint8_t> ids;

    for (const Terminal& t: this->_terminals)
        if (t.session == session) ids.append(t.id);
    for (const std::uint8_t id: ids)
        this->closeTerminal_(session, id);
}

void xdesk::host::Host::onClosed_(Session* session)
{
    const QString id = session->id();

    if (this->_stream) QMetaObject::invokeMethod(this->_stream, "removeClient", Qt::QueuedConnection, Q_ARG(QString, id));
    if (session->mode() == "control" && this->_injector) this->_injector->releaseAll();
    this->closeTerminals_(id);
    this->_sessions.remove(id);
    this->_scales.remove(id);
    this->_remember.remove(id);
    this->_pendingProof.remove(id);
    this->_log.info("session", id, "Session closed: " + session->name() + " (" + session->address() + ")", session->describe());
    emit this->sessionRemoved(id);
    session->deleteLater();
    this->updateMarker_();
}

void xdesk::host::Host::onTamper_(const QString& session, const QString& reason)
{
    QStringList suspects;

    if (!session.isEmpty()) suspects.append(session);
    else {
        // Unknown author: every session able to act on the PC
        for (Session* s: this->_sessions)
            if (s->state() == State::Active && ((s->permissions().control && s->mode() == "control") || std::any_of(this->_terminals.cbegin(), this->_terminals.cend(), [&](const Terminal& t) {return t.session == s->id();})))
                suspects.append(s->id());
    }
    if (suspects.isEmpty()) {
        this->_log.warn("tamper", "", "Protected file changed with no remote session able to act (local change): " + reason);
        return;
    }
    this->_log.alert("tamper", session, reason, {{"suspects", QJsonArray::fromStringList(suspects)}});
    for (const QString& id: suspects) {
        emit this->tamperDetected(id, reason);
        if (this->_config.settings().banOnTamper) this->ban(id, "Tampering attempt: " + reason);
        else this->kick(id, "Tampering attempt: " + reason);
    }
    this->_code.rotate();
}

QList<xdesk::host::Session*> xdesk::host::Host::sessions(void) const
{
    return this->_sessions.values();
}

xdesk::host::Session* xdesk::host::Host::session(const QString& id) const
{
    return this->_sessions.value(id);
}

void xdesk::host::Host::kick(const QString& id, const QString& reason)
{
    Session* session = this->_sessions.value(id);

    if (!session) return;
    this->_log.info("session", id, "Kicked: " + reason);
    session->kick(reason);
}

void xdesk::host::Host::kickAll(const QString& reason)
{
    for (Session* session: this->_sessions)
        session->kick(reason);
}

void xdesk::host::Host::ban(const QString& id, const QString& reason)
{
    Session* session = this->_sessions.value(id);

    if (!session) return;
    this->_guard.beginTrustedWrite();
    if (!session->deviceId().isEmpty() && this->_registry.find(session->deviceId()).has_value()) this->_registry.ban(session->deviceId(), reason);
    if (session->address() != "127.0.0.1" && session->address() != "::1") this->_registry.banAddress(session->address(), reason);
    this->_guard.endTrustedWrite();
    this->_log.alert("device", id, "Banned " + session->name() + " (" + session->address() + "): " + reason);
    this->closeTerminals_(id);
    session->kick(reason, true);
}

void xdesk::host::Host::setPermissions(const QString& id, const Permissions& perms)
{
    Session* session = this->_sessions.value(id);
    Permissions p = perms;
    const core::Settings& s = this->_config.settings();

    if (!session) return;
    p.control = p.control && s.allowControl;
    p.terminal = p.terminal && s.allowTerminal;
    if (!p.control && session->permissions().control && this->_injector) this->_injector->releaseAll();
    if (!p.terminal) this->closeTerminals_(id);
    session->setPermissions(p);
    this->_log.info("session", id, "Rights: view " + QString::number(p.view) + ", control " + QString::number(p.control) + ", terminal " + QString::number(p.terminal) + ", clipboard " + QString::number(p.clipboard));
    this->updateMarker_();
}

bool xdesk::host::Host::localActionsLocked(void) const
{
    for (const Session* s: this->_sessions)
        if (s->state() == State::Active && s->permissions().control && s->mode() == "control") return true;
    return !this->_terminals.isEmpty();
}

bool xdesk::host::Host::unban(const QString& deviceId)
{
    if (this->localActionsLocked()) return false;
    this->_registry.unban(deviceId);
    this->_log.info("device", "", "Device unbanned locally: " + deviceId);
    return true;
}

bool xdesk::host::Host::unbanAddress(const QString& address)
{
    if (this->localActionsLocked()) return false;
    this->_registry.unbanAddress(address);
    this->_log.info("device", "", "Address unbanned locally: " + address);
    return true;
}

bool xdesk::host::Host::setTrusted(const QString& deviceId, const bool trusted)
{
    if (this->localActionsLocked()) return false;
    this->_registry.setTrusted(deviceId, trusted);
    this->_log.info("device", "", QString(trusted ? "Device trusted: " : "Device untrusted: ") + deviceId);
    return true;
}

bool xdesk::host::Host::forget(const QString& deviceId)
{
    if (this->localActionsLocked()) return false;
    this->_registry.remove(deviceId);
    this->_log.info("device", "", "Device forgotten: " + deviceId);
    return true;
}

bool xdesk::host::Host::applySettings(const core::Settings& settings)
{
    const core::Settings old = this->_config.settings();

    if (this->localActionsLocked()) return false;
    this->_config.settings() = settings;
    this->_guard.beginTrustedWrite();
    this->_config.save();
    this->_guard.endTrustedWrite();
    if (old.codeLength != settings.codeLength || old.codeLifetime != settings.codeLifetime) this->_code.configure(settings.codeLength, settings.codeLifetime);
    if (old.port != settings.port || old.bind != settings.bind) {
        QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
        tls.setLocalCertificate(this->_cert.certificate());
        tls.setPrivateKey(this->_cert.key());
        security::configure_tls(tls);
        this->_server.stop();
        this->_server.listen(settings.bind, settings.port, tls);
    }
    if (old.exposure != settings.exposure || old.publicHost != settings.publicHost || old.port != settings.port || old.discovery != settings.discovery)
        this->_exposure.start(settings, this->_server.port(), QHostInfo::localHostName(), security::fingerprint_short(this->_cert.fingerprint()));
    this->_log.info("settings", "", "Settings changed locally");
    emit this->stateChanged();
    return true;
}

void xdesk::host::Host::setPanelFocused(const bool focused)
{
    if (focused == this->_panelFocused) return;
    this->_panelFocused = focused;
    if (focused && this->_injector) this->_injector->releaseAll();
    for (Session* s: this->_sessions)
        s->setControlPaused(focused);
}

void xdesk::host::Host::rotateCode(void)
{
    this->_code.rotate();
    this->_log.info("auth", "", "Code rotated locally");
}

void xdesk::host::Host::updateMarker_(void)
{
    QStringList alive;

    for (const Session* s: this->_sessions) {
        const bool terminal = std::any_of(this->_terminals.cbegin(), this->_terminals.cend(), [&](const Terminal& t) {return t.session == s->id();});
        if (s->state() == State::Active && ((s->permissions().control && s->mode() == "control") || terminal)) alive.append(s->deviceId() + "|" + s->address());
    }
    this->_guard.writeMarker(alive);
}
