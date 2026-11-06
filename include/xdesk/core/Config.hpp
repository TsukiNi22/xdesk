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
##  @file Config.hpp

File Description:
##  Settings of xdesk (config.json), folders of the configuration, the data and the logs
\**************************************************************/

#ifndef CONFIG_H
    #define CONFIG_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <QStringList>      // QStringList
    #include <QString>          // QString
    #include <cstdint>          // std::uint16_t

namespace xdesk::core { // namespace start
//----------------------------------------------------------------//
/* ENUM */

// How the host is reachable from the clients
enum class Exposure {
    Lan, // every local interface, LAN discovery
    Tailscale, // the tailnet address of the host (end to end, WireGuard + TLS)
    Upnp, // port opened on the router with UPnP (upnpc), public address
    Bore, // tcp tunnel bore.pub (TLS kept end to end)
    Cloudflared, // https tunnel trycloudflare.com (the relay terminates the outer TLS)
    Ngrok, // tcp tunnel ngrok (TLS kept end to end, account needed)
    Manual, // port forwarded by hand, public address given in the settings
};

// When a local confirmation is needed before a client gets in
enum class Approval {
    Untrusted, // every device not trusted yet (default)
    Always, // every connection
    Never, // the code (or the trusted device secret) is enough
};

//----------------------------------------------------------------//
/* STRUCT */

struct Settings {
    /* network */
    std::uint16_t port = 47800; // https + wss port
    QString bind = "0.0.0.0"; // listening address ("::" for IPv6)
    Exposure exposure = Exposure::Lan; // how the host is reachable
    QString publicHost = ""; // Manual: public address / domain shown to the clients
    bool discovery = true; // udp broadcast on the LAN (xdesk scan)
    bool udp = true; // video of the native viewers (and the android app) over encrypted udp on the same port (tcp when it does not get through)
    bool lanes = true; // web viewers: extra websockets for the video while one connection holds it back (lossy far paths)

    /* access */
    int codeLength = 9; // characters of the pairing code (Crockford base32, 5 bits each)
    int codeLifetime = 300; // seconds before the code rotates (0 = only after use)
    Approval approval = Approval::Untrusted; // local confirmation of the connections
    int maxSessions = 8; // simultaneous sessions
    int maxAuthFailures = 5; // failed codes of one address before its ban
    int idleTimeout = 0; // seconds without input before a kick (0 = never)
    bool allowControl = true; // keyboard / mouse of the clients
    bool allowTerminal = true; // remote terminals
    bool allowWeb = true; // browsers and the android app
    bool viewOnlyDefault = false; // new sessions start in view only mode
    bool banOnTamper = true; // ban a session caught touching xdesk
    bool focusRequest = true; // a viewer paused by the focus of the host panel may ask its control back
    int focusRequestDelay = 60; // seconds before that request is granted (time to refuse / unplug)
    QString shell = ""; // shell of the remote terminals (empty = $SHELL / cmd.exe)

    /* stream */
    int fps = 60; // frames per second cap
    int quality = 70; // jpeg quality (1 - 100)
    double scale = 1.0; // scale of the frames sent (0.25 - 1.0)
    int maxHeight = 0; // frames at most this tall, the width follows (0 = the screen size): 1080, 720, 480...

    /* ui */
    QString theme = "system"; // system | dark | light
    QString language = "en"; // en | fr
    bool startMinimized = false; // tray only at start (gui)
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* names */
_cold _nodiscard QString exposure_name(const Exposure exposure); // "lan" | "tailscale" ...
_cold _nodiscard Exposure exposure_from(const QString& name); // unknown -> Lan
_cold _nodiscard QString approval_name(const Approval approval); // "untrusted" | "always" | "never"
_cold _nodiscard Approval approval_from(const QString& name); // unknown -> Untrusted

//----------------------------------------------------------------//
/* CLASS */

class Config {
    private:
        Settings _settings; // current values
        QString _path; // config.json

    public:
        // ---------- Pre-Function -------- //
        void load(void); // read config.json (written with the defaults when missing)
        void save(void) const; // write config.json (0600)

        /* folders */
        _cold _nodiscard static QString configDir(void); // ~/.config/xdesk | %APPDATA%/xdesk
        _cold _nodiscard static QString dataDir(void); // ~/.local/share/xdesk | %LOCALAPPDATA%/xdesk (certificate, devices, sessions)
        _cold _nodiscard static QString logDir(void); // <data>/logs (audit jsonl, terminal transcripts)
        _cold _nodiscard static QString executablePath(void); // absolute path of the running binary

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline Settings& settings(void)             {return this->_settings;};
        _cold _nodiscard inline const Settings& settings(void) const {return this->_settings;};
        _cold _nodiscard inline const QString& path(void) const      {return this->_path;};

        // ------------ Operator ---------- //
        Config& operator=(const Config& other) = delete;
        Config& operator=(Config&& other) = delete;

        // ---------- Constructor --------- //
        Config(void); // path from configDir(), nothing read yet
        Config(const Config& other) = delete;
        Config(Config&& other) = delete;

        // ----------- Destructor --------- //
        ~Config() = default;
};

} // namespace end
#endif /* CONFIG_H */
