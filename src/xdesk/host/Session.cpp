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
##  @file Session.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/Crypto.hpp"
#include "xdesk/host/Session.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QString>

xdesk::host::Session::Session(net::WebSocket* ws, const QString& userAgent, QObject* parent)
: QObject(parent), _ws{ws}, _sid{security::random_id(8)}, _userAgent{userAgent}, _since{QDateTime::currentMSecsSinceEpoch()}
{
    this->_ws->setParent(this);
    this->_address = ws->peerAddress().toString();
    if (this->_address.startsWith("::ffff:")) this->_address = this->_address.mid(7);
    if (ws->socket() && !ws->socket()->property("xdesk_forwarded").toString().isEmpty()) this->_address = ws->socket()->property("xdesk_forwarded").toString(); // cloudflared
    QObject::connect(this->_ws, &net::WebSocket::textReceived, this, &Session::onText_);
    QObject::connect(this->_ws, &net::WebSocket::binaryReceived, this, &Session::onBinary_);
    QObject::connect(this->_ws, &net::WebSocket::closed, this, [this]() {
        if (this->_state == State::Closed) return;
        this->_state = State::Closed;
        emit this->closed(this);
    });
    QObject::connect(&this->_tick, &QTimer::timeout, this, &Session::onTick_);
    this->_tick.start(1000);

    // 15 s to authenticate
    QTimer::singleShot(15000, this, [this]() {
        if (this->_state == State::Handshake) this->deny("Authentication timeout");
    });
}

xdesk::host::Session::~Session()
{
    if (this->_ws) this->_ws->disconnect(this);
}

void xdesk::host::Session::onTick_(void)
{
    this->_stats.fps = this->_secondFrames;
    this->_stats.kbps = static_cast<int>(this->_secondBytes * 8 / 1000);
    this->_secondFrames = 0;
    this->_secondBytes = 0;
    if (this->_state == State::Active) {
        this->sendJson({{"t", "ping"}, {"ts", QDateTime::currentMSecsSinceEpoch()}});
        emit this->changed(this);
    }
}

void xdesk::host::Session::onText_(const QString& text)
{
    const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
    const QString type = message.value("t").toString();

    this->_stats.bytesIn += text.size();
    if (type.isEmpty()) {this->kick("Malformed message"); return;}
    switch (this->_state) {
        case State::Handshake:
            if (type == "hello" && this->_serverNonce.isEmpty()) {
                this->_kind = net::client_kind_from(message.value("kind").toString());
                this->_name = message.value("name").toString("[None]").left(80);
                this->_os = message.value("os").toString("[None]").left(80);
                for (const QJsonValue& encoding: message.value("enc").toArray())
                    if (this->_encodings.size() < 8) this->_encodings.append(encoding.toString().left(16));
                this->_clientNonce = QByteArray::fromBase64(message.value("nonce").toString().toLatin1());
                if (this->_clientNonce.size() != 32) {this->deny("Invalid nonce"); return;}
                this->_serverNonce = security::random_bytes(32);
                emit this->hello(this, message);
            } else if (type == "auth" && !this->_serverNonce.isEmpty()) emit this->auth(this, message);
            else this->deny("Unexpected message during the handshake");
            break;
        case State::Pending: break; // nothing accepted while the host decides
        case State::Active:
            if (type == "pong") {
                this->_stats.latency = static_cast<int>(QDateTime::currentMSecsSinceEpoch() - message.value("ts").toInteger());
            } else if (type == "ping") this->sendJson({{"t", "pong"}, {"ts", message.value("ts")}});
            else emit this->control(this, message);
            break;
        case State::Closed: break;
    }
}

void xdesk::host::Session::onBinary_(const QByteArray& data)
{
    this->_stats.bytesIn += data.size();
    if (this->_state != State::Active || data.isEmpty()) return;
    emit this->input(this, data);
}

void xdesk::host::Session::sendJson(const QJsonObject& object)
{
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);

    this->_stats.bytesOut += data.size();
    this->_secondBytes += data.size();
    this->_ws->sendText(QString::fromUtf8(data));
}

void xdesk::host::Session::sendBinary(const QByteArray& data, const bool frame)
{
    this->_stats.bytesOut += data.size();
    this->_secondBytes += data.size();
    if (frame) {this->_stats.frames++; this->_secondFrames++;}
    this->_ws->sendBinary(data);
}

void xdesk::host::Session::countFrame(const qint64 bytes)
{
    this->_stats.bytesOut += bytes;
    this->_secondBytes += bytes;
    this->_stats.frames++;
    this->_secondFrames++;
}

void xdesk::host::Session::sendChallenge(const QString& hostName, const QByteArray& salt, const bool knownDevice)
{
    this->sendJson({
        {"t", "challenge"}, {"v", XDESK_PROTOCOL_VERSION}, {"host", hostName}, {"version", XDESK_VERSION},
        {"nonce", QString::fromLatin1(this->_serverNonce.toBase64())}, {"salt", QString::fromLatin1(salt.toBase64())},
        {"kdf", QJsonObject{{"alg", "argon2id"}, {"m", XDESK_ARGON2_MEMORY}, {"t", XDESK_ARGON2_ITERATIONS}, {"p", XDESK_ARGON2_LANES}}},
        {"known", knownDevice},
    });
}

static QJsonObject perms_json(const xdesk::host::Permissions& p)
{
    return {{"view", p.view}, {"control", p.control}, {"terminal", p.terminal}, {"clipboard", p.clipboard}};
}

void xdesk::host::Session::accept(const Permissions& perms, const QJsonObject& extra)
{
    QJsonObject welcome = extra;

    this->_perms = perms;
    this->_state = State::Active;
    this->_mode = perms.control ? "control" : "view";
    welcome["t"] = "welcome";
    welcome["session"] = this->_sid;
    welcome["perms"] = perms_json(perms);
    this->sendJson(welcome);
    emit this->changed(this);
}

void xdesk::host::Session::deny(const QString& reason)
{
    if (this->_state == State::Closed) return;
    this->sendJson({{"t", "denied"}, {"reason", reason}});
    this->_ws->close(1008, reason);
}

void xdesk::host::Session::kick(const QString& reason, const bool banned)
{
    if (this->_state == State::Closed) return;
    this->sendJson({{"t", "bye"}, {"reason", reason}, {"banned", banned}});
    this->_ws->close(banned ? 1008 : 1000, reason);
}

void xdesk::host::Session::setPermissions(const Permissions& perms)
{
    this->_perms = perms;
    if (!perms.control && this->_mode == "control") this->_mode = "view";
    this->sendJson({{"t", "perms"}, {"perms", perms_json(perms)}});
    emit this->changed(this);
}

void xdesk::host::Session::setState(const State state)
{
    this->_state = state;
    if (state == State::Pending) this->sendJson({{"t", "pending"}});
    emit this->changed(this);
}

void xdesk::host::Session::setActivity(const QString& activity)
{
    this->_lastInput = QDateTime::currentMSecsSinceEpoch();
    this->_lastActivity = activity;
    this->_stats.inputs++;
    emit this->activity(this, activity);
}

QJsonObject xdesk::host::Session::describe(void) const
{
    static const char* STATES[] = {"handshake", "pending", "active", "closed"};

    return {
        {"id", this->_sid}, {"state", STATES[static_cast<int>(this->_state)]}, {"kind", net::client_kind_name(this->_kind)},
        {"name", this->_name}, {"os", this->_os}, {"address", this->_address}, {"device", this->_deviceId},
        {"mode", this->_mode}, {"perms", perms_json(this->_perms)}, {"since", this->_since}, {"lastInput", this->_lastInput},
        {"fps", this->_stats.fps}, {"latency", this->_stats.latency}, {"kbps", this->_stats.kbps},
        {"bytesIn", this->_stats.bytesIn}, {"bytesOut", this->_stats.bytesOut}, {"inputs", this->_stats.inputs},
        {"activity", this->_lastActivity}, {"userAgent", this->_userAgent},
    };
}

qint64 xdesk::host::Session::pendingBytes(void) const
{
    return this->_ws->pendingBytes();
}
