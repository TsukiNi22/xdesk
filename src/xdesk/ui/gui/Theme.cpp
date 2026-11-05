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
##  @file Theme.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/gui/Theme.hpp"
#include <QFontDatabase>
#include <QStyleFactory>
#include <QPainterPath>
#include <QStyleHints>
#include <QPainter>
#include <QPalette>
#include <QFont>
#include <cmath>

static xdesk::ui::gui::Palette current;

static xdesk::ui::gui::Palette dark_palette(void)
{
    xdesk::ui::gui::Palette p;
    p.dark = true;
    p.window = QColor("#0f1117");
    p.sidebar = QColor("#0b0d12");
    p.surface = QColor("#171a23");
    p.surfaceAlt = QColor("#1e2230");
    p.border = QColor("#2a2f3e");
    p.text = QColor("#e7e9f0");
    p.muted = QColor("#8a91a6");
    p.accent = QColor("#6d8bff");
    p.accentText = QColor("#ffffff");
    p.success = QColor("#3ecf8e");
    p.warning = QColor("#f5b14c");
    p.danger = QColor("#ff5d6c");
    p.terminalBg = QColor("#0b0d12");
    p.terminalFg = QColor("#d6dae6");
    return p;
}

static xdesk::ui::gui::Palette light_palette(void)
{
    xdesk::ui::gui::Palette p;
    p.dark = false;
    p.window = QColor("#f4f6fb");
    p.sidebar = QColor("#eceff6");
    p.surface = QColor("#ffffff");
    p.surfaceAlt = QColor("#f1f3f9");
    p.border = QColor("#dde1ec");
    p.text = QColor("#1b1f2a");
    p.muted = QColor("#626a80");
    p.accent = QColor("#3f63f2");
    p.accentText = QColor("#ffffff");
    p.success = QColor("#16a06a");
    p.warning = QColor("#c27c0e");
    p.danger = QColor("#d93348");
    p.terminalBg = QColor("#fbfcfe");
    p.terminalFg = QColor("#1f2430");
    return p;
}

static QString style_sheet(const xdesk::ui::gui::Palette& p)
{
    QString qss = R"(
* { color: @text; outline: 0; }
QMainWindow, QDialog, QStackedWidget > QWidget, QScrollArea, QScrollArea > QWidget > QWidget { background: @window; }
QToolTip { background: @surface; color: @text; border: 1px solid @border; padding: 6px 8px; border-radius: 6px; }

#sidebar { background: @sidebar; border-right: 1px solid @border; }
#sidebar QListWidget { background: transparent; border: none; }
#sidebar QListWidget::item { padding: 10px 14px; margin: 2px 10px; border-radius: 8px; color: @muted; }
#sidebar QListWidget::item:hover { background: @surfaceAlt; color: @text; }
#sidebar QListWidget::item:selected { background: @accentSoft; color: @accent; }
#brand { font-size: 17px; font-weight: 700; }

#card { background: @surface; border: 1px solid @border; border-radius: 14px; }
#title { font-size: 22px; font-weight: 700; }
#subtitle { font-size: 14px; font-weight: 600; }
#muted, QLabel[muted="true"] { color: @muted; }
#code { font-size: 46px; font-weight: 700; letter-spacing: 6px; color: @accent; font-family: "@mono"; }
#banner { background: @danger; border-radius: 10px; }
#banner QLabel { color: white; font-weight: 600; }
#badge { border-radius: 9px; padding: 2px 8px; background: @surfaceAlt; color: @muted; font-size: 11px; }
QLabel[tone="success"] { color: @success; }
QLabel[tone="warning"] { color: @warning; }
QLabel[tone="danger"] { color: @danger; }
QLabel[tone="accent"] { color: @accent; }

QPushButton, QToolButton { background: @surfaceAlt; border: 1px solid @border; border-radius: 8px; padding: 7px 14px; }
QPushButton:hover, QToolButton:hover { border-color: @accent; }
QPushButton:pressed, QToolButton:pressed { background: @border; }
QPushButton:disabled, QToolButton:disabled { color: @muted; background: @surface; }
QToolButton:checked { background: @accentSoft; border-color: @accent; color: @accent; }
QPushButton[kind="primary"] { background: @accent; color: @accentText; border: none; font-weight: 600; }
QPushButton[kind="primary"]:hover { background: @accentHover; }
QPushButton[kind="primary"]:disabled { background: @border; color: @muted; }
QPushButton[kind="danger"] { background: transparent; color: @danger; border: 1px solid @danger; }
QPushButton[kind="danger"]:hover { background: @danger; color: white; }
QPushButton[kind="ghost"] { background: transparent; border: none; color: @muted; padding: 6px; }
QPushButton[kind="ghost"]:hover { color: @accent; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: @surfaceAlt; border: 1px solid @border; border-radius: 8px; padding: 6px 8px;
    selection-background-color: @accent; selection-color: @accentText;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus { border-color: @accent; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: @surface; border: 1px solid @border; selection-background-color: @accentSoft; selection-color: @text; }
QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 0; border: none; }

QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px; border: 1px solid @border; background: @surfaceAlt; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
QCheckBox::indicator:disabled { background: @surface; }

QSlider::groove:horizontal { height: 4px; background: @border; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
QSlider::handle:horizontal { background: @accent; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }
QProgressBar { background: @surfaceAlt; border: none; border-radius: 2px; max-height: 4px; }
QProgressBar::chunk { background: @accent; border-radius: 2px; }

QTableWidget, QTreeWidget, QListWidget {
    background: @surface; border: 1px solid @border; border-radius: 10px; gridline-color: transparent;
    alternate-background-color: @surfaceAlt; selection-background-color: @accentSoft; selection-color: @text;
}
QTableWidget::item, QTreeWidget::item { padding: 6px; border: none; }
QHeaderView::section { background: @surface; color: @muted; border: none; border-bottom: 1px solid @border; padding: 8px 6px; font-weight: 600; }
QTableCornerButton::section { background: @surface; border: none; }

QTabWidget::pane { border: 1px solid @border; border-radius: 10px; top: -1px; background: @surface; }
QTabBar::tab { background: transparent; color: @muted; padding: 8px 16px; border: none; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: @accent; border-bottom: 2px solid @accent; }
QTabBar::tab:hover { color: @text; }

QToolBar { background: @surface; border: none; border-bottom: 1px solid @border; padding: 4px; spacing: 6px; }
QToolBar QToolButton { background: transparent; border: 1px solid transparent; padding: 6px 10px; }
QToolBar QToolButton:hover { border-color: @border; background: @surfaceAlt; }
QStatusBar { background: @surface; color: @muted; border-top: 1px solid @border; }
QMenu { background: @surface; border: 1px solid @border; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 6px 18px; border-radius: 6px; }
QMenu::item:selected { background: @accentSoft; color: @accent; }
QMenu::separator { height: 1px; background: @border; margin: 4px 8px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @muted; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @border; border-radius: 4px; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSplitter::handle { background: @border; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }
)";
    QColor soft = p.accent;
    soft.setAlphaF(p.dark ? 0.18f : 0.12f);
    const QString softRgba = QString("rgba(%1, %2, %3, %4)").arg(soft.red()).arg(soft.green()).arg(soft.blue()).arg(soft.alphaF(), 0, 'f', 2);

    qss.replace("@accentSoft", softRgba);
    qss.replace("@accentHover", p.accent.lighter(112).name());
    qss.replace("@accentText", p.accentText.name());
    qss.replace("@accent", p.accent.name());
    qss.replace("@surfaceAlt", p.surfaceAlt.name());
    qss.replace("@surface", p.surface.name());
    qss.replace("@sidebar", p.sidebar.name());
    qss.replace("@window", p.window.name());
    qss.replace("@border", p.border.name());
    qss.replace("@text", p.text.name());
    qss.replace("@muted", p.muted.name());
    qss.replace("@success", p.success.name());
    qss.replace("@warning", p.warning.name());
    qss.replace("@danger", p.danger.name());
    qss.replace("@mono", xdesk::ui::gui::Theme::monospaceFamily());
    return qss;
}

void xdesk::ui::gui::Theme::apply(QApplication& app, const QString& setting)
{
    bool dark = true;

    if (setting == "light") dark = false;
    else if (setting == "dark") dark = true;
    else dark = (QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light);
    current = (dark ? dark_palette() : light_palette());

    // Fusion: same metrics on every OS, fully driven by the palette + the style sheet
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette pal;
    pal.setColor(QPalette::Window, current.window);
    pal.setColor(QPalette::WindowText, current.text);
    pal.setColor(QPalette::Base, current.surfaceAlt);
    pal.setColor(QPalette::AlternateBase, current.surface);
    pal.setColor(QPalette::Text, current.text);
    pal.setColor(QPalette::Button, current.surfaceAlt);
    pal.setColor(QPalette::ButtonText, current.text);
    pal.setColor(QPalette::Highlight, current.accent);
    pal.setColor(QPalette::HighlightedText, current.accentText);
    pal.setColor(QPalette::ToolTipBase, current.surface);
    pal.setColor(QPalette::ToolTipText, current.text);
    pal.setColor(QPalette::PlaceholderText, current.muted);
    pal.setColor(QPalette::Link, current.accent);
    pal.setColor(QPalette::Disabled, QPalette::Text, current.muted);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, current.muted);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, current.muted);
    app.setPalette(pal);

    QFont font = app.font();
    if (font.pointSizeF() < 10.0) font.setPointSizeF(10.0);
    app.setFont(font);
    app.setStyleSheet(style_sheet(current));
    app.setWindowIcon(QIcon(logo(256)));
}

const xdesk::ui::gui::Palette& xdesk::ui::gui::Theme::palette(void)
{
    if (!current.window.isValid()) current = dark_palette();
    return current;
}

QString xdesk::ui::gui::Theme::monospaceFamily(void)
{
    const QStringList preferred = {"JetBrains Mono", "Cascadia Mono", "Fira Code", "Source Code Pro", "DejaVu Sans Mono", "Menlo", "Consolas"};
    const QStringList families = QFontDatabase::families();

    for (const QString& family: preferred)
        if (families.contains(family)) return family;
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

QPixmap xdesk::ui::gui::Theme::logo(const int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    const double s = size / 64.0;

    // Rounded square with a diagonal accent gradient-free two tone, a screen and an X cursor
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#3f63f2"));
    p.drawRoundedRect(QRectF(2 * s, 2 * s, 60 * s, 60 * s), 14 * s, 14 * s);
    p.setBrush(QColor("#6d8bff"));
    QPainterPath corner;
    corner.moveTo(62 * s, 22 * s);
    corner.lineTo(62 * s, 48 * s);
    corner.quadTo(62 * s, 62 * s, 48 * s, 62 * s);
    corner.lineTo(22 * s, 62 * s);
    corner.closeSubpath();
    p.drawPath(corner);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, 4 * s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(QRectF(13 * s, 15 * s, 38 * s, 26 * s), 4 * s, 4 * s);
    p.drawLine(QPointF(26 * s, 49 * s), QPointF(38 * s, 49 * s));
    p.setPen(QPen(Qt::white, 3.5 * s, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(25 * s, 23 * s), QPointF(39 * s, 33 * s));
    p.drawLine(QPointF(39 * s, 23 * s), QPointF(25 * s, 33 * s));
    return pix;
}

QIcon xdesk::ui::gui::Theme::icon(const QString& name, const QColor& color)
{
    const int size = 64;
    const QColor c = (color.isValid() ? color : palette().muted);
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    if (name == "overview") {
        p.drawRoundedRect(QRectF(10, 10, 18, 18), 4, 4);
        p.drawRoundedRect(QRectF(36, 10, 18, 18), 4, 4);
        p.drawRoundedRect(QRectF(10, 36, 18, 18), 4, 4);
        p.drawRoundedRect(QRectF(36, 36, 18, 18), 4, 4);
    } else if (name == "sessions" || name == "screen") {
        p.drawRoundedRect(QRectF(8, 12, 48, 32), 5, 5);
        p.drawLine(QPointF(24, 54), QPointF(40, 54));
        p.drawLine(QPointF(32, 44), QPointF(32, 54));
    } else if (name == "devices") {
        p.drawRoundedRect(QRectF(20, 6, 24, 52), 6, 6);
        p.drawLine(QPointF(29, 50), QPointF(35, 50));
    } else if (name == "logs") {
        for (int y: {16, 28, 40, 52})
            p.drawLine(QPointF(22, y - 2), QPointF(54, y - 2));
        for (int y: {16, 28, 40, 52})
            p.drawPoint(QPointF(11, y - 2));
    } else if (name == "settings") {
        p.drawEllipse(QPointF(32, 32), 8, 8);
        for (int i = 0; i < 8; ++i) {
            const double a = i * M_PI / 4.0;
            p.drawLine(QPointF(32 + 15 * std::cos(a), 32 + 15 * std::sin(a)), QPointF(32 + 23 * std::cos(a), 32 + 23 * std::sin(a)));
        }
        p.drawEllipse(QPointF(32, 32), 16, 16);
    } else if (name == "copy") {
        p.drawRoundedRect(QRectF(22, 22, 32, 32), 5, 5);
        p.drawPolyline(QPolygonF({QPointF(14, 42), QPointF(10, 42), QPointF(10, 10), QPointF(42, 10), QPointF(42, 14)}));
    } else if (name == "refresh") {
        p.drawArc(QRectF(12, 12, 40, 40), 30 * 16, 290 * 16);
        p.drawPolyline(QPolygonF({QPointF(46, 8), QPointF(50, 20), QPointF(38, 22)}));
    } else if (name == "shield") {
        QPainterPath path;
        path.moveTo(32, 6);
        path.lineTo(54, 14);
        path.cubicTo(54, 38, 44, 52, 32, 58);
        path.cubicTo(20, 52, 10, 38, 10, 14);
        path.closeSubpath();
        p.drawPath(path);
        p.drawPolyline(QPolygonF({QPointF(22, 32), QPointF(29, 39), QPointF(43, 25)}));
    } else if (name == "terminal") {
        p.drawRoundedRect(QRectF(6, 10, 52, 44), 6, 6);
        p.drawPolyline(QPolygonF({QPointF(16, 24), QPointF(24, 32), QPointF(16, 40)}));
        p.drawLine(QPointF(30, 42), QPointF(44, 42));
    } else if (name == "power") {
        p.drawArc(QRectF(12, 14, 40, 40), 120 * 16, 300 * 16);
        p.drawLine(QPointF(32, 8), QPointF(32, 32));
    } else if (name == "control") {
        QPainterPath arrow;
        arrow.moveTo(16, 8);
        arrow.lineTo(48, 36);
        arrow.lineTo(33, 37);
        arrow.lineTo(42, 54);
        arrow.lineTo(35, 57);
        arrow.lineTo(27, 40);
        arrow.lineTo(16, 50);
        arrow.closeSubpath();
        p.drawPath(arrow);
    } else if (name == "fullscreen") {
        p.drawPolyline(QPolygonF({QPointF(8, 22), QPointF(8, 8), QPointF(22, 8)}));
        p.drawPolyline(QPolygonF({QPointF(42, 8), QPointF(56, 8), QPointF(56, 22)}));
        p.drawPolyline(QPolygonF({QPointF(56, 42), QPointF(56, 56), QPointF(42, 56)}));
        p.drawPolyline(QPolygonF({QPointF(22, 56), QPointF(8, 56), QPointF(8, 42)}));
    } else if (name == "fit") {
        p.drawRoundedRect(QRectF(8, 14, 48, 36), 5, 5);
        p.drawPolyline(QPolygonF({QPointF(18, 28), QPointF(18, 22), QPointF(24, 22)}));
        p.drawPolyline(QPolygonF({QPointF(46, 36), QPointF(46, 42), QPointF(40, 42)}));
    } else if (name == "keyboard") {
        p.drawRoundedRect(QRectF(6, 16, 52, 32), 5, 5);
        for (int x = 14; x <= 50; x += 9)
            p.drawPoint(QPointF(x, 26));
        p.drawLine(QPointF(20, 38), QPointF(44, 38));
    } else if (name == "lock") {
        p.drawRoundedRect(QRectF(14, 28, 36, 28), 5, 5);
        p.drawArc(QRectF(20, 8, 24, 30), 0, 180 * 16);
        p.drawLine(QPointF(20, 23), QPointF(20, 28));
        p.drawLine(QPointF(44, 23), QPointF(44, 28));
    } else if (name == "scan") {
        p.drawArc(QRectF(8, 8, 48, 48), 45 * 16, 90 * 16);
        p.drawArc(QRectF(18, 18, 28, 28), 45 * 16, 90 * 16);
        p.setBrush(c);
        p.drawEllipse(QPointF(32, 40), 3, 3);
    } else {
        p.setBrush(c);
        p.drawEllipse(QPointF(32, 32), 8, 8);
    }
    return QIcon(pix);
}

QColor xdesk::ui::gui::Theme::terminalColor(const int index)
{
    static const char* base[16] = {
        "#1d2029", "#ff5d6c", "#3ecf8e", "#f5b14c", "#6d8bff", "#c678dd", "#4fc1e9", "#c8ccd8",
        "#5c6370", "#ff7a87", "#62e0a6", "#ffc97a", "#8fa7ff", "#d79bea", "#7fd6f2", "#ffffff"
    };
    static const char* lightBase[16] = {
        "#1f2430", "#c92a3f", "#12865a", "#a86a08", "#2f55e0", "#9a3fbf", "#0f86a8", "#9aa1b2",
        "#5b6273", "#e0405a", "#18a06c", "#c27c0e", "#4f6ff0", "#b259d6", "#1b9ec2", "#3a4050"
    };

    if (index < 16) return QColor(palette().dark ? base[index] : lightBase[index]);
    if (index < 232) {
        // 6x6x6 cube
        const int i = index - 16;
        const int levels[6] = {0, 95, 135, 175, 215, 255};
        return QColor(levels[(i / 36) % 6], levels[(i / 6) % 6], levels[i % 6]);
    }
    const int gray = 8 + (index - 232) * 10;
    return QColor(gray, gray, gray);
}
