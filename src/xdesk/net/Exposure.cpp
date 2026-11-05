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
#include "xdesk/core/Config.hpp"
#include <QRegularExpression>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDateTime>
#include <QHostInfo>
#include <QString>
#include <QFile>
#include <QDir>

//----------------------------------------------------------------//
/* DEFINE */

#define CERT_RENEW 43200000 // ms between two tailscale cert runs (it renews the certificate itself when it gets old)
#define CERT_RETRY 300000 // ms before trying again after a failure
#define CERT_MARGIN 86400 // s: a certificate expiring sooner is not served

_cold static QString certificate_path(const QString& name, const QString& ext)
{
    return QDir(xdesk::core::Config::dataDir()).filePath("tailscale/" + name + "." + ext);
}

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
    QObject::connect(&this->_tailscale, &QProcess::finished, this, &Exposure::onTailnet_);
    QObject::connect(&this->_tailscale, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {this->onTailnet_();});
    QObject::connect(&this->_probe, &QTimer::timeout, this, &Exposure::probeTailnet_);
    QObject::connect(&this->_certTool, &QProcess::finished, this, &Exposure::onCertificate_);
    QObject::connect(&this->_certTool, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) this->onCertificate_();
    });
}

xdesk::net::Exposure::~Exposure()
{
    this->stop();
}

QList<QPair<QString, QString>> xdesk::net::Exposure::lanInterfaces(void)
{
    QList<QPair<QString, QString>> out;

    for (const QNetworkInterface& iface: QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack)) continue;
        if (iface.name().startsWith("docker") || iface.name().startsWith("br-") || iface.name().startsWith("veth") || iface.name().startsWith("virbr") || iface.name().startsWith("tailscale")) continue;
        for (const QNetworkAddressEntry& entry: iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol || isTailnet(ip)) continue;
            out.append({iface.humanReadableName(), ip.toString()});
        }
    }
    return out;
}

QStringList xdesk::net::Exposure::lanAddresses(void)
{
    QStringList out;

    for (const QPair<QString, QString>& entry: lanInterfaces())
        out.append(entry.second);
    return out;
}

bool xdesk::net::Exposure::isLocalNetwork(const QHostAddress& ip)
{
    return ip.isInSubnet(QHostAddress("10.0.0.0"), 8) || ip.isInSubnet(QHostAddress("172.16.0.0"), 12) || ip.isInSubnet(QHostAddress("192.168.0.0"), 16)
        || ip.isInSubnet(QHostAddress("169.254.0.0"), 16) || ip.isInSubnet(QHostAddress("fe80::"), 10) || ip.isInSubnet(QHostAddress("fc00::"), 7);
}

QString xdesk::net::Exposure::refusal(const QHostAddress& peer) const
{
    bool v4 = false;
    const quint32 raw = peer.toIPv4Address(&v4);
    const QHostAddress ip = v4 ? QHostAddress(raw) : peer; // ::ffff:a.b.c.d -> a.b.c.d

    // This PC (and the tunnels: bore, cloudflared and ngrok connect from here)
    if (ip.isLoopback()) return "";

    // Tailnet: only when Tailscale is the chosen exposure
    if (isTailnet(ip)) return this->_mode == core::Exposure::Tailscale ? QString() : "came through Tailscale, which is not the chosen exposure (" + core::exposure_name(this->_mode) + ")";

    // LAN: always open while sharing
    if (isLocalNetwork(ip)) return "";

    // Public address: only with a port opened on the router
    if (this->_mode == core::Exposure::Upnp || this->_mode == core::Exposure::Manual) return "";
    return "came from the internet, while the exposure is " + core::exposure_name(this->_mode) + " (no port opened on the router)";
}

void xdesk::net::Exposure::probeTailnet_(void)
{
    if (this->_tailscale.state() != QProcess::NotRunning) return;
    if (QStandardPaths::findExecutable("tailscale").isEmpty()) {this->onTailnet_(); return;}
    this->_tailscale.start("tailscale", {"status", "--json"});
}

void xdesk::net::Exposure::onTailnet_(void)
{
    const QByteArray answer = this->_tailscale.state() == QProcess::NotRunning ? this->_tailscale.readAllStandardOutput() : QByteArray();
    const QJsonObject o = QJsonDocument::fromJson(answer.isEmpty() ? this->_tailnetAnswer : answer).object();
    const QJsonObject self = o.value("Self").toObject();
    Tailnet tailnet;

    if (!this->_active) return; // answer of a probe killed by stop
    if (!answer.isEmpty()) this->_tailnetAnswer = answer;

    tailnet.up = o.value("BackendState").toString() == "Running";
    tailnet.name = o.value("CurrentTailnet").toObject().value("Name").toString();
    if (o.value("CurrentTailnet").toObject().value("MagicDNSEnabled").toBool(true)) tailnet.dns = self.value("DNSName").toString();
    if (tailnet.dns.endsWith('.')) tailnet.dns.chop(1);
    for (const QJsonValue& value: self.value("TailscaleIPs").toArray())
        if (QHostAddress(value.toString()).protocol() == QAbstractSocket::IPv4Protocol) {tailnet.ip = value.toString(); break;}
    for (const QJsonValue& value: o.value("CertDomains").toArray())
        if (!tailnet.dns.isEmpty() && value.toString().compare(tailnet.dns, Qt::CaseInsensitive) == 0) tailnet.https = true;

    // No CLI (or no answer): the address of the tailscale interface, without a name
    if (tailnet.ip.isEmpty()) {
        tailnet.ip = tailscaleAddress();
        tailnet.up = !tailnet.ip.isEmpty();
    }
    this->_tailnet = tailnet;

    // Browsers on the name: a real certificate (every *.ts.net name is HSTS preloaded, a self-signed one is refused there)
    if (this->_mode == core::Exposure::Tailscale && tailnet.up && tailnet.https) {
        if (this->_certName != tailnet.dns && this->loadCertificate_(tailnet.dns)) emit this->certificateChanged();
        const qint64 since = QDateTime::currentMSecsSinceEpoch() - this->_certFetched;
        if (since > (this->_certName == tailnet.dns ? CERT_RENEW : CERT_RETRY)) this->fetchCertificate_();
    }
    this->refresh_();
    if (this->_mode != core::Exposure::Tailscale) return;
    if (!tailnet.up) {
        this->setStatus_("Tailscale is not connected on this PC (run: tailscale up): LAN only for now");
        return;
    }
    QString status = "Tailscale: reachable by the devices of " + (tailnet.name.isEmpty() ? QString("your tailnet") : tailnet.name) + ", and on the LAN";
    if (tailnet.dns.isEmpty()) status += ". MagicDNS is off: the browsers use the address";
    else if (!tailnet.https) status += ". Browsers use the address: enable HTTPS in the Tailscale admin console (DNS page) to use " + tailnet.dns;
    else if (this->_certName.isEmpty() && !this->_certError.isEmpty()) status += ". Browsers use the address: " + this->_certError;
    this->setStatus_(status);
}

void xdesk::net::Exposure::fetchCertificate_(void)
{
    const QString name = this->_tailnet.dns;

    if (this->_certTool.state() != QProcess::NotRunning || name.isEmpty()) return;
    if (!QDir().mkpath(QFileInfo(certificate_path(name, "crt")).absolutePath())) return;
    this->_certFetched = QDateTime::currentMSecsSinceEpoch();
    this->_certTool.setProperty("xdesk_name", name);
    this->_certTool.start("tailscale", {"cert", "--cert-file", certificate_path(name, "crt"), "--key-file", certificate_path(name, "key"), name});
}

void xdesk::net::Exposure::onCertificate_(void)
{
    const QString name = this->_certTool.property("xdesk_name").toString();
    const QString output = QString::fromUtf8(this->_certTool.readAllStandardError() + this->_certTool.readAllStandardOutput()).trimmed();
    const bool ok = this->_certTool.exitStatus() == QProcess::NormalExit && this->_certTool.exitCode() == 0 && this->_certTool.error() != QProcess::FailedToStart;

    const QSslCertificate before = this->_certName == name ? this->_certChain.value(0) : QSslCertificate();

    (void)QFile::setPermissions(certificate_path(name, "key"), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (ok && this->loadCertificate_(name)) {
        this->_certError.clear();
        if (this->_certChain.value(0) != before) emit this->certificateChanged(); // renewed (the same one again: nothing to do)
    } else if (!ok) {
        // Most common: not the operator of tailscaled (sudo tailscale set --operator=$USER), HTTPS off in the console
        this->_certError = output.contains("Access denied", Qt::CaseInsensitive) || output.contains("permission", Qt::CaseInsensitive)
            ? "tailscale cert needs the operator right (once, as root: tailscale set --operator=$USER)"
            : "tailscale cert failed (" + output.section('\n', 0, 0).left(160) + ")";
    }
    if (this->_active) this->onTailnet_(); // status and endpoints with the result (no new tailscale status run)
}

bool xdesk::net::Exposure::loadCertificate_(const QString& name)
{
    QFile keyFile(certificate_path(name, "key"));
    const QList<QSslCertificate> chain = QSslCertificate::fromPath(certificate_path(name, "crt"), QSsl::Pem);
    QSslKey key;

    if (chain.isEmpty() || chain.first().isNull() || !keyFile.open(QIODevice::ReadOnly)) return false;
    const QByteArray pem = keyFile.readAll();
    key = QSslKey(pem, QSsl::Ec, QSsl::Pem);
    if (key.isNull()) key = QSslKey(pem, QSsl::Rsa, QSsl::Pem);
    if (key.isNull() || chain.first().expiryDate() < QDateTime::currentDateTimeUtc().addSecs(CERT_MARGIN)) return false;
    if (!chain.first().subjectAlternativeNames().values(QSsl::DnsEntry).contains(name, Qt::CaseInsensitive)) return false;
    this->_certName = name;
    this->_certChain = chain;
    this->_certKey = key;
    return true;
}

void xdesk::net::Exposure::lookupMdns_(void)
{
    const QString name = QHostInfo::localHostName() + ".local";

    // Published by avahi / Bonjour: the browsers can use the name on the LAN (only kept when it resolves to an IPv4 of this PC)
    QHostInfo::lookupHost(name, this, [this, name](const QHostInfo& info) {
        QStringList addresses;
        for (const QHostAddress& address: info.addresses())
            if (address.protocol() == QAbstractSocket::IPv4Protocol) addresses.append(address.toString());
        this->_mdnsName = addresses.isEmpty() ? QString() : name;
        this->_mdnsAddresses = addresses;
        this->refresh_();
    });
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
    this->_active = true;
    this->_mode = settings.exposure;
    this->_port = port;
    this->_publicHost = settings.publicHost;
    this->_endpoints.clear();
    this->refresh_();
    this->probeTailnet_();
    this->lookupMdns_();
    this->_probe.start(30000);
    if (this->_mode == core::Exposure::Upnp || this->_mode == core::Exposure::Bore || this->_mode == core::Exposure::Cloudflared || this->_mode == core::Exposure::Ngrok)
        this->startTool_();
    else if (this->_mode == core::Exposure::Lan) this->setStatus_("LAN only: devices on the same network can connect");
    else if (this->_mode == core::Exposure::Manual) this->setStatus_(this->_publicHost.isEmpty() ? "Manual: set the public address in the settings (LAN works meanwhile)" : "Manual: reachable on " + this->_publicHost + " if port " + QString::number(port) + " is forwarded to this PC");
    else this->setStatus_("Tailscale: asking tailscale...");

    // LAN discovery
    if (settings.discovery) {
        this->_announcement = QJsonDocument(QJsonObject{{"xdesk", XDESK_PROTOCOL_VERSION}, {"name", name}, {"port", port}, {"fp", fingerprint}}).toJson(QJsonDocument::Compact);
        this->_discovery.bind(QHostAddress::AnyIPv4, 0);
        this->_announce.start(2000);
    }
}

void xdesk::net::Exposure::stop(void)
{
    this->_active = false;
    this->_probe.stop();
    if (this->_tailscale.state() != QProcess::NotRunning) this->_tailscale.kill();
    this->_tailnetAnswer.clear();
    this->_announce.stop();
    this->_discovery.close();
    if (this->_tool.state() != QProcess::NotRunning) {
        this->_tool.terminate();
        if (!this->_tool.waitForFinished(1500)) this->_tool.kill();
    }

    // Remove the router mapping (best effort, detached)
    if (this->_mode == core::Exposure::Upnp && this->_port != 0 && !QStandardPaths::findExecutable("upnpc").isEmpty())
        QProcess::startDetached("upnpc", {"-d", QString::number(this->_port), "TCP"});
    this->_endpoints.clear();
    this->_status = "";
}

void xdesk::net::Exposure::refresh_(void)
{
    QList<Endpoint> endpoints;
    const QString port = QString::number(this->_port);

    if (!this->_active) return;
    // The public endpoint first (found by the tool output for the tunnels), only the one of the chosen mode
    for (const Endpoint& e: this->_endpoints)
        if (e.label != "LAN" && e.label != "Tailscale" && e.label != "This PC" && e.label != "Internet") endpoints.append(e);
    if (this->_mode == core::Exposure::Manual && !this->_publicHost.isEmpty())
        endpoints.append({"Internet", "Anyone on the internet, through the port you forwarded on the router", "https://" + this->_publicHost + ":" + port, this->_publicHost, "", this->_port, true});
    if (this->_mode == core::Exposure::Tailscale && this->_tailnet.up) {
        const QString host = this->_tailnet.dns.isEmpty() ? this->_tailnet.ip : this->_tailnet.dns;
        const bool named = !this->_tailnet.dns.isEmpty() && this->_certName == this->_tailnet.dns;
        const QString web = named || this->_tailnet.ip.isEmpty() ? host : this->_tailnet.ip;

        // The name only with the tailscale certificate: every *.ts.net name is HSTS preloaded, a self-signed certificate is refused there without any exception
        endpoints.append({"Tailscale", "Your devices signed in to " + (this->_tailnet.name.isEmpty() ? QString("the tailnet") : "the tailnet " + this->_tailnet.name), "https://" + web + ":" + port, host, host == this->_tailnet.ip ? QString() : this->_tailnet.ip, this->_port, true});
    }

    // LAN: always open while sharing (the .local name when it points to this interface)
    for (const QPair<QString, QString>& entry: lanInterfaces()) {
        const QString host = this->_mdnsAddresses.contains(entry.second) ? this->_mdnsName : entry.second;
        endpoints.append({"LAN", "Devices on the same network (" + entry.first + ")", "https://" + host + ":" + port, host, host == entry.second ? QString() : entry.second, this->_port, true});
    }
    endpoints.append({"This PC", "Only this computer", "https://localhost:" + port, "localhost", "127.0.0.1", this->_port, true});
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
                this->_endpoints.prepend({"UPnP", "Anyone on the internet, through the port opened on the router", "https://" + m.captured(1) + ":" + QString::number(this->_port), m.captured(1), "", this->_port, true});
                this->setStatus_("Port " + QString::number(this->_port) + " opened on the router");
            } else if (output.contains("failed", Qt::CaseInsensitive) || output.contains("No IGD", Qt::CaseInsensitive)) this->setStatus_("UPnP failed: " + output.trimmed().right(160));
            break;
        case core::Exposure::Bore:
            m = QRegularExpression(R"(listening at ([\w.\-]+):(\d+))").match(output);
            if (m.hasMatch()) {
                this->_endpoints.prepend({"bore.pub", "Anyone on the internet, through the bore.pub relay (it cannot read the traffic)", "https://" + m.captured(1) + ":" + m.captured(2), m.captured(1), "", static_cast<quint16>(m.captured(2).toUInt()), true});
                this->setStatus_("Tunnel bore.pub up (TLS end to end)");
            }
            break;
        case core::Exposure::Cloudflared:
            m = QRegularExpression(R"((https://[\w\-]+\.trycloudflare\.com))").match(output);
            if (m.hasMatch()) {
                const QString host = QString(m.captured(1)).mid(8);
                this->_endpoints.prepend({"Cloudflare", "Anyone on the internet, through Cloudflare (the relay can read the traffic)", m.captured(1), host, "", 443, false});
                this->setStatus_("Tunnel trycloudflare.com up (the relay terminates TLS)");
            }
            break;
        case core::Exposure::Ngrok:
            m = QRegularExpression(R"(url=tcp://([\w.\-]+):(\d+))").match(output);
            if (m.hasMatch()) {
                this->_endpoints.prepend({"ngrok", "Anyone on the internet, through the ngrok relay (it cannot read the traffic)", "https://" + m.captured(1) + ":" + m.captured(2), m.captured(1), "", static_cast<quint16>(m.captured(2).toUInt()), true});
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
