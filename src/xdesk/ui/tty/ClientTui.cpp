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
##  @file ClientTui.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/tty/ClientTui.hpp"
#include "xdesk/net/Exposure.hpp"
#include "xdesk/net/Protocol.hpp"
#include <QJsonDocument>
#include <QDateTime>
#include <algorithm>
#include <QPointer>
#include <iostream>
#include <QThread>
#include <QImage>
#include <QFile>

//----------------------------------------------------------------//
/* DEFINE */

#define PREFIX 0x1d // ctrl+]
#define NOTE_TIME 4000 // ms a note of the bottom bar stays

//----------------------------------------------------------------//
/* TOOLS */

static const QColor ACCENT(61, 220, 151);
static const QColor MUTED(128, 136, 150);
static const QColor WARN(230, 190, 100);
static const QColor BAD(240, 110, 110);
static const QColor BAR(26, 30, 36);
static const QColor SELECTED(34, 66, 54);
static const QColor DARK(12, 14, 16);
static const QByteArray BRIGHTNESS(" .:-=+*#%@"); // remote screen without blocks or colors: one character per cell, darkest first

static xdesk::ui::tty::Style st(const QColor& fg = QColor(), const QColor& bg = QColor(), const bool bold = false)
{
    xdesk::ui::tty::Style style;

    style.fg = fg;
    style.bg = bg;
    style.bold = bold;
    return style;
}

// Keys of the screen tab -> KeyboardEvent.code
static const QHash<QString, QString> REMOTE_KEYS = {
    {"up", "ArrowUp"}, {"down", "ArrowDown"}, {"right", "ArrowRight"}, {"left", "ArrowLeft"}, {"home", "Home"}, {"end", "End"},
    {"pgup", "PageUp"}, {"pgdn", "PageDown"}, {"del", "Delete"}, {"ins", "Insert"}, {"enter", "Enter"}, {"tab", "Tab"},
    {"backspace", "Backspace"}, {"esc", "Escape"}, {"f1", "F1"}, {"f2", "F2"}, {"f3", "F3"}, {"f4", "F4"}, {"f5", "F5"},
    {"f6", "F6"}, {"f7", "F7"}, {"f8", "F8"}, {"f9", "F9"}, {"f10", "F10"}, {"f11", "F11"}, {"f12", "F12"},
};

//----------------------------------------------------------------//
/* CLASS */

xdesk::ui::tty::ClientTui::ClientTui(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_term, &Terminal::input, this, &ClientTui::onInput_);
    QObject::connect(&this->_term, &Terminal::resized, this, [this](const QSize& size) {
        for (const std::uint8_t id: this->_terminals) {
            if (this->_exited.contains(id)) continue;
            this->_vts.value(id)->resize(size.width(), std::max(2, size.height() - 1));
            this->_client.resizeTerminal(id, size.width(), std::max(2, size.height() - 1));
        }
        this->sendQuality_();
        this->_dirty = true;
    });
    QObject::connect(&this->_frameTimer, &QTimer::timeout, this, [this]() {
        if (this->_dirty || (!this->_note.isEmpty() && QDateTime::currentMSecsSinceEpoch() - this->_noteAt > NOTE_TIME)) this->draw_();
    });
    QObject::connect(&this->_client, &client::Client::frameUpdated, this, [this](const QRect&) {
        if (this->_tab == 0) this->_dirty = true;
        if (this->_picture.isEmpty()) this->sendQuality_(); // first frame: its size is known
    });
    QObject::connect(&this->_client, &client::Client::cursorMoved, this, [this](const QPoint& position, const bool visible) {
        this->_cursor = position;
        this->_cursorVisible = visible;
        if (this->_tab == 0) this->_dirty = true;
    });
    QObject::connect(&this->_client, &client::Client::message, this, [this](const QString& text) {this->note_(text);});
    QObject::connect(&this->_client, &client::Client::latencyChanged, this, [this](const int) {this->_dirty = true;});
    QObject::connect(&this->_client, &client::Client::fingerprintCheck, this, [this](const QString& fingerprint, const bool changed) {
        this->_fingerprint = fingerprint;
        this->_fingerprintChanged = changed;
        this->_typed.clear();
        this->_dirty = true;
    });
    QObject::connect(&this->_client, &client::Client::stateChanged, this, [this](const client::State state) {
        switch (state) {
            case client::State::Connecting:     this->_status = "Connecting (TLS 1.3, post-quantum key exchange)..."; break;
            case client::State::Authenticating: this->_status = this->_client.options().code.isEmpty() ? "Authenticating (trusted device secret)..." : "Authenticating (Argon2id proof of the code)..."; break;
            case client::State::Pending:        this->_status = "Waiting for the person in front of the PC to accept..."; break;
            case client::State::Active:
                this->_view = ClientView::Session;
                this->_fingerprint.clear();
                this->_tab = 0;
                if (!this->_startScreen && this->_client.permissions().value("terminal").toBool()) this->openTerminal_();
                else if (!this->_client.permissions().value("view").toBool()) this->note_("The host gave no right to see the screen: ctrl+] t opens a terminal when allowed");
                this->selectTab_(this->_tab);
                this->sendQuality_();
                break;
            default: break;
        }
        this->_dirty = true;
    });
    QObject::connect(&this->_client, &client::Client::permissionsChanged, this, [this](const QJsonObject& perms) {
        if (!perms.value("control").toBool() && this->_control) {this->_control = false; this->note_("The host took the control back");}
        this->_dirty = true;
    });
    QObject::connect(&this->_client, &client::Client::terminalOutput, this, [this](const std::uint8_t id, const QByteArray& data) {
        gui::VtScreen* vt = this->_vts.value(id);
        if (!vt) return;
        vt->feed(data);
        if (this->currentVt_() == vt) this->_dirty = true;
    });
    QObject::connect(&this->_client, &client::Client::terminalClosed, this, [this](const std::uint8_t id, const int code) {
        gui::VtScreen* vt = this->_vts.value(id);
        if (!vt) return;
        this->_exited.insert(id, code);
        vt->feed("\r\n\x1b[2m[remote shell ended (code " + QByteArray::number(code) + "): ctrl+] w closes this tab]\x1b[0m\r\n");
        // The only thing open and nothing else to see: done
        if (this->_terminals.size() == 1 && !this->_client.permissions().value("view").toBool()) {
            this->_client.disconnectFromHost();
            this->finish_("Remote shell ended (code " + QString::number(code) + ")", 0);
            return;
        }
        this->_dirty = true;
    });
    QObject::connect(&this->_client, &client::Client::closed, this, [this](const QString& reason, const bool banned) {
        const QString text = reason + (banned ? " (banned: only the person in front of the host can unban this device)" : "");
        if (this->_fromConnectPage && !banned) {
            // Back to the connect page with the reason
            for (gui::VtScreen* vt: this->_vts)
                vt->deleteLater();
            this->_vts.clear();
            this->_terminals.clear();
            this->_exited.clear();
            this->_control = false;
            this->_fingerprint.clear();
            this->_error = text;
            this->_view = ClientView::Connect;
            this->_term.setMouse(true);
            this->_dirty = true;
            return;
        }
        this->finish_(text, banned ? 3 : 1);
    });
}

void xdesk::ui::tty::ClientTui::start(const client::Options& options, const bool screen)
{
    this->_options = options;
    this->_startScreen = screen;
    this->_term.enableRaw();
    this->_term.enterAlternate();
    this->_term.setMouse(true);
    this->_frameTimer.start(16);
    if (options.host.isEmpty()) {
        this->_fromConnectPage = true;
        this->_view = ClientView::Connect;
        this->_formPort = QString::number(options.port);
        this->_formCode = options.code;
        this->_formViewOnly = options.viewOnly;
        this->_focus = 1;
        this->loadHosts_();
        this->scan_();
    } else {
        this->_view = ClientView::Status;
        this->_status = "Connecting to " + options.host + ":" + QString::number(options.port) + "...";
        this->_client.connectToHost(options);
    }
    this->draw_();
}

void xdesk::ui::tty::ClientTui::finish_(const QString& reason, const int code)
{
    this->_frameTimer.stop();
    this->_term.restore();
    std::cerr << "xdesk: " << reason.toStdString() << std::endl;
    this->_exitCode = code;
    emit this->quit();
}

void xdesk::ui::tty::ClientTui::note_(const QString& text)
{
    this->_note = text;
    this->_noteAt = QDateTime::currentMSecsSinceEpoch();
    this->_dirty = true;
}

xdesk::ui::gui::VtScreen* xdesk::ui::tty::ClientTui::currentVt_(void) const
{
    if (this->_tab <= 0 || this->_tab > this->_terminals.size()) return nullptr;
    return this->_vts.value(this->_terminals[this->_tab - 1]);
}

//----------------------------------------------------------------//
// Connect page

void xdesk::ui::tty::ClientTui::loadHosts_(void)
{
    QFile file(client::Client::knownHostsPath());
    const QJsonObject hosts = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
    QList<QJsonObject> known;

    for (QJsonObject::const_iterator it = hosts.constBegin(); it != hosts.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        known.append({{"label", entry.value("name").toString(it.key())}, {"host", it.key().section(':', 0, -2)}, {"port", it.key().section(':', -1).toInt()},
            {"trusted", entry.contains("secret")}, {"last", entry.value("last").toInteger()}});
    }
    std::sort(known.begin(), known.end(), [](const QJsonObject& a, const QJsonObject& b) {return a.value("last").toInteger() > b.value("last").toInteger();});
    this->_hosts = known;
}

void xdesk::ui::tty::ClientTui::scan_(void)
{
    QPointer<ClientTui> self = this;
    QThread* thread = nullptr;

    if (this->_scanning) return;
    this->_scanning = true;
    thread = QThread::create([self]() {
        const QList<net::Discovered> found = net::Exposure::scan(2500);
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, found]() {
            if (!self) return;
            self->_scanning = false;
            for (const net::Discovered& d: found) {
                bool known = false;
                for (QJsonObject& h: self->_hosts)
                    if (h.value("host").toString() == d.host && h.value("port").toInt() == d.port) {h["found"] = true; known = true;}
                if (!known) self->_hosts.append({{"label", d.name}, {"host", d.host}, {"port", d.port}, {"trusted", false}, {"found", true}, {"id", d.fingerprint}});
            }
            self->note_(QString::number(found.size()) + " host(s) announced on this LAN");
        }, Qt::QueuedConnection);
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    this->_dirty = true;
}

void xdesk::ui::tty::ClientTui::connectNow_(void)
{
    client::Options options = this->_options;
    QString host = this->_formHost.trimmed();

    // host[:port] and https://host:port accepted (copied from the host panel)
    if (host.startsWith("https://")) host = host.mid(8);
    if (host.endsWith('/')) host.chop(1);
    if (host.count(':') == 1) {
        this->_formPort = host.section(':', 1);
        host = host.section(':', 0, 0);
    }
    if (host.isEmpty()) {this->_error = "Type the address shown by the host (or pick one above)"; this->_focus = 1; return;}
    options.host = host;
    options.port = static_cast<quint16>(std::clamp(this->_formPort.toInt(), 1, 65535));
    options.code = this->_formCode.trimmed();
    options.viewOnly = this->_formViewOnly;
    options.kind = net::ClientKind::Tty;
    this->_error.clear();
    this->_view = ClientView::Status;
    this->_status = "Connecting to " + host + ":" + QString::number(options.port) + "...";
    this->_client.connectToHost(options);
}

//----------------------------------------------------------------//
// Session

void xdesk::ui::tty::ClientTui::openTerminal_(void)
{
    const QSize size = this->_term.size();
    std::uint8_t id = 0;
    gui::VtScreen* vt = nullptr;

    if (!this->_client.permissions().value("terminal").toBool()) {this->note_("The host did not allow terminals for this session"); return;}
    id = this->_client.openTerminal(size.width(), std::max(2, size.height() - 1));
    if (id == 0) return;
    vt = new gui::VtScreen(size.width(), std::max(2, size.height() - 1), this);
    QObject::connect(vt, &gui::VtScreen::reply, this, [this, id](const QByteArray& data) {this->_client.terminalInput(id, data);}); // cursor position / device answers
    QObject::connect(vt, &gui::VtScreen::titleChanged, this, [this]() {this->_dirty = true;});
    this->_vts.insert(id, vt);
    this->_terminals.append(id);
    this->selectTab_(static_cast<int>(this->_terminals.size()));
}

void xdesk::ui::tty::ClientTui::closeTab_(void)
{
    const int index = this->_tab - 1;
    std::uint8_t id = 0;

    if (index < 0 || index >= this->_terminals.size()) return;
    id = this->_terminals.takeAt(index);
    this->_client.closeTerminal(id); // its id is free again (the host ignores the close of an ended one)
    this->_exited.remove(id);
    if (this->_vts.contains(id)) this->_vts.take(id)->deleteLater();
    this->selectTab_(std::min(this->_tab, static_cast<int>(this->_terminals.size())));
}

void xdesk::ui::tty::ClientTui::selectTab_(const int tab)
{
    const bool screen = tab <= 0;

    this->_tab = std::clamp(tab, 0, static_cast<int>(this->_terminals.size()));
    this->_scroll = 0;
    if (this->_paused != (this->_tab != 0)) {
        this->_paused = this->_tab != 0;
        this->_client.setPaused(this->_paused); // no frame behind the terminal bytes
    }
    (void)screen;
    this->_term.invalidate();
    this->_dirty = true;
}

void xdesk::ui::tty::ClientTui::sendQuality_(void)
{
    const QSize term = this->_term.size();
    const QImage& canvas = this->_client.canvas();
    double scale = 1.0;

    if (this->_client.state() != client::State::Active) return;
    // Two pixels per cell are enough: the host sends small frames (its minimum is a quarter)
    if (!canvas.isNull() && canvas.width() > 0 && canvas.height() > 0) {
        const double fitted = std::min(static_cast<double>(term.width()) / canvas.width(), static_cast<double>(std::max(1, term.height() - 1) * 2) / canvas.height());
        scale = std::clamp(fitted * 1.5, 0.25, 1.0); // a bit more for the smooth scaling
    } else scale = 0.5;
    this->_client.setQuality(this->_quality, scale, this->_fps);
}

//----------------------------------------------------------------//
// Input

void xdesk::ui::tty::ClientTui::onInput_(const QByteArray& data)
{
    for (const QByteArray& key: split_keys(data)) {
        const QString name = key_name(key);
        switch (this->_view) {
            case ClientView::Connect: this->connectKey_(name, key); break;
            case ClientView::Status:  this->statusKey_(name, key); break;
            case ClientView::Session: this->sessionKey_(name, key); break;
        }
    }
    this->_dirty = true;
}

void xdesk::ui::tty::ClientTui::connectKey_(const QString& key, const QByteArray& raw)
{
    Mouse mouse;
    QString* field = this->_focus == 1 ? &this->_formHost : (this->_focus == 2 ? &this->_formPort : (this->_focus == 3 ? &this->_formCode : nullptr));

    if (parse_mouse(raw, mouse)) {
        if (mouse.wheel != 0 && this->_focus == 0) this->_hostRow = std::clamp(this->_hostRow - mouse.wheel, 0, std::max(0, static_cast<int>(this->_hosts.size()) - 1));
        if (!mouse.press || mouse.motion || mouse.button != 0 || mouse.wheel != 0) return;
        for (qsizetype i = this->_hits.size() - 1; i >= 0; --i)
            if (this->_hits[i].area.contains(mouse.col, mouse.row)) {const std::function<void(void)> action = this->_hits[i].action; action(); return;}
        return;
    }
    if (key == "ctrl+c" || (key == "esc" && this->_focus == 0)) {this->finish_("Cancelled", 1); return;}
    if (key == "esc") {this->_focus = 0; return;}
    if (key == "tab" || (key == "down" && this->_focus > 0)) {this->_focus = (this->_focus + 1) % 6; return;}
    if (key == "shift+tab" || (key == "up" && this->_focus > 1)) {this->_focus = (this->_focus + 5) % 6; return;}
    if (this->_focus == 0) {
        if (key == "up") this->_hostRow = std::max(0, this->_hostRow - 1);
        else if (key == "down") {
            if (this->_hostRow + 1 < this->_hosts.size()) this->_hostRow++;
            else this->_focus = 1;
        } else if (key == "s" || key == "r") this->scan_();
        else if (key == "enter" && !this->_hosts.isEmpty()) {
            const QJsonObject h = this->_hosts.value(std::clamp(this->_hostRow, 0, static_cast<int>(this->_hosts.size()) - 1));
            this->_formHost = h.value("host").toString();
            this->_formPort = QString::number(h.value("port").toInt());
            if (h.value("trusted").toBool()) {this->_formCode.clear(); this->connectNow_();} // trusted device: no code
            else this->_focus = 3;
        }
        return;
    }
    if (this->_focus == 4 && (key == "space" || key == "enter")) {this->_formViewOnly = !this->_formViewOnly; return;}
    if (this->_focus == 5 || key == "enter") {if (key == "enter" || key == "space") this->connectNow_(); return;}
    if (!field) return;
    if (key == "backspace") field->chop(1);
    else if (key == "ctrl+u") field->clear();
    else if (key == "paste") *field += QString::fromUtf8(paste_text(raw)).trimmed();
    else if (!raw.isEmpty() && static_cast<unsigned char>(raw[0]) >= 0x20 && raw[0] != 0x7f && !raw.startsWith('\x1b')) {
        if (this->_focus == 2 && !key.at(0).isDigit()) return;
        *field += key == "space" ? QString(" ") : QString::fromUtf8(raw);
        if (this->_focus == 3) *field = field->toUpper();
    }
}

void xdesk::ui::tty::ClientTui::statusKey_(const QString& key, const QByteArray& raw)
{
    Mouse mouse;

    if (parse_mouse(raw, mouse)) {
        if (!mouse.press || mouse.motion || mouse.button != 0 || mouse.wheel != 0) return;
        for (qsizetype i = this->_hits.size() - 1; i >= 0; --i)
            if (this->_hits[i].area.contains(mouse.col, mouse.row)) {const std::function<void(void)> action = this->_hits[i].action; action(); return;}
        return;
    }
    if (!this->_fingerprint.isEmpty()) {
        // First connection: y / n; a changed identity: the word yes typed
        if (!this->_fingerprintChanged && (key == "y" || key == "enter")) {this->_fingerprint.clear(); this->_client.acceptFingerprint(true, true); return;}
        if (key == "n" || key == "esc" || key == "ctrl+c") {this->_fingerprint.clear(); this->_client.acceptFingerprint(false); return;}
        if (this->_fingerprintChanged) {
            if (key == "enter") {
                const bool yes = this->_typed.trimmed().compare("yes", Qt::CaseInsensitive) == 0;
                this->_fingerprint.clear();
                this->_client.acceptFingerprint(yes, true);
            } else if (key == "backspace") this->_typed.chop(1);
            else if (key.size() == 1) this->_typed += key;
        }
        return;
    }
    if (key == "ctrl+c" || key == "esc" || key == "q") {
        this->_client.disconnectFromHost();
        if (!this->_fromConnectPage) this->finish_("Cancelled", 1);
    }
}

void xdesk::ui::tty::ClientTui::sessionKey_(const QString& key, const QByteArray& raw)
{
    Mouse mouse;
    gui::VtScreen* vt = this->currentVt_();

    // Mouse: the bottom bar is ours, the rest goes to the screen / the remote program, the wheel scrolls back
    if (parse_mouse(raw, mouse)) {
        if (mouse.row == this->_term.size().height() - 1 || this->_help) {
            if (!mouse.press || mouse.motion || mouse.button != 0 || mouse.wheel != 0) return;
            if (this->_help) {this->_help = false; return;}
            for (qsizetype i = this->_hits.size() - 1; i >= 0; --i)
                if (this->_hits[i].area.contains(mouse.col, mouse.row)) {const std::function<void(void)> action = this->_hits[i].action; action(); return;}
            return;
        }
        if (vt) this->terminalMouse_(mouse, raw);
        else this->screenMouse_(mouse);
        return;
    }

    // Prefix
    if (this->_prefix) {
        this->_prefix = false;
        if (raw.size() == 1 && raw[0] == PREFIX) {
            if (vt) this->_client.terminalInput(this->_terminals[this->_tab - 1], raw);
            return;
        }
        this->prefixKey_(key);
        return;
    }
    if (raw.size() == 1 && raw[0] == PREFIX) {this->_prefix = true; return;}
    if (this->_help) {this->_help = false; return;}

    // Terminal tab: the bytes as the remote program expects them
    if (vt) {
        const std::uint8_t id = this->_terminals[this->_tab - 1];
        QByteArray out = raw;
        if (this->_exited.contains(id)) return;
        if (key == "paste") out = vt->bracketedPaste() ? raw : paste_text(raw);
        else if (vt->appCursor() && raw.size() == 3 && raw.startsWith("\x1b[") && QByteArray("ABCDHF").contains(raw[2])) out = "\x1bO" + raw.mid(2); // application cursor keys
        this->_scroll = 0;
        this->_client.terminalInput(id, out);
        return;
    }
    this->screenKey_(key, raw);
}

void xdesk::ui::tty::ClientTui::prefixKey_(const QString& key)
{
    gui::VtScreen* vt = this->currentVt_();

    if (key == "q") {this->_client.disconnectFromHost(); this->finish_("Disconnected", 0);}
    else if (key == "s" || key == "0") this->selectTab_(0);
    else if (key == "t") this->openTerminal_();
    else if (key == "w") this->closeTab_();
    else if (key == "n" || key == "right" || key == "tab") this->selectTab_((this->_tab + 1) % (static_cast<int>(this->_terminals.size()) + 1));
    else if (key == "p" || key == "left" || key == "shift+tab") this->selectTab_((this->_tab + static_cast<int>(this->_terminals.size())) % (static_cast<int>(this->_terminals.size()) + 1));
    else if (key.size() == 1 && key[0] >= '1' && key[0] <= '9') this->selectTab_(key[0].unicode() - '0');
    else if (key == "?" || key == "h") this->_help = true;
    else if (key == "c") {
        if (this->_client.options().viewOnly) {this->note_("View only (asked when connecting)"); return;}
        if (!this->_control && !this->_client.permissions().value("control").toBool()) {this->note_("The host did not allow the control for this session"); return;}
        this->_control = !this->_control;
        this->_client.requestMode(this->_control ? "control" : "view");
        this->note_(this->_control ? "Control on: keys and mouse go to the remote screen" : "View only");
        if (this->_control && this->_tab != 0) this->selectTab_(0);
    } else if (key == "d") {
        if (!this->_control) {this->note_("Take the control first (ctrl+] c)"); return;}
        for (const QString& code: {"ControlLeft", "AltLeft", "Delete"})
            this->_client.key(code, true);
        for (const QString& code: {"Delete", "AltLeft", "ControlLeft"})
            this->_client.key(code, false);
        this->note_("Ctrl+Alt+Del sent");
    } else if (key == "+" || key == "=") {this->_quality = std::min(95, this->_quality + 10); this->sendQuality_(); this->note_("Quality " + QString::number(this->_quality));}
    else if (key == "-") {this->_quality = std::max(20, this->_quality - 10); this->sendQuality_(); this->note_("Quality " + QString::number(this->_quality));}
    else if (key == "f") {this->_fps = this->_fps >= 30 ? 5 : (this->_fps >= 15 ? 30 : 15); this->sendQuality_(); this->note_(QString::number(this->_fps) + " frames per second");}
    else if (key == "pgup" || key == "[" || key == "up") {if (vt) this->_scroll = std::min(vt->scrollbackSize(), this->_scroll + std::max(1, vt->rows() / 2));}
    else if (key == "pgdn" || key == "]" || key == "down") this->_scroll = std::max(0, this->_scroll - std::max(1, vt ? vt->rows() / 2 : 1));
    else if (key == "r") {this->_client.setQuality(this->_quality, 1.0, this->_fps); this->sendQuality_(); this->note_("Refreshed");}
}

void xdesk::ui::tty::ClientTui::screenKey_(const QString& key, const QByteArray& raw)
{
    if (!this->_control) {
        if (key == "enter" || key == "space") this->note_("View only: ctrl+] c takes the control, ctrl+] ? shows the keys");
        return;
    }
    if (REMOTE_KEYS.contains(key)) {this->_client.key(REMOTE_KEYS.value(key), true); this->_client.key(REMOTE_KEYS.value(key), false); return;}
    if (key.startsWith("ctrl+") && key.size() == 6) {
        const QString letter = "Key" + key.right(1).toUpper();
        this->_client.key("ControlLeft", true);
        this->_client.key(letter, true);
        this->_client.key(letter, false);
        this->_client.key("ControlLeft", false);
        return;
    }
    if (key.startsWith("alt+")) {
        this->_client.key("AltLeft", true);
        this->_client.text(key.mid(4));
        this->_client.key("AltLeft", false);
        return;
    }
    if (key == "paste") {this->_client.text(QString::fromUtf8(paste_text(raw))); return;}
    if (key == "space") {this->_client.text(" "); return;}
    if (!raw.startsWith('\x1b') && !raw.isEmpty()) this->_client.text(QString::fromUtf8(raw));
}

void xdesk::ui::tty::ClientTui::screenMouse_(const Mouse& mouse)
{
    const QSize frame = this->_client.canvas().size();

    if (!this->_control || frame.isEmpty() || this->_picture.isEmpty() || !this->_picture.contains(mouse.col, mouse.row)) return;
    // Cell -> frame pixels (a cell = 2 pixels high in the picture)
    const int x = (mouse.col - this->_picture.left()) * frame.width() / std::max(1, this->_picture.width());
    const int y = (mouse.row - this->_picture.top()) * frame.height() / std::max(1, this->_picture.height());
    this->_client.mouseMove(x, y);
    if (mouse.motion) return;
    if (mouse.wheel != 0) {this->_client.mouseWheel(0, mouse.wheel * 120); return;}
    this->_client.mouseButton(static_cast<net::MouseButton>(mouse.button), mouse.press);
}

void xdesk::ui::tty::ClientTui::terminalMouse_(const Mouse& mouse, const QByteArray& raw)
{
    gui::VtScreen* vt = this->currentVt_();
    const std::uint8_t id = this->_terminals.value(this->_tab - 1);

    if (!vt) return;
    // The remote program reads the mouse: given as it asked (SGR or X10)
    if (vt->mouseMode() > 0 && this->_scroll == 0) {
        if (mouse.motion && vt->mouseMode() < 1002) return;
        if (vt->mouseSgr()) this->_client.terminalInput(id, raw);
        else {
            QByteArray x10 = "\x1b[M";
            x10 += static_cast<char>(32 + (mouse.press || mouse.wheel ? mouse.raw : 3 | (mouse.raw & ~3)));
            x10 += static_cast<char>(33 + std::min(222, mouse.col));
            x10 += static_cast<char>(33 + std::min(222, mouse.row));
            this->_client.terminalInput(id, x10);
        }
        return;
    }
    // Else the wheel scrolls back
    if (mouse.wheel != 0) this->_scroll = std::clamp(this->_scroll + mouse.wheel * 3, 0, vt->scrollbackSize());
}

//----------------------------------------------------------------//
// Drawing

void xdesk::ui::tty::ClientTui::draw_(void)
{
    const QSize size = this->_term.size();
    QPoint cursor(-1, -1);

    this->_dirty = false;
    this->_canvas.resize(size.width(), size.height());
    this->_hits.clear();
    switch (this->_view) {
        case ClientView::Connect: this->drawConnect_(); break;
        case ClientView::Status:  this->drawStatus_(); break;
        case ClientView::Session: this->drawSession_(); break;
    }

    // Terminal tab: the real cursor where the remote one is
    const gui::VtScreen* vt = this->currentVt_();
    if (this->_view == ClientView::Session && vt && !this->_help && this->_scroll == 0 && vt->cursorVisible()) cursor = vt->cursor();
    else if (this->_view == ClientView::Connect && this->_focus >= 1 && this->_focus <= 3) cursor = QPoint(-1, -1);

    // Mouse reports: motion only when something needs it (control of the screen, a remote program asking)
    const bool motion = this->_view == ClientView::Session && ((this->_tab == 0 && this->_control) || (vt && vt->mouseMode() >= 1002));
    if (motion != this->_mouseMotion) {
        this->_mouseMotion = motion;
        this->_term.setMouse(true, motion);
    }
    this->_term.present(this->_canvas, cursor);
}

void xdesk::ui::tty::ClientTui::drawConnect_(void)
{
    const int w = this->_canvas.width();
    const int h = this->_canvas.height();
    const int left = std::max(2, (w - 76) / 2);
    const int width = std::min(76, w - 4);
    int y = 1;

    this->_canvas.fill(QRect(0, 0, w, 1), st(QColor(), BAR));
    this->_canvas.text(0, 0, " xdesk ", st(DARK, ACCENT, true));
    this->_canvas.text(8, 0, "  connect to a PC  ·  tab / ↑ ↓ / click move  ·  enter connect  ·  esc quit", st(MUTED, BAR));
    if (w > 12) {
        this->_canvas.text(w - 4, 0, " ✕ ", st(DARK, MUTED, true));
        this->_hits.append(Hit{QRect(w - 4, 0, 3, 1), [this]() {this->finish_("Cancelled", 1);}});
    }
    y = 2;

    // Known and found hosts
    {
        const int end = this->_canvas.text(left, y, "HOSTS" + QString(this->_scanning ? "  (scanning the LAN...)" : "  (s or click: scan the LAN again)"), st(this->_focus == 0 ? ACCENT : MUTED, QColor(), true));
        if (!this->_scanning) this->_hits.append(Hit{QRect(left, y, end - left, 1), [this]() {this->scan_();}});
        y++;
    }
    if (this->_hosts.isEmpty()) this->_canvas.text(left + 2, y++, this->_scanning ? "looking for hosts announced on this network..." : "none known yet: type the address shown by the host", st(MUTED), width - 2);
    const int rows = std::max(1, h - 16);
    const int first = std::clamp(this->_hostRow - rows + 1, 0, std::max(0, static_cast<int>(this->_hosts.size()) - rows));
    for (int i = first; i < this->_hosts.size() && i < first + rows; ++i, ++y) {
        const QJsonObject& host = this->_hosts[i];
        const bool current = i == this->_hostRow && this->_focus == 0;
        const Style base = current ? st(QColor(), SELECTED) : st();
        this->_canvas.fill(QRect(left, y, width, 1), base);
        this->_canvas.text(left + 1, y, current ? "▸" : " ", st(ACCENT, base.bg, true));
        this->_canvas.text(left + 3, y, host.value("label").toString(), st(QColor(), base.bg, current), 24);
        const int x = this->_canvas.text(left + 28, y, host.value("host").toString() + ":" + QString::number(host.value("port").toInt()), st(MUTED, base.bg), 28);
        this->_canvas.text(std::max(x + 1, left + 57), y, host.value("trusted").toBool() ? "trusted: no code" : (host.value("found").toBool() ? "on this LAN" : "known"), st(host.value("trusted").toBool() ? ACCENT : MUTED, base.bg), width - 57);
        this->_hits.append(Hit{QRect(left, y, width, 1), [this, i]() {
            if (this->_focus == 0 && this->_hostRow == i) this->connectKey_("enter", "\r");
            this->_focus = 0;
            this->_hostRow = i;
        }});
    }
    y++;

    // Form
    const QList<std::tuple<int, QString, QString>> fields = {{1, "Address", this->_formHost.isEmpty() && this->_focus != 1 ? "192.168.1.20 or https://name:47800" : this->_formHost}, {2, "Port", this->_formPort}, {3, "Code", this->_formCode.isEmpty() && this->_focus != 3 ? "XXX-XXX-XXX (empty for a trusted device)" : this->_formCode}};
    for (const std::tuple<int, QString, QString>& f: fields) {
        const int index = std::get<0>(f);
        const bool focused = this->_focus == index;
        const bool hint = (index == 1 && this->_formHost.isEmpty() && !focused) || (index == 3 && this->_formCode.isEmpty() && !focused);
        this->_canvas.text(left, y, std::get<1>(f), st(focused ? ACCENT : MUTED, QColor(), focused));
        this->_canvas.fill(QRect(left + 10, y, width - 10, 1), st(QColor(), focused ? SELECTED : BAR));
        const int end = this->_canvas.text(left + 11, y, std::get<2>(f), st(hint ? MUTED : QColor(), focused ? SELECTED : BAR, !hint), width - 13);
        if (focused) this->_canvas.text(end, y, "█", st(ACCENT, SELECTED));
        this->_hits.append(Hit{QRect(left, y, width, 1), [this, index]() {this->_focus = index;}});
        y += 2;
    }
    {
        const bool focused = this->_focus == 4;
        const int end = this->_canvas.text(left + 10, y, QString(this->_formViewOnly ? "[x]" : "[ ]") + " View only (never send keyboard / mouse)", st(focused ? ACCENT : QColor(), focused ? SELECTED : QColor(), focused));
        this->_hits.append(Hit{QRect(left + 10, y, end - left - 10, 1), [this]() {this->_focus = 4; this->_formViewOnly = !this->_formViewOnly;}});
        y += 2;
    }
    {
        const int end = this->_canvas.text(left + 10, y, "  Connect  ", st(DARK, ACCENT, true));
        if (this->_focus == 5) this->_canvas.text(left + 8, y, "▸", st(ACCENT, QColor(), true));
        this->_hits.append(Hit{QRect(left + 10, y, end - left - 10, 1), [this]() {this->connectNow_();}});
        y += 2;
    }
    if (!this->_error.isEmpty()) this->_canvas.text(left, y, this->_error, st(BAD, QColor(), true), width);
    if (!this->_note.isEmpty() && QDateTime::currentMSecsSinceEpoch() - this->_noteAt < NOTE_TIME) this->_canvas.text(left, h - 1, this->_note, st(MUTED), width);
}

void xdesk::ui::tty::ClientTui::drawStatus_(void)
{
    const int w = this->_canvas.width();
    const int h = this->_canvas.height();
    const int width = std::min(w - 4, 78);
    const QRect box((w - width) / 2, std::max(1, h / 2 - 5), width, this->_fingerprint.isEmpty() ? 6 : 11);
    int y = box.top() + 2;

    this->_canvas.fill(QRect(0, 0, w, 1), st(QColor(), BAR));
    this->_canvas.text(0, 0, " xdesk ", st(DARK, ACCENT, true));
    if (this->_fingerprint.isEmpty()) {
        this->_canvas.box(box, "Connection", st(ACCENT, DARK), st(QColor(), DARK));
        this->_canvas.text(box.left() + 2, y, this->_status, st(QColor(), DARK, true), box.width() - 4);
        const int end = this->_canvas.text(box.left() + 2, box.bottom(), " esc cancel ", st(DARK, MUTED, true));
        this->_hits.append(Hit{QRect(box.left() + 2, box.bottom(), end - box.left() - 2, 1), [this]() {this->statusKey_("esc", "\x1b");}});
        return;
    }
    this->_canvas.box(box, this->_fingerprintChanged ? "IDENTITY CHANGED" : "First connection to this PC", st(this->_fingerprintChanged ? BAD : ACCENT, DARK, true), st(QColor(), DARK));
    if (this->_fingerprintChanged) {
        this->_canvas.text(box.left() + 2, y++, "The identity of this host changed since the last connection.", st(BAD, DARK, true), box.width() - 4);
        this->_canvas.text(box.left() + 2, y++, "Someone may be intercepting the connection: check the host panel.", st(QColor(), DARK), box.width() - 4);
    } else {
        this->_canvas.text(box.left() + 2, y++, "Check that the panel of the host shows the same identity:", st(QColor(), DARK), box.width() - 4);
        y++;
    }
    y++;
    this->_canvas.text(box.left() + 4, y++, this->_fingerprint.left(23), st(ACCENT, DARK, true));
    this->_canvas.text(box.left() + 4, y++, this->_fingerprint, st(MUTED, DARK), box.width() - 6);
    y++;
    if (this->_fingerprintChanged) {
        const int x = this->_canvas.text(box.left() + 2, y, "Type yes to connect anyway: ", st(QColor(), DARK, true));
        this->_canvas.text(this->_canvas.text(x, y, this->_typed, st(WARN, DARK, true)), y, "█", st(WARN, DARK));
        const int no = this->_canvas.text(box.left() + 2, box.bottom(), " n  Cancel ", st(DARK, MUTED, true)); // connecting anyway stays typed
        this->_hits.append(Hit{QRect(box.left() + 2, box.bottom(), no - box.left() - 2, 1), [this]() {this->statusKey_("esc", "\x1b");}});
    } else {
        const int yes = this->_canvas.text(box.left() + 2, y, " y  Connect ", st(DARK, ACCENT, true));
        const int no = this->_canvas.text(yes + 2, y, " n  Cancel ", st(DARK, MUTED, true));
        this->_hits.append(Hit{QRect(box.left() + 2, y, yes - box.left() - 2, 1), [this]() {this->statusKey_("y", "y");}});
        this->_hits.append(Hit{QRect(yes + 2, y, no - yes - 2, 1), [this]() {this->statusKey_("n", "n");}});
    }
}

void xdesk::ui::tty::ClientTui::drawSession_(void)
{
    const int w = this->_canvas.width();
    const int h = this->_canvas.height();
    const gui::VtScreen* vt = this->currentVt_();

    if (vt) {
        // Remote terminal (scrolled back or live)
        for (int row = 0; row < vt->rows() && row < h - 1; ++row)
            this->_canvas.vtLine(0, row, vt->line(row, this->_scroll), w);
        this->_picture = QRect();
    } else {
        // Remote screen: each cell = 2 pixels (upper half block: foreground, lower: background)
        const QImage& canvas = this->_client.canvas();
        const int rows = h - 1;
        if (canvas.isNull() || w < 2 || rows < 2) {
            this->_canvas.text(2, rows / 2, this->_client.permissions().value("view").toBool() ? "Waiting for the first frame of the screen..." : "No right to see the screen (ctrl+] t: a terminal, when allowed)", st(MUTED));
            this->_picture = QRect();
        } else {
            const QImage view = canvas.scaled(w, rows * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
            const Capabilities& caps = this->_term.caps();
            const bool blocks = caps.unicode && caps.colors != ColorDepth::None; // else characters by brightness
            const int cols = view.width();
            const int lines = (view.height() + 1) / 2;
            const int left = (w - cols) / 2;
            const int top = (rows - lines) / 2;
            Style style;
            this->_picture = QRect(left, top, cols, lines);
            for (int y = 0; y < lines; ++y) {
                const QRgb* upper = reinterpret_cast<const QRgb*>(view.constScanLine(y * 2));
                const QRgb* lower = y * 2 + 1 < view.height() ? reinterpret_cast<const QRgb*>(view.constScanLine(y * 2 + 1)) : upper;
                for (int x = 0; x < cols; ++x) {
                    if (blocks) {
                        style.fg = QColor(upper[x]);
                        style.bg = QColor(lower[x]);
                        this->_canvas.at(left + x, top + y) = TextCell{U'▀', style};
                        continue;
                    }
                    const QColor mean((qRed(upper[x]) + qRed(lower[x])) / 2, (qGreen(upper[x]) + qGreen(lower[x])) / 2, (qBlue(upper[x]) + qBlue(lower[x])) / 2);
                    style.fg = caps.colors == ColorDepth::None ? QColor() : mean;
                    style.bg = QColor();
                    this->_canvas.at(left + x, top + y) = TextCell{static_cast<char32_t>(BRIGHTNESS[qGray(mean.rgb()) * (BRIGHTNESS.size() - 1) / 255]), style};
                }
            }
            // Remote cursor
            if (this->_cursorVisible && this->_cursor.x() >= 0 && canvas.width() > 0) {
                const int cx = left + this->_cursor.x() * cols / canvas.width();
                const int cy = top + this->_cursor.y() * lines / std::max(1, canvas.height());
                if (cx >= 0 && cx < w && cy >= 0 && cy < rows) this->_canvas.at(cx, cy) = TextCell{U'◤', st(QColor(Qt::white), QColor(Qt::black), true)};
            }
        }
    }
    this->drawBar_();
    if (this->_help) this->drawHelp_();
}

void xdesk::ui::tty::ClientTui::drawBar_(void)
{
    const int w = this->_canvas.width();
    const int y = this->_canvas.height() - 1;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    int x = 0;

    this->_canvas.fill(QRect(0, y, w, 1), st(QColor(), BAR));
    x = this->_canvas.text(x, y, this->_prefix ? " ctrl+] … " : " xdesk ", st(DARK, this->_prefix ? WARN : ACCENT, true));

    // Tabs
    for (int i = 0; i <= this->_terminals.size(); ++i) {
        QString label = i == 0 ? " Screen " : " " + QString::number(i) + " ";
        if (i > 0) {
            const std::uint8_t id = this->_terminals[i - 1];
            const QString title = this->_vts.value(id)->title();
            label += (title.isEmpty() ? QString("terminal") : title.left(18)) + (this->_exited.contains(id) ? " (ended)" : "") + " ";
        }
        const int start = x + 1;
        x = this->_canvas.text(start, y, label, i == this->_tab ? st(ACCENT, SELECTED, true) : st(MUTED, BAR));
        this->_hits.append(Hit{QRect(start, y, x - start, 1), [this, i]() {this->selectTab_(i);}});
    }
    {
        const int start = x + 1;
        x = this->_canvas.text(start, y, " + ", st(MUTED, BAR, true));
        this->_hits.append(Hit{QRect(start, y, x - start, 1), [this]() {this->openTerminal_();}});
    }

    // State on the right (or the last note)
    QString right;
    if (!this->_note.isEmpty() && now - this->_noteAt < NOTE_TIME) right = this->_note;
    else {
        right = this->_client.hostName() + "  ·  " + (this->_tab == 0 ? (this->_control ? "CONTROL" : "view only") : QString("terminal")) + "  ·  " + (this->_client.latency() >= 0 ? QString::number(this->_client.latency()) + " ms" : QString("- ms"));
        if (this->_tab == 0) right += "  ·  q" + QString::number(this->_quality) + " " + QString::number(this->_fps) + " fps";
        if (this->_scroll > 0) right += "  ·  " + QString::number(this->_scroll) + " lines up";
    }
    right += "   ctrl+] ? keys ";
    const int start = std::max(x + 2, w - static_cast<int>(right.size()));
    if (this->_tab == 0) {
        // Control toggle (click)
        const int mode = start + static_cast<int>(right.indexOf(this->_control ? "CONTROL" : "view only"));
        this->_hits.append(Hit{QRect(mode, y, this->_control ? 7 : 9, 1), [this]() {this->prefixKey_("c");}});
    }
    this->_canvas.text(start, y, right, st(this->_control && this->_tab == 0 ? WARN : MUTED, BAR, this->_control && this->_tab == 0), w - start);
    this->_hits.append(Hit{QRect(w - 16, y, 16, 1), [this]() {this->_help = true;}});
}

void xdesk::ui::tty::ClientTui::drawHelp_(void)
{
    const int w = this->_canvas.width();
    const int h = this->_canvas.height();
    const QStringList lines = {
        "Everything goes to the remote PC, except ctrl+] followed by:",
        "",
        "  s / 0        screen tab            t      new terminal",
        "  1-9          terminal N            n / p  next / previous tab",
        "  w            close this terminal   q      disconnect",
        "  c            control on / off (keyboard and mouse to the screen)",
        "  d            ctrl+alt+del          r      refresh the screen",
        "  + / -        image quality         f      frame rate (5 / 15 / 30)",
        "  pgup / pgdn  scroll back the terminal (the wheel too)",
        "  ctrl+]       a real ctrl+]         ?      this help",
        "",
        "Mouse: the tabs and the bottom bar; on the screen tab it moves / clicks the remote one",
        "(control on); shift + drag selects text in most terminals.",
    };
    const int width = std::min(w - 2, 92);
    const QRect box((w - width) / 2, std::max(0, (h - static_cast<int>(lines.size()) - 4) / 2), width, std::min(h - 1, static_cast<int>(lines.size()) + 4));

    this->_canvas.box(box, "Keys", st(ACCENT, DARK), st(QColor(), DARK));
    for (int i = 0; i < lines.size() && i < box.height() - 4; ++i)
        this->_canvas.text(box.left() + 2, box.top() + 2 + i, lines[i], st(QColor(), DARK, i == 0), box.width() - 4);
}
