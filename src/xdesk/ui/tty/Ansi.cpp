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
#include <QSocketNotifier>
#include <QThread>
#include <cstdio>
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

xdesk::ui::tty::Terminal::Terminal(QObject* parent)
: QObject(parent)
{
    this->_size = this->size();
    QObject::connect(&this->_poll, &QTimer::timeout, this, [this]() {
        const QSize current = this->size();
        if (current == this->_size) return;
        this->_size = current;
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
    this->write("\x1b[?1049h\x1b[?25l\x1b[2J");
}

void xdesk::ui::tty::Terminal::setMouse(const bool enabled)
{
    this->write(enabled ? "\x1b[?1003h\x1b[?1006h" : "\x1b[?1003l\x1b[?1006l");
}

void xdesk::ui::tty::Terminal::restore(void)
{
    if (this->_alternate) this->write("\x1b[?1003l\x1b[?1006l\x1b[0m\x1b[?25h\x1b[?1049l");
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
