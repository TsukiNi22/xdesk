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
##  @file Ui.hpp

File Description:
##  Entry points of the interfaces: host panel and viewer, graphical (Qt Widgets) or terminal (TUI)
\**************************************************************/

#ifndef UI_H
    #define UI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../client/Client.hpp"     // xdesk::client::Options
    #include "../core/Config.hpp"       // xdesk::core::Config

namespace xdesk::ui { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

/*
 * Each entry point creates its own application object (QApplication for the gui, QGuiApplication for a tty host on a
 * desktop session: the screen capture needs it, QCoreApplication without display), runs the event loop and returns
 * the exit code
*/
_cold _nodiscard bool has_display(void); // a graphical session is available (Windows / macOS: always)
_cold _nodiscard int run_host_gui(int& argc, char** argv, core::Config& config); // panel + tray icon
_cold _nodiscard int run_host_tty(int& argc, char** argv, core::Config& config); // full screen TUI panel
_cold _nodiscard int run_viewer_gui(int& argc, char** argv, const client::Options& options); // empty host: connect dialog
_cold _nodiscard int run_viewer_tty(int& argc, char** argv, const client::Options& options, const bool screen); // terminal (screen: half block view)

} // namespace end
#endif /* UI_H */
