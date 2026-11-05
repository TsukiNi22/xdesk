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
##  @file Ui.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/gui/ViewerWindow.hpp"
#include "xdesk/ui/gui/HostWindow.hpp"
#include "xdesk/ui/tty/ClientTui.hpp"
#include "xdesk/ui/tty/HostTui.hpp"
#include "xdesk/ui/gui/Theme.hpp"
#include "xdesk/host/Host.hpp"
#include "xdesk/ui/Ui.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QApplication>
#include <QMessageBox>
#include <iostream>
#include <memory>
#include <QTimer>

// XDESK_SNAPSHOT=<file.png>: save the window after 2 s and quit (screenshots of the docs, ui checks)
static void snapshot_if_asked(QWidget* window)
{
    const QString path = qEnvironmentVariable("XDESK_SNAPSHOT");

    if (path.isEmpty()) return;
    QTimer::singleShot(2000, window, [window, path]() {
        window->grab().save(path);
        QCoreApplication::quit();
    });
}

bool xdesk::ui::has_display(void)
{
    #if defined(_WIN32) || defined(__APPLE__)
        return true;
    #else
        return !qEnvironmentVariableIsEmpty("DISPLAY") || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    #endif
}

int xdesk::ui::run_host_gui(int& argc, char** argv, core::Config& config)
{
    QApplication app(argc, argv);
    std::unique_ptr<host::Host> host;
    std::unique_ptr<gui::HostWindow> window;
    int code = 0;

    QApplication::setApplicationName("xdesk");
    QApplication::setApplicationVersion(XDESK_VERSION);
    QApplication::setQuitOnLastWindowClosed(false);
    gui::Theme::apply(app, config.settings().theme);
    host = std::make_unique<host::Host>(config);
    try {
        host->start();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "xdesk", QString("Sharing could not start:\n") + e.what());
        return 1;
    }
    window = std::make_unique<gui::HostWindow>(*host);
    if (!config.settings().startMinimized) window->show();
    snapshot_if_asked(window.get());
    code = app.exec();
    host->stop();
    window.reset();
    return code;
}

int xdesk::ui::run_host_tty(int& argc, char** argv, core::Config& config)
{
    std::unique_ptr<QCoreApplication> app(has_display() ? new QGuiApplication(argc, argv) : new QCoreApplication(argc, argv)); // capture needs a gui application
    std::unique_ptr<host::Host> host = std::make_unique<host::Host>(config);
    std::unique_ptr<tty::HostTui> panel;
    int code = 0;

    if (!tty::Terminal::isTty()) {std::cerr << "xdesk: the terminal panel needs a terminal (use the graphical panel or run it in a terminal)" << std::endl; return 1;}
    try {
        host->start();
    } catch (const std::exception& e) {
        std::cerr << "xdesk: sharing could not start: " << e.what() << std::endl;
        return 1;
    }
    panel = std::make_unique<tty::HostTui>(*host);
    QObject::connect(panel.get(), &tty::HostTui::quit, app.get(), &QCoreApplication::quit);
    code = app->exec();
    host->stop();
    panel.reset();
    return code;
}

int xdesk::ui::run_viewer_gui(int& argc, char** argv, const client::Options& options)
{
    QApplication app(argc, argv);
    gui::ViewerWindow window;
    core::Config config;

    QApplication::setApplicationName("xdesk");
    config.load();
    gui::Theme::apply(app, config.settings().theme);
    window.show();
    if (!options.host.isEmpty()) window.connectNow(options);
    snapshot_if_asked(&window);
    return app.exec();
}

int xdesk::ui::run_viewer_tty(int& argc, char** argv, const client::Options& options, const bool screen)
{
    QCoreApplication app(argc, argv);
    tty::ClientTui client;
    client::Options o = options;

    if (!tty::Terminal::isTty()) {std::cerr << "xdesk: the terminal client needs a terminal" << std::endl; return 1;}
    o.kind = net::ClientKind::Tty;
    QObject::connect(&client, &tty::ClientTui::quit, &app, &QCoreApplication::quit, Qt::QueuedConnection);
    client.start(o, screen);
    app.exec();
    return client.exitCode();
}
