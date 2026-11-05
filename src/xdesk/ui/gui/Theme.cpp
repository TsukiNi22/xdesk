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
#include <QStyledItemDelegate>
#include <QTemporaryDir>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QPainterPath>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>
#include <QStyleHints>
#include <functional>
#include <QPainter>
#include <QPalette>
#include <QFile>
#include <QFont>
#include <cmath>

static xdesk::ui::gui::Palette current;
static QString currentSetting = "system";

/*
 * xdesk look: "graphite + signal green"
 *  - flat surfaces separated by hairlines, square corners (3 px at most)
 *  - one signal colour for what is live (the code, the selection, the primary action)
 *  - small capitals for the section labels, monospace for everything a person copies (code, addresses, ids)
*/
static xdesk::ui::gui::Palette dark_palette(void)
{
    xdesk::ui::gui::Palette p;
    p.dark = true;
    p.window = QColor("#101315");
    p.sidebar = QColor("#0b0d0f");
    p.surface = QColor("#161a1d");
    p.surfaceAlt = QColor("#1d2226");
    p.border = QColor("#2a3035");
    p.text = QColor("#e4e7e9");
    p.muted = QColor("#8d969d");
    p.accent = QColor("#3ddc97");
    p.accentText = QColor("#07130d");
    p.success = QColor("#3ddc97");
    p.warning = QColor("#f0a742");
    p.danger = QColor("#f2625f");
    p.terminalBg = QColor("#0b0d0f");
    p.terminalFg = QColor("#d5dadd");
    return p;
}

static xdesk::ui::gui::Palette light_palette(void)
{
    xdesk::ui::gui::Palette p;
    p.dark = false;
    p.window = QColor("#f2f0eb");
    p.sidebar = QColor("#e8e5de");
    p.surface = QColor("#fbfaf7");
    p.surfaceAlt = QColor("#ebe8e1");
    p.border = QColor("#d3cec4");
    p.text = QColor("#1b1e20");
    p.muted = QColor("#62676b");
    p.accent = QColor("#0a7c55");
    p.accentText = QColor("#ffffff");
    p.success = QColor("#0f7a4f");
    p.warning = QColor("#9a5d00");
    p.danger = QColor("#c2342f");
    p.terminalBg = QColor("#fbfaf7");
    p.terminalFg = QColor("#1b1e20");
    return p;
}

// Rows of the tables / trees: no focus frame on the clicked cell, one accent bar at the start of the selected row
class RowDelegate final: public QStyledItemDelegate {
    public:
        // ------------ Function ---------- //
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyleOptionViewItem item = option;

            item.state &= ~QStyle::State_HasFocus;
            QStyledItemDelegate::paint(painter, item, index);
            if ((option.state & QStyle::State_Selected) && index.column() == 0)
                painter->fillRect(QRect(option.rect.left(), option.rect.top(), 3, option.rect.height()), xdesk::ui::gui::Theme::palette().accent);
        };

        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {return QStyledItemDelegate::sizeHint(option, index) + QSize(24, 0);}; // + the 10 px padding of each side (style sheet)

        // ---------- Constructor --------- //
        explicit RowDelegate(QObject* parent): QStyledItemDelegate(parent) { /* Nothing */ };
};

// Glyphs of the style sheet (check mark, chevrons): drawn once per colour in a private temporary folder (the style sheets only take files)
static QString glyph_image(const QString& glyph, const QColor& color)
{
    static QTemporaryDir dir;
    const QString path = dir.path() + "/" + glyph + "-" + color.name().mid(1) + ".png";

    if (QFile::exists(path)) return path;
    for (const int scale: {1, 2}) {
        QPixmap pix(16 * scale, 16 * scale);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(scale, scale);
        if (glyph == "check") {
            p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
            p.drawPolyline(QPolygonF({QPointF(3.5, 8.5), QPointF(6.5, 11.5), QPointF(12.5, 4.5)}));
        } else {
            const double tip = glyph == "down" ? 10.5 : 5.5;
            const double base = glyph == "down" ? 6.0 : 10.0;
            p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(QPolygonF({QPointF(4.0, base), QPointF(8.0, tip), QPointF(12.0, base)}));
        }
        p.end();
        (void)pix.save(scale == 1 ? path : QString(path).replace(".png", "@2x.png"), "PNG");
    }
    return path;
}

static QString style_sheet(const xdesk::ui::gui::Palette& p)
{
    QString qss = R"(
* { color: @text; outline: 0; }
QMainWindow, QDialog, QStackedWidget > QWidget, QScrollArea, QScrollArea > QWidget > QWidget { background: @window; }
QToolTip { background: @surface; color: @text; border: 1px solid @border; padding: 5px 8px; }

#sidebar { background: @sidebar; border-right: 1px solid @border; }
#sidebar QListWidget { background: transparent; border: none; }
#sidebar QListWidget::item { padding: 9px 12px; margin: 0; border: none; border-left: 3px solid transparent; color: @muted; }
#sidebar QListWidget::item:hover { background: @surfaceAlt; color: @text; }
#sidebar QListWidget::item:selected { background: @accentSoft; color: @text; border-left: 3px solid @accent; }
#brand { font-size: 16px; font-weight: 800; letter-spacing: 1px; }
#version { color: @muted; font-family: "@mono"; font-size: 11px; }
#rule { background: @border; max-height: 1px; min-height: 1px; }

#card { background: @surface; border: 1px solid @border; border-radius: 4px; }
#card[accent="true"] { border-top: 2px solid @accent; }
#cardHead { background: @surfaceHead; border: none; border-bottom: 1px solid @border; border-top-left-radius: 3px; border-top-right-radius: 3px; min-height: 40px; }
#cardHead #subtitle { color: @text; }
#card QTableWidget, #card QTreeWidget, #card QPlainTextEdit { border: none; border-radius: 0; }
#card QTabWidget::pane { border: none; border-top: 1px solid @border; }
#pageHead #title { padding: 0; }
#title { font-size: 22px; font-weight: 700; }
#subtitle { font-size: 11px; font-weight: 700; letter-spacing: 1px; color: @muted; }
#muted, QLabel[muted="true"] { color: @muted; }
#mono { font-family: "@mono"; }
#code { font-family: "@mono"; font-size: 44px; font-weight: 700; letter-spacing: 8px; color: @accent; background: @surfaceAlt; border-left: 3px solid @accent; padding: 4px 14px; }
#banner { background: @danger; border-radius: 3px; }
#banner QLabel { color: white; font-weight: 600; }
#badge { border-radius: 2px; padding: 1px 6px; background: @surfaceAlt; color: @muted; font-family: "@mono"; font-size: 11px; }
#pill { font-family: "@mono"; font-size: 11px; font-weight: 700; letter-spacing: 1px; padding: 4px 10px; border-radius: 2px; }
#pill[on="true"] { color: @success; background: @successSoft; border: 1px solid @success; }
#pill[on="false"] { color: @danger; background: @dangerSoft; border: 1px solid @danger; }
#code[off="true"] { color: @muted; border-left-color: @border; }
#endpoint { background: transparent; border: none; border-bottom: 1px solid @border; }
#endpoint[last="true"] { border-bottom: none; }
#endpoint:hover { background: @accentSoft; }
#tag { font-family: "@mono"; font-size: 10px; font-weight: 700; letter-spacing: 1px; padding: 3px 0; border: 1px solid @border; border-radius: 2px; color: @muted; background: @surfaceAlt; }
#tag[kind="tailscale"] { color: @accent; border-color: @accent; background: @accentSoft; }
#tag[kind="internet"] { color: @warning; border-color: @warning; }
#tag[kind="lan"] { color: @text; }
#address { font-family: "@mono"; font-size: 13px; font-weight: 600; }
#qr { background: white; border: 1px solid @border; border-radius: 4px; padding: 10px; }
QLabel[tone="success"] { color: @success; }
QLabel[tone="warning"] { color: @warning; }
QLabel[tone="danger"] { color: @danger; }
QLabel[tone="accent"] { color: @accent; }
QLabel[tone="muted"] { color: @muted; }

QPushButton, QToolButton { background: @surfaceAlt; border: 1px solid @border; border-radius: 3px; padding: 6px 14px; font-weight: 600; }
QPushButton:hover, QToolButton:hover { border-color: @accent; }
QPushButton:pressed, QToolButton:pressed { background: @border; }
QPushButton:disabled, QToolButton:disabled { color: @muted; background: @surface; }
QToolButton:checked { background: @accentSoft; border-color: @accent; color: @text; }
QPushButton[kind="primary"] { background: @accent; color: @accentText; border: 1px solid @accent; }
QPushButton[kind="primary"]:hover { background: @accentHover; }
QPushButton[kind="primary"]:disabled { background: @border; border-color: @border; color: @muted; }
QPushButton[kind="danger"] { background: transparent; color: @danger; border: 1px solid @danger; }
QPushButton[kind="danger"]:hover { background: @danger; color: white; }
QPushButton[kind="ghost"] { background: transparent; border: 1px solid transparent; color: @muted; padding: 6px 8px; }
QPushButton[kind="ghost"]:hover { color: @text; border-color: @border; }
QPushButton#share { padding: 11px 14px; font-weight: 700; font-size: 13px; text-align: left; border-radius: 3px; }
QPushButton#share[on="false"] { background: @success; color: @accentText; border: 1px solid @success; }
QPushButton#share[on="false"]:hover { background: @accentHover; }
QPushButton#share[on="true"] { background: @danger; color: white; border: 1px solid @danger; }
QPushButton#share[on="true"]:hover { background: @dangerHover; }

#segmented { background: @surfaceAlt; border: 1px solid @border; border-radius: 3px; }
#segmented QToolButton { background: transparent; border: none; border-radius: 0; padding: 5px 0; color: @muted; font-weight: 600; font-size: 11px; }
#segmented QToolButton:hover { color: @text; }
#segmented QToolButton:checked { background: @accent; color: @accentText; }

QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
    background: @surfaceAlt; border: 1px solid @border; border-radius: 3px; padding: 5px 8px;
    selection-background-color: @accent; selection-color: @accentText;
}
QLineEdit#codeInput { font-family: "@mono"; font-size: 16px; letter-spacing: 2px; padding: 7px 10px; }
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover { border-color: @borderHover; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus { border-color: @accent; }
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled { color: @muted; background: @surface; }

QComboBox { padding-right: 30px; }
QComboBox::drop-down { subcontrol-origin: border; subcontrol-position: center right; width: 26px; border: none; border-left: 1px solid @border; }
QComboBox::down-arrow { image: url(@chevronDown); width: 14px; height: 14px; }
QComboBox::down-arrow:on { image: url(@chevronUp); }
QComboBox::down-arrow:disabled { image: url(@chevronOff); }
QComboBox QAbstractItemView { background: @surface; border: 1px solid @border; padding: 4px; outline: 0; selection-background-color: @accentSoft; selection-color: @text; }
QComboBox QAbstractItemView::item { min-height: 28px; padding: 0 8px; }

QSpinBox, QDoubleSpinBox { padding-right: 26px; }
QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 22px; border: none; border-left: 1px solid @border; border-bottom: 1px solid @border; border-top-right-radius: 3px; }
QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 22px; border: none; border-left: 1px solid @border; border-bottom-right-radius: 3px; }
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover { background: @accentSoft; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(@chevronUp); width: 11px; height: 11px; }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(@chevronDown); width: 11px; height: 11px; }
QSpinBox::up-arrow:disabled, QSpinBox::up-arrow:off, QDoubleSpinBox::up-arrow:disabled, QDoubleSpinBox::up-arrow:off { image: url(@chevronUpOff); }
QSpinBox::down-arrow:disabled, QSpinBox::down-arrow:off, QDoubleSpinBox::down-arrow:disabled, QDoubleSpinBox::down-arrow:off { image: url(@chevronOff); }

QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 2px; border: 1px solid @muted; background: @surfaceAlt; }
QCheckBox::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; image: url(@checkOn); }
QCheckBox::indicator:checked:disabled { background: @border; border-color: @border; image: url(@checkOff); }
QCheckBox::indicator:disabled { background: @surface; border-color: @border; }
QCheckBox:disabled { color: @muted; }

QSlider::groove:horizontal { height: 3px; background: @border; }
QSlider::sub-page:horizontal { background: @accent; }
QSlider::handle:horizontal { background: @accent; width: 10px; height: 14px; margin: -6px 0; border-radius: 1px; }
QProgressBar { background: @surfaceAlt; border: none; max-height: 3px; }
QProgressBar::chunk { background: @accent; }

QTableWidget, QTreeWidget, QListWidget {
    background: @surface; border: 1px solid @border; border-radius: 0; gridline-color: transparent;
    alternate-background-color: @surfaceAlt; selection-background-color: @accentSoft; selection-color: @text;
}
QTableWidget::item, QTreeWidget::item { padding: 6px 10px; border: none; }
QTableWidget::item:hover, QTreeWidget::item:hover { background: @rowHover; }
QTableWidget::item:selected, QTreeWidget::item:selected { background: @accentSoft; color: @text; }
QHeaderView::section { background: @surfaceHead; color: @muted; border: none; border-bottom: 1px solid @border; padding: 8px 10px; font-weight: 700; font-size: 11px; }
QTableCornerButton::section { background: @surface; border: none; }

QTabWidget::pane { border: 1px solid @border; top: -1px; background: @surface; }
QTabBar::tab { background: transparent; color: @muted; padding: 7px 16px; border: none; border-bottom: 2px solid transparent; font-weight: 600; }
QTabBar::tab:selected { color: @text; border-bottom: 2px solid @accent; }
QTabBar::tab:hover { color: @text; }

QToolBar { background: @surface; border: none; border-bottom: 1px solid @border; padding: 3px; spacing: 4px; }
QToolBar QToolButton { background: transparent; border: 1px solid transparent; padding: 5px 9px; }
QToolBar QToolButton:hover { border-color: @border; background: @surfaceAlt; }
QStatusBar { background: @surface; color: @muted; border-top: 1px solid @border; }
QMenu { background: @surface; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 6px 18px; border-left: 2px solid transparent; }
QMenu::item:selected { background: @accentSoft; border-left: 2px solid @accent; }
QMenu::separator { height: 1px; background: @border; margin: 4px 6px; }

QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: @border; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @muted; }
QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
QScrollBar::handle:horizontal { background: @border; min-width: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSplitter::handle { background: @border; }
QSplitter#gap::handle { background: transparent; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical { height: 1px; }
)";
    const std::function<QString(const QColor&, const double)> rgba = [](const QColor& c, const double alpha) {
        return QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha, 0, 'f', 2);
    };

    qss.replace("@accentSoft", rgba(p.accent, p.dark ? 0.14 : 0.12));
    qss.replace("@successSoft", rgba(p.success, p.dark ? 0.12 : 0.10));
    qss.replace("@dangerSoft", rgba(p.danger, p.dark ? 0.12 : 0.10));
    qss.replace("@dangerHover", (p.dark ? p.danger.lighter(110) : p.danger.darker(112)).name());
    qss.replace("@surfaceHead", (p.dark ? p.surface.lighter(112) : p.surface.darker(102)).name());
    qss.replace("@checkOn", glyph_image("check", p.accentText));
    qss.replace("@checkOff", glyph_image("check", p.muted));
    qss.replace("@chevronDown", glyph_image("down", p.text));
    qss.replace("@chevronUpOff", glyph_image("up", p.border));
    qss.replace("@chevronUp", glyph_image("up", p.text));
    qss.replace("@chevronOff", glyph_image("down", p.border));
    qss.replace("@borderHover", (p.dark ? p.border.lighter(140) : p.border.darker(118)).name());
    qss.replace("@rowHover", rgba(p.text, p.dark ? 0.04 : 0.035));
    qss.replace("@accentHover", (p.dark ? p.accent.lighter(112) : p.accent.darker(115)).name());
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
    static bool following = false;
    bool dark = true;

    currentSetting = (setting == "light" || setting == "dark") ? setting : QString("system");
    if (currentSetting == "light") dark = false;
    else if (currentSetting == "dark") dark = true;
    else dark = (QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light);
    current = (dark ? dark_palette() : light_palette());

    // "system": follow the desktop when it switches between light and dark
    if (!following) {
        following = true;
        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app, [&app](Qt::ColorScheme) {
            if (currentSetting == "system") Theme::apply(app, "system");
        });
    }

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

    // The windows draw their icons again (they hold pixmaps in the old colours)
    for (QWidget* window: QApplication::topLevelWidgets())
        if (window->metaObject()->indexOfMethod("retheme_()") >= 0) QMetaObject::invokeMethod(window, "retheme_", Qt::QueuedConnection);
}

const xdesk::ui::gui::Palette& xdesk::ui::gui::Theme::palette(void)
{
    if (!current.window.isValid()) current = dark_palette();
    return current;
}

const QString& xdesk::ui::gui::Theme::setting(void)
{
    return currentSetting;
}

QWidget* xdesk::ui::gui::Theme::switcher(const std::function<void(const QString&)>& saved, QWidget* parent)
{
    QWidget* box = new QWidget(parent);
    QHBoxLayout* layout = new QHBoxLayout(box);
    QButtonGroup* group = new QButtonGroup(box);

    box->setObjectName("segmented");
    box->setAttribute(Qt::WA_StyledBackground, true);
    box->setToolTip("Theme of xdesk: follow the system, or always light / dark");
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    for (const QString& name: {QString("system"), QString("light"), QString("dark")}) {
        QToolButton* button = new QToolButton(box);
        button->setText(name == "system" ? "Auto" : (name == "light" ? "Light" : "Dark"));
        button->setCheckable(true);
        button->setChecked(name == currentSetting);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        group->addButton(button);
        layout->addWidget(button);
        QObject::connect(button, &QToolButton::clicked, box, [name, saved]() {
            QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
            if (!app || name == currentSetting) return;
            Theme::apply(*app, name);
            if (saved) saved(name);
        });
    }
    group->setExclusive(true);
    return box;
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

    // Graphite tile, signal green screen and X (same drawing as docs/logo.svg)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#14181b"));
    p.drawRoundedRect(QRectF(2 * s, 2 * s, 60 * s, 60 * s), 6 * s, 6 * s);
    p.setBrush(QColor("#3ddc97"));
    p.drawRect(QRectF(2 * s, 54 * s, 60 * s, 4 * s));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#3ddc97"), 4 * s, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.drawRect(QRectF(12 * s, 13 * s, 40 * s, 28 * s));
    p.drawLine(QPointF(24 * s, 47 * s), QPointF(40 * s, 47 * s));
    p.setPen(QPen(QColor("#e4e7e9"), 4 * s, Qt::SolidLine, Qt::SquareCap));
    p.drawLine(QPointF(26 * s, 21 * s), QPointF(38 * s, 33 * s));
    p.drawLine(QPointF(38 * s, 21 * s), QPointF(26 * s, 33 * s));
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
    p.setPen(QPen(c, 5, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    p.setBrush(Qt::NoBrush);

    if (name == "overview") {
        p.drawRect(QRectF(10, 10, 18, 18));
        p.drawRect(QRectF(36, 10, 18, 18));
        p.drawRect(QRectF(10, 36, 18, 18));
        p.drawRect(QRectF(36, 36, 18, 18));
    } else if (name == "sessions" || name == "screen") {
        p.drawRect(QRectF(8, 12, 48, 32));
        p.drawLine(QPointF(24, 54), QPointF(40, 54));
        p.drawLine(QPointF(32, 44), QPointF(32, 54));
    } else if (name == "devices") {
        p.drawRect(QRectF(20, 6, 24, 52));
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
        p.drawRect(QRectF(22, 22, 32, 32));
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
        p.drawRect(QRectF(6, 10, 52, 44));
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
        p.drawRect(QRectF(8, 14, 48, 36));
        p.drawPolyline(QPolygonF({QPointF(18, 28), QPointF(18, 22), QPointF(24, 22)}));
        p.drawPolyline(QPolygonF({QPointF(46, 36), QPointF(46, 42), QPointF(40, 42)}));
    } else if (name == "keyboard") {
        p.drawRect(QRectF(6, 16, 52, 32));
        for (int x = 14; x <= 50; x += 9)
            p.drawPoint(QPointF(x, 26));
        p.drawLine(QPointF(20, 38), QPointF(44, 38));
    } else if (name == "lock") {
        p.drawRect(QRectF(14, 28, 36, 28));
        p.drawArc(QRectF(20, 8, 24, 30), 0, 180 * 16);
        p.drawLine(QPointF(20, 23), QPointF(20, 28));
        p.drawLine(QPointF(44, 23), QPointF(44, 28));
    } else if (name == "qr") {
        // Three finder squares and a few modules
        for (const QPointF& corner: {QPointF(8, 8), QPointF(38, 8), QPointF(8, 38)})
            p.drawRect(QRectF(corner, QSizeF(18, 18)));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        for (const QRectF& module: {QRectF(13, 13, 8, 8), QRectF(43, 13, 8, 8), QRectF(13, 43, 8, 8), QRectF(38, 38, 7, 7), QRectF(49, 49, 7, 7), QRectF(49, 38, 7, 7)})
            p.drawRect(module);
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

void xdesk::ui::gui::Theme::decorate(QAbstractItemView* view)
{
    view->setItemDelegate(new RowDelegate(view));
    view->setFocusPolicy(Qt::StrongFocus);
    view->setMouseTracking(true); // hover of the rows
}
