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
##  @file Desktop.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#endif
#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xdesk/security/DeviceRegistry.hpp"
#include "xdesk/security/IntegrityGuard.hpp"
#include "xdesk/security/Certificate.hpp"
#include "xdesk/security/Crypto.hpp"
#include "xdesk/net/Exposure.hpp"
#include "xdesk/Desktop.hpp"
#include "xdesk/ui/Ui.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QLockFile>
#include <iostream>
#include <string>
#ifndef _WIN32
    #include <unistd.h>
#endif

// A real person typing on this machine (not a pipe, not a remote xdesk terminal)
static bool local_interactive(void)
{
    #ifdef _WIN32
        return _isatty(_fileno(stdin)) && !xdesk::security::IntegrityGuard::isInsideRemoteSession();
    #else
        return isatty(STDIN_FILENO) && !xdesk::security::IntegrityGuard::isInsideRemoteSession();
    #endif
}

void xdesk::Desktop::run(void)
{
    if (this->_command == "version") {std::cout << "xdesk " XDESK_VERSION " (" << security::tls_summary().toStdString() << ")" << std::endl; return;}
    if (this->_command == "paths") {
        std::cout << "config  " << this->_config.path().toStdString() << std::endl;
        std::cout << "data    " << core::Config::dataDir().toStdString() << std::endl;
        std::cout << "logs    " << core::Config::logDir().toStdString() << std::endl;
        std::cout << "binary  " << core::Config::executablePath().toStdString() << std::endl;
        return;
    }
    if (this->_command == "scan") {this->runScan_(); return;}
    if (this->_command == "devices") {this->runDevices_(); return;}
    if (this->_command == "unban") {this->runUnban_(); return;}
    if (this->_command == "connect" || this->_command == "viewer") {this->runConnect_(); return;}
    this->runHost_();
}

void xdesk::Desktop::runHost_(void)
{
    QLockFile lock(core::Config::dataDir() + "/host.lock");

    // A host started from a remote terminal of xdesk would escape the guard
    if (security::IntegrityGuard::isInsideRemoteSession())
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Tamper, "A host cannot be started from a remote xdesk session");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100))
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "xdesk is already sharing this PC (one host per user)");
    if (this->_port > 0) this->_config.settings().port = static_cast<std::uint16_t>(this->_port);
    if (!this->_expose.empty()) this->_config.settings().exposure = core::exposure_from(QString::fromStdString(this->_expose));
    if (this->_regenCert) {
        security::Certificate cert(core::Config::dataDir());
        cert.load(true);
        std::cout << "New host identity: " << security::fingerprint_text(cert.fingerprint()).toStdString() << std::endl;
    }
    if (this->_tty || (!this->_gui && !ui::has_display())) this->_exit = ui::run_host_tty(this->_argc, this->_argv, this->_config);
    else this->_exit = ui::run_host_gui(this->_argc, this->_argv, this->_config);
}

void xdesk::Desktop::runConnect_(void)
{
    client::Options options;
    const QString target = QString::fromStdString(this->_target);

    // host[:port] accepted, https://host:port too (copied from the host panel)
    options.host = target;
    if (options.host.startsWith("https://")) options.host = options.host.mid(8);
    if (options.host.endsWith('/')) options.host.chop(1);
    options.port = XDESK_DEFAULT_PORT;
    if (options.host.count(':') == 1) {
        options.port = static_cast<quint16>(options.host.section(':', 1).toUInt());
        options.host = options.host.section(':', 0, 0);
    }
    if (this->_port > 0) options.port = static_cast<quint16>(this->_port);
    options.code = QString::fromStdString(this->_code);
    options.viewOnly = this->_viewOnly;
    options.kind = this->_tty ? net::ClientKind::Tty : net::ClientKind::Gui;
    if (this->_tty || (!this->_gui && !ui::has_display())) {
        options.kind = net::ClientKind::Tty;
        this->_exit = ui::run_viewer_tty(this->_argc, this->_argv, options, this->_screen);
    } else this->_exit = ui::run_viewer_gui(this->_argc, this->_argv, options);
}

void xdesk::Desktop::runScan_(void)
{
    QCoreApplication app(this->_argc, this->_argv);
    const QList<net::Discovered> found = net::Exposure::scan(3000);

    if (found.isEmpty()) {std::cout << "No xdesk host announced on this LAN (3 s)." << std::endl; return;}
    for (const net::Discovered& d: found)
        std::cout << d.name.toStdString() << "\t" << d.host.toStdString() << ":" << d.port << "\tid " << d.fingerprint.toStdString() << "\txdesk connect " << d.host.toStdString() << std::endl;
}

void xdesk::Desktop::runDevices_(void)
{
    security::DeviceRegistry registry(core::Config::dataDir());

    registry.load();
    if (registry.wasTampered()) std::cout << "WARNING: devices.json was modified outside xdesk (trusted devices reset)" << std::endl;
    std::cout << "Devices:" << std::endl;
    for (const security::Device& d: registry.devices())
        std::cout << "  " << d.id.toStdString() << "  " << (d.banned ? "BANNED " : (d.trusted ? "trusted" : "known  ")) << "  " << d.name.toStdString()
            << " (" << d.kind.toStdString() << ", " << d.address.toStdString() << ")" << (d.banned ? "  reason: " + d.banReason.toStdString() : "") << std::endl;
    std::cout << "Banned addresses:" << std::endl;
    for (const security::AddressBan& a: registry.addresses())
        std::cout << "  " << a.address.toStdString() << "  " << a.reason.toStdString() << (a.until ? "  until " + QDateTime::fromMSecsSinceEpoch(a.until).toString().toStdString() : "") << std::endl;
}

void xdesk::Desktop::runUnban_(void)
{
    QLockFile lock(core::Config::dataDir() + "/host.lock");
    security::DeviceRegistry registry(core::Config::dataDir());
    const QString target = QString::fromStdString(this->_target);
    std::string typed;

    // Physical presence: a local terminal, not a remote xdesk shell, the host stopped
    if (!local_interactive())
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Tamper, "Unban needs a person typing on this PC (not a pipe, not a remote session)");
    lock.setStaleLockTime(0);
    if (!lock.tryLock(100))
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "The host is running: unban from its panel (Devices)");
    std::cout << "To unban " << target.toStdString() << ", type " << XDESK_UNBAN_WORD << ": " << std::flush;
    std::getline(std::cin, typed);
    if (QString::fromStdString(typed).trimmed().compare(XDESK_UNBAN_WORD, Qt::CaseInsensitive) != 0)
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Usage, "Wrong confirmation: nothing changed");
    registry.load();
    if (registry.find(target).has_value()) registry.unban(target);
    else registry.unbanAddress(target);
    std::cout << "Unbanned " << target.toStdString() << std::endl;
}
