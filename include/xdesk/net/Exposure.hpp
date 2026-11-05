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
##  @file Exposure.hpp

File Description:
##  Reachability of the host: LAN addresses, tailscale, UPnP, tunnels (bore, cloudflared, ngrok), LAN discovery
\**************************************************************/

#ifndef EXPOSURE_H
    #define EXPOSURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _cold, _nodiscard
    #include "../core/Config.hpp"   // xdesk::core::Exposure, xdesk::core::Settings
    #include <QSslCertificate>      // QSslCertificate
    #include <QHostAddress>         // QHostAddress
    #include <QStringList>          // QStringList
    #include <QUdpSocket>           // QUdpSocket
    #include <QByteArray>           // QByteArray
    #include <QProcess>             // QProcess
    #include <QSslKey>              // QSslKey
    #include <QObject>              // QObject
    #include <QString>              // QString
    #include <QTimer>               // QTimer
    #include <QList>                // QList

namespace xdesk::net { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Endpoint {
    QString label; // "LAN", "Tailscale", "bore.pub"...
    QString reach; // who can use it, in plain words
    QString url; // https://host:port (open in a browser): the name when it has a valid certificate / resolves, else the address
    QString host; // name or address for the native clients
    QString ip; // numeric address behind host / url ("" = host is already one): the android app links use it
    quint16 port = 0;
    bool endToEnd = true; // false: the relay sees the traffic (cloudflared)
};

struct Tailnet {
    bool up = false; // tailscale connected on this PC
    QString dns; // MagicDNS name of this PC ("pc.tailnet.ts.net", "" = MagicDNS off)
    QString ip; // tailnet IPv4
    QString name; // name of the tailnet
    bool https = false; // HTTPS certificates enabled on the tailnet (tailscale cert works for dns)
};

struct Discovered {
    QString name; // host name
    QString host; // address
    quint16 port = 0;
    QString fingerprint; // short fingerprint (check against the host screen)
};

//----------------------------------------------------------------//
/* CLASS */

class Exposure: public QObject {
    Q_OBJECT

    private:
        core::Exposure _mode = core::Exposure::Lan;
        quint16 _port = 0; // local port of the server
        QString _publicHost; // Manual
        QProcess _tool; // tunnel process (bore, cloudflared, ngrok)
        QList<Endpoint> _endpoints; // current endpoints
        QString _status; // human readable state ("tunnel up", "upnpc missing"...)
        QUdpSocket _discovery; // broadcast announce
        QTimer _announce; // every 2 s
        QByteArray _announcement; // json sent
        QProcess _tailscale; // tailscale status --json
        Tailnet _tailnet; // last answer of tailscale
        QByteArray _tailnetAnswer; // its json (status recomputed when the certificate arrives)
        QTimer _probe; // every 30 s: interfaces and tailnet may change
        bool _active = false; // between start and stop (a late tailscale answer adds no endpoint after stop)
        QProcess _certTool; // tailscale cert
        QString _certName; // name of the loaded tailnet certificate ("" = none)
        QList<QSslCertificate> _certChain; // leaf first
        QSslKey _certKey;
        QString _certError; // last failure of tailscale cert ("" = none)
        qint64 _certFetched = 0; // ms since epoch of the last tailscale cert run
        QString _mdnsName; // <host>.local when it resolves to a LAN address of this PC ("" = not published)
        QStringList _mdnsAddresses; // IPv4 of _mdnsName

        // ---------- Pre-Function -------- //
        void refresh_(void); // recompute the endpoints
        void startTool_(void);
        void onToolOutput_(void);
        void setStatus_(const QString& status);
        void probeTailnet_(void); // async, refresh_ when it answers
        void onTailnet_(void);
        void fetchCertificate_(void); // tailscale cert (async), the files of the last run used meanwhile
        void onCertificate_(void);
        bool loadCertificate_(const QString& name); // false = missing, unreadable or expiring
        void lookupMdns_(void); // async, refresh_ when it answers

    public:
        // ---------- Pre-Function -------- //
        void start(const core::Settings& settings, const quint16 port, const QString& name, const QString& fingerprint);
        void stop(void); // tunnel stopped, UPnP mapping removed
        _cold _nodiscard QString refusal(const QHostAddress& peer) const; // "" = the mode accepts this peer, else why not
        _cold _nodiscard static QList<QPair<QString, QString>> lanInterfaces(void); // <interface, IPv4> of the up interfaces (no loopback, no tailnet, no container bridge)
        _cold _nodiscard static QStringList lanAddresses(void); // IPv4 of lanInterfaces()
        _cold _nodiscard static QString tailscaleAddress(void); // "" when not connected
        _cold _nodiscard static bool isLocalNetwork(const QHostAddress& ip); // private / link local ranges (RFC 1918, RFC 4193)
        _cold _nodiscard static QList<Discovered> scan(const int milliseconds = 2500); // LAN discovery (blocking)

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const QList<Endpoint>& endpoints(void) const               {return this->_endpoints;};
        _cold _nodiscard inline const QString& status(void) const                          {return this->_status;};
        _cold _nodiscard inline core::Exposure mode(void) const                            {return this->_mode;};
        _cold _nodiscard inline const Tailnet& tailnet(void) const                         {return this->_tailnet;};
        _cold _nodiscard inline const QString& certificateName(void) const                 {return this->_certName;};
        _cold _nodiscard inline const QList<QSslCertificate>& certificateChain(void) const {return this->_certChain;};
        _cold _nodiscard inline const QSslKey& certificateKey(void) const                  {return this->_certKey;};

        /* tools */
        _cold _nodiscard static inline bool isTailnet(const QHostAddress& ip) {return ip.isInSubnet(QHostAddress("100.64.0.0"), 10) || ip.isInSubnet(QHostAddress("fd7a:115c:a1e0::"), 48);}; // 100.64.0.0/10, fd7a:115c:a1e0::/48

        // ---------- Constructor --------- //
        explicit Exposure(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Exposure() override;

    signals:
        void changed(void); // endpoints / status
        void certificateChanged(void); // tailnet certificate loaded / renewed / dropped (certificateName)
};

} // namespace end
#endif /* EXPOSURE_H */
