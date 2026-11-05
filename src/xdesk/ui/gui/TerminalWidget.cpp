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
##  @file TerminalWidget.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/gui/TerminalWidget.hpp"
#include "xdesk/ui/gui/Theme.hpp"
#include <QInputMethodEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <functional>
#include <QClipboard>
#include <QKeyEvent>
#include <algorithm>
#include <QPainter>
#include <QTimer>

xdesk::ui::gui::TerminalWidget::TerminalWidget(QWidget* parent)
: QWidget(parent)
{
    this->_font = QFont(Theme::monospaceFamily());
    this->_font.setStyleHint(QFont::TypeWriter);
    this->_font.setPointSizeF(11.0);
    this->setFocusPolicy(Qt::StrongFocus);
    this->setAttribute(Qt::WA_OpaquePaintEvent);
    this->setAttribute(Qt::WA_InputMethodEnabled);
    this->setCursor(Qt::IBeamCursor);
    this->setMinimumSize(200, 120);
    this->_scrollBar = new QScrollBar(Qt::Vertical, this);
    this->_scrollBar->setRange(0, 0);
    this->updateMetrics_();

    // Repaints are coalesced: a burst of output = one paint
    QObject::connect(&this->_screen, &VtScreen::changed, this, [this](void) {
        if (this->_repaintQueued) return;
        this->_repaintQueued = true;
        QTimer::singleShot(8, this, [this](void) {this->_repaintQueued = false; this->updateScrollBar_(); this->update();});
    });
    QObject::connect(&this->_screen, &VtScreen::titleChanged, this, &TerminalWidget::titleChanged);
    QObject::connect(&this->_screen, &VtScreen::reply, this, [this](const QByteArray& data) {
        if (!this->_readOnly) emit this->input(data);
    });
    QObject::connect(this->_scrollBar, &QScrollBar::valueChanged, this, [this](const int value) {
        this->_scroll = this->_scrollBar->maximum() - value;
        this->update();
    });
}

void xdesk::ui::gui::TerminalWidget::updateMetrics_(void)
{
    const QFontMetrics metrics(this->_font);
    this->_cellWidth = std::max(metrics.horizontalAdvance(QChar('M')), 1);
    this->_cellHeight = std::max(metrics.height(), 1);
    this->_ascent = metrics.ascent();
}

void xdesk::ui::gui::TerminalWidget::updateScrollBar_(void)
{
    const int back = this->_screen.scrollbackSize();
    const bool atBottom = (this->_scroll == 0);

    QSignalBlocker block(this->_scrollBar);
    this->_scrollBar->setRange(0, back);
    this->_scrollBar->setPageStep(this->_screen.rows());
    // Stay at the bottom while following the output, keep the position while reading the history
    if (atBottom) this->_scrollBar->setValue(back);
    else this->_scroll = std::min(this->_scroll, back);
    this->_scrollBar->setVisible(back > 0);
}

void xdesk::ui::gui::TerminalWidget::feed(const QByteArray& data)
{
    this->_screen.feed(data);
}

void xdesk::ui::gui::TerminalWidget::setReadOnly(const bool read_only)
{
    this->_readOnly = read_only;
    this->setCursor(read_only ? Qt::ArrowCursor : Qt::IBeamCursor);
}

void xdesk::ui::gui::TerminalWidget::setGridSize(const int cols, const int rows)
{
    this->_screen.resize(cols, rows);
    this->update();
}

void xdesk::ui::gui::TerminalWidget::clear(void)
{
    this->_screen.reset();
}

QColor xdesk::ui::gui::TerminalWidget::color_(const std::int32_t color, const bool foreground) const
{
    if (color == VT_DEFAULT_COLOR) return foreground ? Theme::palette().terminalFg : Theme::palette().terminalBg;
    if (color >= VT_TRUECOLOR) return QColor::fromRgb(static_cast<QRgb>(color - VT_TRUECOLOR));
    return Theme::terminalColor(color);
}

bool xdesk::ui::gui::TerminalWidget::selected_(const int x, const int y) const
{
    if (this->_selFrom.x() < 0) return false;
    QPoint a = this->_selFrom;
    QPoint b = this->_selTo;
    if (b.y() < a.y() || (b.y() == a.y() && b.x() < a.x())) std::swap(a, b);
    if (y < a.y() || y > b.y()) return false;
    if (y == a.y() && x < a.x()) return false;
    if (y == b.y() && x > b.x()) return false;
    return true;
}

void xdesk::ui::gui::TerminalWidget::paintEvent(QPaintEvent* event)
{
    (void)event;
    QPainter p(this);
    const Palette& pal = Theme::palette();
    const int padding = 6;

    p.fillRect(this->rect(), pal.terminalBg);
    for (int y = 0; y < this->_screen.rows(); ++y) {
        const Line& line = this->_screen.line(y, this->_scroll);
        const int top = padding + y * this->_cellHeight;
        int x = 0;

        // Runs of cells with the same style: one fill + one drawText
        while (x < line.size()) {
            const Cell& first = line[x];
            const bool sel = this->selected_(x, y);
            int end = x + 1;
            while (end < line.size() && line[end].sameStyle(first) && this->selected_(end, y) == sel)
                ++end;

            QColor fg = this->color_(first.fg, true);
            QColor bg = this->color_(first.bg, false);
            if ((first.attr & Bold) && first.fg >= 0 && first.fg < 8) fg = Theme::terminalColor(first.fg + 8);
            if (first.attr & Inverse) std::swap(fg, bg);
            if (sel) {bg = pal.accent; fg = pal.accentText;}
            if (first.attr & Dim) fg.setAlphaF(0.6f);

            const QRect area(padding + x * this->_cellWidth, top, (end - x) * this->_cellWidth, this->_cellHeight);
            if (bg != pal.terminalBg) p.fillRect(area, bg);

            QString text;
            for (int i = x; i < end; ++i) {
                if (line[i].attr & WideTail) continue;
                const char32_t ch = line[i].ch;
                text += QString::fromUcs4(&ch, 1);
            }
            if (!text.trimmed().isEmpty()) {
                QFont font = this->_font;
                font.setBold(first.attr & Bold);
                font.setItalic(first.attr & Italic);
                font.setUnderline(first.attr & Underline);
                font.setStrikeOut(first.attr & Strike);
                p.setFont(font);
                p.setPen(fg);
                // Cell by cell when wide characters are present (keeps the grid aligned)
                bool hasWide = false;
                for (int i = x; i < end && !hasWide; ++i)
                    hasWide = (line[i].attr & Wide);
                if (hasWide) {
                    for (int i = x; i < end; ++i) {
                        if (line[i].attr & WideTail) continue;
                        const char32_t ch = line[i].ch;
                        p.drawText(padding + i * this->_cellWidth, top + this->_ascent, QString::fromUcs4(&ch, 1));
                    }
                } else {
                    p.drawText(area.left(), top + this->_ascent, text);
                }
            }
            x = end;
        }
    }

    // Cursor (only on the live view)
    if (this->_screen.cursorVisible() && this->_scroll == 0) {
        const QPoint c = this->_screen.cursor();
        const QRect cell(padding + c.x() * this->_cellWidth, padding + c.y() * this->_cellHeight, this->_cellWidth, this->_cellHeight);
        QColor color = pal.accent;
        if (this->hasFocus() && !this->_readOnly) {
            color.setAlphaF(0.75f);
            p.fillRect(cell, color);
        } else {
            p.setPen(color);
            p.drawRect(cell.adjusted(0, 0, -1, -1));
        }
    }
}

void xdesk::ui::gui::TerminalWidget::resizeEvent(QResizeEvent* event)
{
    (void)event;
    const int bar = this->_scrollBar->sizeHint().width();
    this->_scrollBar->setGeometry(this->width() - bar, 0, bar, this->height());
    const int cols = std::max((this->width() - 12 - bar) / this->_cellWidth, 10);
    const int rows = std::max((this->height() - 12) / this->_cellHeight, 4);

    if (this->_readOnly) return; // a mirror keeps the size of the remote terminal
    if (cols != this->_screen.cols() || rows != this->_screen.rows()) {
        this->_screen.resize(cols, rows);
        emit this->sizeChanged(cols, rows);
    }
}

QByteArray xdesk::ui::gui::TerminalWidget::keyToBytes(const QKeyEvent* event, const bool app_cursor)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods & Qt::ControlModifier;
    const bool alt = mods & Qt::AltModifier;
    const bool shift = mods & Qt::ShiftModifier;
    const int modCode = 1 + (shift ? 1 : 0) + (alt ? 2 : 0) + (ctrl ? 4 : 0); // xterm modifier parameter

    // Cursor / editing keys: CSI with the modifier parameter when modified
    const std::function<QByteArray(const char, const bool)> csiKey = [&](const char final, const bool ss3_when_app) -> QByteArray {
        if (modCode > 1) return QByteArray("\x1b[1;") + QByteArray::number(modCode) + final;
        if (ss3_when_app && app_cursor) return QByteArray("\x1bO") + final;
        return QByteArray("\x1b[") + final;
    };
    const std::function<QByteArray(const int)> tildeKey = [&](const int number) -> QByteArray {
        if (modCode > 1) return QByteArray("\x1b[") + QByteArray::number(number) + ';' + QByteArray::number(modCode) + '~';
        return QByteArray("\x1b[") + QByteArray::number(number) + '~';
    };

    switch (event->key()) {
        case Qt::Key_Up: return csiKey('A', true);
        case Qt::Key_Down: return csiKey('B', true);
        case Qt::Key_Right: return csiKey('C', true);
        case Qt::Key_Left: return csiKey('D', true);
        case Qt::Key_Home: return csiKey('H', true);
        case Qt::Key_End: return csiKey('F', true);
        case Qt::Key_Insert: return tildeKey(2);
        case Qt::Key_Delete: return tildeKey(3);
        case Qt::Key_PageUp: return tildeKey(5);
        case Qt::Key_PageDown: return tildeKey(6);
        case Qt::Key_F1: return modCode > 1 ? csiKey('P', false) : QByteArray("\x1bOP");
        case Qt::Key_F2: return modCode > 1 ? csiKey('Q', false) : QByteArray("\x1bOQ");
        case Qt::Key_F3: return modCode > 1 ? csiKey('R', false) : QByteArray("\x1bOR");
        case Qt::Key_F4: return modCode > 1 ? csiKey('S', false) : QByteArray("\x1bOS");
        case Qt::Key_F5: return tildeKey(15);
        case Qt::Key_F6: return tildeKey(17);
        case Qt::Key_F7: return tildeKey(18);
        case Qt::Key_F8: return tildeKey(19);
        case Qt::Key_F9: return tildeKey(20);
        case Qt::Key_F10: return tildeKey(21);
        case Qt::Key_F11: return tildeKey(23);
        case Qt::Key_F12: return tildeKey(24);
        case Qt::Key_Return:
        case Qt::Key_Enter: return alt ? QByteArray("\x1b\r") : QByteArray("\r");
        case Qt::Key_Backspace: return ctrl ? QByteArray("\x08") : (alt ? QByteArray("\x1b\x7f") : QByteArray("\x7f"));
        case Qt::Key_Tab: return QByteArray("\t");
        case Qt::Key_Backtab: return QByteArray("\x1b[Z");
        case Qt::Key_Escape: return QByteArray("\x1b");
        default: break;
    }

    // Control characters: Ctrl + letter / @ [ \ ] ^ _ / space
    if (ctrl && !alt) {
        const int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z) return QByteArray(1, static_cast<char>(key - Qt::Key_A + 1));
        if (key == Qt::Key_Space || key == Qt::Key_At || key == Qt::Key_2) return QByteArray(1, '\0');
        if (key == Qt::Key_BracketLeft || key == Qt::Key_3) return QByteArray("\x1b");
        if (key == Qt::Key_Backslash || key == Qt::Key_4) return QByteArray("\x1c");
        if (key == Qt::Key_BracketRight || key == Qt::Key_5) return QByteArray("\x1d");
        if (key == Qt::Key_AsciiCircum || key == Qt::Key_6) return QByteArray("\x1e");
        if (key == Qt::Key_Underscore || key == Qt::Key_Minus || key == Qt::Key_7) return QByteArray("\x1f");
        if (key == Qt::Key_Question || key == Qt::Key_8) return QByteArray("\x7f");
    }

    const QByteArray text = event->text().toUtf8();
    if (text.isEmpty()) return {};
    if (alt && !ctrl) return QByteArray("\x1b") + text; // meta sends escape
    return text;
}

void xdesk::ui::gui::TerminalWidget::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();

    // Local shortcuts: copy / paste / scroll the history
    if ((mods & Qt::ControlModifier) && (mods & Qt::ShiftModifier)) {
        if (event->key() == Qt::Key_C) {this->copySelection(); return;}
        if (event->key() == Qt::Key_V) {this->paste(); return;}
    }
    if (mods & Qt::ShiftModifier) {
        if (event->key() == Qt::Key_PageUp) {this->_scrollBar->setValue(this->_scrollBar->value() - this->_screen.rows() / 2); return;}
        if (event->key() == Qt::Key_PageDown) {this->_scrollBar->setValue(this->_scrollBar->value() + this->_screen.rows() / 2); return;}
        if (event->key() == Qt::Key_Insert) {this->paste(); return;}
    }
    if (this->_readOnly) return;

    const QByteArray bytes = keyToBytes(event, this->_screen.appCursor());
    if (bytes.isEmpty()) return;
    this->_selFrom = {-1, -1};
    this->_scrollBar->setValue(this->_scrollBar->maximum()); // typing returns to the live view
    emit this->input(bytes);
}

void xdesk::ui::gui::TerminalWidget::inputMethodEvent(QInputMethodEvent* event)
{
    if (!this->_readOnly && !event->commitString().isEmpty()) emit this->input(event->commitString().toUtf8());
    event->accept();
}

bool xdesk::ui::gui::TerminalWidget::focusNextPrevChild(const bool next)
{
    (void)next;
    return false;
}

QPoint xdesk::ui::gui::TerminalWidget::cellAt_(const QPoint& pos) const
{
    const int x = std::clamp((pos.x() - 6) / this->_cellWidth, 0, this->_screen.cols() - 1);
    const int y = std::clamp((pos.y() - 6) / this->_cellHeight, 0, this->_screen.rows() - 1);
    return QPoint(x, y);
}

void xdesk::ui::gui::TerminalWidget::sendMouse_(QMouseEvent* event, const int button, const bool press, const bool motion)
{
    const QPoint cell = this->cellAt_(event->position().toPoint());
    int code = button + (motion ? 32 : 0);
    if (event->modifiers() & Qt::ShiftModifier) code += 4;
    if (event->modifiers() & Qt::AltModifier) code += 8;
    if (event->modifiers() & Qt::ControlModifier) code += 16;

    if (this->_screen.mouseSgr()) {
        emit this->input(QString("\x1b[<%1;%2;%3%4").arg(code).arg(cell.x() + 1).arg(cell.y() + 1).arg(press ? 'M' : 'm').toLatin1());
    } else {
        QByteArray legacy("\x1b[M");
        legacy.append(static_cast<char>(32 + (press ? code : 3)));
        legacy.append(static_cast<char>(std::min(32 + cell.x() + 1, 255)));
        legacy.append(static_cast<char>(std::min(32 + cell.y() + 1, 255)));
        emit this->input(legacy);
    }
}

void xdesk::ui::gui::TerminalWidget::mousePressEvent(QMouseEvent* event)
{
    this->setFocus();
    // Applications asking for the mouse get it (shift keeps the local selection)
    if (!this->_readOnly && this->_screen.mouseMode() != 0 && !(event->modifiers() & Qt::ShiftModifier)) {
        const int button = (event->button() == Qt::LeftButton ? 0 : (event->button() == Qt::MiddleButton ? 1 : 2));
        this->sendMouse_(event, button, true, false);
        return;
    }
    if (event->button() == Qt::MiddleButton) {this->paste(); return;}
    if (event->button() != Qt::LeftButton) return;
    this->_selecting = true;
    this->_selFrom = this->_selTo = this->cellAt_(event->position().toPoint());
    this->update();
}

void xdesk::ui::gui::TerminalWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!this->_readOnly && this->_screen.mouseMode() >= 1002 && !(event->modifiers() & Qt::ShiftModifier) && (event->buttons() || this->_screen.mouseMode() == 1003)) {
        const int button = (event->buttons() & Qt::LeftButton ? 0 : (event->buttons() & Qt::MiddleButton ? 1 : (event->buttons() & Qt::RightButton ? 2 : 3)));
        this->sendMouse_(event, button, true, true);
        return;
    }
    if (!this->_selecting) return;
    this->_selTo = this->cellAt_(event->position().toPoint());
    this->update();
}

void xdesk::ui::gui::TerminalWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!this->_readOnly && this->_screen.mouseMode() != 0 && !(event->modifiers() & Qt::ShiftModifier) && !this->_selecting) {
        const int button = (event->button() == Qt::LeftButton ? 0 : (event->button() == Qt::MiddleButton ? 1 : 2));
        this->sendMouse_(event, button, false, false);
        return;
    }
    if (!this->_selecting) return;
    this->_selecting = false;
    if (this->_selFrom == this->_selTo) this->_selFrom = {-1, -1};
    else this->copySelection(); // X11 habit: selecting copies
    this->update();
}

void xdesk::ui::gui::TerminalWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Select the word under the cursor
    const QPoint cell = this->cellAt_(event->position().toPoint());
    const Line& line = this->_screen.line(cell.y(), this->_scroll);
    const std::function<bool(const int)> isWord = [&](const int x) {const char32_t ch = line[x].ch; return ch != U' ' && ch != U'\t' && ch != U'"' && ch != U'\'' && ch != U'(' && ch != U')';};
    int from = cell.x();
    int to = cell.x();

    if (!isWord(cell.x())) return;
    while (from > 0 && isWord(from - 1))
        --from;
    while (to < line.size() - 1 && isWord(to + 1))
        ++to;
    this->_selFrom = QPoint(from, cell.y());
    this->_selTo = QPoint(to, cell.y());
    this->copySelection();
    this->update();
}

void xdesk::ui::gui::TerminalWidget::wheelEvent(QWheelEvent* event)
{
    const int notches = event->angleDelta().y() / 40; // 3 lines per notch
    if (!this->_readOnly && this->_screen.mouseMode() != 0) {
        // Wheel as mouse buttons 64 / 65 for the applications
        const QPoint cell = this->cellAt_(event->position().toPoint());
        const int button = (event->angleDelta().y() > 0 ? 64 : 65);
        emit this->input(QString("\x1b[<%1;%2;%3M").arg(button).arg(cell.x() + 1).arg(cell.y() + 1).toLatin1());
        return;
    }
    this->_scrollBar->setValue(this->_scrollBar->value() - notches);
}

void xdesk::ui::gui::TerminalWidget::copySelection(void)
{
    if (this->_selFrom.x() < 0) return;
    const QString text = this->_screen.text(this->_selFrom, this->_selTo, this->_scroll);
    QApplication::clipboard()->setText(text);
    if (QApplication::clipboard()->supportsSelection()) QApplication::clipboard()->setText(text, QClipboard::Selection);
}

void xdesk::ui::gui::TerminalWidget::paste(void)
{
    if (this->_readOnly) return;
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty()) return;
    text.replace("\r\n", "\r");
    text.replace('\n', '\r');
    QByteArray bytes = text.toUtf8();
    // Bracketed paste: the shell knows it is not typed (no command executed by a pasted newline)
    if (this->_screen.bracketedPaste()) bytes = QByteArray("\x1b[200~") + bytes.replace("\x1b[201~", "") + QByteArray("\x1b[201~");
    emit this->input(bytes);
}
