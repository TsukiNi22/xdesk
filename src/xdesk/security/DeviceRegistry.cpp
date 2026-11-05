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
##  @file DeviceRegistry.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xdesk/security/DeviceRegistry.hpp"
#include "xdesk/security/Crypto.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSaveFile>
#include <optional>
#include <QString>
#include <QFile>

static QJsonObject device_to_json(const xdesk::security::Device& d)
{
    QJsonObject o;

    o["id"] = d.id;
    o["name"] = d.name;
    o["kind"] = d.kind;
    o["os"] = d.os;
    o["address"] = d.address;
    o["secret"] = QString::fromLatin1(d.secret.toHex());
    o["trusted"] = d.trusted;
    o["banned"] = d.banned;
    o["banReason"] = d.banReason;
    o["since"] = d.since;
    o["seen"] = d.seen;
    o["sessions"] = d.sessions;
    return o;
}

static xdesk::security::Device device_from_json(const QJsonObject& o)
{
    xdesk::security::Device d;

    d.id = o.value("id").toString();
    d.name = o.value("name").toString();
    d.kind = o.value("kind").toString();
    d.os = o.value("os").toString();
    d.address = o.value("address").toString();
    d.secret = QByteArray::fromHex(o.value("secret").toString().toLatin1());
    d.trusted = o.value("trusted").toBool();
    d.banned = o.value("banned").toBool();
    d.banReason = o.value("banReason").toString();
    d.since = o.value("since").toInteger();
    d.seen = o.value("seen").toInteger();
    d.sessions = o.value("sessions").toInt();
    return d;
}

xdesk::security::DeviceRegistry::DeviceRegistry(const QString& dir, QObject* parent)
: QObject(parent), _path{dir + "/devices.json"}
{
    QFile keyFile(dir + "/registry.key");

    // Mac key of the registry: only the host process writes the file
    if (keyFile.open(QIODevice::ReadOnly)) this->_key = keyFile.readAll();
    if (this->_key.size() != 32) {
        QSaveFile out(dir + "/registry.key");
        this->_key = random_bytes(32);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(this->_key);
            out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            out.commit();
        }
    }
}

void xdesk::security::DeviceRegistry::load(void)
{
    QFile file(this->_path);
    QJsonObject root;
    QJsonObject content;

    this->_devices.clear();
    this->_addresses.clear();
    this->_tampered = false;
    if (!file.open(QIODevice::ReadOnly)) return;
    root = QJsonDocument::fromJson(file.readAll()).object();
    content = root.value("content").toObject();

    // A file not signed by the host: refused, every device has to pair again (bans are kept)
    if (!constant_equal(hmac_sha256(this->_key, QJsonDocument(content).toJson(QJsonDocument::Compact)), QByteArray::fromHex(root.value("mac").toString().toLatin1()))) {
        this->_tampered = true;
        for (const QJsonValue& v: content.value("devices").toArray()) {
            Device d = device_from_json(v.toObject());
            if (!d.banned) continue;
            d.trusted = false;
            d.secret.clear();
            this->_devices.append(d);
        }
        for (const QJsonValue& v: content.value("addresses").toArray())
            this->_addresses.append({v["address"].toString(), v["reason"].toString(), v["since"].toInteger(), v["until"].toInteger()});
        this->save_();
        return;
    }
    for (const QJsonValue& v: content.value("devices").toArray())
        this->_devices.append(device_from_json(v.toObject()));
    for (const QJsonValue& v: content.value("addresses").toArray())
        this->_addresses.append({v["address"].toString(), v["reason"].toString(), v["since"].toInteger(), v["until"].toInteger()});
}

void xdesk::security::DeviceRegistry::save_(void)
{
    QJsonArray devices;
    QJsonArray addresses;
    QJsonObject content;
    QJsonObject root;
    QSaveFile file(this->_path);

    for (const Device& d: this->_devices)
        devices.append(device_to_json(d));
    for (const AddressBan& a: this->_addresses)
        addresses.append(QJsonObject{{"address", a.address}, {"reason", a.reason}, {"since", a.since}, {"until", a.until}});
    content["devices"] = devices;
    content["addresses"] = addresses;
    root["content"] = content;
    root["mac"] = QString::fromLatin1(hmac_sha256(this->_key, QJsonDocument(content).toJson(QJsonDocument::Compact)).toHex());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.commit();
    }
    emit this->changed();
}

std::optional<xdesk::security::Device> xdesk::security::DeviceRegistry::find(const QString& id) const
{
    if (id.isEmpty()) return std::nullopt;
    for (const Device& d: this->_devices)
        if (d.id == id) return d;
    return std::nullopt;
}

xdesk::security::Device& xdesk::security::DeviceRegistry::upsert(const Device& device)
{
    for (Device& d: this->_devices) {
        if (d.id != device.id) continue;
        d = device;
        this->save_();
        return d;
    }
    this->_devices.append(device);
    this->save_();
    return this->_devices.back();
}

void xdesk::security::DeviceRegistry::remove(const QString& id)
{
    this->_devices.removeIf([&](const Device& d) {return d.id == id;});
    this->save_();
}

void xdesk::security::DeviceRegistry::setTrusted(const QString& id, const bool trusted)
{
    for (Device& d: this->_devices) {
        if (d.id != id) continue;
        d.trusted = trusted;
        if (!trusted) d.secret.clear(); // the device has to use a code again
    }
    this->save_();
}

void xdesk::security::DeviceRegistry::ban(const QString& id, const QString& reason)
{
    for (Device& d: this->_devices) {
        if (d.id != id) continue;
        d.banned = true;
        d.trusted = false;
        d.secret.clear();
        d.banReason = reason;
    }
    this->save_();
}

void xdesk::security::DeviceRegistry::unban(const QString& id)
{
    for (Device& d: this->_devices) {
        if (d.id != id) continue;
        d.banned = false;
        d.banReason.clear();
    }
    this->save_();
}

void xdesk::security::DeviceRegistry::banAddress(const QString& address, const QString& reason, const qint64 seconds)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    this->_addresses.removeIf([&](const AddressBan& a) {return a.address == address;});
    this->_addresses.append({address, reason, now, seconds > 0 ? now + seconds * 1000 : 0});
    this->save_();
}

void xdesk::security::DeviceRegistry::unbanAddress(const QString& address)
{
    this->_addresses.removeIf([&](const AddressBan& a) {return a.address == address;});
    this->save_();
}

void xdesk::security::DeviceRegistry::touch(const QString& id, const QString& address)
{
    for (Device& d: this->_devices) {
        if (d.id != id) continue;
        d.seen = QDateTime::currentMSecsSinceEpoch();
        d.address = address;
        d.sessions++;
    }
    this->save_();
}

bool xdesk::security::DeviceRegistry::isBanned(const QString& id) const
{
    const std::optional<Device> d = this->find(id);

    return d.has_value() && d->banned;
}

std::optional<xdesk::security::AddressBan> xdesk::security::DeviceRegistry::addressBan(const QString& address) const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (const AddressBan& a: this->_addresses)
        if (a.address == address && (a.until == 0 || a.until > now)) return a;
    return std::nullopt;
}
