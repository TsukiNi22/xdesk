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
#include <QJsonDocument>
#include <QApplication>
#include <QMessageBox>
#include <QJsonObject>
#include <iostream>
#include <QDialog>
#include <memory>
#include <QTimer>
#include <QFile>

// XDESK_SNAPSHOT=<file.png>: save the window after 2 s and quit (screenshots of the docs, ui checks)
static void snapshot_if_asked(QWidget* window)
{
    const QString path = qEnvironmentVariable("XDESK_SNAPSHOT");

    if (path.isEmpty()) return;
    if (qEnvironmentVariableIsSet("XDESK_SNAPSHOT_PAGE")) QMetaObject::invokeMethod(window, "showPage_", Q_ARG(int, qEnvironmentVariableIntValue("XDESK_SNAPSHOT_PAGE")));
    if (qEnvironmentVariableIsSet("XDESK_SNAPSHOT_QR")) QMetaObject::invokeMethod(window, "showQr_", Q_ARG(int, -1));
    QTimer::singleShot(2000, window, [window, path]() {
        QWidget* shown = window;
        for (QWidget* top: QApplication::topLevelWidgets())
            if (top != window && top->isVisible() && qobject_cast<QDialog*>(top)) shown = top; // a dialog opened for the snapshot
        shown->grab().save(path);
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
        host->start(false); // sharing off until the person in front of the PC starts it
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "xdesk", QString("xdesk could not start:\n") + e.what());
        return 1;
    }
    if (qEnvironmentVariableIntValue("XDESK_SNAPSHOT_SHARE") == 1) host->setSharing(true); // snapshots of the sharing state
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
        host->start(false); // sharing off until s is pressed
    } catch (const std::exception& e) {
        std::cerr << "xdesk: could not start: " << e.what() << std::endl;
        return 1;
    }
    panel = std::make_unique<tty::HostTui>(*host);
    QObject::connect(panel.get(), &tty::HostTui::quit, app.get(), &QCoreApplication::quit);
    code = app->exec();
    host->stop();
    panel.reset();
    return code;
}

// Theme chosen in the viewer (viewer.json), else the one of the settings
static QString viewer_theme(const QString& fallback)
{
    QFile file(xdesk::core::Config::configDir() + "/viewer.json");

    if (!file.open(QIODevice::ReadOnly)) return fallback;
    return QJsonDocument::fromJson(file.readAll()).object().value("theme").toString(fallback);
}

int xdesk::ui::run_viewer_gui(int& argc, char** argv, const client::Options& options)
{
    QApplication app(argc, argv);
    std::unique_ptr<gui::ViewerWindow> window;
    core::Config config;

    QApplication::setApplicationName("xdesk");
    config.load();
    gui::Theme::apply(app, viewer_theme(config.settings().theme)); // before the window: its icons take the colours of the theme
    window = std::make_unique<gui::ViewerWindow>();
    window->show();
    if (!options.host.isEmpty()) window->connectNow(options);
    snapshot_if_asked(window.get());
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
