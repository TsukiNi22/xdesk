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
    #include <QStringList>          // QStringList
    #include <QUdpSocket>           // QUdpSocket
    #include <QByteArray>           // QByteArray
    #include <QProcess>             // QProcess
    #include <QObject>              // QObject
    #include <QString>              // QString
    #include <QTimer>               // QTimer
    #include <QList>                // QList

namespace xdesk::net { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Endpoint {
    QString label; // "LAN eth0", "Tailscale", "bore.pub"...
    QString url; // https://host:port (open in a browser)
    QString host; // host for the native clients
    quint16 port = 0;
    bool endToEnd = true; // false: the relay sees the traffic (cloudflared)
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

        // ---------- Pre-Function -------- //
        void refresh_(void); // recompute the endpoints
        void startTool_(void);
        void onToolOutput_(void);
        void setStatus_(const QString& status);

    public:
        // ---------- Pre-Function -------- //
        void start(const core::Settings& settings, const quint16 port, const QString& name, const QString& fingerprint);
        void stop(void); // tunnel stopped, UPnP mapping removed
        _cold _nodiscard static QStringList lanAddresses(void); // IPv4 of the up interfaces (no loopback)
        _cold _nodiscard static QString tailscaleAddress(void); // "" when not connected
        _cold _nodiscard static QList<Discovered> scan(const int milliseconds = 2500); // LAN discovery (blocking)

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const QList<Endpoint>& endpoints(void) const {return this->_endpoints;};
        _cold _nodiscard inline const QString& status(void) const            {return this->_status;};
        _cold _nodiscard inline core::Exposure mode(void) const              {return this->_mode;};

        // ---------- Constructor --------- //
        explicit Exposure(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Exposure() override;

    signals:
        void changed(void); // endpoints / status
};

} // namespace end
#endif /* EXPOSURE_H */
