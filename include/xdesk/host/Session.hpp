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
##  @file Session.hpp

File Description:
##  One connected client on the host: protocol state, rights, statistics, live activity
\**************************************************************/

#ifndef SESSION_H
    #define SESSION_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _hot, _cold, _nodiscard
    #include "../net/WebSocket.hpp"     // xdesk::net::WebSocket
    #include "../net/Protocol.hpp"      // xdesk::net::ClientKind, xdesk::net::Binary
    #include <QElapsedTimer>            // QElapsedTimer
    #include <QHostAddress>             // QHostAddress
    #include <QJsonObject>              // QJsonObject
    #include <QByteArray>               // QByteArray
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <cstdint>                  // std::uint8_t
    #include <QTimer>                   // QTimer

namespace xdesk::host { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class State {
    Handshake, // waiting hello / auth
    Pending, // waiting the local approval
    Active, // streaming
    Closed,
};

//----------------------------------------------------------------//
/* STRUCT */

struct Permissions {
    bool view = true; // screen stream
    bool control = false; // keyboard / mouse
    bool terminal = false; // remote terminals
    bool clipboard = false; // clipboard sync
};

struct Stats {
    qint64 bytesIn = 0;
    qint64 bytesOut = 0;
    qint64 frames = 0; // frames sent
    qint64 inputs = 0; // input events received
    int fps = 0; // frames sent the last second
    int latency = -1; // round trip ms (ping), -1 = unknown
    int kbps = 0; // outgoing kbit/s the last second
};

//----------------------------------------------------------------//
/* CLASS */

class Session: public QObject {
    Q_OBJECT

    private:
        net::WebSocket* _ws = nullptr; // owned
        QString _sid; // random session id (not _id: moc uses that name)
        State _state = State::Handshake;
        net::ClientKind _kind = net::ClientKind::Web;
        QString _name = "[None]"; // device name sent by the client
        QString _os = "[None]";
        QString _userAgent = "";
        QString _address; // peer ip
        QString _deviceId = ""; // registry id once authenticated
        QString _mode = "view"; // view | control | terminal (what the client is doing now)
        QByteArray _serverNonce; // challenge
        QByteArray _clientNonce;
        Permissions _perms;
        Stats _stats;
        qint64 _since = 0; // ms
        qint64 _lastInput = 0; // ms
        QString _lastActivity = ""; // last human readable input
        QTimer _tick; // stats + ping every second
        qint64 _secondBytes = 0; // bytes sent this second
        int _secondFrames = 0; // frames sent this second
        bool _controlPaused = false; // the host panel has the focus: injected input ignored

        // ---------- Pre-Function -------- //
        void onText_(const QString& text);
        void onBinary_(const QByteArray& data);
        void onTick_(void);

    public:
        // ---------- Pre-Function -------- //
        _hot void sendJson(const QJsonObject& object);
        _hot void sendBinary(const QByteArray& data, const bool frame = false);
        _hot void countFrame(const qint64 bytes); // a frame of this session sent on another path (lane): its stats
        void sendChallenge(const QString& hostName, const QByteArray& salt, const bool knownDevice);
        void accept(const Permissions& perms, const QJsonObject& extra); // welcome
        void deny(const QString& reason); // auth refused, closed
        void kick(const QString& reason, const bool banned = false); // bye, closed
        void setPermissions(const Permissions& perms); // perms message
        void setState(const State state);
        void setActivity(const QString& activity); // live feed + lastInput
        _cold _nodiscard QJsonObject describe(void) const; // for the UIs / logs
        _hot _nodiscard qint64 pendingBytes(void) const;

        // ------------ Function ---------- //
        /* setter */
        _cold inline void setDeviceId(const QString& id)      {this->_deviceId = id;};
        _cold inline void setMode(const QString& mode)        {this->_mode = mode; emit this->changed(this);};
        _cold inline void setControlPaused(const bool paused) {this->_controlPaused = paused;};

        /* getter */
        _cold _nodiscard inline const QString& id(void) const              {return this->_sid;};
        _cold _nodiscard inline State state(void) const                    {return this->_state;};
        _cold _nodiscard inline net::ClientKind kind(void) const           {return this->_kind;};
        _cold _nodiscard inline const QString& name(void) const            {return this->_name;};
        _cold _nodiscard inline const QString& os(void) const              {return this->_os;};
        _cold _nodiscard inline const QString& address(void) const         {return this->_address;};
        _cold _nodiscard inline QHostAddress peer(void) const              {return this->_ws ? this->_ws->peerAddress() : QHostAddress();}; // address of the socket itself (loopback for the tunnels)
        _cold _nodiscard inline const QString& deviceId(void) const        {return this->_deviceId;};
        _cold _nodiscard inline const QString& mode(void) const            {return this->_mode;};
        _cold _nodiscard inline const QString& userAgent(void) const       {return this->_userAgent;};
        _cold _nodiscard inline const Permissions& permissions(void) const {return this->_perms;};
        _cold _nodiscard inline const Stats& stats(void) const             {return this->_stats;};
        _cold _nodiscard inline const QByteArray& serverNonce(void) const  {return this->_serverNonce;};
        _cold _nodiscard inline const QByteArray& clientNonce(void) const  {return this->_clientNonce;};
        _cold _nodiscard inline qint64 since(void) const                   {return this->_since;};
        _cold _nodiscard inline qint64 lastInput(void) const               {return this->_lastInput;};
        _cold _nodiscard inline const QString& lastActivity(void) const    {return this->_lastActivity;};
        _cold _nodiscard inline bool isControlPaused(void) const           {return this->_controlPaused;};

        // ---------- Constructor --------- //
        Session(net::WebSocket* ws, const QString& userAgent, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Session() override;

    signals:
        /* protocol (handled by the host) */
        void hello(xdesk::host::Session* session, const QJsonObject& message);
        void auth(xdesk::host::Session* session, const QJsonObject& message);
        void control(xdesk::host::Session* session, const QJsonObject& message); // every other json message once active
        void input(xdesk::host::Session* session, const QByteArray& message); // Binary input / term data / ack once active

        /* state */
        void changed(xdesk::host::Session* session); // state, rights, stats (1 Hz), mode
        void activity(xdesk::host::Session* session, const QString& description); // live feed of the inputs
        void closed(xdesk::host::Session* session);
};

} // namespace end
#endif /* SESSION_H */
