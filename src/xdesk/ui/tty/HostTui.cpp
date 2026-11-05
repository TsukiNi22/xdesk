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
#include <QDateTime>
#include <algorithm>

// Palette of the terminal panel
static const QColor ACCENT(122, 162, 247);
static const QColor MUTED(130, 136, 150);
static const QColor GOOD(115, 201, 145);
static const QColor WARN(230, 190, 100);
static const QColor BAD(240, 110, 110);

xdesk::ui::tty::HostTui::HostTui(host::Host& host, QObject* parent)
: QObject(parent), _host{host}
{
    this->_term.enableRaw();
    this->_term.enterAlternate();
    QObject::connect(&this->_term, &Terminal::input, this, &HostTui::onInput_);
    QObject::connect(&this->_term, &Terminal::resized, this, &HostTui::draw_);
    QObject::connect(&this->_redraw, &QTimer::timeout, this, &HostTui::draw_);
    QObject::connect(&host, &host::Host::approvalRequested, this, [this](host::Session* s) {this->_approval = s->id(); this->draw_();});
    QObject::connect(&host, &host::Host::activity, this, [this](const QString& session, const QString& line) {
        QStringList& lines = this->_activity[session];
        lines.append(QDateTime::currentDateTime().toString("HH:mm:ss ") + line);
        if (lines.size() > 200) lines.removeFirst();
    });
    QObject::connect(&host, &host::Host::tamperDetected, this, [this](const QString&, const QString& reason) {this->_flash = "TAMPER BLOCKED: " + reason;});
    this->_redraw.start(200);
    this->draw_();
}

QList<xdesk::host::Session*> xdesk::ui::tty::HostTui::ordered_(void) const
{
    QList<host::Session*> sessions = this->_host.sessions();

    std::sort(sessions.begin(), sessions.end(), [](const host::Session* a, const host::Session* b) {return a->since() < b->since();});
    return sessions;
}

void xdesk::ui::tty::HostTui::ask_(const QString& question, const QString& answer, const std::function<void(void)>& action)
{
    this->_prompt = question;
    this->_promptAnswer = answer;
    this->_promptTyped.clear();
    this->_promptAction = action;
}

void xdesk::ui::tty::HostTui::onInput_(const QByteArray& data)
{
    const QList<host::Session*> sessions = this->ordered_();
    host::Session* current = sessions.isEmpty() ? nullptr : sessions.value(std::clamp(this->_selected, 0, static_cast<int>(sessions.size()) - 1));
    const QList<security::Device>& devices = this->_host.registry().devices();

    // Typed prompt (presence confirmation)
    if (!this->_prompt.isEmpty()) {
        for (const char c: data) {
            if (c == 0x1b) {this->_prompt.clear(); this->_flash = "Cancelled"; break;}
            if (c == '\r' || c == '\n') {
                if (security::normalize_code(this->_promptTyped) == security::normalize_code(this->_promptAnswer) && this->_promptAction) this->_promptAction();
                else this->_flash = "Wrong confirmation: nothing changed";
                this->_prompt.clear();
                break;
            }
            if (c == 0x7f || c == 0x08) this->_promptTyped.chop(1);
            else if (static_cast<unsigned char>(c) >= 0x20) this->_promptTyped.append(QChar(c));
        }
        this->draw_();
        return;
    }

    // Approval
    if (!this->_approval.isEmpty()) {
        host::Session* s = this->_host.session(this->_approval);
        const char c = data.isEmpty() ? 0 : data[0];
        if (!s || s->state() != host::State::Pending) this->_approval.clear();
        else if (c == 'a') {this->_host.approve(this->_approval, true, false); this->_approval.clear();}
        else if (c == 'A') {this->_host.approve(this->_approval, true, true); this->_approval.clear();}
        else if (c == 'r' || c == 'R' || c == 0x1b) {this->_host.approve(this->_approval, false); this->_approval.clear();}
        this->draw_();
        return;
    }

    // Navigation
    if (data == "\x1b[A") this->_selected = std::max(0, this->_selected - 1);
    else if (data == "\x1b[B") this->_selected++;
    else if (data == "\t" || data == "1" || data == "2" || data == "3") {
        if (data == "\t") this->_page = static_cast<Page>((static_cast<int>(this->_page) + 1) % 3);
        else this->_page = static_cast<Page>(data[0] - '1');
        this->_selected = 0;
    } else if (data == "q") {
        if (this->_quitAsked) {emit this->quit(); return;}
        this->_quitAsked = true;
        this->_flash = "Press q again to stop sharing and quit";
    } else if (data == "r") {
        this->_host.rotateCode();
        this->_flash = "New code";
    } else if (this->_page == Page::Dashboard && current) {
        host::Permissions p = current->permissions();
        if (data == "k") {this->_host.kick(current->id()); this->_flash = "Disconnected " + current->name();}
        else if (data == "b") {this->_host.ban(current->id(), "Banned by the host (terminal panel)"); this->_flash = "Banned " + current->name();}
        else if (data == "c") {p.control = !p.control; this->_host.setPermissions(current->id(), p);}
        else if (data == "t") {p.terminal = !p.terminal; this->_host.setPermissions(current->id(), p);}
        else if (data == "v") {p.view = !p.view; this->_host.setPermissions(current->id(), p);}
        else if (data == "p") {p.clipboard = !p.clipboard; this->_host.setPermissions(current->id(), p);}
    } else if (this->_page == Page::Devices && !devices.isEmpty()) {
        const security::Device d = devices.value(std::clamp(this->_selected, 0, static_cast<int>(devices.size()) - 1));
        const QString challenge = security::random_code(6).remove('-');
        if (this->_host.localActionsLocked() && (data == "u" || data == "T" || data == "f")) this->_flash = "Locked: a remote session controls this PC or has a terminal";
        else if (data == "u") this->ask_("Unban " + d.name + ": type " + challenge, challenge, [this, d]() {(void)this->_host.unban(d.id); this->_flash = "Unbanned " + d.name;});
        else if (data == "T") this->ask_("Trust " + d.name + ": type " + challenge, challenge, [this, d]() {(void)this->_host.setTrusted(d.id, true); this->_flash = "Trusted " + d.name;});
        else if (data == "U") {(void)this->_host.setTrusted(d.id, false); this->_flash = "Untrusted " + d.name;}
        else if (data == "f") this->ask_("Forget " + d.name + ": type " + challenge, challenge, [this, d]() {(void)this->_host.forget(d.id); this->_flash = "Forgot " + d.name;});
    } else if (this->_page == Page::Devices && data == "a" && !this->_host.registry().addresses().isEmpty()) {
        const security::AddressBan a = this->_host.registry().addresses().front();
        const QString challenge = security::random_code(6).remove('-');
        if (this->_host.localActionsLocked()) this->_flash = "Locked: a remote session controls this PC or has a terminal";
        else this->ask_("Unban address " + a.address + ": type " + challenge, challenge, [this, a]() {(void)this->_host.unbanAddress(a.address); this->_flash = "Unbanned " + a.address;});
    }
    if (data != "q") this->_quitAsked = false;
    this->draw_();
}

void xdesk::ui::tty::HostTui::draw_(void)
{
    const QSize size = this->_term.size();
    const int w = size.width();
    const int h = size.height();
    const QList<host::Session*> sessions = this->ordered_();
    QByteArray out = "\x1b[H";
    QStringList lines;
    int row = 0;
    std::function<void(const QByteArray&)> line = [&](const QByteArray& text) {
        if (row >= h - 1) return;
        out += move(++row, 1) + text + reset() + "\x1b[K";
    };

    // Header
    const int left = this->_host.code().secondsLeft();
    line(bg(QColor(30, 34, 46)) + bold() + fg(ACCENT) + " xdesk " + reset() + bg(QColor(30, 34, 46)) + fg(MUTED)
        + fit(QString(" sharing  ·  ") + (this->_page == Page::Dashboard ? "[1 sessions]" : "1 sessions") + "  " + (this->_page == Page::Devices ? "[2 devices]" : "2 devices") + "  " + (this->_page == Page::Logs ? "[3 logs]" : "3 logs"), w - 7).toUtf8());
    line("");
    line("  " + fg(MUTED) + "Code  " + reset() + bold() + fg(ACCENT) + this->_host.code().display().toUtf8() + reset() + fg(MUTED)
        + (left >= 0 ? ("   changes in " + QString::number(left / 60) + ":" + QString::number(left % 60).rightJustified(2, '0')).toUtf8() : QByteArray("   one time"))
        + "   ·   identity " + security::fingerprint_short(this->_host.certificate().fingerprint()).toUtf8());
    for (const net::Endpoint& e: this->_host.exposure().endpoints())
        line("  " + fg(MUTED) + fit(e.label, 11).toUtf8() + reset() + e.url.toUtf8() + fg(MUTED) + (e.endToEnd ? "" : "  (relay sees the traffic)") + "   xdesk connect " + e.host.toUtf8() + (e.port != XDESK_DEFAULT_PORT ? " -p " + QByteArray::number(e.port) : ""));
    line("  " + fg(MUTED) + "Screen " + reset() + this->_host.captureBackend().toUtf8() + fg(MUTED) + "  ·  input " + reset() + this->_host.inputBackend().toUtf8()
        + fg(MUTED) + "  ·  " + security::tls_summary().toUtf8() + "  ·  " + this->_host.exposure().status().toUtf8());
    line("");

    if (this->_page == Page::Dashboard) {
        line(bold() + fit("  Device                    Client  Address          Mode     Rights                FPS  Lat.   Since", w).toUtf8());
        for (int i = 0; i < sessions.size(); ++i) {
            const host::Session* s = sessions[i];
            const host::Permissions& p = s->permissions();
            const QString rights = QString(p.view ? "view " : "") + (p.control ? "control " : "") + (p.terminal ? "terminal " : "") + (p.clipboard ? "clip" : "");
            const QString state = s->state() == host::State::Pending ? "PENDING" : (s->state() == host::State::Handshake ? "auth" : s->mode());
            const QString text = "  " + fit(s->name(), 25) + " " + fit(net::client_kind_name(s->kind()), 7) + " " + fit(s->address(), 16) + " " + fit(state, 8) + " " + fit(rights, 21) + " "
                + fit(QString::number(s->stats().fps), 4) + " " + fit(s->stats().latency >= 0 ? QString::number(s->stats().latency) + "ms" : "-", 6) + " " + QString::number((QDateTime::currentMSecsSinceEpoch() - s->since()) / 1000) + "s";
            line((i == std::clamp(this->_selected, 0, static_cast<int>(sessions.size()) - 1) ? inverse() : QByteArray()) + fit(text, w).toUtf8());
        }
        if (sessions.isEmpty()) line(fg(MUTED) + "  No session yet: give the code and an address above to the person who connects.");
        line("");
        if (!sessions.isEmpty()) {
            const host::Session* s = sessions.value(std::clamp(this->_selected, 0, static_cast<int>(sessions.size()) - 1));
            const QStringList& activity = this->_activity.value(s->id());
            line(bold() + "  Live activity of " + s->name().toUtf8() + reset() + fg(MUTED) + "  (" + s->os().toUtf8() + ", device " + s->deviceId().toUtf8() + ")");
            for (qsizetype i = std::max<qsizetype>(0, activity.size() - std::max(1, h - row - 4)); i < activity.size(); ++i)
                line("  " + fg(MUTED) + fit(activity[i], w - 2).toUtf8());
        }
    } else if (this->_page == Page::Devices) {
        const QList<security::Device>& devices = this->_host.registry().devices();
        line(bold() + fit("  Device                    Client  State    Last address     Sessions  Reason", w).toUtf8());
        for (int i = 0; i < devices.size(); ++i) {
            const security::Device& d = devices[i];
            const QByteArray color = d.banned ? fg(BAD) : (d.trusted ? fg(GOOD) : QByteArray());
            const QString text = "  " + fit(d.name, 25) + " " + fit(d.kind, 7) + " " + fit(d.banned ? "banned" : (d.trusted ? "trusted" : "known"), 8) + " " + fit(d.address, 16) + " " + fit(QString::number(d.sessions), 9) + " " + d.banReason;
            line((i == std::clamp(this->_selected, 0, static_cast<int>(devices.size()) - 1) ? inverse() : color) + fit(text, w).toUtf8());
        }
        line("");
        line(bold() + "  Banned addresses");
        for (const security::AddressBan& a: this->_host.registry().addresses())
            line("  " + fg(BAD) + fit(a.address, 18).toUtf8() + reset() + a.reason.toUtf8() + fg(MUTED) + (a.until ? "  until " + QDateTime::fromMSecsSinceEpoch(a.until).toString("HH:mm").toUtf8() : QByteArray("  until a local unban")));
        if (this->_host.localActionsLocked()) line(fg(WARN) + "  Locked: a remote session controls this PC or has a terminal (unban / trust need the person in front of the PC)");
    } else {
        const QList<QJsonObject> entries = this->_host.log().recent(std::max(1, h - row - 3));
        for (const QJsonObject& e: entries) {
            const QString level = e.value("level").toString();
            line((level == "alert" ? fg(BAD) : (level == "warn" ? fg(WARN) : fg(MUTED))) + QDateTime::fromMSecsSinceEpoch(e.value("ts").toInteger()).toString(" HH:mm:ss ").toUtf8()
                + fit(e.value("cat").toString(), 9).toUtf8() + reset() + fit(e.value("msg").toString(), w - 20).toUtf8());
        }
    }

    // Approval / prompt / help
    out += "\x1b[J";
    if (!this->_approval.isEmpty()) {
        host::Session* s = this->_host.session(this->_approval);
        if (s) out += move(h - 3, 1) + bg(QColor(60, 50, 20)) + bold() + fit("  " + s->name() + " (" + net::client_kind_name(s->kind()) + ", " + s->address() + ") typed the right code and wants to connect", w).toUtf8() + reset()
            + move(h - 2, 1) + bg(QColor(60, 50, 20)) + fit("  [a] accept   [A] accept & trust   [r] refuse   (refused in 60 s)", w).toUtf8() + reset();
    }
    if (!this->_prompt.isEmpty()) out += move(h - 2, 1) + bg(QColor(40, 40, 70)) + fit("  " + this->_prompt + "  > " + this->_promptTyped, w).toUtf8() + reset();
    QString help = "  ↑↓ select  tab/1-3 pages  r new code  q quit";
    if (this->_page == Page::Dashboard) help += "  ·  k kick  b ban  v/c/t/p toggle view/control/terminal/clipboard";
    if (this->_page == Page::Devices) help += "  ·  u unban  T trust  U untrust  f forget  a unban first address";
    out += move(h - 1, 1) + (this->_flash.contains("TAMPER") ? fg(BAD) + bold() : fg(WARN)) + fit("  " + this->_flash, w).toUtf8() + reset();
    out += move(h, 1) + fg(MUTED) + fit(help, w).toUtf8() + reset();
    this->_term.write(out);
}
