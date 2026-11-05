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
##  @file Host.hpp

File Description:
##  Host side of xdesk: server, sessions, auth, rights, capture, input, terminals, bans, tamper reaction
\**************************************************************/

#ifndef HOST_H
    #define HOST_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>                          // _hot, _cold, _nodiscard
    #include "../security/IntegrityGuard.hpp"           // xdesk::security::IntegrityGuard
    #include "../security/DeviceRegistry.hpp"           // xdesk::security::DeviceRegistry
    #include "../platform/IInputInjector.hpp"           // xdesk::platform::IInputInjector
    #include "../security/PairingCode.hpp"              // xdesk::security::PairingCode
    #include "../security/Certificate.hpp"              // xdesk::security::Certificate
    #include "../platform/ITerminal.hpp"                // xdesk::platform::ITerminal
    #include "../core/AuditLog.hpp"                     // xdesk::core::AuditLog
    #include "../net/Exposure.hpp"                      // xdesk::net::Exposure
    #include "../core/Config.hpp"                       // xdesk::core::Config
    #include "../net/Server.hpp"                        // xdesk::net::Server
    #include "Session.hpp"                              // xdesk::host::Session, Permissions
    #include "Stream.hpp"                               // xdesk::host::Stream
    #include <QStringList>                              // QStringList
    #include <QJsonObject>                              // QJsonObject
    #include <QThread>                                  // QThread
    #include <QObject>                                  // QObject
    #include <QString>                                  // QString
    #include <memory>                                   // std::unique_ptr
    #include <QHash>                                    // QHash
    #include <QSet>                                     // QSet
    #include <QList>                                    // QList
    #include <QSize>                                    // QSize

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct Terminal {
    platform::ITerminal* pty = nullptr; // owned by the host
    QString session; // owner
    std::uint8_t id = 0; // id inside the session
    QByteArray line; // input typed since the last Enter (tamper check)
    QString transcript; // path of the transcript file (output + input)
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Every method is called from the UI thread (the host lives in it)
 * Local only actions (unban, trust, settings) are refused while a remote session controls the screen or has a
 * terminal: the person in front of the PC must stop them first (localActionsLocked)
*/
class Host: public QObject {
    Q_OBJECT

    private:
        core::Config& _config;
        core::AuditLog _log;
        security::Certificate _cert;
        security::DeviceRegistry _registry;
        security::PairingCode _code;
        security::IntegrityGuard _guard;
        net::Server _server;
        net::Exposure _exposure;
        Stream* _stream = nullptr; // lives in _streamThread
        QThread _streamThread;
        std::unique_ptr<platform::IInputInjector> _injector; // created when the screen size is known
        QSize _screen; // captured size
        QString _captureBackend = ""; // name, or the error ("" = idle: starts with the first viewer)
        QString _inputError = ""; // why no input backend could start
        QSet<QString> _refused; // addresses refused by the exposure (logged once)
        QHash<QString, Session*> _sessions; // id -> session (owned)
        QList<Terminal> _terminals;
        QHash<QString, int> _authFailures; // address -> failed codes
        QHash<QString, double> _scales; // session id -> scale of its frames (input / cursor mapping)
        QHash<QString, QString> _pendingProof; // session id -> server proof (hex) sent in the welcome
        QHash<QString, bool> _remember; // session id -> the client asked to be remembered
        qint64 _lastMoveLog = 0; // throttle of the mouse moves in the activity feed
        bool _panelFocused = false; // the local panel has the focus: remote input paused
        bool _running = false;

        // ---------- Pre-Function -------- //
        void onSocket_(net::WebSocket* ws, const QString& userAgent);
        void onHello_(Session* session, const QJsonObject& message);
        void onAuth_(Session* session, const QJsonObject& message);
        void onControl_(Session* session, const QJsonObject& message);
        void onInput_(Session* session, const QByteArray& message);
        void onClosed_(Session* session);
        void onTamper_(const QString& session, const QString& reason);
        void admit_(Session* session, const bool remember); // after auth (+ approval)
        void authFailed_(Session* session, const QString& reason);
        void openTerminal_(Session* session, const std::uint8_t id, const int cols, const int rows);
        void closeTerminal_(const QString& session, const std::uint8_t id);
        void closeTerminals_(const QString& session);
        void terminalInput_(Terminal& terminal, const QByteArray& data);
        void updateMarker_(void);
        void openInjector_(void); // input backend for the captured screen (logs once why it fails)
        _cold _nodiscard Permissions defaultPermissions_(const Session* session) const;

    public:
        // ---------- Pre-Function -------- //
        void start(void); // certificate, registry, server, exposure, capture, guard (throws Network / Certificate)
        void stop(void); // every session closed (bye), clean marker

        /* sessions */
        _cold _nodiscard QList<Session*> sessions(void) const;
        _cold _nodiscard Session* session(const QString& id) const;
        void approve(const QString& id, const bool accepted, const bool trust = false); // pending session
        void kick(const QString& id, const QString& reason = "Disconnected by the host");
        void ban(const QString& id, const QString& reason = "Banned by the host"); // device + address
        void setPermissions(const QString& id, const Permissions& perms);
        void kickAll(const QString& reason = "Host stopped sharing");

        /* devices (local only) */
        _cold _nodiscard bool localActionsLocked(void) const; // a remote session controls / has a terminal
        _cold _nodiscard bool unban(const QString& deviceId); // false = locked
        _cold _nodiscard bool unbanAddress(const QString& address);
        _cold _nodiscard bool setTrusted(const QString& deviceId, const bool trusted);
        _cold _nodiscard bool forget(const QString& deviceId);
        _cold _nodiscard bool applySettings(const core::Settings& settings); // save + restart what changed
        _cold void setTheme(const QString& theme); // look of the local panel only: no confirmation needed

        /* panel */
        void setPanelFocused(const bool focused); // remote input paused while the panel is used
        void rotateCode(void);

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline core::Config& config(void)                           {return this->_config;};
        _cold _nodiscard inline core::AuditLog& log(void)                            {return this->_log;};
        _cold _nodiscard inline security::DeviceRegistry& registry(void)             {return this->_registry;};
        _cold _nodiscard inline security::PairingCode& code(void)                    {return this->_code;};
        _cold _nodiscard inline net::Exposure& exposure(void)                        {return this->_exposure;};
        _cold _nodiscard inline const security::Certificate& certificate(void) const {return this->_cert;};
        _cold _nodiscard inline QSize screen(void) const                             {return this->_screen;};
        _cold _nodiscard inline const QString& captureBackend(void) const            {return this->_captureBackend;};
        _cold _nodiscard inline QString inputBackend(void) const                     {return this->_injector ? this->_injector->name() : QString("none");};
        _cold _nodiscard inline const QString& inputError(void) const                {return this->_inputError;};
        _cold _nodiscard inline bool isRunning(void) const                           {return this->_running;};

        // ------------ Operator ---------- //
        Host& operator=(const Host& other) = delete;
        Host& operator=(Host&& other) = delete;

        // ---------- Constructor --------- //
        explicit Host(core::Config& config, QObject* parent = nullptr);
        Host(const Host& other) = delete;
        Host(Host&& other) = delete;

        // ----------- Destructor --------- //
        ~Host() override;

    signals:
        void sessionAdded(xdesk::host::Session* session);
        void sessionChanged(xdesk::host::Session* session);
        void sessionRemoved(const QString& id);
        void approvalRequested(xdesk::host::Session* session); // the UI asks the local user (approve)
        void activity(const QString& session, const QString& description); // live input feed
        void terminalOutput(const QString& session, const std::uint8_t id, const QByteArray& data); // mirror for the panel
        void terminalInput(const QString& session, const std::uint8_t id, const QByteArray& data);
        void tamperDetected(const QString& session, const QString& reason);
        void stateChanged(void); // code, endpoints, capture, devices, settings
};

} // namespace end
#endif /* HOST_H */
