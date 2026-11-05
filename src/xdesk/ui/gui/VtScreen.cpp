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
##  @file VtScreen.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/gui/VtScreen.hpp"
#include <algorithm>

// DEC special graphics (ESC ( 0): characters 0x60 - 0x7E
static const char32_t LINE_DRAWING[] = {
    U'◆', U'▒', U'␉', U'␌', U'␍', U'␊', U'°', U'±', U'␤', U'␋', U'┘', U'┐', U'┌', U'└', U'┼', U'⎺',
    U'⎻', U'─', U'⎼', U'⎽', U'├', U'┤', U'┴', U'┬', U'│', U'≤', U'≥', U'π', U'≠', U'£', U'·'
};

xdesk::ui::gui::VtScreen::VtScreen(const int cols, const int rows, QObject* parent)
: QObject(parent)
{
    this->_cols = std::max(cols, 2);
    this->_rows = std::max(rows, 2);
    this->reset();
}

void xdesk::ui::gui::VtScreen::reset(void)
{
    this->_pen = Cell();
    this->_savedPen = Cell();
    this->_main = QVector<Line>(this->_rows, this->blankLine_());
    this->_alt = QVector<Line>(this->_rows, this->blankLine_());
    this->_altActive = false;
    this->_x = this->_y = this->_savedX = this->_savedY = 0;
    this->_top = 0;
    this->_bottom = this->_rows - 1;
    this->_wrapPending = this->_originMode = this->_insertMode = this->_appCursor = false;
    this->_appKeypad = this->_bracketedPaste = this->_lineDrawing = this->_mouseSgr = false;
    this->_autoWrap = this->_cursorVisible = true;
    this->_mouseMode = 0;
    this->_tabs = QVector<bool>(this->_cols, false);
    for (int i = 0; i < this->_cols; i += 8)
        this->_tabs[i] = true;
    this->_state = Parse::Ground;
    emit this->changed();
}

xdesk::ui::gui::Line xdesk::ui::gui::VtScreen::blankLine_(void) const
{
    Cell blank;
    blank.bg = this->_pen.bg; // erase with the current background (xterm behavior)
    return Line(this->_cols, blank);
}

QVector<xdesk::ui::gui::Line>& xdesk::ui::gui::VtScreen::screen_(void)
{
    return this->_altActive ? this->_alt : this->_main;
}

int xdesk::ui::gui::VtScreen::charWidth_(const char32_t ch)
{
    // Combining marks: drawn over the previous cell (ignored)
    if ((ch >= 0x0300 && ch <= 0x036F) || (ch >= 0x200B && ch <= 0x200F) || (ch >= 0xFE00 && ch <= 0xFE0F)) return 0;
    // East asian wide and emoji
    if ((ch >= 0x1100 && ch <= 0x115F) || (ch >= 0x2E80 && ch <= 0xA4CF && ch != 0x303F) || (ch >= 0xAC00 && ch <= 0xD7A3)
        || (ch >= 0xF900 && ch <= 0xFAFF) || (ch >= 0xFE30 && ch <= 0xFE4F) || (ch >= 0xFF00 && ch <= 0xFF60)
        || (ch >= 0xFFE0 && ch <= 0xFFE6) || (ch >= 0x1F300 && ch <= 0x1F64F) || (ch >= 0x1F900 && ch <= 0x1F9FF)
        || (ch >= 0x20000 && ch <= 0x3FFFD)) return 2;
    return 1;
}

void xdesk::ui::gui::VtScreen::feed(const QByteArray& data)
{
    for (const char c: data) {
        const unsigned char b = static_cast<unsigned char>(c);

        // UTF-8 decoding (only in the ground / osc text)
        if (this->_utf8Left > 0) {
            if ((b & 0xC0) == 0x80) {
                this->_utf8 = (this->_utf8 << 6) | (b & 0x3F);
                if (--this->_utf8Left == 0 && this->_state == Parse::Ground) this->put_(this->_utf8);
                continue;
            }
            this->_utf8Left = 0; // invalid sequence: dropped, the byte is processed below
            this->put_(U'�');
        }

        // Controls are executed in every state (except the osc text)
        if (b < 0x20 && this->_state != Parse::Osc) {
            if (b == 0x1B) {
                this->_state = Parse::Escape;
                this->_params.clear();
                this->_intermediate.clear();
            } else if (b == 0x18 || b == 0x1A) {
                this->_state = Parse::Ground; // CAN / SUB abort the sequence
            } else {
                this->control_(c);
            }
            continue;
        }
        switch (this->_state) {
            case Parse::Ground:
                if (b < 0x80) this->put_(static_cast<char32_t>(b));
                else if ((b & 0xE0) == 0xC0) {this->_utf8 = b & 0x1F; this->_utf8Left = 1;}
                else if ((b & 0xF0) == 0xE0) {this->_utf8 = b & 0x0F; this->_utf8Left = 2;}
                else if ((b & 0xF8) == 0xF0) {this->_utf8 = b & 0x07; this->_utf8Left = 3;}
                else this->put_(U'�');
                break;
            case Parse::Escape:
                this->escape_(c);
                break;
            case Parse::Csi:
                if (b >= 0x30 && b <= 0x3F) {
                    if (b == '?' || b == '>' || b == '<' || b == '=') this->_intermediate.append(c);
                    else if (this->_params.size() < 128) this->_params.append(c);
                } else if (b >= 0x20 && b <= 0x2F) {
                    this->_intermediate.append(c);
                } else if (b >= 0x40 && b <= 0x7E) {
                    this->csi_(c);
                    this->_state = Parse::Ground;
                } else {
                    this->_state = Parse::Ground;
                }
                break;
            case Parse::Osc:
                if (b == 0x07) {this->osc_(); this->_state = Parse::Ground;}
                else if (b == 0x1B) this->_state = Parse::OscEscape;
                else if (this->_osc.size() < 4096) this->_osc.append(c);
                break;
            case Parse::OscEscape:
                this->osc_(); // ESC \ (string terminator)
                this->_state = Parse::Ground;
                break;
            case Parse::Charset:
                this->_lineDrawing = (c == '0');
                this->_state = Parse::Ground;
                break;
            case Parse::Ignore:
                if (b == 0x9C || c == '\\') this->_state = Parse::Ground;
                break;
        }
    }
    emit this->changed();
}

void xdesk::ui::gui::VtScreen::control_(const char c)
{
    switch (c) {
        case '\a': emit this->bell(); break;
        case '\b':
            if (this->_x > 0) --this->_x;
            this->_wrapPending = false;
            break;
        case '\t': {
            int x = this->_x + 1;
            while (x < this->_cols - 1 && !this->_tabs[x])
                ++x;
            this->_x = std::min(x, this->_cols - 1);
            break;
        }
        case '\n':
        case '\v':
        case '\f': this->lineFeed_(); break;
        case '\r': this->_x = 0; this->_wrapPending = false; break;
        case 0x0E: this->_lineDrawing = true; break; // SO (G1, used as line drawing by some programs)
        case 0x0F: this->_lineDrawing = false; break; // SI
        default: break;
    }
}

void xdesk::ui::gui::VtScreen::lineFeed_(void)
{
    this->_wrapPending = false;
    if (this->_y == this->_bottom) this->scrollUp_(this->_top, this->_bottom, 1);
    else if (this->_y < this->_rows - 1) ++this->_y;
}

void xdesk::ui::gui::VtScreen::scrollUp_(const int top, const int bottom, const int count)
{
    QVector<Line>& screen = this->screen_();
    const int n = std::min(count, bottom - top + 1);

    for (int i = 0; i < n; ++i) {
        // Only full screen scrolls of the primary screen feed the scrollback
        if (!this->_altActive && top == 0) {
            this->_scrollback.append(screen[top]);
            if (this->_scrollback.size() > VT_SCROLLBACK) this->_scrollback.removeFirst();
        }
        screen.remove(top);
        screen.insert(bottom, this->blankLine_());
    }
}

void xdesk::ui::gui::VtScreen::scrollDown_(const int top, const int bottom, const int count)
{
    QVector<Line>& screen = this->screen_();
    const int n = std::min(count, bottom - top + 1);

    for (int i = 0; i < n; ++i) {
        screen.remove(bottom);
        screen.insert(top, this->blankLine_());
    }
}

void xdesk::ui::gui::VtScreen::put_(const char32_t raw)
{
    char32_t ch = raw;
    if (this->_lineDrawing && ch >= 0x60 && ch <= 0x7E) ch = LINE_DRAWING[ch - 0x60];
    const int width = charWidth_(ch);
    if (width == 0) return;

    // Deferred wrap: the previous character filled the last column
    if (this->_wrapPending) {
        if (this->_autoWrap) {
            this->_x = 0;
            this->lineFeed_();
        }
        this->_wrapPending = false;
    }
    if (width == 2 && this->_x == this->_cols - 1) {
        if (!this->_autoWrap) return;
        this->_x = 0;
        this->lineFeed_();
    }

    Line& line = this->screen_()[this->_y];
    if (this->_insertMode) {
        for (int i = 0; i < width; ++i) {
            line.insert(this->_x, Cell());
            line.removeLast();
        }
    }
    Cell cell = this->_pen;
    cell.ch = ch;
    cell.attr = static_cast<std::uint8_t>((cell.attr & 0x3F) | (width == 2 ? Wide : 0));
    line[this->_x] = cell;
    if (width == 2) {
        Cell tail = this->_pen;
        tail.ch = U' ';
        tail.attr = static_cast<std::uint8_t>((tail.attr & 0x3F) | WideTail);
        line[this->_x + 1] = tail;
    }

    // Advance
    if (this->_x + width >= this->_cols) {
        this->_x = this->_cols - 1;
        this->_wrapPending = true;
    } else {
        this->_x += width;
    }
}

void xdesk::ui::gui::VtScreen::escape_(const char c)
{
    this->_state = Parse::Ground;
    switch (c) {
        case '[': this->_state = Parse::Csi; break;
        case ']': this->_state = Parse::Osc; this->_osc.clear(); break;
        case '(': this->_state = Parse::Charset; break;
        case ')':
        case '*':
        case '+': this->_state = Parse::Ignore; break; // G1 - G3 designations: next byte dropped
        case 'P':
        case '^':
        case '_': this->_state = Parse::Ignore; break; // DCS / PM / APC strings
        case '7': this->_savedX = this->_x; this->_savedY = this->_y; this->_savedPen = this->_pen; break;
        case '8': this->_x = this->_savedX; this->_y = this->_savedY; this->_pen = this->_savedPen; this->clamp_(); break;
        case 'D': this->lineFeed_(); break;
        case 'E': this->_x = 0; this->lineFeed_(); break;
        case 'H': if (this->_x < this->_cols) this->_tabs[this->_x] = true; break;
        case 'M':
            if (this->_y == this->_top) this->scrollDown_(this->_top, this->_bottom, 1);
            else if (this->_y > 0) --this->_y;
            break;
        case 'c': this->reset(); break;
        case '=': this->_appKeypad = true; break;
        case '>': this->_appKeypad = false; break;
        default: break;
    }
}

void xdesk::ui::gui::VtScreen::clamp_(void)
{
    this->_x = std::clamp(this->_x, 0, this->_cols - 1);
    this->_y = std::clamp(this->_y, 0, this->_rows - 1);
    this->_wrapPending = false;
}

void xdesk::ui::gui::VtScreen::eraseInLine_(const int mode)
{
    Line& line = this->screen_()[this->_y];
    const Cell blank = this->blankLine_().front();
    const int from = (mode == 0 ? this->_x : 0);
    const int to = (mode == 1 ? this->_x : this->_cols - 1);

    for (int i = from; i <= to && i < this->_cols; ++i)
        line[i] = blank;
}

void xdesk::ui::gui::VtScreen::eraseInDisplay_(const int mode)
{
    QVector<Line>& screen = this->screen_();

    if (mode == 3) {this->_scrollback.clear(); return;}
    this->eraseInLine_(mode == 2 ? 2 : mode);
    if (mode == 0 || mode == 2)
        for (int y = (mode == 0 ? this->_y + 1: 0); y < this->_rows; ++y)
            if (y != this->_y || mode == 2) screen[y] = this->blankLine_();
    if (mode == 1)
        for (int y = 0; y < this->_y; ++y)
            screen[y] = this->blankLine_();
}

void xdesk::ui::gui::VtScreen::csi_(const char final)
{
    QVector<int> p;
    const QList<QByteArray> parts = this->_params.split(';');
    const bool priv = this->_intermediate.contains('?');
    QVector<Line>& screen = this->screen_();

    for (const QByteArray& part: parts) {
        if (p.size() >= VT_MAX_PARAMS) break;
        // Sub parameters (38:2:r:g:b) are flattened
        for (const QByteArray& sub: part.split(':'))
            p.append(sub.isEmpty() ? -1 : std::min(sub.toInt(), 65535));
    }
    const int a = (p.isEmpty() || p[0] <= 0 ? 1 : p[0]); // first parameter, default 1
    const int b = (p.size() < 2 || p[1] <= 0 ? 1 : p[1]);
    const int raw = (p.isEmpty() || p[0] < 0 ? 0 : p[0]); // first parameter, default 0

    if (!this->_intermediate.isEmpty() && !priv && final != 'q') return; // >, =, intermediates: unsupported
    switch (final) {
        case 'A': this->_y = std::max(this->_y - a, (this->_y >= this->_top ? this->_top : 0)); this->_wrapPending = false; break;
        case 'B': this->_y = std::min(this->_y + a, (this->_y <= this->_bottom ? this->_bottom : this->_rows - 1)); this->_wrapPending = false; break;
        case 'C': this->_x = std::min(this->_x + a, this->_cols - 1); this->_wrapPending = false; break;
        case 'D': this->_x = std::max(this->_x - a, 0); this->_wrapPending = false; break;
        case 'E': this->_x = 0; this->_y = std::min(this->_y + a, this->_rows - 1); this->_wrapPending = false; break;
        case 'F': this->_x = 0; this->_y = std::max(this->_y - a, 0); this->_wrapPending = false; break;
        case 'G':
        case '`': this->_x = a - 1; this->clamp_(); break;
        case 'd': this->_y = a - 1 + (this->_originMode ? this->_top : 0); this->clamp_(); break;
        case 'H':
        case 'f': this->_y = a - 1 + (this->_originMode ? this->_top : 0); this->_x = b - 1; this->clamp_(); break;
        case 'J': this->eraseInDisplay_(raw); break;
        case 'K': this->eraseInLine_(raw); break;
        case 'L':
            if (this->_y >= this->_top && this->_y <= this->_bottom) this->scrollDown_(this->_y, this->_bottom, a);
            break;
        case 'M':
            if (this->_y >= this->_top && this->_y <= this->_bottom)
                for (int i = 0; i < std::min(a, this->_bottom - this->_y + 1); ++i) {
                    screen.remove(this->_y);
                    screen.insert(this->_bottom, this->blankLine_());
                }
            break;
        case '@': {
            Line& line = screen[this->_y];
            for (int i = 0; i < std::min(a, this->_cols - this->_x); ++i) {
                line.insert(this->_x, this->blankLine_().front());
                line.removeLast();
            }
            break;
        }
        case 'P': {
            Line& line = screen[this->_y];
            for (int i = 0; i < std::min(a, this->_cols - this->_x); ++i) {
                line.remove(this->_x);
                line.append(this->blankLine_().front());
            }
            break;
        }
        case 'X': {
            Line& line = screen[this->_y];
            for (int i = this->_x; i < std::min(this->_x + a, this->_cols); ++i)
                line[i] = this->blankLine_().front();
            break;
        }
        case 'S': this->scrollUp_(this->_top, this->_bottom, a); break;
        case 'T': this->scrollDown_(this->_top, this->_bottom, a); break;
        case 'g':
            if (raw == 0 && this->_x < this->_cols) this->_tabs[this->_x] = false;
            else if (raw == 3) this->_tabs.fill(false);
            break;
        case 'm': this->sgr_(p); break;
        case 'h':
        case 'l':
            if (priv) this->privateMode_(p, final == 'h');
            else if (raw == 4) this->_insertMode = (final == 'h');
            break;
        case 'r': {
            const int top = (p.isEmpty() || p[0] <= 0 ? 1 : p[0]) - 1;
            const int bottom = (p.size() < 2 || p[1] <= 0 ? this->_rows : p[1]) - 1;
            if (top < bottom && bottom < this->_rows) {
                this->_top = top;
                this->_bottom = bottom;
                this->_x = 0;
                this->_y = (this->_originMode ? this->_top : 0);
            }
            break;
        }
        case 's': this->_savedX = this->_x; this->_savedY = this->_y; break;
        case 'u': this->_x = this->_savedX; this->_y = this->_savedY; this->clamp_(); break;
        case 'n':
            if (raw == 6) emit this->reply(QString("\x1b[%1;%2R").arg(this->_y + 1).arg(this->_x + 1).toLatin1());
            else if (raw == 5) emit this->reply("\x1b[0n");
            break;
        case 'c':
            if (!priv) emit this->reply("\x1b[?62;22c"); // VT220 with ANSI colors
            break;
        case 't': break; // window manipulation: ignored
        default: break;
    }
}

void xdesk::ui::gui::VtScreen::privateMode_(const QVector<int>& params, const bool set)
{
    for (const int mode: params) {
        switch (mode) {
            case 1: this->_appCursor = set; break;
            case 6: this->_originMode = set; this->_x = 0; this->_y = (set ? this->_top : 0); break;
            case 7: this->_autoWrap = set; break;
            case 25: this->_cursorVisible = set; break;
            case 1000:
            case 1002:
            case 1003: this->_mouseMode = (set ? mode : 0); break;
            case 1006: this->_mouseSgr = set; break;
            case 2004: this->_bracketedPaste = set; break;
            case 47:
            case 1047:
            case 1049:
                if (set == this->_altActive) break;
                if (set) {
                    if (mode == 1049) {this->_savedX = this->_x; this->_savedY = this->_y; this->_savedPen = this->_pen;}
                    this->_alt = QVector<Line>(this->_rows, this->blankLine_());
                    this->_altActive = true;
                } else {
                    this->_altActive = false;
                    if (mode == 1049) {this->_x = this->_savedX; this->_y = this->_savedY; this->_pen = this->_savedPen; this->clamp_();}
                }
                break;
            default: break;
        }
    }
}

void xdesk::ui::gui::VtScreen::sgr_(const QVector<int>& params)
{
    if (params.isEmpty()) {this->_pen = Cell(); return;}
    for (int i = 0; i < params.size(); ++i) {
        const int v = (params[i] < 0 ? 0 : params[i]);

        if (v == 0) this->_pen = Cell();
        else if (v == 1) this->_pen.attr |= Bold;
        else if (v == 2) this->_pen.attr |= Dim;
        else if (v == 3) this->_pen.attr |= Italic;
        else if (v == 4) this->_pen.attr |= Underline;
        else if (v == 7) this->_pen.attr |= Inverse;
        else if (v == 9) this->_pen.attr |= Strike;
        else if (v == 21 || v == 22) this->_pen.attr &= static_cast<std::uint8_t>(~(Bold | Dim));
        else if (v == 23) this->_pen.attr &= static_cast<std::uint8_t>(~Italic);
        else if (v == 24) this->_pen.attr &= static_cast<std::uint8_t>(~Underline);
        else if (v == 27) this->_pen.attr &= static_cast<std::uint8_t>(~Inverse);
        else if (v == 29) this->_pen.attr &= static_cast<std::uint8_t>(~Strike);
        else if (v >= 30 && v <= 37) this->_pen.fg = v - 30;
        else if (v == 39) this->_pen.fg = VT_DEFAULT_COLOR;
        else if (v >= 40 && v <= 47) this->_pen.bg = v - 40;
        else if (v == 49) this->_pen.bg = VT_DEFAULT_COLOR;
        else if (v >= 90 && v <= 97) this->_pen.fg = v - 90 + 8;
        else if (v >= 100 && v <= 107) this->_pen.bg = v - 100 + 8;
        else if (v == 38 || v == 48) {
            std::int32_t color = VT_DEFAULT_COLOR;
            // 38;5;n (palette) or 38;2;r;g;b (truecolor)
            if (i + 2 < params.size() && params[i + 1] == 5) {
                color = std::clamp(params[i + 2], 0, 255);
                i += 2;
            } else if (i + 4 < params.size() && params[i + 1] == 2) {
                color = VT_TRUECOLOR + ((std::clamp(params[i + 2], 0, 255) << 16) | (std::clamp(params[i + 3], 0, 255) << 8) | std::clamp(params[i + 4], 0, 255));
                i += 4;
            } else {
                break;
            }
            if (v == 38) this->_pen.fg = color;
            else this->_pen.bg = color;
        }
    }
}

void xdesk::ui::gui::VtScreen::osc_(void)
{
    const qsizetype sep = this->_osc.indexOf(';');
    if (sep < 0) return;
    const QByteArray command = this->_osc.left(sep);

    if (command == "0" || command == "2") {
        this->_title = QString::fromUtf8(this->_osc.mid(sep + 1)).left(256);
        emit this->titleChanged(this->_title);
    }
}

void xdesk::ui::gui::VtScreen::resize(const int cols, const int rows)
{
    const int c = std::max(cols, 2);
    const int r = std::max(rows, 2);
    if (c == this->_cols && r == this->_rows) return;

    for (QVector<Line>* screen: {&this->_main, &this->_alt}) {
        // Rows removed from the top of the primary screen go to the scrollback when the cursor would leave it
        while (screen->size() > r) {
            if (screen == &this->_main && this->_y >= r && !this->_altActive) {
                this->_scrollback.append(screen->front());
                screen->removeFirst();
                --this->_y;
            } else {
                screen->removeLast();
            }
        }
        for (Line& line: *screen)
            line.resize(c);
        while (screen->size() < r)
            screen->append(Line(c, Cell()));
    }
    for (Line& line: this->_scrollback)
        line.resize(c);
    this->_cols = c;
    this->_rows = r;
    this->_top = 0;
    this->_bottom = r - 1;
    this->_tabs.resize(c);
    for (int i = 0; i < c; i += 8)
        this->_tabs[i] = true;
    this->clamp_();
    emit this->changed();
}

const xdesk::ui::gui::Line& xdesk::ui::gui::VtScreen::line(const int row, const int scroll) const
{
    const QVector<Line>& screen = (this->_altActive ? this->_alt : this->_main);
    const int back = (this->_altActive ? 0 : std::clamp(scroll, 0, static_cast<int>(this->_scrollback.size())));
    const int index = row - back; // < 0: inside the scrollback

    if (index >= 0) return screen[std::min(index, this->_rows - 1)];
    return this->_scrollback[this->_scrollback.size() + index];
}

QString xdesk::ui::gui::VtScreen::text(const QPoint& from, const QPoint& to, const int scroll) const
{
    QPoint a = from;
    QPoint b = to;
    QString out;

    if (b.y() < a.y() || (b.y() == a.y() && b.x() < a.x())) std::swap(a, b);
    for (int y = std::max(a.y(), 0); y <= std::min(b.y(), this->_rows - 1); ++y) {
        const Line& line = this->line(y, scroll);
        const int start = (y == a.y() ? a.x() : 0);
        const int end = (y == b.y() ? b.x() : this->_cols - 1);
        QString row;
        for (int x = std::max(start, 0); x <= std::min(end, static_cast<int>(line.size()) - 1); ++x) {
            if (line[x].attr & WideTail) continue;
            const char32_t ch = line[x].ch;
            row += QString::fromUcs4(&ch, 1);
        }
        // Trailing blanks of a line are not copied
        while (row.endsWith(' '))
            row.chop(1);
        out += row;
        if (y != b.y()) out += '\n';
    }
    return out;
}
