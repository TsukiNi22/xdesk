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
##  @file Ansi.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/tty/Ansi.hpp"
#include <qrcodegen/qrcodegen.hpp>
#include <QSocketNotifier>
#include <algorithm>
#include <QThread>
#include <cstdio>
#include <QHash>
#ifndef _WIN32
    #include <sys/ioctl.h>
    #include <termios.h>
    #include <unistd.h>
#endif

#ifndef _WIN32
    static struct termios g_saved; // terminal state before the raw mode
#else
    static DWORD g_savedIn = 0; // console modes before the raw mode
    static DWORD g_savedOut = 0;
#endif

QByteArray xdesk::ui::tty::move(const int row, const int col)
{
    return "\x1b[" + QByteArray::number(row) + ";" + QByteArray::number(col) + "H";
}

QByteArray xdesk::ui::tty::fg(const QColor& color)
{
    return "\x1b[38;2;" + QByteArray::number(color.red()) + ";" + QByteArray::number(color.green()) + ";" + QByteArray::number(color.blue()) + "m";
}

QByteArray xdesk::ui::tty::bg(const QColor& color)
{
    return "\x1b[48;2;" + QByteArray::number(color.red()) + ";" + QByteArray::number(color.green()) + ";" + QByteArray::number(color.blue()) + "m";
}

QByteArray xdesk::ui::tty::reset(void)   {return "\x1b[0m";}
QByteArray xdesk::ui::tty::bold(void)    {return "\x1b[1m";}
QByteArray xdesk::ui::tty::dim(void)     {return "\x1b[2m";}
QByteArray xdesk::ui::tty::inverse(void) {return "\x1b[7m";}

QString xdesk::ui::tty::fit(const QString& text, const int width)
{
    if (width <= 0) return "";
    if (text.size() <= width) return text.leftJustified(width, ' ');
    return text.left(std::max(0, width - 1)) + QChar(0x2026);
}

//----------------------------------------------------------------//
// Input

QList<QByteArray> xdesk::ui::tty::split_keys(const QByteArray& data)
{
    QList<QByteArray> keys;
    qsizetype i = 0;

    while (i < data.size()) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        qsizetype end = i + 1;

        if (c == 0x1b && i + 1 < data.size() && data[i + 1] == '[') {
            // Bracketed paste: everything up to its end marker is one entry
            if (data.mid(i, 6) == "\x1b[200~") {
                const qsizetype stop = data.indexOf("\x1b[201~", i + 6);
                end = stop < 0 ? data.size() : stop + 6;
            } else {
                // CSI: parameters / intermediates, then the final byte
                end = i + 2;
                while (end < data.size() && static_cast<unsigned char>(data[end]) >= 0x20 && static_cast<unsigned char>(data[end]) <= 0x3F)
                    ++end;
                while (end < data.size() && static_cast<unsigned char>(data[end]) >= 0x20 && static_cast<unsigned char>(data[end]) <= 0x2F)
                    ++end;
                end = std::min(data.size(), end + 1);
            }
        } else if (c == 0x1b && i + 2 < data.size() && data[i + 1] == 'O') end = i + 3; // SS3 (F1-F4, keypad)
        else if (c == 0x1b && i + 1 < data.size() && data[i + 1] != 0x1b) {
            // alt + key (utf-8 continuation included)
            end = i + 2;
            while (end < data.size() && (static_cast<unsigned char>(data[end]) & 0xC0) == 0x80)
                ++end;
        } else if (c >= 0xC0) {
            // utf-8 character
            while (end < data.size() && (static_cast<unsigned char>(data[end]) & 0xC0) == 0x80)
                ++end;
        }
        keys.append(data.mid(i, end - i));
        i = end;
    }
    return keys;
}

QString xdesk::ui::tty::key_name(const QByteArray& key)
{
    static const QHash<QByteArray, QString> NAMES = {
        {"\x1b[A", "up"}, {"\x1b[B", "down"}, {"\x1b[C", "right"}, {"\x1b[D", "left"},
        {"\x1bOA", "up"}, {"\x1bOB", "down"}, {"\x1bOC", "right"}, {"\x1bOD", "left"},
        {"\x1b[H", "home"}, {"\x1b[F", "end"}, {"\x1bOH", "home"}, {"\x1bOF", "end"}, {"\x1b[1~", "home"}, {"\x1b[4~", "end"},
        {"\x1b[5~", "pgup"}, {"\x1b[6~", "pgdn"}, {"\x1b[3~", "del"}, {"\x1b[2~", "ins"}, {"\x1b[Z", "shift+tab"},
        {"\x1bOP", "f1"}, {"\x1bOQ", "f2"}, {"\x1bOR", "f3"}, {"\x1bOS", "f4"}, {"\x1b[15~", "f5"}, {"\x1b[17~", "f6"},
        {"\x1b[18~", "f7"}, {"\x1b[19~", "f8"}, {"\x1b[20~", "f9"}, {"\x1b[21~", "f10"}, {"\x1b[23~", "f11"}, {"\x1b[24~", "f12"},
        {"\x1b[1;5A", "ctrl+up"}, {"\x1b[1;5B", "ctrl+down"}, {"\x1b[1;5C", "ctrl+right"}, {"\x1b[1;5D", "ctrl+left"},
        {"\r", "enter"}, {"\n", "enter"}, {"\t", "tab"}, {"\x1b", "esc"}, {"\x7f", "backspace"}, {"\x08", "backspace"}, {" ", "space"},
    };
    const unsigned char first = key.isEmpty() ? 0 : static_cast<unsigned char>(key[0]);

    if (key.isEmpty()) return "";
    if (NAMES.contains(key)) return NAMES.value(key);
    if (key.startsWith("\x1b[200~")) return "paste";
    if (key.startsWith("\x1b[<")) return "mouse";
    if (key.size() == 1 && first < 0x20) return "ctrl+" + QString(QChar('a' + first - 1));
    if (key.size() >= 2 && first == 0x1b && key[1] != '[' && key[1] != 'O') return "alt+" + QString::fromUtf8(key.mid(1));
    if (first == 0x1b) return "unknown";
    return QString::fromUtf8(key);
}

bool xdesk::ui::tty::parse_mouse(const QByteArray& key, Mouse& mouse)
{
    QList<QByteArray> parts;

    if (!key.startsWith("\x1b[<") || key.size() < 6 || (!key.endsWith('M') && !key.endsWith('m'))) return false;
    parts = key.mid(3, key.size() - 4).split(';');
    if (parts.size() != 3) return false;
    mouse.raw = parts[0].toInt();
    mouse.col = std::max(0, parts[1].toInt() - 1);
    mouse.row = std::max(0, parts[2].toInt() - 1);
    mouse.press = key.endsWith('M');
    mouse.motion = (mouse.raw & 32) != 0;
    mouse.wheel = (mouse.raw & 64) ? ((mouse.raw & 1) ? -1 : 1) : 0;
    mouse.button = mouse.raw & 3;
    return true;
}

QByteArray xdesk::ui::tty::paste_text(const QByteArray& key)
{
    QByteArray text = key;

    if (text.startsWith("\x1b[200~")) text.remove(0, 6);
    if (text.endsWith("\x1b[201~")) text.chop(6);
    return text;
}

//----------------------------------------------------------------//
// Drawing

QColor xdesk::ui::tty::vt_color(const std::int32_t color)
{
    static const QRgb BASE[16] = {
        0x000000, 0xcd3131, 0x0dbc79, 0xe5e510, 0x2472c8, 0xbc3fbc, 0x11a8cd, 0xe5e5e5,
        0x666666, 0xf14c4c, 0x23d18b, 0xf5f543, 0x3b8eea, 0xd670d6, 0x29b8db, 0xffffff,
    };

    if (color == VT_DEFAULT_COLOR || color < 0) return QColor();
    if (color >= VT_TRUECOLOR) return QColor(QRgb(color - VT_TRUECOLOR));
    if (color < 16) return QColor(BASE[color]);
    if (color < 232) {
        // 6 x 6 x 6 cube
        const int c = color - 16;
        const int level[6] = {0, 95, 135, 175, 215, 255};
        return QColor(level[c / 36], level[(c / 6) % 6], level[c % 6]);
    }
    return QColor(8 + (color - 232) * 10, 8 + (color - 232) * 10, 8 + (color - 232) * 10);
}

QStringList xdesk::ui::tty::qr_lines(const QString& text)
{
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::LOW);
    const int quiet = 2;
    const int size = qr.getSize() + quiet * 2;
    QStringList lines;

    // Two modules per cell (upper / lower half), light = the block drawn: the scanners want dark on light
    for (int y = 0; y < size; y += 2) {
        QString line;
        for (int x = 0; x < size; ++x) {
            const int mx = x - quiet;
            const bool top = mx >= 0 && mx < qr.getSize() && y - quiet >= 0 && y - quiet < qr.getSize() && qr.getModule(mx, y - quiet);
            const bool bottom = mx >= 0 && mx < qr.getSize() && y + 1 - quiet >= 0 && y + 1 - quiet < qr.getSize() && y + 1 < size && qr.getModule(mx, y + 1 - quiet);
            if (!top && !bottom) line += QChar(0x2588); // full block (light)
            else if (!top) line += QChar(0x2580); // upper light
            else if (!bottom) line += QChar(0x2584); // lower light
            else line += ' ';
        }
        lines.append(line);
    }
    return lines;
}

//----------------------------------------------------------------//
// Canvas

void xdesk::ui::tty::Canvas::resize(const int w, const int h)
{
    this->_w = std::max(0, w);
    this->_h = std::max(0, h);
    this->_cells = QVector<TextCell>(static_cast<qsizetype>(this->_w) * this->_h);
}

void xdesk::ui::tty::Canvas::clear(const Style& style)
{
    for (TextCell& cell: this->_cells)
        cell = TextCell{U' ', style};
}

int xdesk::ui::tty::Canvas::text(const int x, const int y, const QString& text, const Style& style, const int max)
{
    const QList<uint> codes = text.toUcs4();
    const int limit = max < 0 ? this->_w : std::min(this->_w, x + max);
    int col = x;

    if (y < 0 || y >= this->_h) return x;
    for (qsizetype i = 0; i < codes.size() && col < limit; ++i) {
        const char32_t ch = static_cast<char32_t>(codes[i]);
        if (ch < 0x20 || ch == 0x7F) continue;
        if (col >= 0) {
            // Cut text: the last visible cell shows an ellipsis
            const bool cut = col == limit - 1 && i + 1 < codes.size();
            this->at(col, y) = TextCell{cut ? U'…' : ch, style};
        }
        ++col;
    }
    return col;
}

void xdesk::ui::tty::Canvas::fill(const QRect& area, const Style& style, const char32_t ch)
{
    const QRect r = area.intersected(QRect(0, 0, this->_w, this->_h));

    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x)
            this->at(x, y) = TextCell{ch, style};
}

void xdesk::ui::tty::Canvas::box(const QRect& area, const QString& title, const Style& border, const Style& inside)
{
    const QRect r = area.intersected(QRect(0, 0, this->_w, this->_h));

    if (r.width() < 2 || r.height() < 2) return;
    this->fill(r, inside);
    for (int x = r.left() + 1; x < r.right(); ++x) {
        this->at(x, r.top()) = TextCell{U'─', border};
        this->at(x, r.bottom()) = TextCell{U'─', border};
    }
    for (int y = r.top() + 1; y < r.bottom(); ++y) {
        this->at(r.left(), y) = TextCell{U'│', border};
        this->at(r.right(), y) = TextCell{U'│', border};
    }
    this->at(r.left(), r.top()) = TextCell{U'╭', border};
    this->at(r.right(), r.top()) = TextCell{U'╮', border};
    this->at(r.left(), r.bottom()) = TextCell{U'╰', border};
    this->at(r.right(), r.bottom()) = TextCell{U'╯', border};
    if (!title.isEmpty()) {
        Style strong = border;
        strong.bold = true;
        (void)this->text(r.left() + 2, r.top(), " " + title + " ", strong, r.width() - 4);
    }
}

void xdesk::ui::tty::Canvas::vtLine(const int x, const int y, const gui::Line& line, const int max)
{
    if (y < 0 || y >= this->_h) return;
    for (int i = 0; i < line.size() && i < max && x + i < this->_w; ++i) {
        const gui::Cell& cell = line[i];
        Style style;
        style.fg = vt_color(cell.fg);
        style.bg = vt_color(cell.bg);
        style.bold = cell.attr & gui::Bold;
        style.dim = cell.attr & gui::Dim;
        style.underline = cell.attr & gui::Underline;
        style.inverse = cell.attr & gui::Inverse;
        this->at(x + i, y) = TextCell{(cell.attr & gui::WideTail) ? char32_t(0) : cell.ch, style};
    }
}

QByteArray xdesk::ui::tty::Canvas::row(const int y) const
{
    QByteArray out = "\x1b[0m";
    Style current;
    bool first = true;

    out.reserve(this->_w * 4 + 16);
    for (int x = 0; x < this->_w; ++x) {
        const TextCell& cell = this->_cells[static_cast<qsizetype>(y) * this->_w + x];
        if (cell.ch == 0) continue;
        if (first || !(cell.style == current)) {
            const Style& s = cell.style;
            // Attributes changed: full reset, else only the colors
            if (first || s.bold != current.bold || s.dim != current.dim || s.underline != current.underline || s.inverse != current.inverse) {
                out += "\x1b[0";
                if (s.bold) out += ";1";
                if (s.dim) out += ";2";
                if (s.underline) out += ";4";
                if (s.inverse) out += ";7";
                out += "m";
                if (s.fg.isValid()) out += fg(s.fg);
                if (s.bg.isValid()) out += bg(s.bg);
            } else {
                if (s.fg != current.fg) out += s.fg.isValid() ? fg(s.fg) : QByteArray("\x1b[39m");
                if (s.bg != current.bg) out += s.bg.isValid() ? bg(s.bg) : QByteArray("\x1b[49m");
            }
            current = s;
            first = false;
        }
        if (cell.ch < 0x80) out += static_cast<char>(cell.ch);
        else out += QString::fromUcs4(&cell.ch, 1).toUtf8();
    }
    out += "\x1b[0m";
    return out;
}

xdesk::ui::tty::Terminal::Terminal(QObject* parent)
: QObject(parent)
{
    this->_size = this->size();
    QObject::connect(&this->_poll, &QTimer::timeout, this, [this]() {
        const QSize current = this->size();
        if (current == this->_size) return;
        this->_size = current;
        this->_shown.clear(); // the terminal reflowed or cleared its screen
        emit this->resized(current);
    });
    this->_poll.start(250);

    // Keyboard bytes
    #ifdef _WIN32
        QThread* reader = QThread::create([this]() {
            HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
            char buffer[4096];
            DWORD count = 0;
            while (ReadFile(in, buffer, sizeof(buffer), &count, nullptr) && count > 0) {
                const QByteArray data(buffer, static_cast<qsizetype>(count));
                QMetaObject::invokeMethod(this, [this, data]() {emit this->input(data);}, Qt::QueuedConnection);
            }
        });
        reader->setParent(this);
        reader->start();
        this->_reader = reader;
    #else
        QSocketNotifier* notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
        QObject::connect(notifier, &QSocketNotifier::activated, this, [this, notifier]() {
            char buffer[4096];
            const ssize_t count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count <= 0) {notifier->setEnabled(false); return;}
            emit this->input(QByteArray(buffer, static_cast<qsizetype>(count)));
        });
        this->_reader = notifier;
    #endif
}

xdesk::ui::tty::Terminal::~Terminal()
{
    this->restore();
    #ifdef _WIN32
        // The reader is blocked in ReadFile: the process exit ends it
        if (this->_reader) static_cast<QThread*>(this->_reader)->setParent(nullptr);
    #endif
}

bool xdesk::ui::tty::Terminal::isTty(void)
{
    #ifdef _WIN32
        return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
    #else
        return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
    #endif
}

void xdesk::ui::tty::Terminal::enableRaw(void)
{
    if (this->_raw) return;
    #ifdef _WIN32
        HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(in, &g_savedIn);
        GetConsoleMode(out, &g_savedOut);
        SetConsoleMode(in, ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_WINDOW_INPUT);
        SetConsoleMode(out, g_savedOut | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
        SetConsoleOutputCP(CP_UTF8);
    #else
        struct termios raw;
        if (tcgetattr(STDIN_FILENO, &g_saved) != 0) return;
        raw = g_saved;
        cfmakeraw(&raw);
        raw.c_oflag |= OPOST | ONLCR; // keep \n -> \r\n for the panel output
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    #endif
    this->_raw = true;
}

void xdesk::ui::tty::Terminal::enterAlternate(void)
{
    this->_alternate = true;
    this->_shown.clear();
    this->write("\x1b[?1049h\x1b[?25l\x1b[?2004h\x1b[2J");
}

void xdesk::ui::tty::Terminal::setMouse(const bool enabled, const bool motion)
{
    if (!enabled) this->write("\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1006l");
    else this->write(motion ? QByteArray("\x1b[?1000h\x1b[?1003h\x1b[?1006h") : QByteArray("\x1b[?1003l\x1b[?1000h\x1b[?1006h"));
}

void xdesk::ui::tty::Terminal::present(const Canvas& canvas, const QPoint& cursor)
{
    QByteArray out = "\x1b[?2026h\x1b[?25l"; // synchronized output: the terminal shows the frame at once (ignored when unknown)

    if (this->_shown.size() != canvas.height()) {
        this->_shown = QVector<QByteArray>(canvas.height());
        out += "\x1b[0m\x1b[2J";
    }
    for (int y = 0; y < canvas.height(); ++y) {
        QByteArray row = canvas.row(y);
        if (row == this->_shown[y]) continue;
        out += move(y + 1, 1) + row;
        this->_shown[y] = std::move(row);
    }
    if (cursor.x() >= 0 && cursor.y() >= 0) out += move(cursor.y() + 1, cursor.x() + 1) + "\x1b[?25h";
    out += "\x1b[?2026l";
    this->write(out);
}

void xdesk::ui::tty::Terminal::restore(void)
{
    if (this->_alternate) this->write("\x1b[?1000l\x1b[?1002l\x1b[?1003l\x1b[?1006l\x1b[?2004l\x1b[?1l\x1b[0m\x1b[?25h\x1b[?1049l");
    this->_alternate = false;
    if (!this->_raw) return;
    #ifdef _WIN32
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), g_savedIn);
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), g_savedOut);
    #else
        tcsetattr(STDIN_FILENO, TCSANOW, &g_saved);
    #endif
    this->_raw = false;
}

void xdesk::ui::tty::Terminal::write(const QByteArray& data)
{
    std::fwrite(data.constData(), 1, static_cast<std::size_t>(data.size()), stdout);
    std::fflush(stdout);
}

QSize xdesk::ui::tty::Terminal::size(void) const
{
    #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
            return QSize(info.srWindow.Right - info.srWindow.Left + 1, info.srWindow.Bottom - info.srWindow.Top + 1);
    #else
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return QSize(ws.ws_col, ws.ws_row);
    #endif
    return QSize(80, 24);
}
