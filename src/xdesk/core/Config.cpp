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
##  @file Config.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #include <windows.h>
#endif
#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xdesk/core/Config.hpp"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFileInfo>
#include <algorithm>
#include <QString>
#include <QFile>
#include <QDir>

QString xdesk::core::exposure_name(const Exposure exposure)
{
    switch (exposure) {
        case Exposure::Lan:         return "lan";
        case Exposure::Tailscale:   return "tailscale";
        case Exposure::Upnp:        return "upnp";
        case Exposure::Bore:        return "bore";
        case Exposure::Cloudflared: return "cloudflared";
        case Exposure::Ngrok:       return "ngrok";
        case Exposure::Manual:      return "manual";
    }
    return "lan";
}

xdesk::core::Exposure xdesk::core::exposure_from(const QString& name)
{
    const QString n = name.toLower();

    if (n == "tailscale")   return Exposure::Tailscale;
    if (n == "upnp")        return Exposure::Upnp;
    if (n == "bore")        return Exposure::Bore;
    if (n == "cloudflared") return Exposure::Cloudflared;
    if (n == "ngrok")       return Exposure::Ngrok;
    if (n == "manual")      return Exposure::Manual;
    return Exposure::Lan;
}

QString xdesk::core::approval_name(const Approval approval)
{
    switch (approval) {
        case Approval::Untrusted: return "untrusted";
        case Approval::Always:    return "always";
        case Approval::Never:     return "never";
    }
    return "untrusted";
}

xdesk::core::Approval xdesk::core::approval_from(const QString& name)
{
    if (name == "always") return Approval::Always;
    if (name == "never")  return Approval::Never;
    return Approval::Untrusted;
}

// Folder created with owner only rights
static QString private_dir(const QString& path)
{
    QDir dir;

    if (!dir.mkpath(path)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Io, "Cannot create " + path.toStdString());
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return path;
}

QString xdesk::core::Config::configDir(void)
{
    return private_dir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/xdesk");
}

QString xdesk::core::Config::dataDir(void)
{
    return private_dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/xdesk");
}

QString xdesk::core::Config::logDir(void)
{
    return private_dir(dataDir() + "/logs");
}

QString xdesk::core::Config::executablePath(void)
{
    if (QCoreApplication::instance()) return QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    #ifdef _WIN32
        wchar_t buffer[MAX_PATH] = {0};
        const DWORD size = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return QString::fromWCharArray(buffer, static_cast<qsizetype>(size));
    #else
        return QFileInfo("/proc/self/exe").canonicalFilePath();
    #endif
}

xdesk::core::Config::Config(void)
: _path{configDir() + "/config.json"}
{
    /* Nothing */
}

void xdesk::core::Config::load(void)
{
    QFile file(this->_path);
    QJsonParseError error;
    QJsonObject o;
    Settings& s = this->_settings;

    if (!file.exists()) {this->save(); return;}
    if (!file.open(QIODevice::ReadOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, "Cannot read " + this->_path.toStdString());
    o = QJsonDocument::fromJson(file.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, this->_path.toStdString() + ": " + error.errorString().toStdString());

    // Network
    s.port = static_cast<std::uint16_t>(std::clamp(o.value("port").toInt(s.port), 1, 65535));
    s.bind = o.value("bind").toString(s.bind);
    s.exposure = exposure_from(o.value("exposure").toString(exposure_name(s.exposure)));
    s.publicHost = o.value("publicHost").toString(s.publicHost);
    s.discovery = o.value("discovery").toBool(s.discovery);

    // Access
    s.codeLength = std::clamp(o.value("codeLength").toInt(s.codeLength), 6, 24);
    s.codeLifetime = std::max(0, o.value("codeLifetime").toInt(s.codeLifetime));
    s.approval = approval_from(o.value("approval").toString(approval_name(s.approval)));
    s.maxSessions = std::clamp(o.value("maxSessions").toInt(s.maxSessions), 1, 64);
    s.maxAuthFailures = std::clamp(o.value("maxAuthFailures").toInt(s.maxAuthFailures), 1, 100);
    s.idleTimeout = std::max(0, o.value("idleTimeout").toInt(s.idleTimeout));
    s.allowControl = o.value("allowControl").toBool(s.allowControl);
    s.allowTerminal = o.value("allowTerminal").toBool(s.allowTerminal);
    s.allowWeb = o.value("allowWeb").toBool(s.allowWeb);
    s.viewOnlyDefault = o.value("viewOnlyDefault").toBool(s.viewOnlyDefault);
    s.banOnTamper = o.value("banOnTamper").toBool(s.banOnTamper);
    s.shell = o.value("shell").toString(s.shell);

    // Stream
    s.fps = std::clamp(o.value("fps").toInt(s.fps), 1, 120);
    s.quality = std::clamp(o.value("quality").toInt(s.quality), 1, 100);
    s.scale = std::clamp(o.value("scale").toDouble(s.scale), 0.25, 1.0);
    s.maxHeight = std::clamp(o.value("maxHeight").toInt(s.maxHeight), 0, 4320);

    // Ui
    s.theme = o.value("theme").toString(s.theme);
    s.language = o.value("language").toString(s.language);
    s.startMinimized = o.value("startMinimized").toBool(s.startMinimized);
}

void xdesk::core::Config::save(void) const
{
    const Settings& s = this->_settings;
    QJsonObject o;
    QSaveFile file(this->_path);

    o["port"] = s.port;
    o["bind"] = s.bind;
    o["exposure"] = exposure_name(s.exposure);
    o["publicHost"] = s.publicHost;
    o["discovery"] = s.discovery;
    o["codeLength"] = s.codeLength;
    o["codeLifetime"] = s.codeLifetime;
    o["approval"] = approval_name(s.approval);
    o["maxSessions"] = s.maxSessions;
    o["maxAuthFailures"] = s.maxAuthFailures;
    o["idleTimeout"] = s.idleTimeout;
    o["allowControl"] = s.allowControl;
    o["allowTerminal"] = s.allowTerminal;
    o["allowWeb"] = s.allowWeb;
    o["viewOnlyDefault"] = s.viewOnlyDefault;
    o["banOnTamper"] = s.banOnTamper;
    o["shell"] = s.shell;
    o["fps"] = s.fps;
    o["quality"] = s.quality;
    o["scale"] = s.scale;
    o["maxHeight"] = s.maxHeight;
    o["theme"] = s.theme;
    o["language"] = s.language;
    o["startMinimized"] = s.startMinimized;
    if (!file.open(QIODevice::WriteOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, "Cannot write " + this->_path.toStdString());
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.commit()) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Config, "Cannot write " + this->_path.toStdString());
}
