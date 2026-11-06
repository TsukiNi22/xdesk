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
##  @file HostTui.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/Crypto.hpp"
#include "xdesk/ui/tty/HostTui.hpp"
#include "xdesk/net/Protocol.hpp"
#include "xdesk/core/Config.hpp"
#include <QJsonDocument>
#include <QHostInfo>
#include <QDateTime>
#include <algorithm>
#include <QVariant>
#include <QUrl>

//----------------------------------------------------------------//
/* DEFINE */

#define FLASH_TIME 6000 // ms a message of the status line stays
#define MAX_LOGS 3000 // entries kept in memory

//----------------------------------------------------------------//
/* ENUM */

enum class FieldKind {
    Number,
    Text,
    Toggle,
    Choice,
};

//----------------------------------------------------------------//
/* STRUCT */

// One row of the settings page
struct SettingField {
    QString section;
    QString label;
    FieldKind kind = FieldKind::Number;
    int min = 0;
    int max = 0;
    int step = 1;
    QString suffix = "";
    QList<QPair<QString, QVariant>> choices; // <label, value>
    std::function<QVariant(const xdesk::core::Settings&)> get;
    std::function<void(xdesk::core::Settings&, const QVariant&)> set;
};

//----------------------------------------------------------------//
/* TOOLS */

// Palette of the panel (the green of the graphical one)
static const QColor ACCENT(61, 220, 151);
static const QColor MUTED(128, 136, 150);
static const QColor GOOD(115, 201, 145);
static const QColor WARN(230, 190, 100);
static const QColor BAD(240, 110, 110);
static const QColor BAR(26, 30, 36);
static const QColor SELECTED(34, 66, 54);
static const QColor DARK(12, 14, 16);
static const QColor BORDER(58, 66, 74);
static const QColor BORDER_TEXT(100, 108, 118);

static const QStringList PAGE_NAMES = {"Overview", "Sessions", "Devices", "Logs", "Settings"};
static const QStringList LOG_LEVELS = {"all", "info", "warn", "alert"};
static const QStringList LOG_CATEGORIES = {"all", "server", "auth", "session", "input", "terminal", "tamper", "device", "settings"};

static xdesk::ui::tty::Style st(const QColor& fg = QColor(), const QColor& bg = QColor(), const bool bold = false)
{
    xdesk::ui::tty::Style style;

    style.fg = fg;
    style.bg = bg;
    style.bold = bold;
    return style;
}

static QString ago(const qint64 ms)
{
    const qint64 s = std::max<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - ms) / 1000);

    if (s < 60) return QString::number(s) + "s";
    if (s < 3600) return QString::number(s / 60) + "m" + QString::number(s % 60).rightJustified(2, '0');
    return QString::number(s / 3600) + "h" + QString::number((s / 60) % 60).rightJustified(2, '0');
}

static QString bytes_text(const qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024) + " KB";
    return QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1) + " MB";
}

static const QList<SettingField>& settings_fields(void)
{
    using S = xdesk::core::Settings;
    static const QList<SettingField> FIELDS = {
        {"Network", "Port", FieldKind::Number, 1, 65535, 1, "", {}, [](const S& s) {return QVariant(s.port);}, [](S& s, const QVariant& v) {s.port = static_cast<std::uint16_t>(v.toInt());}},
        {"Network", "Listen on", FieldKind::Text, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.bind);}, [](S& s, const QVariant& v) {s.bind = v.toString().trimmed();}},
        {"Network", "Exposure", FieldKind::Choice, 0, 0, 0, "", {{"LAN only", "lan"}, {"LAN + Tailscale", "tailscale"}, {"LAN + internet (UPnP router port)", "upnp"}, {"LAN + internet (bore.pub tunnel)", "bore"}, {"LAN + internet (Cloudflare tunnel)", "cloudflared"}, {"LAN + internet (ngrok tunnel)", "ngrok"}, {"LAN + internet (port forwarded by hand)", "manual"}},
            [](const S& s) {return QVariant(xdesk::core::exposure_name(s.exposure));}, [](S& s, const QVariant& v) {s.exposure = xdesk::core::exposure_from(v.toString());}},
        {"Network", "Public address", FieldKind::Text, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.publicHost);}, [](S& s, const QVariant& v) {s.publicHost = v.toString().trimmed();}},
        {"Network", "Announce on the LAN", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.discovery);}, [](S& s, const QVariant& v) {s.discovery = v.toBool();}},
        {"Network", "Video over UDP (native, app)", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.udp);}, [](S& s, const QVariant& v) {s.udp = v.toBool();}},
        {"Network", "Extra sockets (web viewers)", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.lanes);}, [](S& s, const QVariant& v) {s.lanes = v.toBool();}},
        {"Stream", "Max FPS", FieldKind::Number, 1, 120, 5, " fps", {}, [](const S& s) {return QVariant(s.fps);}, [](S& s, const QVariant& v) {s.fps = v.toInt();}},
        {"Stream", "Resolution", FieldKind::Choice, 0, 0, 0, "", {{"Screen size (native)", 0}, {"1440p", 1440}, {"1080p - Full HD", 1080}, {"720p - HD", 720}, {"480p", 480}, {"360p", 360}, {"240p", 240}, {"144p", 144}},
            [](const S& s) {return QVariant(s.maxHeight);}, [](S& s, const QVariant& v) {s.maxHeight = v.toInt();}},
        {"Stream", "Automatic resolution (slow links)", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.autoResolution);}, [](S& s, const QVariant& v) {s.autoResolution = v.toBool();}},
        {"Stream", "Image quality", FieldKind::Choice, 0, 0, 0, "", {{"Low - 40", 40}, {"Medium - 55", 55}, {"High - 70 (default)", 70}, {"Very high - 85", 85}, {"Maximum - 100", 100}},
            [](const S& s) {return QVariant(s.quality);}, [](S& s, const QVariant& v) {s.quality = v.toInt();}},
        {"Stream", "Language", FieldKind::Choice, 0, 0, 0, "", {{"en", "en"}, {"fr", "fr"}}, [](const S& s) {return QVariant(s.language);}, [](S& s, const QVariant& v) {s.language = v.toString();}},
        {"Stream", "Start in the tray (gui)", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.startMinimized);}, [](S& s, const QVariant& v) {s.startMinimized = v.toBool();}},
        {"Access & security", "Code length", FieldKind::Number, 6, 24, 1, "", {}, [](const S& s) {return QVariant(s.codeLength);}, [](S& s, const QVariant& v) {s.codeLength = v.toInt();}},
        {"Access & security", "Code lifetime", FieldKind::Number, 0, 86400, 60, " s", {}, [](const S& s) {return QVariant(s.codeLifetime);}, [](S& s, const QVariant& v) {s.codeLifetime = v.toInt();}},
        {"Access & security", "Ask before accepting", FieldKind::Choice, 0, 0, 0, "", {{"untrusted", "untrusted"}, {"always", "always"}, {"never", "never"}},
            [](const S& s) {return QVariant(xdesk::core::approval_name(s.approval));}, [](S& s, const QVariant& v) {s.approval = xdesk::core::approval_from(v.toString());}},
        {"Access & security", "Max sessions", FieldKind::Number, 1, 64, 1, "", {}, [](const S& s) {return QVariant(s.maxSessions);}, [](S& s, const QVariant& v) {s.maxSessions = v.toInt();}},
        {"Access & security", "Failures before ban", FieldKind::Number, 1, 100, 1, "", {}, [](const S& s) {return QVariant(s.maxAuthFailures);}, [](S& s, const QVariant& v) {s.maxAuthFailures = v.toInt();}},
        {"Access & security", "Idle kick", FieldKind::Number, 0, 86400, 60, " s", {}, [](const S& s) {return QVariant(s.idleTimeout);}, [](S& s, const QVariant& v) {s.idleTimeout = v.toInt();}},
        {"Access & security", "Allow keyboard & mouse control", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.allowControl);}, [](S& s, const QVariant& v) {s.allowControl = v.toBool();}},
        {"Access & security", "Allow remote terminals", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.allowTerminal);}, [](S& s, const QVariant& v) {s.allowTerminal = v.toBool();}},
        {"Access & security", "Allow browsers and the android app", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.allowWeb);}, [](S& s, const QVariant& v) {s.allowWeb = v.toBool();}},
        {"Access & security", "New sessions start view only", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.viewOnlyDefault);}, [](S& s, const QVariant& v) {s.viewOnlyDefault = v.toBool();}},
        {"Access & security", "Automatic bans (tamper, unclean stop, wrong codes)", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.banOnTamper);}, [](S& s, const QVariant& v) {s.banOnTamper = v.toBool();}},
        {"Access & security", "Viewers may ask the control back", FieldKind::Toggle, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.focusRequest);}, [](S& s, const QVariant& v) {s.focusRequest = v.toBool();}},
        {"Access & security", "Control given back after", FieldKind::Number, 5, 3600, 5, " s", {}, [](const S& s) {return QVariant(s.focusRequestDelay);}, [](S& s, const QVariant& v) {s.focusRequestDelay = v.toInt();}},
        {"Access & security", "Terminal shell", FieldKind::Text, 0, 0, 0, "", {}, [](const S& s) {return QVariant(s.shell);}, [](S& s, const QVariant& v) {s.shell = v.toString().trimmed();}},
    };

    return FIELDS;
}

// Words of a text in lines of at most width cells
static QStringList wrap(const QString& text, const int width)
{
    QStringList lines;
    QString line;

    for (const QString& word: text.split(' ', Qt::SkipEmptyParts)) {
        if (!line.isEmpty() && line.size() + 1 + word.size() > width) {lines.append(line); line.clear();}
        line += (line.isEmpty() ? "" : " ") + word;
    }
    if (!line.isEmpty()) lines.append(line);
    return lines;
}

// Text of a setting value
static QString field_value(const SettingField& field, const xdesk::core::Settings& settings)
{
    const QVariant value = field.get(settings);

    switch (field.kind) {
        case FieldKind::Toggle: return value.toBool() ? "● on" : "○ off";
        case FieldKind::Number: return "◂ " + QString::number(value.toInt()) + field.suffix + " ▸";
        case FieldKind::Text:   return value.toString();
        case FieldKind::Choice:
            for (const QPair<QString, QVariant>& choice: field.choices)
                if (choice.second == value) return "◂ " + choice.first + " ▸";
            return "◂ " + value.toString() + " (config.json) ▸";
    }
    return "";
}

//----------------------------------------------------------------//
/* CLASS */

xdesk::ui::tty::HostTui::HostTui(host::Host& host, QObject* parent)
: QObject(parent), _host{host}
{
    this->_draft = host.config().settings();
    this->_term.enableRaw();
    this->_term.enterAlternate();
    this->_term.setMouse(true);
    QObject::connect(&this->_term, &Terminal::input, this, &HostTui::onInput_);
    QObject::connect(&this->_term, &Terminal::resized, this, &HostTui::draw_);
    QObject::connect(&this->_redraw, &QTimer::timeout, this, &HostTui::draw_);
    QObject::connect(&host, &host::Host::stateChanged, this, &HostTui::draw_);
    QObject::connect(&host, &host::Host::focusRequestChanged, this, &HostTui::draw_);
    QObject::connect(&host, &host::Host::sessionChanged, this, &HostTui::draw_);
    QObject::connect(&host, &host::Host::sessionRemoved, this, [this](const QString& id) {
        for (const QString& key: this->mirrorsOf_(id))
            delete this->_mirrors.take(key);
        if (this->_mirror.startsWith(id + "/")) {this->_mirror.clear(); if (this->_modal == Modal::Mirror) this->_modal = Modal::None;}
        if (this->_approval == id) this->_approval.clear();
        this->draw_();
    });
    QObject::connect(&host, &host::Host::approvalRequested, this, [this](host::Session* s) {
        if (this->_approval.isEmpty()) this->_approval = s->id();
        this->_term.write("\x07"); // bell: someone waits at the door
        this->draw_();
    });
    QObject::connect(&host, &host::Host::activity, this, [this](const QString& session, const QString& line) {
        QStringList& lines = this->_activity[session];
        lines.append(QDateTime::currentDateTime().toString("HH:mm:ss ") + line);
        if (lines.size() > 200) lines.removeFirst();
    });
    QObject::connect(&host, &host::Host::terminalOutput, this, [this](const QString& session, const std::uint8_t id, const QByteArray& data) {
        this->mirror_(session, id)->feed(data);
    });
    QObject::connect(&host, &host::Host::terminalInput, this, [this](const QString& session, const std::uint8_t id, const QByteArray& data) {
        const QString key = session + "/" + QString::number(id);
        for (const char c: data) {
            if (c == '\r' || c == '\n') {
                if (!this->_typed[key].trimmed().isEmpty()) {
                    QStringList& lines = this->_activity[session];
                    lines.append(QDateTime::currentDateTime().toString("HH:mm:ss ") + "terminal " + QString::number(id) + " $ " + QString::fromUtf8(this->_typed[key]));
                    if (lines.size() > 200) lines.removeFirst();
                }
                this->_typed[key].clear();
            } else if (c == 0x7F || c == 0x08) this->_typed[key].chop(1);
            else if (static_cast<unsigned char>(c) >= 0x20) this->_typed[key].append(c);
        }
    });
    QObject::connect(&host, &host::Host::tamperDetected, this, [this](const QString&, const QString& reason) {
        this->_tamper = reason;
        this->_term.write("\x07");
        this->draw_();
    });
    QObject::connect(&host.log(), &core::AuditLog::entry, this, &HostTui::addLog_);
    this->_logs = host.log().recent(500);
    this->_redraw.start(250);
    this->draw_();
}

//----------------------------------------------------------------//
// Model helpers

QList<xdesk::host::Session*> xdesk::ui::tty::HostTui::ordered_(void) const
{
    QList<host::Session*> sessions = this->_host.sessions();

    std::sort(sessions.begin(), sessions.end(), [](const host::Session* a, const host::Session* b) {return a->since() < b->since();});
    return sessions;
}

xdesk::host::Session* xdesk::ui::tty::HostTui::currentSession_(void) const
{
    const QList<host::Session*> sessions = this->ordered_();

    return sessions.isEmpty() ? nullptr : sessions.value(std::clamp(this->_session, 0, static_cast<int>(sessions.size()) - 1));
}

QList<QJsonObject> xdesk::ui::tty::HostTui::filteredLogs_(void) const
{
    QList<QJsonObject> out;

    for (const QJsonObject& e: this->_logs) {
        if (this->_logLevel > 0 && e.value("level").toString() != LOG_LEVELS[this->_logLevel]) continue;
        if (this->_logCategory > 0 && e.value("cat").toString() != LOG_CATEGORIES[this->_logCategory]) continue;
        if (!this->_logSearch.isEmpty() && !QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Compact)).contains(this->_logSearch, Qt::CaseInsensitive)) continue;
        out.append(e);
    }
    return out;
}

xdesk::ui::gui::VtScreen* xdesk::ui::tty::HostTui::mirror_(const QString& session, const std::uint8_t id)
{
    const QString key = session + "/" + QString::number(id);
    gui::VtScreen* vt = this->_mirrors.value(key);
    const QSize size = this->_term.size();

    if (vt) return vt;
    vt = new gui::VtScreen(std::max(20, size.width() - 4), std::max(5, size.height() - 6), this);
    this->_mirrors.insert(key, vt);
    return vt;
}

QStringList xdesk::ui::tty::HostTui::mirrorsOf_(const QString& session) const
{
    QStringList keys;

    for (QHash<QString, gui::VtScreen*>::const_iterator it = this->_mirrors.cbegin(); it != this->_mirrors.cend(); ++it)
        if (it.key().startsWith(session + "/")) keys.append(it.key());
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) {return a.section('/', 1).toInt() < b.section('/', 1).toInt();});
    return keys;
}

void xdesk::ui::tty::HostTui::addLog_(const QJsonObject& entry)
{
    this->_logs.append(entry);
    while (this->_logs.size() > MAX_LOGS)
        this->_logs.removeFirst();
}

bool xdesk::ui::tty::HostTui::locked_(void)
{
    if (!this->_host.localActionsLocked()) return false;
    this->flash_("Locked: a remote session controls this PC or has a terminal (stop it first)", true);
    return true;
}

void xdesk::ui::tty::HostTui::flash_(const QString& text, const bool bad)
{
    this->_flash = text;
    this->_flashBad = bad;
    this->_flashAt = QDateTime::currentMSecsSinceEpoch();
}

void xdesk::ui::tty::HostTui::copy_(const QString& text, const QString& what)
{
    this->_term.write(osc52(text));
    this->flash_(what + " copied (clipboard of this terminal, shift + mouse selection works too)");
}

//----------------------------------------------------------------//
// Modals

void xdesk::ui::tty::HostTui::ask_(const QString& title, const QString& initial, const std::function<void(const QString&)>& done)
{
    this->_modal = Modal::Input;
    this->_modalTitle = title;
    this->_input = initial;
    this->_inputDone = done;
}

void xdesk::ui::tty::HostTui::confirm_(const QString& question, const QString& word, const std::function<void(void)>& action)
{
    this->_modal = Modal::Confirm;
    this->_modalTitle = question;
    this->_confirmWord = word;
    this->_input.clear();
    this->_confirmAction = action;
}

void xdesk::ui::tty::HostTui::showText_(const QString& title, const QStringList& lines)
{
    this->_modal = Modal::Text;
    this->_modalTitle = title;
    this->_modalLines = lines;
    this->_modalScroll = 0;
}

void xdesk::ui::tty::HostTui::showQr_(void)
{
    const QList<net::Endpoint> endpoints = this->_host.exposure().endpoints();
    const net::Endpoint e = endpoints.value(std::clamp(this->_endpoint, 0, std::max(0, static_cast<int>(endpoints.size()) - 1)));
    const QUrl url(e.url);
    const QString code = security::normalize_code(this->_host.code().display());
    QString host;
    QString hostPort;
    QString text;
    QStringList lines;

    if (!this->_host.isSharing()) {this->flash_("Start sharing first (s): the QR code holds an address and the code", true); return;}
    if (endpoints.isEmpty()) {this->flash_("No address to share yet (no network for the chosen exposure)", true); return;}
    // The app always gets the numeric address (pinned certificate), the browser the name (trusted certificate)
    host = this->_qrApp && !e.ip.isEmpty() ? e.ip : url.host();
    hostPort = (host.contains(':') ? "[" + host + "]" : host) + ":" + QString::number(url.port(e.port));
    text = this->_qrApp ? "xdesk://" + hostPort + "?fp=" + QString::fromLatin1(this->_host.certificate().fingerprint().toHex()) + "&code=" + code : "https://" + hostPort + "/?code=" + code;
    lines = qr_lines(text, this->_term.caps().unicode);
    lines.append("");
    lines.append(text.section('?', 0, 0) + "  ·  code " + this->_host.code().display());
    lines.append(this->_qrApp ? "xdesk app (Android): the camera opens the app, the identity is pinned" : "Browser: the web client opens with the code filled in");
    lines.append("tab: app / browser link   c: copy the link   esc: close");
    this->showText_(QString("QR code - ") + e.label + (this->_qrApp ? " (app)" : " (browser)"), lines);
    this->_modalLines.append(text); // last line: the link itself (copied with c, not drawn)
}

//----------------------------------------------------------------//
// Actions

void xdesk::ui::tty::HostTui::toggleSharing_(void)
{
    try {
        this->_host.setSharing(!this->_host.isSharing());
        this->flash_(this->_host.isSharing() ? "Sharing on: give the code and an address" : "Sharing off: nobody can connect");
    } catch (const std::exception& e) {
        this->flash_(QString("Sharing could not start: ") + e.what(), true);
    }
}

void xdesk::ui::tty::HostTui::setExposure_(const int delta)
{
    const SettingField& field = settings_fields()[2];
    core::Settings s = this->_host.config().settings();
    int index = 0;

    if (this->locked_()) return;
    for (int i = 0; i < field.choices.size(); ++i)
        if (field.choices[i].second == field.get(s)) index = i;
    index = (index + delta + static_cast<int>(field.choices.size())) % static_cast<int>(field.choices.size());
    field.set(s, field.choices[index].second);
    try {
        if (!this->_host.applySettings(s)) {this->flash_("Locked: settings unchanged", true); return;}
    } catch (const std::exception& e) {
        this->flash_(QString("Exposure not changed: ") + e.what(), true);
        return;
    }
    field.set(this->_draft, field.choices[index].second);
    this->flash_("Open to: " + field.choices[index].first);
}

void xdesk::ui::tty::HostTui::saveSettings_(void)
{
    if (this->locked_()) return;
    try {
        if (!this->_host.applySettings(this->_draft)) {this->flash_("Locked: settings unchanged", true); return;}
    } catch (const std::exception& e) {
        this->flash_(QString("Settings not applied: ") + e.what(), true);
        return;
    }
    this->_draft = this->_host.config().settings();
    this->flash_("Settings saved");
}

void xdesk::ui::tty::HostTui::changeField_(const int delta, const bool edit)
{
    const QList<SettingField>& fields = settings_fields();
    const SettingField& field = fields[std::clamp(this->_field, 0, static_cast<int>(fields.size()) - 1)];
    const QVariant value = field.get(this->_draft);

    switch (field.kind) {
        case FieldKind::Toggle: field.set(this->_draft, !value.toBool()); break;
        case FieldKind::Choice: {
            int index = -1;
            for (int i = 0; i < field.choices.size(); ++i)
                if (field.choices[i].second == value) index = i;
            index = (index + (delta == 0 ? 1 : delta) + static_cast<int>(field.choices.size())) % static_cast<int>(field.choices.size());
            field.set(this->_draft, field.choices[index].second);
            break;
        }
        case FieldKind::Number:
            if (edit) this->ask_(field.label + " (" + QString::number(field.min) + " - " + QString::number(field.max) + ")", QString::number(value.toInt()), [this, field](const QString& text) {
                bool ok = false;
                const int number = text.trimmed().toInt(&ok);
                if (!ok) {this->flash_("Not a number: unchanged", true); return;}
                field.set(this->_draft, std::clamp(number, field.min, field.max));
            });
            else field.set(this->_draft, std::clamp(value.toInt() + delta * field.step, field.min, field.max));
            break;
        case FieldKind::Text:
            this->ask_(field.label, value.toString(), [this, field](const QString& text) {field.set(this->_draft, text);});
            break;
    }
}

void xdesk::ui::tty::HostTui::move_(const int delta)
{
    const int sessions = static_cast<int>(this->_host.sessions().size());
    const int devices = static_cast<int>(this->_host.registry().devices().size() + this->_host.registry().addresses().size());
    const int logs = static_cast<int>(this->filteredLogs_().size());

    switch (this->_page) {
        case Page::Overview: this->_endpoint = std::clamp(this->_endpoint + delta, 0, std::max(0, static_cast<int>(this->_host.exposure().endpoints().size()) - 1)); break;
        case Page::Sessions: this->_session = std::clamp(this->_session + delta, 0, std::max(0, sessions - 1)); break;
        case Page::Devices:  this->_device = std::clamp(this->_device + delta, 0, std::max(0, devices - 1)); break;
        case Page::Settings: this->_field = std::clamp(this->_field + delta, 0, static_cast<int>(settings_fields().size()) - 1); break;
        case Page::Logs:
            if (logs == 0) break;
            this->_logRow = std::clamp((this->_logRow < 0 ? logs - 1 : this->_logRow) + delta, 0, logs - 1);
            if (this->_logRow == logs - 1 && delta > 0) this->_logRow = -1; // back to the newest: follow
            break;
    }
}

//----------------------------------------------------------------//
// Input

void xdesk::ui::tty::HostTui::onInput_(const QByteArray& data)
{
    // Focus of the terminal of the panel: the remote input waits while it is used here (not for a panel used over ssh)
    static const bool local = qEnvironmentVariableIsEmpty("SSH_CONNECTION") && qEnvironmentVariableIsEmpty("SSH_TTY");

    for (const QByteArray& key: split_keys(data)) {
        Mouse mouse;
        const QString name = key_name(key);
        if (name == "focus-in" || name == "focus-out") {
            if (local) this->_host.setPanelFocused(name == "focus-in");
            continue;
        }
        if (parse_mouse(key, mouse)) {
            if (mouse.press) this->_host.localPanelInput(); // someone in front of the PC: a control given back by a request waits again
            this->onMouse_(mouse);
        } else {
            this->_host.localPanelInput();
            this->onKey_(name, key);
        }
    }
    this->draw_();
}

void xdesk::ui::tty::HostTui::onMouse_(const Mouse& mouse)
{
    if (mouse.motion) return;
    if (mouse.wheel != 0) {
        if (this->_modal == Modal::Mirror) this->_mirrorScroll = std::max(0, this->_mirrorScroll + mouse.wheel * 3);
        else if (this->_modal == Modal::Text) this->_modalScroll = std::max(0, this->_modalScroll - mouse.wheel * 3);
        else if (this->_modal == Modal::None && this->_approval.isEmpty()) this->move_(-mouse.wheel);
        return;
    }
    if (!mouse.press || mouse.button != 0) return;
    for (qsizetype i = this->_hits.size() - 1; i >= 0; --i) {
        if (!this->_hits[i].area.contains(mouse.col, mouse.row)) continue;
        const std::function<void(void)> action = this->_hits[i].action; // the hits are rebuilt by the action
        action();
        return;
    }
}

void xdesk::ui::tty::HostTui::onKey_(const QString& key, const QByteArray& raw)
{
    // Someone at the door: answered first
    if (!this->_approval.isEmpty()) {
        host::Session* s = this->_host.session(this->_approval);
        if (!s || s->state() != host::State::Pending) this->_approval.clear();
        else if (key == "a") {this->_host.approve(this->_approval, true, false); this->_approval.clear();}
        else if (key == "t" || key == "A") {this->_host.approve(this->_approval, true, true); this->_approval.clear();}
        else if (key == "r" || key == "esc") {this->_host.approve(this->_approval, false); this->_approval.clear();}
        else return;
        // Next one waiting
        for (host::Session* other: this->ordered_())
            if (other->state() == host::State::Pending && this->_approval.isEmpty()) this->_approval = other->id();
        return;
    }

    // Modals
    switch (this->_modal) {
        case Modal::Help:
            this->_modal = Modal::None;
            return;
        case Modal::Text:
            if (key == "esc" || key == "q" || key == "enter") this->_modal = Modal::None;
            else if (key == "up") this->_modalScroll = std::max(0, this->_modalScroll - 1);
            else if (key == "down") this->_modalScroll++;
            else if (key == "pgup") this->_modalScroll = std::max(0, this->_modalScroll - 10);
            else if (key == "pgdn") this->_modalScroll += 10;
            else if (this->_modalTitle.startsWith("QR code") && (key == "tab" || key == "a" || key == "b")) {this->_qrApp = !this->_qrApp; this->showQr_();}
            else if (this->_modalTitle.startsWith("QR code") && key == "c") this->copy_(this->_modalLines.value(this->_modalLines.size() - 1), "Link");
            else if (key == "c") this->copy_(this->_modalLines.join('\n'), "Text");
            return;
        case Modal::Input:
            if (key == "esc") {this->_modal = Modal::None; return;}
            if (key == "enter") {
                const std::function<void(const QString&)> done = this->_inputDone;
                this->_modal = Modal::None;
                if (done) done(this->_input);
                return;
            }
            if (key == "backspace") this->_input.chop(1);
            else if (key == "ctrl+u") this->_input.clear();
            else if (key == "paste") this->_input += QString::fromUtf8(paste_text(raw)).remove('\r').remove('\n');
            else if (key == "space") this->_input += ' ';
            else if (!raw.isEmpty() && static_cast<unsigned char>(raw[0]) >= 0x20 && raw[0] != 0x7f) this->_input += QString::fromUtf8(raw);
            return;
        case Modal::Confirm:
            if (key == "esc" || (this->_confirmWord.isEmpty() && key == "n")) {this->_modal = Modal::None; this->flash_("Cancelled"); return;}
            if (this->_confirmWord.isEmpty() && (key == "y" || key == "enter")) {
                const std::function<void(void)> action = this->_confirmAction;
                this->_modal = Modal::None;
                if (action) action();
                return;
            }
            if (this->_confirmWord.isEmpty()) return;
            if (key == "enter") {
                const std::function<void(void)> action = this->_confirmAction;
                this->_modal = Modal::None;
                if (this->_input.trimmed().compare(this->_confirmWord, Qt::CaseInsensitive) != 0) {this->flash_("Not confirmed: nothing changed", true); return;}
                if (action) action();
                return;
            }
            if (key == "backspace") this->_input.chop(1);
            else if (!raw.isEmpty() && static_cast<unsigned char>(raw[0]) >= 0x20 && raw[0] != 0x7f) this->_input += QString::fromUtf8(raw);
            return;
        case Modal::Mirror: {
            const QStringList keys = this->mirrorsOf_(this->_mirror.section('/', 0, 0));
            const int index = static_cast<int>(keys.indexOf(this->_mirror));
            if (key == "esc" || key == "q") this->_modal = Modal::None;
            else if ((key == "right" || key == "tab") && !keys.isEmpty()) {this->_mirror = keys[(index + 1) % keys.size()]; this->_mirrorScroll = 0;}
            else if ((key == "left" || key == "shift+tab") && !keys.isEmpty()) {this->_mirror = keys[(index - 1 + keys.size()) % keys.size()]; this->_mirrorScroll = 0;}
            else if (key == "up") this->_mirrorScroll++;
            else if (key == "down") this->_mirrorScroll = std::max(0, this->_mirrorScroll - 1);
            else if (key == "pgup") this->_mirrorScroll += 10;
            else if (key == "pgdn") this->_mirrorScroll = std::max(0, this->_mirrorScroll - 10);
            else if (key == "end") this->_mirrorScroll = 0;
            return;
        }
        case Modal::None: break;
    }

    // Everywhere
    if (key == "?" || key == "f1" || key == "h") {this->_modal = Modal::Help; return;}
    if (key == "q" || key == "ctrl+c") {
        this->confirm_(this->_host.sessions().isEmpty() ? "Quit xdesk? (sharing stops)" : "Quit xdesk? Every session is disconnected", "", [this]() {emit this->quit();});
        return;
    }
    if (key.size() == 1 && key[0] >= '1' && key[0] <= '5') {this->_page = static_cast<Page>(key[0].unicode() - '1'); return;}
    if (key == "tab") {this->_page = static_cast<Page>((static_cast<int>(this->_page) + 1) % 5); return;}
    if (key == "shift+tab") {this->_page = static_cast<Page>((static_cast<int>(this->_page) + 4) % 5); return;}
    if (key == "up") {this->move_(-1); return;}
    if (key == "down") {this->move_(1); return;}
    if (key == "pgup") {this->move_(-10); return;}
    if (key == "pgdn") {this->move_(10); return;}
    if (key == "home") {this->move_(-100000); return;}
    if (key == "end") {this->move_(100000); return;}
    if (key == "s") {this->toggleSharing_(); return;}
    if (key == "x" && !this->_tamper.isEmpty()) {this->_tamper.clear(); return;}
    this->pageKey_(key);
}

void xdesk::ui::tty::HostTui::pageKey_(const QString& key)
{
    switch (this->_page) {
        case Page::Overview:
            if (key == "n") {this->_host.rotateCode(); this->flash_("New code");}
            else if (key == "y") {if (this->_host.isSharing()) this->copy_(this->_host.code().display(), "Code");}
            else if (key == "enter") this->showQr_();
            else if (key == "c") {
                const QList<net::Endpoint> endpoints = this->_host.exposure().endpoints();
                if (!endpoints.isEmpty()) this->copy_(endpoints.value(std::clamp(this->_endpoint, 0, std::max(0, static_cast<int>(endpoints.size()) - 1))).url, "Link");
            } else if (key == "e" || key == "right") this->setExposure_(1);
            else if (key == "E" || key == "left") this->setExposure_(-1);
            break;
        case Page::Sessions: {
            host::Session* s = this->currentSession_();
            if (!s) break;
            host::Permissions p = s->permissions();
            const QString id = s->id();
            const QString name = s->name();
            if (key == "v") {p.view = !p.view; this->_host.setPermissions(id, p);}
            else if (key == "c") {p.control = !p.control; this->_host.setPermissions(id, p);}
            else if (key == "t") {p.terminal = !p.terminal; this->_host.setPermissions(id, p);}
            else if (key == "p") {p.clipboard = !p.clipboard; this->_host.setPermissions(id, p);}
            else if (key == "k") this->confirm_("Disconnect " + name + "?", "", [this, id, name]() {this->_host.kick(id); this->flash_("Disconnected " + name);});
            else if (key == "b") this->confirm_("Ban " + name + "? Its device and address are refused until a local unban", "", [this, id, name]() {this->_host.ban(id, "Banned by the host (terminal panel)"); this->flash_("Banned " + name);});
            else if (key == "K") this->confirm_("Disconnect every session?", "", [this]() {this->_host.kickAll("Disconnected by the host"); this->flash_("Everybody disconnected");});
            else if (key == "a" && s->state() == host::State::Pending) this->_approval = id;
            else if (key == "m" || key == "enter") {
                const QStringList keys = this->mirrorsOf_(id);
                if (keys.isEmpty()) {this->flash_("No terminal opened by this session yet", true); break;}
                this->_mirror = keys.first();
                this->_mirrorScroll = 0;
                this->_modal = Modal::Mirror;
            }
            break;
        }
        case Page::Devices: {
            const QList<security::Device>& devices = this->_host.registry().devices();
            const QList<security::AddressBan>& addresses = this->_host.registry().addresses();
            const int index = this->_device;
            if (index < devices.size()) {
                const security::Device d = devices[index];
                if (key == "T" && !this->locked_()) this->confirm_("Trust " + d.name + "? It connects without a code from now on", "", [this, d]() {if (this->_host.setTrusted(d.id, true)) this->flash_("Trusted " + d.name); else this->flash_("Locked", true);});
                else if (key == "U" && !this->locked_()) {(void)this->_host.setTrusted(d.id, false); this->flash_("Untrusted " + d.name);}
                else if (key == "u" && d.banned && !this->locked_()) this->confirm_("Unban " + d.name + "?", XDESK_UNBAN_WORD, [this, d]() {if (this->_host.unban(d.id)) this->flash_("Unbanned " + d.name); else this->flash_("Locked", true);});
                else if (key == "f" && !this->locked_()) this->confirm_(d.banned ? "Forget the banned " + d.name + "? Its ban is lifted too" : "Forget " + d.name + "? It needs a code again", d.banned ? XDESK_UNBAN_WORD : "", [this, d]() {if (this->_host.forget(d.id)) this->flash_("Forgot " + d.name); else this->flash_("Locked", true);});
            } else if (index - devices.size() < addresses.size()) {
                const security::AddressBan a = addresses[index - devices.size()];
                if ((key == "u" || key == "enter") && !this->locked_()) this->confirm_("Unban the address " + a.address + "?", XDESK_UNBAN_WORD, [this, a]() {if (this->_host.unbanAddress(a.address)) this->flash_("Unbanned " + a.address); else this->flash_("Locked", true);});
            }
            break;
        }
        case Page::Logs: {
            const QList<QJsonObject> logs = this->filteredLogs_();
            if (key == "l") {this->_logLevel = (this->_logLevel + 1) % static_cast<int>(LOG_LEVELS.size()); this->_logRow = -1;}
            else if (key == "c") {this->_logCategory = (this->_logCategory + 1) % static_cast<int>(LOG_CATEGORIES.size()); this->_logRow = -1;}
            else if (key == "/") this->ask_("Search the logs (session, address, text)", this->_logSearch, [this](const QString& text) {this->_logSearch = text.trimmed(); this->_logRow = -1;});
            else if (key == "esc") {this->_logLevel = 0; this->_logCategory = 0; this->_logSearch.clear(); this->_logRow = -1;}
            else if (key == "f") this->_logRow = -1;
            else if (key == "enter" && !logs.isEmpty()) {
                const QJsonObject e = logs.value(this->_logRow < 0 ? logs.size() - 1 : this->_logRow);
                this->showText_("Log entry", QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Indented)).split('\n'));
            } else if (key == "o") this->flash_("Log folder: " + core::Config::logDir());
            break;
        }
        case Page::Settings:
            if (key == "left") this->changeField_(-1, false);
            else if (key == "right") this->changeField_(1, false);
            else if (key == "space") this->changeField_(0, false);
            else if (key == "enter") this->changeField_(0, true);
            else if (key == "ctrl+s" || key == "w") this->saveSettings_();
            else if (key == "r") {this->_draft = this->_host.config().settings(); this->flash_("Changes reverted");}
            break;
    }
}

//----------------------------------------------------------------//
// Drawing

void xdesk::ui::tty::HostTui::draw_(void)
{
    const QSize size = this->_term.size();
    const int w = size.width();
    const int h = size.height();

    this->_canvas.resize(w, h);
    this->_hits.clear();
    if (w < 60 || h < 16) {
        this->_canvas.text(1, 0, "xdesk: make the terminal bigger (60 x 16 at least)", st(WARN, QColor(), true));
        this->_canvas.text(1, 1, QString("sharing ") + (this->_host.isSharing() ? "on  code " + this->_host.code().display() : "off"), st());
        this->_term.present(this->_canvas);
        return;
    }
    this->drawHeader_();
    if (!this->_tamper.isEmpty()) {
        this->_canvas.fill(QRect(0, 1, w, 1), st(QColor(Qt::white), QColor(150, 40, 40), true));
        this->_canvas.text(1, 1, "TAMPERING BLOCKED: " + this->_tamper + "   (x or click to dismiss)", st(QColor(Qt::white), QColor(150, 40, 40), true), w - 2);
        this->_hits.append(Hit{QRect(0, 1, w, 1), [this]() {this->_tamper.clear();}});
    } else if (const host::Session* asking = this->_host.session(this->_host.focusRequest())) {
        // A viewer paused by the focus of this panel asks its control back
        const qint64 left = std::max<qint64>(0, this->_host.focusDeadline() - QDateTime::currentMSecsSinceEpoch());
        this->_canvas.fill(QRect(0, 1, w, 1), st(DARK, WARN, true));
        this->_canvas.text(1, 1, asking->name() + " asks its control back: given in " + QString::number((left + 999) / 1000) + " s   (a key or a click here refuses)", st(DARK, WARN, true), w - 2);
        this->_hits.append(Hit{QRect(0, 1, w, 1), [this]() {this->_host.refuseFocusRequest();}});
    } else if (const host::Session* given = this->_host.session(this->_host.focusOverride())) {
        this->_canvas.fill(QRect(0, 1, w, 1), st(DARK, WARN, true));
        this->_canvas.text(1, 1, given->name() + " controls this PC although the panel has the focus (request granted): a key here pauses it", st(DARK, WARN, true), w - 2);
    }
    const QRect area(0, 2, w, h - 4);
    switch (this->_page) {
        case Page::Overview: this->drawOverview_(area); break;
        case Page::Sessions: this->drawSessions_(area); break;
        case Page::Devices:  this->drawDevices_(area); break;
        case Page::Logs:     this->drawLogs_(area); break;
        case Page::Settings: this->drawSettings_(area); break;
    }
    this->drawFooter_();
    if (this->_modal != Modal::None) this->drawModal_();
    if (!this->_approval.isEmpty()) this->drawApproval_();
    this->_term.present(this->_canvas);
}

QRect xdesk::ui::tty::HostTui::card_(const QRect& area, const QString& title, const QString& note)
{
    return this->_canvas.card(area, title, st(BORDER), st(ACCENT, QColor(), true), note, st(BORDER_TEXT));
}

void xdesk::ui::tty::HostTui::select_(const QRect& row)
{
    this->_canvas.selectRow(row, st(QColor(), SELECTED), st(ACCENT, SELECTED));
}

void xdesk::ui::tty::HostTui::drawHeader_(void)
{
    const int w = this->_canvas.width();
    const bool on = this->_host.isSharing();
    const QString pill = on ? " ● SHARING " : " ○ OFF ";
    const QString host = QHostInfo::localHostName();
    int x = 0;

    this->_canvas.fill(QRect(0, 0, w, 1), st(QColor(), BAR));
    x = this->_canvas.text(x, 0, " ◆ xdesk ", st(DARK, ACCENT, true));
    x = this->_canvas.text(x, 0, " ", st(QColor(), BAR));
    for (int i = 0; i < PAGE_NAMES.size(); ++i) {
        QString label = " " + QString::number(i + 1) + " " + PAGE_NAMES[i];
        if (i == 1 && !this->_host.sessions().isEmpty()) label += " · " + QString::number(this->_host.sessions().size());
        label += " ";
        const bool current = static_cast<int>(this->_page) == i;
        const int start = x;
        x = this->_canvas.text(x, 0, label, current ? st(QColor(Qt::white), SELECTED, true) : st(MUTED, BAR));
        if (current) this->_canvas.fill(QRect(start, 1, x - start, 1), st(ACCENT, QColor()), U'▔'); // the current tab underlined
        this->_hits.append(Hit{QRect(start, 0, x - start, 1), [this, i]() {this->_page = static_cast<Page>(i);}});
    }
    const int start = w - static_cast<int>(pill.size()) - 1;
    if (start > x) {
        this->_canvas.text(start, 0, pill, st(DARK, on ? GOOD : BAD, true));
        this->_hits.append(Hit{QRect(start, 0, static_cast<int>(pill.size()), 1), [this]() {this->toggleSharing_();}});
        if (start - static_cast<int>(host.size()) - 3 > x) this->_canvas.text(start - static_cast<int>(host.size()) - 2, 0, host, st(MUTED, BAR));
    }
}

void xdesk::ui::tty::HostTui::drawOverview_(const QRect& area)
{
    const int w = area.width();
    const QList<net::Endpoint> endpoints = this->_host.exposure().endpoints();
    const security::PairingCode& code = this->_host.code();
    const core::Settings& settings = this->_host.config().settings();
    const QList<host::Session*> sessions = this->_host.sessions();
    const bool wide = w >= 100;
    const int leftW = wide ? w * 11 / 20 : w - 2;
    const bool on = this->_host.isSharing();
    QRect inside;
    int y = area.top();
    int pending = 0;

    // Sharing: state, code and its lifetime, identity
    inside = this->card_(QRect(1, y, leftW, 7), "Sharing");
    if (on) this->_canvas.text(inside.left(), inside.top(), "● ON   anyone with the code and an address can ask to connect", st(GOOD, QColor(), true), inside.width());
    else this->_canvas.text(inside.left(), inside.top(), "○ OFF  nobody can connect (s or the OFF pill to start)", st(BAD, QColor(), true), inside.width());
    this->_canvas.text(inside.left(), inside.top() + 2, "Code", st(MUTED));
    if (on) {
        const int left = code.secondsLeft();
        const int lifetime = std::max(1, settings.codeLifetime);
        int x = this->_canvas.text(inside.left() + 10, inside.top() + 2, " " + QString(code.display()).replace("-", " · ") + " ", st(DARK, ACCENT, true));
        this->_hits.append(Hit{QRect(inside.left() + 10, inside.top() + 2, x - inside.left() - 10, 1), [this]() {this->onKey_("y", "y");}}); // a click copies it
        if (left >= 0) {
            const int bar = std::clamp(inside.width() - 26, 6, 24);
            const int filled = std::clamp(left * bar / lifetime, 0, bar);
            const QColor color = left < 30 ? BAD : (left * 3 < lifetime ? WARN : ACCENT);
            x = this->_canvas.text(x + 2, inside.top() + 2, QString(filled, QChar(0x2501)), st(color));
            x = this->_canvas.text(x, inside.top() + 2, QString(bar - filled, QChar(0x2500)), st(BORDER));
            this->_canvas.text(x + 1, inside.top() + 2, QString::number(left / 60) + ":" + QString::number(left % 60).rightJustified(2, '0'), st(color, QColor(), true));
        } else this->_canvas.text(x + 2, inside.top() + 2, "one time (changes once used)", st(MUTED), inside.right() - x - 2);
    } else this->_canvas.text(inside.left() + 10, inside.top() + 2, "shown once sharing is on", st(MUTED));
    this->_canvas.text(inside.left(), inside.top() + 4, "Identity", st(MUTED));
    this->_canvas.text(inside.left() + 10, inside.top() + 4, security::fingerprint_short(this->_host.certificate().fingerprint()), st(QColor(), QColor(), true), inside.width() - 10);

    // Reach: who can connect
    const QRect reach = wide ? QRect(leftW + 2, y, w - leftW - 3, 7) : QRect(1, y + 7, w - 2, 5);
    inside = this->card_(reach, "Reach");
    {
        const SettingField& field = settings_fields()[2];
        const int start = inside.left() + 9;
        this->_canvas.text(inside.left(), inside.top(), "Open to", st(MUTED));
        const int end = this->_canvas.text(start, inside.top(), field_value(field, settings), st(QColor(), QColor(), true), inside.width() - 9);
        this->_hits.append(Hit{QRect(start, inside.top(), 2, 1), [this]() {this->setExposure_(-1);}});
        this->_hits.append(Hit{QRect(start + 2, inside.top(), std::max(1, end - start - 2), 1), [this]() {this->setExposure_(1);}});
        const QStringList lines = wrap(this->_host.exposure().status(), inside.width());
        for (int i = 0; i < lines.size() && inside.top() + 1 + i < inside.bottom(); ++i)
            this->_canvas.text(inside.left(), inside.top() + 1 + i, lines[i], st(MUTED));
        this->_canvas.text(inside.left(), inside.bottom(), "e / ← → / click: change", st(BORDER_TEXT), inside.width());
    }
    y = reach.bottom() + 1;

    // Addresses (the room left, the system card under them when it fits)
    const int rows = std::max(1, static_cast<int>(endpoints.size()));
    const int addressesH = std::min(rows + 2, area.bottom() - y + 1);
    if (addressesH < 3) return;
    inside = this->card_(QRect(1, y, w - 2, addressesH), "Addresses", on && !endpoints.isEmpty() ? "enter: QR code · c: copy" : "");
    if (!on || endpoints.isEmpty()) this->_canvas.text(inside.left(), inside.top(), on ? "no address for this exposure yet" : "none while sharing is off", st(MUTED));
    else {
        for (int i = 0; i < endpoints.size() && inside.top() + i <= inside.bottom(); ++i) {
            const int row = inside.top() + i;
            const net::Endpoint& e = endpoints[i];
            const bool current = i == std::clamp(this->_endpoint, 0, std::max(0, static_cast<int>(endpoints.size()) - 1));
            const Style base = current ? st(QColor(), SELECTED) : st();
            if (current) this->select_(QRect(inside.left() - 2, row, inside.width() + 4, 1));
            this->_canvas.text(inside.left(), row, e.label, st(current ? ACCENT : MUTED, base.bg, current), 12);
            const int x = this->_canvas.text(inside.left() + 13, row, e.url, st(QColor(), base.bg, true), inside.width() - 13);
            this->_canvas.text(x + 2, row, e.reach + (e.ip.isEmpty() ? "" : " · " + e.ip) + (e.endToEnd ? "" : " · the relay sees the traffic"), st(MUTED, base.bg), inside.right() - x - 1);
            this->_hits.append(Hit{QRect(1, row, w - 2, 1), [this, i, current]() {
                this->_endpoint = i;
                if (current) this->showQr_(); // second click: its QR code
            }});
        }
    }
    y += addressesH;

    // System
    for (host::Session* s: sessions)
        if (s->state() == host::State::Pending) pending++;
    if (area.bottom() - y + 1 < 3) return;
    inside = this->card_(QRect(1, y, w - 2, std::min(6, area.bottom() - y + 1)), "System");
    {
        const QList<std::tuple<QString, QString, QColor>> rowsInfo = {
            {"Screen", this->_host.captureBackend().isEmpty() ? "idle (starts with the first viewer)" : this->_host.captureBackend(), QColor()},
            {"Input", this->_host.inputError().isEmpty() ? this->_host.inputBackend() : "unavailable: " + this->_host.inputError(), this->_host.inputError().isEmpty() ? QColor() : WARN},
            {"Security", security::tls_summary(), QColor()},
            {"Sessions", QString::number(sessions.size() - pending) + " connected" + (pending ? " · " + QString::number(pending) + " waiting for you" : ""), pending ? WARN : QColor()},
        };
        for (int i = 0; i < rowsInfo.size() && inside.top() + i <= inside.bottom(); ++i) {
            this->_canvas.text(inside.left(), inside.top() + i, std::get<0>(rowsInfo[i]), st(MUTED));
            this->_canvas.text(inside.left() + 10, inside.top() + i, std::get<1>(rowsInfo[i]), st(std::get<2>(rowsInfo[i])), inside.width() - 10);
        }
    }
}

void xdesk::ui::tty::HostTui::drawSessions_(const QRect& area)
{
    const int w = area.width();
    const QList<host::Session*> sessions = this->ordered_();
    const int selected = std::clamp(this->_session, 0, std::max(0, static_cast<int>(sessions.size()) - 1));
    const int listRows = std::clamp(static_cast<int>(sessions.size()), 1, std::max(1, area.height() / 3));
    const int first = std::clamp(selected - listRows + 1, 0, std::max(0, static_cast<int>(sessions.size()) - listRows));
    QRect inside = this->card_(QRect(1, area.top(), w - 2, listRows + 3), "Sessions", QString::number(sessions.size()) + " connected");
    int y = inside.top();

    this->_canvas.text(inside.left(), y++, fit("DEVICE", 24) + " " + fit("CLIENT", 6) + " " + fit("ADDRESS", 16) + " " + fit("STATE", 9) + " " + fit("RIGHTS", 7) + " " + fit("FPS", 4) + " " + fit("LAT.", 6) + " " + fit("KBIT/S", 7) + " SINCE", st(BORDER_TEXT, QColor(), true), inside.width());
    if (sessions.isEmpty()) {
        this->_canvas.text(inside.left(), y, "No session yet: give the code and an address of the Overview page (or its QR code)", st(MUTED), inside.width());
        return;
    }
    for (int i = first; i < first + listRows && i < sessions.size(); ++i, ++y) {
        const host::Session* s = sessions[i];
        const host::Permissions& p = s->permissions();
        const bool current = i == selected;
        const Style base = current ? st(QColor(), SELECTED) : st();
        const bool waiting = s->state() == host::State::Pending;
        const QString state = waiting ? "WAITING" : (s->state() == host::State::Handshake ? "auth" : s->mode());
        const QString rights = QString(p.view ? "V" : "·") + (p.control ? "C" : "·") + (p.terminal ? "T" : "·") + (p.clipboard ? "P" : "·");
        if (current) this->select_(QRect(1, y, w - 2, 1));
        int x = this->_canvas.text(inside.left(), y, fit(s->name(), 24) + " " + fit(net::client_kind_name(s->kind()), 6) + " " + fit(s->address(), 16) + " ", st(QColor(), base.bg, current), inside.width());
        x = this->_canvas.text(x, y, fit((waiting ? "◌ " : (s->mode() == "control" ? "● " : "○ ")) + state, 9) + " ", st(waiting ? WARN : (s->mode() == "control" ? ACCENT : QColor()), base.bg, true));
        this->_canvas.text(x, y, fit(rights, 7) + " " + fit(QString::number(s->stats().fps), 4) + " " + fit(s->stats().latency >= 0 ? QString::number(s->stats().latency) + "ms" : "-", 6) + " " + fit(QString::number(s->stats().kbps), 7) + " " + ago(s->since()), st(MUTED, base.bg), inside.right() - x + 1);
        this->_hits.append(Hit{QRect(1, y, w - 2, 1), [this, i]() {this->_session = i;}});
    }
    y = area.top() + listRows + 3;

    // The selected one
    const host::Session* s = sessions[selected];
    const host::Permissions& p = s->permissions();
    const QStringList terminals = this->mirrorsOf_(s->id());
    if (area.bottom() - y + 1 < 5) return;
    inside = this->card_(QRect(1, y, w - 2, 5), s->name(), net::client_kind_name(s->kind()) + " · " + s->os() + " · " + s->address());
    y = inside.top();
    {
        int x = this->_canvas.text(inside.left(), y, "Rights  ", st(MUTED));
        const QList<std::tuple<QString, QString, bool>> toggles = {{"v", "view", p.view}, {"c", "control", p.control}, {"t", "terminal", p.terminal}, {"p", "clipboard", p.clipboard}};
        for (const std::tuple<QString, QString, bool>& t: toggles) {
            const int start = x;
            const QString key = std::get<0>(t);
            x = this->_canvas.text(x, y, (std::get<2>(t) ? " ● " : " ○ ") + std::get<1>(t) + " ", std::get<2>(t) ? st(DARK, ACCENT, true) : st(MUTED, BAR));
            x = this->_canvas.text(x, y, " " + key + "  ", st(BORDER_TEXT));
            this->_hits.append(Hit{QRect(start, y, x - start, 1), [this, key]() {this->pageKey_(key);}});
        }
    }
    this->_canvas.text(inside.left(), y + 1, "Mode " + s->mode() + " · video " + this->_host.transport(s->id()) + " · last input " + (s->lastInput() > 0 ? ago(s->lastInput()) + " ago" : QString("never")) + " · sent " + bytes_text(s->stats().bytesOut) + " · received " + bytes_text(s->stats().bytesIn) + " · " + QString::number(s->stats().inputs) + " inputs · device " + (s->deviceId().isEmpty() ? "-" : s->deviceId()), st(MUTED), inside.width());
    if (terminals.isEmpty()) this->_canvas.text(inside.left(), y + 2, "Terminals: none", st(MUTED));
    else {
        QStringList ids;
        for (const QString& key: terminals)
            ids.append(key.section('/', 1));
        const int end = this->_canvas.text(inside.left(), y + 2, "Terminals " + ids.join(", ") + "  ▸ m / enter / click: watch them live", st(WARN, QColor(), true), inside.width());
        this->_hits.append(Hit{QRect(inside.left(), y + 2, end - inside.left(), 1), [this]() {this->pageKey_("m");}});
    }
    y += 5;

    // Live activity
    if (area.bottom() - y + 1 < 3) return;
    inside = this->card_(QRect(1, y, w - 2, area.bottom() - y + 1), "Live activity");
    const QStringList& activity = this->_activity.value(s->id());
    const int room = inside.height();
    if (activity.isEmpty()) this->_canvas.text(inside.left(), inside.top(), "nothing yet", st(MUTED));
    for (qsizetype i = std::max<qsizetype>(0, activity.size() - room), row = inside.top(); i < activity.size() && row <= inside.bottom(); ++i, ++row) {
        const QString& line = activity[i];
        const int x = this->_canvas.text(inside.left(), row, line.left(8), st(BORDER_TEXT));
        this->_canvas.text(x + 1, row, line.mid(9), st(line.contains(" $ ") ? WARN : QColor()), inside.right() - x);
    }
}

void xdesk::ui::tty::HostTui::drawDevices_(const QRect& area)
{
    const int w = area.width();
    const QList<security::Device>& devices = this->_host.registry().devices();
    const QList<security::AddressBan>& addresses = this->_host.registry().addresses();
    const int total = static_cast<int>(devices.size() + addresses.size());
    const int selected = std::clamp(this->_device, 0, std::max(0, total - 1));
    const bool locked = this->_host.localActionsLocked();
    const int bansRows = std::max(1, static_cast<int>(addresses.size()));
    const int room = area.height() - (locked ? 1 : 0) - std::min(bansRows, 6) - 3;
    const int devicesH = std::clamp(static_cast<int>(devices.size()) + 3, 4, std::max(4, room));
    QRect inside = this->card_(QRect(1, area.top(), w - 2, devicesH), "Devices", QString::number(devices.size()) + " known");
    int y = inside.top();

    this->_canvas.text(inside.left(), y++, fit("DEVICE", 24) + " " + fit("CLIENT", 6) + " " + fit("OS", 10) + " " + fit("LAST ADDRESS", 16) + " " + fit("STATE", 10) + " " + fit("SESSIONS", 8) + " " + fit("LAST SEEN", 12) + " REASON", st(BORDER_TEXT, QColor(), true), inside.width());
    if (devices.isEmpty()) this->_canvas.text(inside.left(), y, "No device yet: every client that connects gets one (bans and trust follow it)", st(MUTED), inside.width());
    for (int i = 0; i < devices.size() && y <= inside.bottom(); ++i, ++y) {
        const security::Device& d = devices[i];
        const bool current = i == selected;
        const Style base = current ? st(QColor(), SELECTED) : st();
        const QColor color = d.banned ? BAD : (d.trusted ? GOOD : MUTED);
        if (current) this->select_(QRect(1, y, w - 2, 1));
        int x = this->_canvas.text(inside.left(), y, fit(d.name, 24) + " " + fit(d.kind, 6) + " " + fit(d.os, 10) + " " + fit(d.address, 16) + " ", st(QColor(), base.bg, current), inside.width());
        x = this->_canvas.text(x, y, fit(d.banned ? "⊘ banned" : (d.trusted ? (d.secret.isEmpty() ? "✓ code once" : "✓ trusted") : "· known"), 10) + " ", st(color, base.bg, true));
        this->_canvas.text(x, y, fit(QString::number(d.sessions), 8) + " " + fit(d.seen > 0 ? QDateTime::fromMSecsSinceEpoch(d.seen).toString("MM-dd HH:mm") : "-", 12) + " " + d.banReason, st(MUTED, base.bg), inside.right() - x + 1);
        this->_hits.append(Hit{QRect(1, y, w - 2, 1), [this, i]() {this->_device = i;}});
    }
    y = area.top() + devicesH;
    if (area.bottom() - y + 1 - (locked ? 1 : 0) < 3) {
        if (locked) this->_canvas.text(2, area.bottom(), "▲ Locked: a remote session controls this PC or has a terminal (trust / unban / forget need the person in front of it)", st(WARN, QColor(), true), w - 4);
        return;
    }
    inside = this->card_(QRect(1, y, w - 2, std::min(bansRows + 2, area.bottom() - y + 1 - (locked ? 1 : 0))), "Banned addresses");
    if (addresses.isEmpty()) this->_canvas.text(inside.left(), inside.top(), "none", st(MUTED));
    for (int i = 0; i < addresses.size() && inside.top() + i <= inside.bottom(); ++i) {
        const int row = inside.top() + i;
        const security::AddressBan& a = addresses[i];
        const int index = static_cast<int>(devices.size()) + i;
        const bool current = index == selected;
        const Style base = current ? st(QColor(), SELECTED) : st();
        if (current) this->select_(QRect(1, row, w - 2, 1));
        this->_canvas.text(inside.left(), row, fit(a.address, 24), st(BAD, base.bg, true));
        this->_canvas.text(inside.left() + 25, row, a.reason + " · since " + QDateTime::fromMSecsSinceEpoch(a.since).toString("MM-dd HH:mm") + (a.until ? " · until " + QDateTime::fromMSecsSinceEpoch(a.until).toString("HH:mm") : QString(" · until a local unban")), st(MUTED, base.bg), inside.width() - 25);
        this->_hits.append(Hit{QRect(1, row, w - 2, 1), [this, index]() {this->_device = index;}});
    }
    if (locked) this->_canvas.text(2, area.bottom(), "▲ Locked: a remote session controls this PC or has a terminal (trust / unban / forget need the person in front of it)", st(WARN, QColor(), true), w - 4);
}

void xdesk::ui::tty::HostTui::drawLogs_(const QRect& area)
{
    const int w = area.width();
    const QList<QJsonObject> logs = this->filteredLogs_();
    const int selected = logs.isEmpty() ? -1 : (this->_logRow < 0 ? static_cast<int>(logs.size()) - 1 : std::clamp(this->_logRow, 0, static_cast<int>(logs.size()) - 1));
    int x = 2;
    int y = area.top();

    // Filters (clickable chips)
    for (const std::tuple<QString, QString, QString>& f: QList<std::tuple<QString, QString, QString>>{{"l", "Level", LOG_LEVELS[this->_logLevel]}, {"c", "Category", LOG_CATEGORIES[this->_logCategory]}, {"/", "Search", this->_logSearch.isEmpty() ? QString("-") : this->_logSearch}}) {
        const int start = x;
        const QString key = std::get<0>(f);
        x = this->_canvas.text(x, y, std::get<1>(f) + " ", st(MUTED));
        x = this->_canvas.text(x, y, " " + std::get<2>(f) + " ▾ ", st(QColor(Qt::white), SELECTED, true));
        x = this->_canvas.text(x, y, " " + key + "   ", st(BORDER_TEXT));
        this->_hits.append(Hit{QRect(start, y, x - start, 1), [this, key]() {this->pageKey_(key);}});
    }
    y += 2;
    const QRect inside = this->card_(QRect(1, y, w - 2, area.bottom() - y + 1), "Log", QString::number(logs.size()) + " entries" + (this->_logRow < 0 ? " · following" : ""));
    const int rows = inside.height();
    const int first = std::clamp(selected - rows + 1, 0, std::max(0, static_cast<int>(logs.size()) - rows));
    if (logs.isEmpty()) this->_canvas.text(inside.left(), inside.top(), "nothing matches these filters", st(MUTED));
    for (int i = first, row = inside.top(); i < logs.size() && row <= inside.bottom(); ++i, ++row) {
        const QJsonObject& e = logs[i];
        const QString level = e.value("level").toString();
        const bool current = i == selected && this->_logRow >= 0;
        const Style base = current ? st(QColor(), SELECTED) : st();
        const QColor color = level == "alert" ? BAD : (level == "warn" ? WARN : MUTED);
        if (current) this->select_(QRect(1, row, w - 2, 1));
        this->_canvas.text(inside.left(), row, QDateTime::fromMSecsSinceEpoch(e.value("ts").toInteger()).toString("MM-dd HH:mm:ss"), st(BORDER_TEXT, base.bg));
        this->_canvas.text(inside.left() + 15, row, (level == "info" ? "· " : "▲ ") + fit(level, 5), st(color, base.bg, level != "info"));
        this->_canvas.text(inside.left() + 23, row, fit(e.value("cat").toString(), 9) + " " + fit(e.value("session").toString(), 8), st(MUTED, base.bg));
        this->_canvas.text(inside.left() + 42, row, e.value("msg").toString(), st(level == "alert" ? BAD : QColor(), base.bg), inside.width() - 42);
        this->_hits.append(Hit{QRect(1, row, w - 2, 1), [this, i, current]() {
            this->_logRow = i;
            if (current) this->onKey_("enter", "\r"); // second click: the details
        }});
    }
}

void xdesk::ui::tty::HostTui::drawSettings_(const QRect& area)
{
    const int w = area.width();
    const QList<SettingField>& fields = settings_fields();
    const core::Settings& saved = this->_host.config().settings();
    const int selected = std::clamp(this->_field, 0, static_cast<int>(fields.size()) - 1);
    QStringList sections;
    int dirty = 0;

    for (const SettingField& field: fields) {
        if (!sections.contains(field.section)) sections.append(field.section);
        if (field.get(this->_draft) != field.get(saved)) dirty++;
    }

    // One field row: label, value (arrows and toggles clickable)
    const std::function<void(const int, const int, const int, const int)> row = [&](const int index, const int x, const int y, const int width) {
        const SettingField& field = fields[index];
        const bool current = index == selected;
        const bool changed = field.get(this->_draft) != field.get(saved);
        const Style base = current ? st(QColor(), SELECTED) : st();
        const int labelW = std::min(38, width * 3 / 5);
        QString value = field_value(field, this->_draft);
        if (field.kind == FieldKind::Text && value.isEmpty()) value = field.label == "Public address" ? "my.domain.tld (manual exposure)" : (field.label == "Terminal shell" ? "default shell" : "-");
        if (current) this->select_(QRect(x - 2, y, width + 4, 1));
        this->_canvas.text(x, y, (changed ? "● " : "  ") + field.label, changed ? st(ACCENT, base.bg, true) : st(current ? QColor() : MUTED, base.bg, current), labelW);
        const bool on = field.kind == FieldKind::Toggle && field.get(this->_draft).toBool();
        const Style look = field.kind == FieldKind::Text && field.get(this->_draft).toString().isEmpty() ? st(BORDER_TEXT, base.bg) : st(field.kind == FieldKind::Toggle ? (on ? ACCENT : MUTED) : QColor(), base.bg, current || on);
        const int end = this->_canvas.text(x + labelW + 1, y, value, look, width - labelW - 1);
        this->_hits.append(Hit{QRect(x - 2, y, width + 4, 1), [this, index]() {
            if (this->_field == index) this->changeField_(0, true); // second click: change it
            this->_field = index;
        }});
        if (field.kind == FieldKind::Choice || field.kind == FieldKind::Number) {
            // The arrows step the value at once
            this->_hits.append(Hit{QRect(x + labelW + 1, y, 2, 1), [this, index]() {this->_field = index; this->changeField_(-1, false);}});
            this->_hits.append(Hit{QRect(std::max(x + labelW + 1, end - 2), y, 2, 1), [this, index]() {this->_field = index; this->changeField_(1, false);}});
        }
    };

    const int bottom = area.bottom() - 1;
    if (w >= 110 && area.height() >= 18) {
        // Two columns of cards: network and stream on the left, access on the right
        const int half = (w - 3) / 2;
        int columnY[2] = {area.top(), area.top()};
        for (const QString& section: sections) {
            QList<int> indexes;
            for (int i = 0; i < fields.size(); ++i)
                if (fields[i].section == section) indexes.append(i);
            const int column = section == sections.last() ? 1 : 0;
            const int x = column == 0 ? 1 : 2 + half;
            const int height = std::min(static_cast<int>(indexes.size()) + 2, bottom - columnY[column] + 1);
            if (height < 3) continue;
            const QRect inside = this->card_(QRect(x, columnY[column], half, height), section);
            for (int i = 0; i < indexes.size() && inside.top() + i <= inside.bottom(); ++i)
                row(indexes[i], inside.left(), inside.top() + i, inside.width());
            columnY[column] += height + 1;
        }
    } else {
        // One column: section titles and the lines around the selected field
        QList<int> rows; // field index per line (-1 - section = its title, -1000 = a gap)
        for (int i = 0; i < fields.size(); ++i) {
            if (i == 0 || fields[i].section != fields[i - 1].section) {
                if (i > 0) rows.append(-1000);
                rows.append(-1 - i);
            }
            rows.append(i);
        }
        const int room = bottom - area.top();
        int at = 0;
        for (int i = 0; i < rows.size(); ++i)
            if (rows[i] == selected) at = i;
        const int first = std::clamp(at - room / 2, 0, std::max(0, static_cast<int>(rows.size()) - room));
        for (int i = first, y = area.top(); i < rows.size() && y < area.top() + room; ++i, ++y) {
            const int index = rows[i];
            if (index == -1000) continue;
            if (index < 0) {
                const int end = this->_canvas.text(2, y, "─ " + fields[-1 - index].section + " ", st(ACCENT, QColor(), true));
                this->_canvas.fill(QRect(end, y, w - end - 2, 1), st(BORDER), U'─');
                continue;
            }
            row(index, 4, y, w - 8);
        }
    }

    // Save line
    const int y = area.bottom();
    if (this->_host.localActionsLocked()) this->_canvas.text(2, y, "▲ Locked: a remote session controls this PC or has a terminal: settings cannot be saved now", st(WARN, QColor(), true), w - 4);
    else if (dirty > 0) {
        const int end = this->_canvas.text(2, y, "● " + QString::number(dirty) + " unsaved change(s)   ", st(WARN, QColor(), true), w - 4);
        const int save = this->_canvas.text(end, y, " ✓ Save  ctrl+s ", st(DARK, ACCENT, true));
        const int revert = this->_canvas.text(save + 1, y, " ↺ Revert  r ", st(DARK, MUTED, true));
        this->_hits.append(Hit{QRect(end, y, save - end, 1), [this]() {this->saveSettings_();}});
        this->_hits.append(Hit{QRect(save + 1, y, revert - save - 1, 1), [this]() {this->pageKey_("r");}});
    } else this->_canvas.text(2, y, "✓ Saved in " + core::Config::configDir() + "/config.json", st(BORDER_TEXT), w - 4);
}

void xdesk::ui::tty::HostTui::drawFooter_(void)
{
    const int w = this->_canvas.width();
    const int h = this->_canvas.height();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    int x = 1;

    // Status line
    if (!this->_flash.isEmpty() && now - this->_flashAt < FLASH_TIME) this->_canvas.text(2, h - 2, (this->_flashBad ? "✗ " : "✓ ") + this->_flash, st(this->_flashBad ? BAD : ACCENT, QColor(), true), w - 4);

    // Buttons of the page (a click = the key)
    this->_canvas.fill(QRect(0, h - 1, w, 1), st(QColor(), BAR));
    QList<QPair<QString, QString>> buttons;
    switch (this->_page) {
        case Page::Overview: buttons = {{"s", this->_host.isSharing() ? "Stop sharing" : "Start sharing"}, {"n", "New code"}, {"y", "Copy code"}, {"enter", "QR code"}, {"c", "Copy link"}, {"e", "Exposure"}}; break;
        case Page::Sessions: buttons = {{"v/c/t/p", "Rights"}, {"m", "Watch terminals"}, {"k", "Disconnect"}, {"b", "Ban"}, {"K", "Disconnect all"}}; break;
        case Page::Devices:  buttons = {{"T", "Trust"}, {"U", "Untrust"}, {"u", "Unban"}, {"f", "Forget"}}; break;
        case Page::Logs:     buttons = {{"l", "Level"}, {"c", "Category"}, {"/", "Search"}, {"esc", "Clear"}, {"enter", "Details"}, {"f", "Follow"}}; break;
        case Page::Settings: buttons = {{"←→", "Change"}, {"enter", "Edit"}, {"ctrl+s", "Save"}, {"r", "Revert"}}; break;
    }
    buttons.append(QPair<QString, QString>("?", "Help"));
    buttons.append(QPair<QString, QString>("q", "Quit"));
    for (const QPair<QString, QString>& b: buttons) {
        if (x + b.first.size() + b.second.size() + 5 > w) break;
        const QString key = b.first == "v/c/t/p" ? "v" : (b.first == "←→" ? "right" : b.first);
        const int start = x;
        x = this->_canvas.text(this->_canvas.text(x, h - 1, " " + b.first + " ", st(DARK, b.first == "q" ? BAD : MUTED, true)), h - 1, " " + b.second + " ", st(QColor(220, 224, 228), BAR));
        x = this->_canvas.text(x, h - 1, " ", st(QColor(), BAR));
        this->_hits.append(Hit{QRect(start, h - 1, x - start, 1), [this, key]() {this->onKey_(key, key.size() == 1 ? key.toUtf8() : QByteArray());}});
    }
}

void xdesk::ui::tty::HostTui::drawModal_(void)
{
    const int w = this->_canvas.width();
    const int h = this->_canvas.height();
    const Style border = st(ACCENT, DARK);
    const Style inside = st(QColor(), DARK);
    QStringList lines;
    QRect box;

    this->_hits.clear(); // only the modal reacts
    this->_hits.append(Hit{QRect(0, 0, w, h), [this]() {if (this->_modal == Modal::Help || this->_modal == Modal::Text) this->_modal = Modal::None;}});
    switch (this->_modal) {
        case Modal::Help:
            lines = {
                "Everywhere",
                "  1-5 / tab / click        pages",
                "  ↑ ↓ pgup pgdn / wheel    select",
                "  s                        start / stop sharing",
                "  x                        dismiss the tampering banner",
                "  q / ctrl+c               quit (sessions disconnected)",
                "  a / t / r                accept / accept & trust / refuse a connection",
                "Overview",
                "  n new code   y copy the code   enter QR code of the address   c copy its link",
                "  e / ← →   who can reach this PC (LAN, Tailscale, tunnels...)",
                "Sessions",
                "  v c t p      view / control / terminal / clipboard rights",
                "  m / enter    watch the remote terminals live (read only)",
                "  k disconnect   b ban (device + address)   K disconnect everybody",
                "Devices",
                "  T trust   U untrust   f forget   u unban (type unban)",
                "Logs",
                "  l level   c category   / search   esc clear   enter details   f follow",
                "Settings",
                "  ← → change   space toggle   enter edit   ctrl+s / w save   r revert",
                "",
                "Mouse: tabs, buttons, rows (a second click opens / changes), ◂ ▸ of a value, ✕ closes",
                "Copy: y / c use the terminal clipboard (OSC 52); shift + drag selects text too",
            };
            box = QRect((w - 86) / 2, std::max(1, (h - static_cast<int>(lines.size()) - 4) / 2), std::min(86, w - 2), std::min(h - 2, static_cast<int>(lines.size()) + 4));
            this->_canvas.box(box, "Keys", border, inside);
            for (int i = 0; i < lines.size() && i < box.height() - 4; ++i)
                this->_canvas.text(box.left() + 2, box.top() + 2 + i, lines[i], lines[i].startsWith(' ') || lines[i].isEmpty() ? inside : st(ACCENT, DARK, true), box.width() - 4);
            break;
        case Modal::Text: {
            const bool qr = this->_modalTitle.startsWith("QR code");
            const QStringList shown = qr ? this->_modalLines.mid(0, this->_modalLines.size() - 1) : this->_modalLines;
            int width = static_cast<int>(this->_modalTitle.size()) + 6;
            for (const QString& line: shown)
                width = std::max(width, static_cast<int>(line.size()) + 4);
            box = QRect(std::max(0, (w - width) / 2), 1, std::min(width, w), std::min(h - 2, static_cast<int>(shown.size()) + 4));
            this->_canvas.box(box, this->_modalTitle, border, inside);
            this->_modalScroll = std::clamp(this->_modalScroll, 0, std::max(0, static_cast<int>(shown.size()) - (box.height() - 4)));
            for (int i = 0; i + this->_modalScroll < shown.size() && i < box.height() - 4; ++i) {
                const QString& line = shown[i + this->_modalScroll];
                const bool code = qr && i + this->_modalScroll < shown.size() - 4;
                this->_canvas.text(box.left() + 2, box.top() + 2 + i, line, code ? st(QColor(Qt::white), QColor(Qt::black)) : inside, box.width() - 4);
            }
            if (qr && box.height() - 4 >= shown.size()) {
                // Clickable hints of the last line
                const QString& hint = shown.last();
                const int row = box.top() + 1 + static_cast<int>(shown.size());
                for (const QPair<QString, QString>& part: QList<QPair<QString, QString>>{{"tab: app / browser link", "tab"}, {"c: copy the link", "c"}, {"esc: close", "esc"}}) {
                    const int at = static_cast<int>(hint.indexOf(part.first));
                    const QString key = part.second;
                    if (at >= 0) this->_hits.append(Hit{QRect(box.left() + 2 + at, row, static_cast<int>(part.first.size()), 1), [this, key]() {this->onKey_(key, key == "c" ? QByteArray("c") : QByteArray());}});
                }
            }
            if (qr && box.height() - 4 < shown.size()) this->_canvas.text(box.left() + 2, box.bottom(), " terminal too small for the whole code: make it taller ", st(WARN, DARK, true), box.width() - 4);
            break;
        }
        case Modal::Input:
        case Modal::Confirm: {
            const bool word = this->_modal == Modal::Confirm && !this->_confirmWord.isEmpty();
            const int width = std::min(w - 4, std::max(60, static_cast<int>(this->_modalTitle.size()) + 6));
            box = QRect((w - width) / 2, h / 2 - 3, width, 6);
            this->_canvas.box(box, this->_modal == Modal::Input ? "Edit" : "Confirm", border, inside);
            this->_canvas.text(box.left() + 2, box.top() + 2, this->_modalTitle, st(QColor(), DARK, true), box.width() - 4);
            if (this->_modal == Modal::Input || word) {
                const QString prompt = word ? "type " + this->_confirmWord + ": " : "> ";
                const int x = this->_canvas.text(box.left() + 2, box.top() + 3, prompt, st(MUTED, DARK));
                const int end = this->_canvas.text(x, box.top() + 3, this->_input.right(box.width() - 6 - static_cast<int>(prompt.size())), st(ACCENT, DARK, true));
                this->_canvas.text(end, box.top() + 3, "█", st(ACCENT, DARK));
                const int ok = this->_canvas.text(box.left() + 2, box.bottom(), " enter confirm ", st(DARK, ACCENT, true));
                const int cancel = this->_canvas.text(ok + 1, box.bottom(), " esc cancel ", st(DARK, MUTED, true));
                this->_hits.append(Hit{QRect(box.left() + 2, box.bottom(), ok - box.left() - 2, 1), [this]() {this->onKey_("enter", "\r");}});
                this->_hits.append(Hit{QRect(ok + 1, box.bottom(), cancel - ok - 1, 1), [this]() {this->onKey_("esc", "\x1b");}});
            } else {
                const int yes = this->_canvas.text(box.left() + 2, box.top() + 3, " y  Yes ", st(DARK, ACCENT, true));
                const int no = this->_canvas.text(yes + 2, box.top() + 3, " n  No ", st(DARK, MUTED, true));
                this->_hits.append(Hit{QRect(box.left() + 2, box.top() + 3, yes - box.left() - 2, 1), [this]() {this->onKey_("y", "y");}});
                this->_hits.append(Hit{QRect(yes + 2, box.top() + 3, no - yes - 2, 1), [this]() {this->onKey_("n", "n");}});
            }
            break;
        }
        case Modal::Mirror: {
            const gui::VtScreen* vt = this->_mirrors.value(this->_mirror);
            const QStringList keys = this->mirrorsOf_(this->_mirror.section('/', 0, 0));
            const host::Session* s = this->_host.session(this->_mirror.section('/', 0, 0));
            box = QRect(0, 1, w, h - 1);
            this->_canvas.box(box, "Terminal " + this->_mirror.section('/', 1) + " of " + (s ? s->name() : QString("?")) + " (read only)" + (keys.size() > 1 ? "  ← → other terminal" : ""), st(WARN, DARK), st());
            if (!vt) break;
            this->_mirrorScroll = std::clamp(this->_mirrorScroll, 0, vt->scrollbackSize());
            for (int row = 0; row < vt->rows() && row < box.height() - 2; ++row)
                this->_canvas.vtLine(1, box.top() + 1 + row, vt->line(row, this->_mirrorScroll), box.width() - 2);
            {
                int x = this->_canvas.text(2, box.bottom(), " esc close ", st(DARK, MUTED, true));
                this->_hits.append(Hit{QRect(2, box.bottom(), x - 2, 1), [this]() {this->onKey_("esc", "\x1b");}});
                if (keys.size() > 1) {
                    const int prev = x + 1;
                    x = this->_canvas.text(prev, box.bottom(), " ◂ ", st(DARK, MUTED, true));
                    this->_hits.append(Hit{QRect(prev, box.bottom(), x - prev, 1), [this]() {this->onKey_("left", QByteArray());}});
                    const int next = x + 1;
                    x = this->_canvas.text(next, box.bottom(), " ▸ ", st(DARK, MUTED, true));
                    this->_hits.append(Hit{QRect(next, box.bottom(), x - next, 1), [this]() {this->onKey_("right", QByteArray());}});
                }
                this->_canvas.text(x + 1, box.bottom(), " wheel / ↑ ↓ pgup pgdn scroll" + (this->_mirrorScroll ? " · " + QString::number(this->_mirrorScroll) + " lines up (end: bottom)" : QString()) + " ", st(MUTED, DARK), box.width() - x - 3);
            }
            break;
        }
        case Modal::None: break;
    }
    if (box.width() > 8 && this->_modal != Modal::None) {
        // Close button in the corner of the frame
        const int x = box.right() - 4;
        this->_canvas.text(x, box.top(), " ✕ ", st(DARK, this->_modal == Modal::Mirror ? WARN : ACCENT, true));
        this->_hits.append(Hit{QRect(x, box.top(), 3, 1), [this]() {this->onKey_("esc", "\x1b");}});
    }
}

void xdesk::ui::tty::HostTui::drawApproval_(void)
{
    const int w = this->_canvas.width();
    const int h = this->_canvas.height();
    const host::Session* s = this->_host.session(this->_approval);
    const QColor back(60, 48, 18);
    QRect box;
    int x = 0;

    if (!s) return;
    box = QRect((w - std::min(w - 4, 72)) / 2, h / 2 - 4, std::min(w - 4, 72), 8);
    this->_hits.clear();
    this->_canvas.box(box, "Someone wants to connect", st(WARN, back, true), st(QColor(), back));
    this->_canvas.text(box.left() + 2, box.top() + 2, s->name(), st(QColor(), back, true), box.width() - 4);
    this->_canvas.text(box.left() + 2, box.top() + 3, net::client_kind_name(s->kind()) + ", " + s->os() + ", from " + s->address() + "  ·  typed the right code", st(QColor(220, 220, 200), back), box.width() - 4);
    x = box.left() + 2;
    for (const QPair<QString, QString>& b: QList<QPair<QString, QString>>{{"a", "Accept"}, {"t", "Accept & trust"}, {"r", "Refuse"}}) {
        const int start = x;
        const QString key = b.first;
        x = this->_canvas.text(x, box.top() + 5, " " + b.first + "  " + b.second + " ", st(DARK, b.first == "r" ? BAD : (b.first == "t" ? ACCENT : GOOD), true));
        this->_hits.append(Hit{QRect(start, box.top() + 5, x - start, 1), [this, key]() {this->onKey_(key, key.toUtf8());}});
        x += 2;
    }
    this->_canvas.text(box.left() + 2, box.bottom(), " refused in 60 s without an answer ", st(QColor(200, 190, 150), back));
}
