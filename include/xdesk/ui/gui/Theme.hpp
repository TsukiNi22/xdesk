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
##  @file Theme.hpp

File Description:
##  Look of the Qt interface: light / dark tokens, style sheet, painted icons, terminal palette
\**************************************************************/

#ifndef THEME_H
    #define THEME_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <QApplication>     // QApplication
    #include <QPixmap>          // QPixmap
    #include <QString>          // QString
    #include <QColor>           // QColor
    #include <QIcon>            // QIcon

namespace xdesk::ui::gui { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Palette {
    bool dark = true;
    QColor window; // background of the window
    QColor sidebar; // navigation
    QColor surface; // cards
    QColor surfaceAlt; // inputs, table rows
    QColor border;
    QColor text;
    QColor muted; // secondary text
    QColor accent; // primary actions, selection
    QColor accentText; // text on the accent
    QColor success;
    QColor warning;
    QColor danger;
    QColor terminalBg;
    QColor terminalFg;
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Object names / properties used by the style sheet:
 *  #sidebar, #card, #title, #subtitle, #muted, #code, #banner, #badge
 *  QPushButton[kind="primary" | "danger" | "ghost"], QLabel[tone="success" | "warning" | "danger" | "accent"]
*/
class Theme {
    public:
        // ---------- Pre-Function -------- //
        _cold static void apply(QApplication& app, const QString& setting); // system | dark | light
        _cold _nodiscard static const Palette& palette(void);
        _cold _nodiscard static QIcon icon(const QString& name, const QColor& color = QColor()); // overview, sessions, devices, logs, settings, copy, refresh, shield, terminal, power, screen, control, fullscreen, fit, keyboard
        _cold _nodiscard static QPixmap logo(const int size); // xdesk mark
        _cold _nodiscard static QColor terminalColor(const int index); // xterm 256 colors (0 - 15 from the theme)
        _cold _nodiscard static QString monospaceFamily(void);

        // ---------- Constructor --------- //
        Theme() = delete;
};

} // namespace end
#endif /* THEME_H */
