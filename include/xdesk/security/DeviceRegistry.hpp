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
##  @file DeviceRegistry.hpp

File Description:
##  Known devices of the host: trusted, banned, secrets, banned addresses (signed file)
\**************************************************************/

#ifndef DEVICEREGISTRY_H
    #define DEVICEREGISTRY_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <optional>         // std::optional
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <QList>            // QList

namespace xdesk::security { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Device {
    QString id; // random id given at the first successful auth
    QString name; // name sent by the client ("Phone - Android (apk)")
    QString kind; // gui | tty | web | apk
    QString os; // os sent by the client
    QString address; // last address
    QByteArray secret; // 32 bytes, HMAC key of the device auth (only when remembered)
    bool trusted = false; // no code / approval needed (secret proof)
    bool banned = false; // refused until a local unban
    QString banReason = ""; // why
    qint64 since = 0; // first seen (ms)
    qint64 seen = 0; // last seen (ms)
    int sessions = 0; // number of sessions opened
};

struct AddressBan {
    QString address; // ip of the peer
    QString reason; // why
    qint64 since = 0; // ms
    qint64 until = 0; // ms, 0 = until a local unban
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * devices.json (0600) = {"devices": [...], "addresses": [...], "mac": hex}
 * mac = HMAC-SHA256(registry.key, content): a file changed by someone else than the host is refused (tamper)
*/
class DeviceRegistry: public QObject {
    Q_OBJECT

    private:
        QString _path; // devices.json
        QByteArray _key; // registry.key (32 bytes, 0600)
        QList<Device> _devices; // every known device
        QList<AddressBan> _addresses; // banned addresses
        bool _tampered = false; // the file did not match its mac at load

        // ---------- Pre-Function -------- //
        void save_(void); // write + mac, emit changed

    public:
        // ---------- Pre-Function -------- //
        void load(void); // read and check the mac
        _cold _nodiscard std::optional<Device> find(const QString& id) const;
        _cold _nodiscard Device& upsert(const Device& device); // add or update by id
        void remove(const QString& id);
        void setTrusted(const QString& id, const bool trusted);
        void ban(const QString& id, const QString& reason);
        void unban(const QString& id); // LOCAL ONLY (the host checks the local presence before)
        void banAddress(const QString& address, const QString& reason, const qint64 seconds = 0); // 0 = until a local unban
        void unbanAddress(const QString& address); // LOCAL ONLY
        void touch(const QString& id, const QString& address); // seen + sessions
        _cold _nodiscard bool isBanned(const QString& id) const;
        _cold _nodiscard std::optional<AddressBan> addressBan(const QString& address) const; // expired bans ignored

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const QList<Device>& devices(void) const       {return this->_devices;};
        _cold _nodiscard inline const QList<AddressBan>& addresses(void) const {return this->_addresses;};
        _cold _nodiscard inline bool wasTampered(void) const                   {return this->_tampered;};
        _cold _nodiscard inline const QString& path(void) const                {return this->_path;};

        // ---------- Constructor --------- //
        explicit DeviceRegistry(const QString& dir, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~DeviceRegistry() override = default;

    signals:
        void changed(void);
};

} // namespace end
#endif /* DEVICEREGISTRY_H */
