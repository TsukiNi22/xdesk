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
##  @file Exposure.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/net/Exposure.hpp"
#include "xdesk/net/Protocol.hpp"
#include <QRegularExpression>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QHostAddress>
#include <QJsonObject>
#include <QString>

xdesk::net::Exposure::Exposure(QObject* parent)
: QObject(parent)
{
    this->_tool.setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(&this->_tool, &QProcess::readyRead, this, &Exposure::onToolOutput_);
    QObject::connect(&this->_tool, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        this->setStatus_(this->_tool.program() + ": " + this->_tool.errorString());
    });
    QObject::connect(&this->_tool, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        this->setStatus_(this->_tool.program() + " stopped (code " + QString::number(code) + ")");
        this->refresh_();
    });
    QObject::connect(&this->_announce, &QTimer::timeout, this, [this]() {
        this->_discovery.writeDatagram(this->_announcement, QHostAddress::Broadcast, XDESK_DISCOVERY_PORT);
    });
}

xdesk::net::Exposure::~Exposure()
{
    this->stop();
}

QStringList xdesk::net::Exposure::lanAddresses(void)
{
    QStringList out;

    for (const QNetworkInterface& iface: QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        if (iface.name().startsWith("docker") || iface.name().startsWith("br-") || iface.name().startsWith("veth") || iface.name().startsWith("virbr")) continue;
        for (const QNetworkAddressEntry& entry: iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || ip.isInSubnet(QHostAddress("100.64.0.0"), 10)) continue;
            out.append(ip.toString());
        }
    }
    return out;
}

QString xdesk::net::Exposure::tailscaleAddress(void)
{
    // Tailnet addresses live in the CGNAT range 100.64.0.0/10
    for (const QHostAddress& ip: QNetworkInterface::allAddresses())
        if (ip.protocol() == QAbstractSocket::IPv4Protocol && ip.isInSubnet(QHostAddress("100.64.0.0"), 10)) return ip.toString();
    return "";
}

void xdesk::net::Exposure::setStatus_(const QString& status)
{
    this->_status = status;
    emit this->changed();
}

void xdesk::net::Exposure::start(const core::Settings& settings, const quint16 port, const QString& name, const QString& fingerprint)
{
    this->stop();
    this->_mode = settings.exposure;
    this->_port = port;
    this->_publicHost = settings.publicHost;
    this->_endpoints.clear();
    this->refresh_();
    if (this->_mode == core::Exposure::Upnp || this->_mode == core::Exposure::Bore || this->_mode == core::Exposure::Cloudflared || this->_mode == core::Exposure::Ngrok)
        this->startTool_();
    else this->setStatus_(core::exposure_name(this->_mode) + " ready");

    // LAN discovery
    if (settings.discovery) {
        this->_announcement = QJsonDocument(QJsonObject{{"xdesk", XDESK_PROTOCOL_VERSION}, {"name", name}, {"port", port}, {"fp", fingerprint}}).toJson(QJsonDocument::Compact);
        this->_discovery.bind(QHostAddress::AnyIPv4, 0);
        this->_announce.start(2000);
    }
}

void xdesk::net::Exposure::stop(void)
{
    this->_announce.stop();
    this->_discovery.close();
    if (this->_tool.state() != QProcess::NotRunning) {
        this->_tool.terminate();
        if (!this->_tool.waitForFinished(1500)) this->_tool.kill();
    }

    // Remove the router mapping (best effort, detached)
    if (this->_mode == core::Exposure::Upnp && this->_port != 0 && !QStandardPaths::findExecutable("upnpc").isEmpty())
        QProcess::startDetached("upnpc", {"-d", QString::number(this->_port), "TCP"});
}

void xdesk::net::Exposure::refresh_(void)
{
    QList<Endpoint> endpoints;
    const QString port = QString::number(this->_port);
    const QString tailnet = tailscaleAddress();

    // The public endpoint first (found by the tool output for the tunnels)
    for (const Endpoint& e: this->_endpoints)
        if (e.label != "LAN" && e.label != "Tailscale" && e.label != "Local" && e.label != "Public") endpoints.append(e);
    if (this->_mode == core::Exposure::Manual && !this->_publicHost.isEmpty())
        endpoints.append({"Public", "https://" + this->_publicHost + ":" + port, this->_publicHost, this->_port, true});
    if (!tailnet.isEmpty()) endpoints.append({"Tailscale", "https://" + tailnet + ":" + port, tailnet, this->_port, true});
    for (const QString& ip: lanAddresses())
        endpoints.append({"LAN", "https://" + ip + ":" + port, ip, this->_port, true});
    endpoints.append({"Local", "https://localhost:" + port, "localhost", this->_port, true});
    this->_endpoints = endpoints;
    emit this->changed();
}

void xdesk::net::Exposure::startTool_(void)
{
    const QString port = QString::number(this->_port);
    QString program;
    QStringList args;

    switch (this->_mode) {
        case core::Exposure::Upnp:        program = "upnpc";       args = {"-e", "xdesk", "-r", port, "TCP"}; break;
        case core::Exposure::Bore:        program = "bore";        args = {"local", port, "--to", "bore.pub"}; break;
        case core::Exposure::Cloudflared: program = "cloudflared"; args = {"tunnel", "--no-autoupdate", "--url", "https://localhost:" + port, "--no-tls-verify"}; break;
        case core::Exposure::Ngrok:       program = "ngrok";       args = {"tcp", port, "--log", "stdout", "--log-format", "logfmt"}; break;
        default: return;
    }
    if (QStandardPaths::findExecutable(program).isEmpty()) {
        this->setStatus_(program + " is not installed (see the README: Exposure)");
        return;
    }
    this->setStatus_("Starting " + program + "...");
    this->_tool.start(program, args);
}

void xdesk::net::Exposure::onToolOutput_(void)
{
    const QString output = QString::fromUtf8(this->_tool.readAll());
    QRegularExpressionMatch m;

    switch (this->_mode) {
        case core::Exposure::Upnp:
            m = QRegularExpression(R"(ExternalIPAddress\s*=\s*([0-9.]+))").match(output);
            if (m.hasMatch()) {
                this->_endpoints.prepend({"UPnP", "https://" + m.captured(1) + ":" + QString::number(this->_port), m.captured(1), this->_port, true});
                this->setStatus_("Port " + QString::number(this->_port) + " opened on the router");
            } else if (output.contains("failed", Qt::CaseInsensitive) || output.contains("No IGD", Qt::CaseInsensitive)) this->setStatus_("UPnP failed: " + output.trimmed().right(160));
            break;
        case core::Exposure::Bore:
            m = QRegularExpression(R"(listening at ([\w.\-]+):(\d+))").match(output);
            if (m.hasMatch()) {
                this->_endpoints.prepend({"bore.pub", "https://" + m.captured(1) + ":" + m.captured(2), m.captured(1), static_cast<quint16>(m.captured(2).toUInt()), true});
                this->setStatus_("Tunnel bore.pub up (TLS end to end)");
            }
            break;
        case core::Exposure::Cloudflared:
            m = QRegularExpression(R"((https://[\w\-]+\.trycloudflare\.com))").match(output);
            if (m.hasMatch()) {
                const QString host = QString(m.captured(1)).mid(8);
                this->_endpoints.prepend({"Cloudflare", m.captured(1), host, 443, false});
                this->setStatus_("Tunnel trycloudflare.com up (the relay terminates TLS)");
            }
            break;
        case core::Exposure::Ngrok:
            m = QRegularExpression(R"(url=tcp://([\w.\-]+):(\d+))").match(output);
            if (m.hasMatch()) {
                this->_endpoints.prepend({"ngrok", "https://" + m.captured(1) + ":" + m.captured(2), m.captured(1), static_cast<quint16>(m.captured(2).toUInt()), true});
                this->setStatus_("Tunnel ngrok up (TLS end to end)");
            }
            break;
        default: break;
    }
    if (m.hasMatch()) this->refresh_();
}

QList<xdesk::net::Discovered> xdesk::net::Exposure::scan(const int milliseconds)
{
    QUdpSocket socket;
    QElapsedTimer timer;
    QList<Discovered> found;

    if (!socket.bind(QHostAddress::AnyIPv4, XDESK_DISCOVERY_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) return found;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        if (!socket.waitForReadyRead(static_cast<int>(milliseconds - timer.elapsed()))) continue;
        while (socket.hasPendingDatagrams()) {
            QByteArray data(static_cast<qsizetype>(socket.pendingDatagramSize()), '\0');
            QHostAddress from;
            QJsonObject o;
            bool known = false;

            socket.readDatagram(data.data(), data.size(), &from);
            o = QJsonDocument::fromJson(data).object();
            if (o.value("xdesk").toInt() != XDESK_PROTOCOL_VERSION) continue;
            const Discovered d = {o.value("name").toString(), QHostAddress(from.toIPv4Address()).toString(), static_cast<quint16>(o.value("port").toInt()), o.value("fp").toString()};
            for (const Discovered& other: found)
                known = known || (other.host == d.host && other.port == d.port);
            if (!known) found.append(d);
        }
    }
    return found;
}
