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
#include <QRegularExpression>
#include <iostream>
#include <QImage>

xdesk::ui::tty::ClientTui::ClientTui(QObject* parent)
: QObject(parent)
{
    QObject::connect(&this->_term, &Terminal::input, this, &ClientTui::onInput_);
    QObject::connect(&this->_term, &Terminal::resized, this, [this](const QSize& size) {
        if (this->_terminal != 0) this->_client.resizeTerminal(this->_terminal, size.width(), size.height());
        this->_dirty = true;
    });
    QObject::connect(&this->_frameTimer, &QTimer::timeout, this, [this]() {
        if (this->_screen && this->_dirty) this->drawScreen_();
    });
    QObject::connect(&this->_client, &client::Client::frameUpdated, this, [this](const QRect&) {this->_dirty = true;});
    QObject::connect(&this->_client, &client::Client::cursorMoved, this, [this](const QPoint&, const bool) {this->_dirty = true;});
    QObject::connect(&this->_client, &client::Client::message, this, &ClientTui::status_);
    QObject::connect(&this->_client, &client::Client::fingerprintCheck, this, [this](const QString& fingerprint, const bool changed) {
        this->_verifying = true;
        this->_line.clear();
        this->_term.write(changed
            ? "\r\n\x1b[1;31mWARNING: the identity of this host CHANGED since the last connection.\x1b[0m\r\nSomeone may be intercepting the connection.\r\n"
            : QByteArray("\r\nFirst connection to this host. Check that its panel shows the same identity:\r\n"));
        this->_term.write("  \x1b[1m" + fingerprint.left(23).toUtf8() + "\x1b[0m  (" + fingerprint.toUtf8() + ")\r\nType yes to connect: ");
    });
    QObject::connect(&this->_client, &client::Client::stateChanged, this, [this](const client::State state) {
        const QSize size = this->_term.size();
        switch (state) {
            case client::State::Connecting:     this->status_("Connecting (TLS 1.3, post-quantum key exchange)..."); break;
            case client::State::Authenticating: this->status_((this->_client.options().code.isEmpty() ? "Authenticating (trusted device secret)..." : "Authenticating (Argon2id proof of the code)...")); break;
            case client::State::Pending:        this->status_("Waiting for the person in front of the host to accept..."); break;
            case client::State::Active:
                this->_term.enableRaw();
                if (this->_screen) {
                    this->_term.enterAlternate();
                    this->_frameTimer.start(66);
                } else {
                    this->status_("Connected to " + this->_client.hostName() + "  (ctrl+] then q to quit, ctrl+] s for the screen)\r\n");
                    if (this->_client.permissions().value("terminal").toBool()) this->_terminal = this->_client.openTerminal(size.width(), size.height());
                    else this->status_("The host did not allow terminals: ctrl+] s shows the screen");
                }
                break;
            default: break;
        }
    });
    QObject::connect(&this->_client, &client::Client::permissionsChanged, this, [this](const QJsonObject& perms) {
        if (!perms.value("control").toBool()) this->_control = false;
    });
    QObject::connect(&this->_client, &client::Client::terminalOutput, this, [this](const std::uint8_t id, const QByteArray& data) {
        if (id == this->_terminal && !this->_screen) this->_term.write(data);
    });
    QObject::connect(&this->_client, &client::Client::terminalClosed, this, [this](const std::uint8_t id, const int code) {
        if (id == this->_terminal) this->finish_("Remote shell ended (code " + QString::number(code) + ")", 0);
    });
    QObject::connect(&this->_client, &client::Client::closed, this, [this](const QString& reason, const bool banned) {
        this->finish_(reason + (banned ? " (banned: only the person in front of the host can unban this device)" : ""), banned ? 3 : 1);
    });
}

void xdesk::ui::tty::ClientTui::start(const client::Options& options, const bool screen)
{
    this->_screen = screen;
    this->_client.connectToHost(options);
}

void xdesk::ui::tty::ClientTui::status_(const QString& text)
{
    if (this->_screen && this->_client.state() == client::State::Active) return;
    this->_term.write("\x1b[2mxdesk\x1b[0m " + text.toUtf8() + "\r\n");
}

void xdesk::ui::tty::ClientTui::finish_(const QString& reason, const int code)
{
    this->_frameTimer.stop();
    this->_term.restore();
    std::cerr << "\r\nxdesk: " << reason.toStdString() << std::endl;
    this->_exitCode = code;
    emit this->quit();
}

void xdesk::ui::tty::ClientTui::onInput_(const QByteArray& data)
{
    // Fingerprint answer (line mode before the session)
    if (this->_verifying) {
        for (const char c: data) {
            if (c == '\r' || c == '\n') {
                this->_verifying = false;
                this->_term.write("\r\n");
                this->_client.acceptFingerprint(this->_line.trimmed().toLower() == "yes", true);
                return;
            }
            if (c == 0x7f || c == 0x08) {this->_line.chop(1); continue;}
            this->_line.append(QChar(c));
        }
        return;
    }
    if (this->_client.state() != client::State::Active) {
        if (data.contains('\x03')) this->finish_("Cancelled", 1);
        return;
    }
    for (qsizetype i = 0; i < data.size(); ++i) {
        const char c = data[i];
        if (this->_prefix) {
            this->_prefix = false;
            if (c == 'q') {this->_client.disconnectFromHost(); this->finish_("Disconnected", 0); return;}
            if (c == 's') {
                this->_screen = !this->_screen;
                if (this->_screen) {this->_term.enterAlternate(); this->_term.setMouse(this->_control); this->_frameTimer.start(66); this->_dirty = true;}
                else {this->_frameTimer.stop(); this->_term.restore(); this->_term.enableRaw(); if (this->_terminal == 0 && this->_client.permissions().value("terminal").toBool()) this->_terminal = this->_client.openTerminal(this->_term.size().width(), this->_term.size().height());}
                continue;
            }
            if (c == 'c' && this->_screen) {
                this->_control = !this->_control && this->_client.permissions().value("control").toBool();
                this->_client.requestMode(this->_control ? "control" : "view");
                this->_term.setMouse(this->_control);
                this->_dirty = true;
                continue;
            }
            if (c != 0x1d) continue;
        } else if (c == 0x1d) {this->_prefix = true; continue;}

        // Rest of the bytes
        const QByteArray rest = data.mid(i);
        if (this->_screen) this->screenInput_(rest);
        else if (this->_terminal != 0) this->_client.terminalInput(this->_terminal, rest.left(rest.indexOf('\x1d') < 0 ? rest.size() : rest.indexOf('\x1d')));
        if (rest.indexOf('\x1d') < 0) return;
        i += rest.indexOf('\x1d') - 1;
    }
}

void xdesk::ui::tty::ClientTui::screenInput_(const QByteArray& data)
{
    static const QRegularExpression MOUSE(R"(\x1b\[<(\d+);(\d+);(\d+)([Mm]))");
    static const QHash<QByteArray, QString> SPECIAL = {
        {"\x1b[A", "ArrowUp"}, {"\x1b[B", "ArrowDown"}, {"\x1b[C", "ArrowRight"}, {"\x1b[D", "ArrowLeft"},
        {"\x1b[H", "Home"}, {"\x1b[F", "End"}, {"\x1b[5~", "PageUp"}, {"\x1b[6~", "PageDown"}, {"\x1b[3~", "Delete"}, {"\x1b[2~", "Insert"},
        {"\x1bOP", "F1"}, {"\x1bOQ", "F2"}, {"\x1bOR", "F3"}, {"\x1bOS", "F4"}, {"\x1b[15~", "F5"}, {"\x1b[17~", "F6"},
        {"\x1b[18~", "F7"}, {"\x1b[19~", "F8"}, {"\x1b[20~", "F9"}, {"\x1b[21~", "F10"}, {"\x1b[23~", "F11"}, {"\x1b[24~", "F12"},
    };
    const QSize term = this->_term.size();
    const QSize frame = this->_client.canvas().size();
    const QString text = QString::fromUtf8(data);
    QRegularExpressionMatchIterator it = MOUSE.globalMatch(text);
    bool mouse = false;

    if (!this->_control) return;

    // SGR mouse: cell -> frame coordinates (each cell = 2 pixels high in the view)
    while (it.hasNext() && !frame.isEmpty()) {
        const QRegularExpressionMatch m = it.next();
        const int b = m.captured(1).toInt();
        const int x = (m.captured(2).toInt() - 1) * frame.width() / std::max(1, term.width());
        const int y = (m.captured(3).toInt() - 1) * frame.height() / std::max(1, term.height() - 1);
        mouse = true;
        this->_client.mouseMove(x, y);
        if (b & 32) continue; // motion
        if (b == 64 || b == 65) {this->_client.mouseWheel(0, b == 64 ? 120 : -120); continue;}
        this->_client.mouseButton(static_cast<net::MouseButton>(b & 3), m.captured(4) == "M");
    }
    if (mouse) return;

    // Keys
    if (SPECIAL.contains(data)) {this->_client.key(SPECIAL.value(data), true); this->_client.key(SPECIAL.value(data), false); return;}
    for (const QChar c: text) {
        QString code;
        if (c == '\r') code = "Enter";
        else if (c == '\t') code = "Tab";
        else if (c == QChar(0x7f)) code = "Backspace";
        else if (c == QChar(0x1b)) code = "Escape";
        else if (c.unicode() < 0x20) {
            // ctrl+letter
            const QString letter = "Key" + QString(QChar('A' + c.unicode() - 1));
            this->_client.key("ControlLeft", true);
            this->_client.key(letter, true);
            this->_client.key(letter, false);
            this->_client.key("ControlLeft", false);
            continue;
        }
        if (!code.isEmpty()) {this->_client.key(code, true); this->_client.key(code, false);}
        else this->_client.text(QString(c));
    }
}

void xdesk::ui::tty::ClientTui::drawScreen_(void)
{
    const QSize term = this->_term.size();
    const int cols = term.width();
    const int rows = term.height() - 1;
    const QImage& canvas = this->_client.canvas();
    QByteArray out = "\x1b[H";
    QImage view;
    QRgb lastTop = 1;
    QRgb lastBottom = 1;

    this->_dirty = false;
    if (canvas.isNull() || cols < 2 || rows < 2) return;

    // Each cell: upper half = foreground, lower half = background
    view = canvas.scaled(cols, rows * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    out.reserve(cols * rows * 24);
    for (int y = 0; y < rows; ++y) {
        out += move(y + 1, 1);
        lastTop = lastBottom = 1;
        for (int x = 0; x < cols; ++x) {
            if (x >= view.width() || y * 2 >= view.height()) {out += "\x1b[0m "; lastTop = lastBottom = 1; continue;}
            const QRgb top = view.pixel(x, y * 2);
            const QRgb bottom = y * 2 + 1 < view.height() ? view.pixel(x, y * 2 + 1) : top;
            if (top != lastTop) out += fg(QColor(top));
            if (bottom != lastBottom) out += bg(QColor(bottom));
            out += "▀";
            lastTop = top;
            lastBottom = bottom;
        }
    }
    out += reset() + move(term.height(), 1) + "\x1b[7m" + fit(" xdesk  " + this->_client.hostName() + "  ·  " + (this->_control ? "CONTROL" : "view only") + "  ·  " + QString::number(this->_client.latency()) + " ms  ·  ctrl+] c control  s terminal  q quit", cols).toUtf8() + reset();
    this->_term.write(out);
}
