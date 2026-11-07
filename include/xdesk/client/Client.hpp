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
##  @file Client.hpp

File Description:
##  Native client of xdesk (Qt viewer and terminal client): connection, pinning, auth, decoding, inputs
\**************************************************************/

#ifndef CLIENT_H
    #define CLIENT_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _hot, _cold, _nodiscard
    #include "../net/WebSocket.hpp"     // xdesk::net::WebSocket
    #include "../platform/H264.hpp"     // xdesk::platform::H264Decoder
    #include "../net/Protocol.hpp"      // xdesk::net::ClientKind, xdesk::net::MouseButton
    #include "UdpLink.hpp"              // xdesk::client::UdpLink
    #include <QJsonObject>              // QJsonObject
    #include <QByteArray>               // QByteArray
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <cstdint>                  // std::uint8_t
    #include <QVector>                  // QVector
    #include <QTimer>                   // QTimer
    #include <QImage>                   // QImage
    #include <QPoint>                   // QPoint
    #include <QSet>                     // QSet

namespace xdesk::client { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class State {
    Idle,
    Connecting, // tcp + tls + websocket
    Verifying, // unknown / changed fingerprint: waiting acceptFingerprint
    Authenticating, // challenge received, proof sent
    Pending, // waiting the approval of the host
    Active,
    Closed,
};

//----------------------------------------------------------------//
/* STRUCT */

struct Options {
    QString host; // address / domain
    quint16 port = 47800;
    QString code = ""; // pairing code (empty: trusted device secret)
    net::ClientKind kind = net::ClientKind::Gui;
    QString name = ""; // device name (default: hostname - os)
    bool remember = true; // ask to be a trusted device
    bool viewOnly = false; // never send inputs
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * known_hosts.json (<config>/known_hosts.json, 0600) = {"host:port": {"fingerprint": hex, "device": id, "secret": hex}}
 * First connection: the fingerprint is shown and must be accepted (TOFU); a changed one is refused unless accepted again
*/
class Client: public QObject {
    Q_OBJECT

    private:
        Options _options;
        net::WebSocket* _ws = nullptr;
        State _state = State::Idle;
        QByteArray _fingerprint; // sha256 of the host certificate
        QByteArray _clientNonce;
        QJsonObject _challenge;
        QJsonObject _perms; // {view, control, terminal, clipboard}
        QString _hostName = "";
        QString _sessionId = "";
        QImage _canvas; // composed screen
        QSize _hostScreen; // captured size of the host (resolution presets of the viewer)
        QTimer _ping; // latency every second
        int _latency = -1;
        UdpLink _udp; // video over encrypted udp when the host offers it
        QVector<std::uint32_t> _tiles; // id of the frame each 64 x 64 tile comes from (udp frames complete in any order: an older one never covers a newer tile)
        int _tileColumns = 0;
        platform::H264Decoder _video; // H.264 frames of the large moves (when openh264 is there)
        bool _videoBroken = false; // a video frame could not be decoded: the next ones wait for a key frame (asked with a refresh)
        QSet<std::uint8_t> _openTerminals; // ids in use: a new terminal takes the smallest free one (1 again after a close or a reconnection)

        // ---------- Pre-Function -------- //
        void onOpened_(void);
        void onText_(const QString& text);
        void onBinary_(const QByteArray& data);
        void sendHello_(void);
        void sendAuth_(void);
        void setState_(const State state);
        void decodeFrame_(const QByteArray& data, const bool udp = false); // udp: already acked by the link
        void decodeVideo_(const QByteArray& data, const bool udp = false); // one H.264 frame of the whole screen
        void fitCanvas_(const int width, const int height); // a black canvas of a new size, no tile drawn yet

    public:
        // ---------- Pre-Function -------- //
        void connectToHost(const Options& options);
        void acceptFingerprint(const bool accepted, const bool remember = true); // Verifying -> go on / close
        void disconnectFromHost(void);

        /* input */
        _hot void mouseMove(const int x, const int y);
        _hot void mouseButton(const net::MouseButton button, const bool down);
        _hot void mouseWheel(const int dx, const int dy);
        _hot void key(const QString& code, const bool down);
        _hot void text(const QString& text);
        void setQuality(const int quality, const double scale, const int fps);
        void requestMode(const QString& mode); // view | control
        void setPaused(const bool paused); // no frame while the screen is not shown (terminal tab): the terminal bytes never wait behind frames

        /* terminal */
        _cold _nodiscard std::uint8_t openTerminal(const int cols, const int rows);
        _hot void terminalInput(const std::uint8_t id, const QByteArray& data);
        void resizeTerminal(const std::uint8_t id, const int cols, const int rows);
        void closeTerminal(const std::uint8_t id);

        /* known hosts */
        _cold _nodiscard static QString knownHostsPath(void);

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline bool overUdp(void) const                   {return this->_udp.isActive();}; // the video comes over udp
        _cold _nodiscard inline State state(void) const                    {return this->_state;};
        _cold _nodiscard inline const QByteArray& fingerprint(void) const  {return this->_fingerprint;};
        _cold _nodiscard inline const QString& hostName(void) const        {return this->_hostName;};
        _cold _nodiscard inline const QJsonObject& permissions(void) const {return this->_perms;};
        _cold _nodiscard inline const QImage& canvas(void) const           {return this->_canvas;};
        _cold _nodiscard inline QSize hostScreen(void) const               {return this->_hostScreen;};
        _cold _nodiscard inline int latency(void) const                    {return this->_latency;};
        _cold _nodiscard inline const Options& options(void) const         {return this->_options;};

        // ---------- Constructor --------- //
        explicit Client(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Client() override;

    signals:
        void stateChanged(xdesk::client::State state);
        void fingerprintCheck(const QString& fingerprint, const bool changed); // answer with acceptFingerprint
        void frameUpdated(const QRect& dirty); // canvas changed (dirty = union of the tiles)
        void cursorMoved(const QPoint& position, const bool visible);
        void permissionsChanged(const QJsonObject& perms);
        void terminalOutput(const std::uint8_t id, const QByteArray& data);
        void terminalClosed(const std::uint8_t id, const int code);
        void latencyChanged(const int milliseconds);
        void hostScreenChanged(const QSize& size); // captured size of the host (0 x 0 until its capture starts)
        void message(const QString& text); // info for the user
        void closed(const QString& reason, const bool banned);
};

} // namespace end
#endif /* CLIENT_H */
