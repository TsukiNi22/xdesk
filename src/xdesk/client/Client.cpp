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
##  @file Client.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/FrameEncoder.hpp"
#include "xdesk/security/Certificate.hpp"
#include "xdesk/security/Crypto.hpp"
#include "xdesk/client/Client.hpp"
#include "xdesk/core/Config.hpp"
#include <QSslConfiguration>
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostInfo>
#include <QDateTime>
#include <QSaveFile>
#include <functional>
#include <algorithm>
#include <QSysInfo>
#include <cstring>
#include <QFile>

// known_hosts.json helpers
static QJsonObject read_known_hosts(void)
{
    QFile file(xdesk::client::Client::knownHostsPath());

    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

static void write_known_hosts(const QJsonObject& hosts)
{
    QSaveFile file(xdesk::client::Client::knownHostsPath());

    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(hosts).toJson(QJsonDocument::Indented));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    file.commit();
}

QString xdesk::client::Client::knownHostsPath(void)
{
    return core::Config::configDir() + "/known_hosts.json";
}

xdesk::client::Client::Client(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_ping, &QTimer::timeout, this, [this]() {
        if (this->_ws) this->_ws->sendText(QString(R"({"t":"ping","ts":%1})").arg(QDateTime::currentMSecsSinceEpoch()));
    });
    QObject::connect(&this->_udp, &UdpLink::frame, this, [this](const QByteArray& message) {
        if (this->_state == State::Active) this->decodeFrame_(message, true);
    });
    QObject::connect(&this->_udp, &UdpLink::gaveUp, this, [this]() {
        if (this->_ws && this->_state == State::Active) this->_ws->sendText(R"({"t":"refresh"})"); // a frame lost for good: a complete one
    });
    QObject::connect(&this->_udp, &UdpLink::activeChanged, this, [this](const bool active) {
        if (!active && this->_ws && this->_state == State::Active) this->_ws->sendText(R"({"t":"udp_off"})"); // blocked / silent: the host sends on tcp
        emit this->message(active ? "Video over encrypted UDP" : "Video over TCP");
    });
}

xdesk::client::Client::~Client()
{
    if (this->_ws) this->_ws->disconnect(this);
}

void xdesk::client::Client::setState_(const State state)
{
    if (state == this->_state) return;
    this->_state = state;
    emit this->stateChanged(state);
}

void xdesk::client::Client::connectToHost(const Options& options)
{
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();

    this->disconnectFromHost();
    this->_openTerminals.clear(); // a new session: terminal 1 first
    this->_options = options;
    if (this->_options.name.isEmpty()) this->_options.name = QHostInfo::localHostName() + " - " + QSysInfo::prettyProductName();
    security::configure_tls(tls, true); // native to native: post-quantum key exchange only
    tls.setSslOption(QSsl::SslOptionDisableServerNameIndication, true); // no SNI: the pinned certificate, even on the tailnet name (the browsers asking it get the tailscale one)
    this->setState_(State::Connecting);
    this->_ws = net::WebSocket::connectTo(options.host, options.port, "/ws", tls, this);
    QObject::connect(this->_ws, &net::WebSocket::opened, this, &Client::onOpened_);
    QObject::connect(this->_ws, &net::WebSocket::textReceived, this, &Client::onText_);
    QObject::connect(this->_ws, &net::WebSocket::binaryReceived, this, &Client::onBinary_);
    QObject::connect(this->_ws, &net::WebSocket::failed, this, [this](const QString& error) {
        this->setState_(State::Closed);
        emit this->closed("Connection failed: " + error, false);
    });
    QObject::connect(this->_ws, &net::WebSocket::closed, this, [this]() {
        this->_ping.stop();
        if (this->_state != State::Closed) {
            this->setState_(State::Closed);
            emit this->closed("Connection closed", false);
        }
    });
}

void xdesk::client::Client::disconnectFromHost(void)
{
    this->_ping.stop();
    this->_udp.stop();
    if (!this->_ws) return;
    this->_ws->disconnect(this);
    this->_ws->close(1000, "bye");
    this->_ws->deleteLater();
    this->_ws = nullptr;
    this->setState_(State::Closed);
}

void xdesk::client::Client::onOpened_(void)
{
    const QString key = this->_options.host + ":" + QString::number(this->_options.port);
    const QJsonObject known = read_known_hosts().value(key).toObject();

    this->_fingerprint = security::Certificate::fingerprintOf(this->_ws->socket()->peerCertificate());
    if (this->_fingerprint.isEmpty()) {this->disconnectFromHost(); emit this->closed("The host sent no certificate", false); return;}
    if (known.value("fingerprint").toString() == QString::fromLatin1(this->_fingerprint.toHex())) {this->sendHello_(); return;}

    // Trust on first use, or a changed identity (possible man in the middle)
    this->setState_(State::Verifying);
    emit this->fingerprintCheck(security::fingerprint_text(this->_fingerprint), known.contains("fingerprint"));
}

void xdesk::client::Client::acceptFingerprint(const bool accepted, const bool remember)
{
    const QString key = this->_options.host + ":" + QString::number(this->_options.port);
    QJsonObject hosts = read_known_hosts();
    QJsonObject entry = hosts.value(key).toObject();

    if (this->_state != State::Verifying) return;
    if (!accepted) {this->disconnectFromHost(); emit this->closed("Host identity refused", false); return;}
    if (remember) {
        // A new identity: the old device secret belongs to the old host
        if (entry.value("fingerprint").toString() != QString::fromLatin1(this->_fingerprint.toHex())) entry = QJsonObject();
        entry["fingerprint"] = QString::fromLatin1(this->_fingerprint.toHex());
        hosts[key] = entry;
        write_known_hosts(hosts);
    }
    this->sendHello_();
}

void xdesk::client::Client::sendHello_(void)
{
    const QString key = this->_options.host + ":" + QString::number(this->_options.port);
    const QJsonObject known = read_known_hosts().value(key).toObject();

    this->_clientNonce = security::random_bytes(32);
    this->setState_(State::Authenticating);
    this->_ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{
        {"t", "hello"}, {"v", XDESK_PROTOCOL_VERSION}, {"kind", net::client_kind_name(this->_options.kind)},
        {"name", this->_options.name}, {"os", QSysInfo::prettyProductName()}, {"device", known.value("device").toString()},
        {"nonce", QString::fromLatin1(this->_clientNonce.toBase64())}, {"enc", QJsonArray{"palette"}},
    }).toJson(QJsonDocument::Compact)));
}

void xdesk::client::Client::sendAuth_(void)
{
    const QString key = this->_options.host + ":" + QString::number(this->_options.port);
    const QJsonObject known = read_known_hosts().value(key).toObject();
    const QByteArray serverNonce = QByteArray::fromBase64(this->_challenge.value("nonce").toString().toLatin1());
    const QByteArray transcript = security::auth_transcript(serverNonce, this->_clientNonce, this->_fingerprint, "client");
    const QByteArray serverTranscript = security::auth_transcript(serverNonce, this->_clientNonce, this->_fingerprint, "server");
    const QByteArray secret = QByteArray::fromHex(known.value("secret").toString().toLatin1());
    QJsonObject auth{{"t", "auth"}, {"remember", this->_options.remember}};
    QByteArray authKey;

    if (!this->_options.code.isEmpty()) {
        // Argon2id of the code: a man in the middle cannot brute force it offline (and the proof is bound to the certificate)
        authKey = security::argon2id(security::normalize_code(this->_options.code).toUtf8(), QByteArray::fromBase64(this->_challenge.value("salt").toString().toLatin1()));
        auth["method"] = "proof";
    } else if (this->_challenge.value("known").toBool() && secret.size() == 32) {
        authKey = secret;
        auth["method"] = "device";
    } else {
        this->disconnectFromHost();
        emit this->closed("This device is not trusted by the host: a code is needed", false);
        return;
    }
    auth["proof"] = QString::fromLatin1(security::hmac_sha256(authKey, transcript).toHex());
    this->setProperty("xdesk_expected_proof", security::hmac_sha256(authKey, serverTranscript));
    this->_ws->sendText(QString::fromUtf8(QJsonDocument(auth).toJson(QJsonDocument::Compact)));
}

void xdesk::client::Client::onText_(const QString& text)
{
    const QJsonObject m = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString type = m.value("t").toString();

    if (type == "challenge" && this->_state == State::Authenticating) {
        this->_challenge = m;
        this->_hostName = m.value("host").toString();
        this->sendAuth_();
    } else if (type == "pending") {
        this->setState_(State::Pending);
        emit this->message("Waiting for the host to accept the connection...");
    } else if (type == "welcome") {
        const QByteArray expected = this->property("xdesk_expected_proof").toByteArray();
        const QString key = this->_options.host + ":" + QString::number(this->_options.port);
        QJsonObject hosts = read_known_hosts();
        QJsonObject entry = hosts.value(key).toObject();

        // Mutual authentication: the host proves it knows the code / the device secret
        if (!security::constant_equal(QByteArray::fromHex(m.value("proof").toString().toLatin1()), expected)) {
            this->disconnectFromHost();
            emit this->closed("The host could not prove it knows the code: connection refused (possible man in the middle)", false);
            return;
        }
        if (m.contains("device")) {
            const QJsonObject device = m.value("device").toObject();
            entry["fingerprint"] = QString::fromLatin1(this->_fingerprint.toHex());
            entry["device"] = device.value("id").toString();
            if (device.contains("secret")) entry["secret"] = device.value("secret").toString();
        }
        entry["name"] = m.value("host").toString();
        entry["last"] = QDateTime::currentMSecsSinceEpoch();
        hosts[key] = entry;
        write_known_hosts(hosts);
        this->_sessionId = m.value("session").toString();
        this->_perms = m.value("perms").toObject();
        this->_hostScreen = QSize(m.value("screen").toObject().value("w").toInt(), m.value("screen").toObject().value("h").toInt());
        this->setState_(State::Active);
        this->_ping.start(1000);
        if (m.contains("udp")) (void)this->_udp.start(this->_ws->peerAddress(), m.value("udp").toObject()); // tcp until the host answers there
        this->requestMode(!this->_options.viewOnly && this->_perms.value("control").toBool() ? "control" : "view");
        emit this->permissionsChanged(this->_perms);
    } else if (type == "denied") {
        this->setState_(State::Closed);
        emit this->closed(m.value("reason").toString("Refused"), false);
    } else if (type == "bye") {
        this->setState_(State::Closed);
        emit this->closed(m.value("reason").toString("Disconnected"), m.value("banned").toBool());
    } else if (type == "perms") {
        this->_perms = m.value("perms").toObject();
        emit this->permissionsChanged(this->_perms);
    } else if (type == "ping") {
        this->_ws->sendText(QString(R"({"t":"pong","ts":%1})").arg(m.value("ts").toInteger()));
    } else if (type == "pong") {
        this->_latency = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - m.value("ts").toInteger());
        this->_udp.setRtt(this->_latency);
        emit this->latencyChanged(this->_latency);
    } else if (type == "term_exit") {
        emit this->terminalClosed(static_cast<std::uint8_t>(m.value("id").toInt()), m.value("code").toInt());
    } else if (type == "info") emit this->message(m.value("text").toString());
    else if (type == "screen") {
        this->_hostScreen = QSize(m.value("w").toInt(), m.value("h").toInt());
        emit this->hostScreenChanged(this->_hostScreen);
    }
}

void xdesk::client::Client::onBinary_(const QByteArray& data)
{
    const net::Binary type = static_cast<net::Binary>(static_cast<std::uint8_t>(data[0]));
    net::ByteReader r(data);

    if (this->_state != State::Active) return;
    switch (type) {
        case net::Binary::Frame: this->decodeFrame_(data); break;
        case net::Binary::Cursor: {
            const int x = r.i16();
            const int y = r.i16();
            const bool visible = r.u8() != 0;
            if (r.ok()) emit this->cursorMoved(QPoint(x, y), visible);
            break;
        }
        case net::Binary::TermData: {
            const std::uint8_t id = r.u8();
            if (r.ok()) emit this->terminalOutput(id, r.rest());
            break;
        }
        default: break;
    }
}

void xdesk::client::Client::decodeFrame_(const QByteArray& data, const bool udp)
{
    net::ByteReader r(data);
    const std::uint32_t id = r.u32();
    const int width = r.u16();
    const int height = r.u16();
    const int count = r.u16();
    QList<QPair<QRect, QByteArray>> jpegs;
    QRect dirty;

    if (!r.ok() || width == 0 || height == 0) return;

    // Ack at once: the host encodes the next frame while this one is decoded (latency over throughput)
    if (!udp) {
        net::ByteWriter ack(net::Binary::FrameAck, 5);
        this->_ws->sendBinary(ack.u32(id).data());
    }
    if (this->_canvas.size() != QSize(width, height)) {
        this->_canvas = QImage(width, height, QImage::Format_RGB32);
        this->_canvas.fill(Qt::black);
        this->_tileColumns = (width + XDESK_TILE_SIZE - 1) / XDESK_TILE_SIZE;
        this->_tiles = QVector<std::uint32_t>(this->_tileColumns * ((height + XDESK_TILE_SIZE - 1) / XDESK_TILE_SIZE), 0);
        dirty = this->_canvas.rect();
    }

    // Tiles of this frame not covered yet by a newer one (udp frames complete in any order), marked as from this frame
    const QRect bounds = this->_canvas.rect();
    const std::function<QList<QRect>(const QRect&)> newer = [this, id, bounds](const QRect& area) {
        QList<QRect> kept;
        const QRect r = area.intersected(bounds);
        for (int y = r.top() / XDESK_TILE_SIZE * XDESK_TILE_SIZE; y <= r.bottom(); y += XDESK_TILE_SIZE)
            for (int x = r.left() / XDESK_TILE_SIZE * XDESK_TILE_SIZE; x <= r.right(); x += XDESK_TILE_SIZE) {
                std::uint32_t& from = this->_tiles[(y / XDESK_TILE_SIZE) * this->_tileColumns + x / XDESK_TILE_SIZE];
                if (from != 0 && static_cast<std::int32_t>(id - from) < 0) continue;
                from = id;
                kept.append(QRect(x, y, XDESK_TILE_SIZE, XDESK_TILE_SIZE).intersected(r));
            }
        return kept;
    };

    // Solid tiles at once, the jpeg ones decoded in parallel (the tiles of a frame never overlap)
    QList<QList<QRect>> keep; // per jpeg: its tiles to draw
    for (int i = 0; i < count && r.ok(); ++i) {
        const QRect tile(r.u16(), r.u16(), r.u16(), r.u16());
        const std::uint8_t encoding = r.u8();
        if (encoding == static_cast<std::uint8_t>(net::TileEncoding::Jpeg)) {
            jpegs.append({tile, r.bytes(static_cast<qsizetype>(r.u32()))});
            keep.append(newer(tile));
        } else if (encoding == static_cast<std::uint8_t>(net::TileEncoding::Palette)) {
            // Lossless palette tile: colors, then one index per pixel (zlib)
            const int colors = r.u8();
            QRgb palette[256] = {};
            for (int c = 0; c < colors; ++c) {
                const int red = r.u8();
                const int green = r.u8();
                const int blue = r.u8();
                palette[c] = qRgb(red, green, blue);
            }
            const QByteArray packed = r.bytes(static_cast<qsizetype>(r.u32()));
            const qsizetype pixels = static_cast<qsizetype>(tile.width()) * tile.height();
            QByteArray sized(4, Qt::Uninitialized);
            sized[0] = static_cast<char>((pixels >> 24) & 0xFF);
            sized[1] = static_cast<char>((pixels >> 16) & 0xFF);
            sized[2] = static_cast<char>((pixels >> 8) & 0xFF);
            sized[3] = static_cast<char>(pixels & 0xFF);
            const QByteArray indices = qUncompress(sized + packed);
            if (indices.size() != pixels || !r.ok()) break; // a broken tile: the frame stops here
            for (const QRect& area: newer(tile))
                for (int y = area.top(); y <= area.bottom(); ++y) {
                    QRgb* line = reinterpret_cast<QRgb*>(this->_canvas.scanLine(y));
                    const uchar* source = reinterpret_cast<const uchar*>(indices.constData()) + static_cast<qsizetype>(y - tile.top()) * tile.width() + (area.left() - tile.left());
                    for (int x = 0; x < area.width(); ++x)
                        line[area.left() + x] = palette[source[x]];
                }
        } else if (encoding == static_cast<std::uint8_t>(net::TileEncoding::Solid)) {
            const int red = r.u8();
            const int green = r.u8();
            const int blue = r.u8();
            const QRgb color = qRgb(red, green, blue);
            for (const QRect& area: newer(tile))
                for (int y = area.top(); y <= area.bottom(); ++y)
                    std::fill_n(reinterpret_cast<QRgb*>(this->_canvas.scanLine(y)) + area.left(), area.width(), color);
        } else break;
        dirty = dirty.united(tile);
    }
    uchar* bits = this->_canvas.bits(); // detached here, before the threads write their own rectangles
    const qsizetype stride = this->_canvas.bytesPerLine();
    platform::parallel_for(static_cast<int>(jpegs.size()), [&jpegs, &keep, bits, stride](const int i) {
        const QImage image = keep[i].isEmpty() ? QImage() : platform::FrameEncoder::decodeJpeg(jpegs[i].second);
        const QPoint origin = jpegs[i].first.topLeft();
        if (image.isNull()) return;
        for (const QRect& tile: keep[i]) {
            const QRect area = tile.intersected(QRect(origin, image.size()));
            for (int y = 0; y < area.height(); ++y)
                std::memcpy(bits + (area.top() + y) * stride + area.left() * 4, image.constScanLine(area.top() - origin.y() + y) + (area.left() - origin.x()) * 4, static_cast<std::size_t>(area.width()) * 4);
        }
    });
    if (!dirty.isEmpty()) emit this->frameUpdated(dirty);
}

void xdesk::client::Client::mouseMove(const int x, const int y)
{
    net::ByteWriter w(net::Binary::MouseMove, 5);

    if (this->_state != State::Active || this->_options.viewOnly) return;
    this->_ws->sendBinary(w.u16(static_cast<std::uint16_t>(std::max(0, x))).u16(static_cast<std::uint16_t>(std::max(0, y))).data());
}

void xdesk::client::Client::mouseButton(const net::MouseButton button, const bool down)
{
    net::ByteWriter w(net::Binary::MouseButton, 3);

    if (this->_state != State::Active || this->_options.viewOnly) return;
    this->_ws->sendBinary(w.u8(static_cast<std::uint8_t>(button)).u8(down ? 1 : 0).data());
}

void xdesk::client::Client::mouseWheel(const int dx, const int dy)
{
    net::ByteWriter w(net::Binary::MouseWheel, 5);

    if (this->_state != State::Active || this->_options.viewOnly) return;
    this->_ws->sendBinary(w.i16(static_cast<std::int16_t>(dx)).i16(static_cast<std::int16_t>(dy)).data());
}

void xdesk::client::Client::key(const QString& code, const bool down)
{
    const QByteArray name = code.toLatin1().left(32);
    net::ByteWriter w(net::Binary::Key, 3 + static_cast<std::size_t>(name.size()));

    if (this->_state != State::Active || this->_options.viewOnly || name.isEmpty()) return;
    this->_ws->sendBinary(w.u8(down ? 1 : 0).u8(static_cast<std::uint8_t>(name.size())).bytes(name).data());
}

void xdesk::client::Client::text(const QString& text)
{
    net::ByteWriter w(net::Binary::Text, 64);

    if (this->_state != State::Active || this->_options.viewOnly || text.isEmpty()) return;
    this->_ws->sendBinary(w.bytes(text.toUtf8()).data());
}

void xdesk::client::Client::setQuality(const int quality, const double scale, const int fps)
{
    if (this->_ws && this->_state == State::Active)
        this->_ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{{"t", "quality"}, {"quality", quality}, {"scale", scale}, {"fps", fps}}).toJson(QJsonDocument::Compact)));
}

void xdesk::client::Client::requestMode(const QString& mode)
{
    if (this->_ws && this->_state == State::Active)
        this->_ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{{"t", "mode"}, {"mode", mode}}).toJson(QJsonDocument::Compact)));
}

void xdesk::client::Client::setPaused(const bool paused)
{
    if (this->_ws && this->_state == State::Active)
        this->_ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{{"t", "pause"}, {"paused", paused}}).toJson(QJsonDocument::Compact)));
}

std::uint8_t xdesk::client::Client::openTerminal(const int cols, const int rows)
{
    std::uint8_t id = 1;

    while (id < 255 && this->_openTerminals.contains(id))
        ++id;
    this->_openTerminals.insert(id);
    if (this->_ws && this->_state == State::Active)
        this->_ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{{"t", "term_open"}, {"id", id}, {"cols", cols}, {"rows", rows}}).toJson(QJsonDocument::Compact)));
    return id;
}

void xdesk::client::Client::terminalInput(const std::uint8_t id, const QByteArray& data)
{
    net::ByteWriter w(net::Binary::TermData, static_cast<std::size_t>(data.size()) + 2);

    if (this->_ws && this->_state == State::Active) this->_ws->sendBinary(w.u8(id).bytes(data).data());
}

void xdesk::client::Client::resizeTerminal(const std::uint8_t id, const int cols, const int rows)
{
    if (this->_ws && this->_state == State::Active)
        this->_ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{{"t", "term_resize"}, {"id", id}, {"cols", cols}, {"rows", rows}}).toJson(QJsonDocument::Compact)));
}

void xdesk::client::Client::closeTerminal(const std::uint8_t id)
{
    this->_openTerminals.remove(id);
    if (this->_ws && this->_state == State::Active)
        this->_ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{{"t", "term_close"}, {"id", id}}).toJson(QJsonDocument::Compact)));
}
