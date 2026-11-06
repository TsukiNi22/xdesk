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
##  @file ClientTui.hpp

File Description:
##  Terminal client: connect page, remote terminals in tabs and the remote screen in truecolor half blocks
\**************************************************************/

#ifndef CLIENTTUI_H
    #define CLIENTTUI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../../client/Client.hpp"  // xdesk::client::Client, Options
    #include "../gui/VtScreen.hpp"      // xdesk::ui::gui::VtScreen
    #include "Ansi.hpp"                 // xdesk::ui::tty::Terminal, Canvas, Hit
    #include <QJsonObject>              // QJsonObject
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <cstdint>                  // std::uint8_t
    #include <QTimer>                   // QTimer
    #include <QHash>                    // QHash
    #include <QList>                    // QList
    #include <QRect>                    // QRect

namespace xdesk::ui::tty { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class ClientView {
    Connect, // address, code, known and found hosts
    Status, // connecting, fingerprint check, waiting for the approval
    Session, // tabs: screen + terminals
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Session keys: everything goes to the remote, except the prefix ctrl+] followed by
 *   s screen | t new terminal | 1-9 / n / p tabs | w close the terminal | c control | d ctrl+alt+del
 *   + / - quality | f frame rate | pgup / pgdn scroll back | q disconnect | ? help | ctrl+] a real ctrl+]
 * Mouse: the tabs and the buttons of the bottom bar, the wheel scrolls back (or goes to the remote program)
*/
class ClientTui: public QObject {
    Q_OBJECT

    private:
        client::Client _client;
        client::Options _options;
        Terminal _term;
        Canvas _canvas;
        QTimer _frameTimer; // 60 Hz at most, only when something changed
        QList<Hit> _hits;
        ClientView _view = ClientView::Status;
        bool _dirty = true;
        bool _fromConnectPage = false; // a closed session goes back to it
        int _exitCode = 0;

        /* connect page */
        QList<QJsonObject> _hosts; // {label, host, port, trusted, found}
        int _hostRow = 0;
        int _focus = 0; // 0 hosts, 1 address, 2 port, 3 code, 4 view only, 5 connect
        QString _formHost = "";
        QString _formPort = "";
        QString _formCode = "";
        bool _formViewOnly = false;
        bool _scanning = false;
        QString _error = "";

        /* status */
        QString _status = "";
        QString _fingerprint = ""; // shown while it waits for an answer
        bool _fingerprintChanged = false;
        QString _typed = ""; // "yes" typed for a changed identity

        /* session */
        int _tab = 0; // 0 = screen, i = terminal i of _terminals
        QList<std::uint8_t> _terminals; // opened, in tab order
        QHash<std::uint8_t, gui::VtScreen*> _vts;
        QHash<std::uint8_t, int> _exited; // id -> code
        int _scroll = 0; // lines scrolled back in the terminal tab
        bool _prefix = false; // ctrl+] typed
        bool _control = false; // screen tab: inputs sent
        bool _help = false;
        bool _startScreen = false; // --screen
        int _quality = 55;
        int _fps = 15;
        QRect _picture; // cells where the screen is drawn
        QPoint _cursor = {-1, -1}; // remote cursor (frame pixels)
        bool _cursorVisible = false;
        bool _mouseMotion = false; // motion reports asked to the terminal
        bool _paused = false;
        QString _note = ""; // message of the bottom bar
        qint64 _noteAt = 0;

        // ---------- Pre-Function -------- //
        void draw_(void);
        void drawConnect_(void);
        void drawStatus_(void);
        void drawSession_(void);
        void drawBar_(void);
        void drawHelp_(void);
        void onInput_(const QByteArray& data);
        void connectKey_(const QString& key, const QByteArray& raw);
        void statusKey_(const QString& key, const QByteArray& raw);
        void sessionKey_(const QString& key, const QByteArray& raw);
        void prefixKey_(const QString& key);
        void screenKey_(const QString& key, const QByteArray& raw);
        void screenMouse_(const Mouse& mouse);
        void terminalMouse_(const Mouse& mouse, const QByteArray& raw);
        void loadHosts_(void);
        void scan_(void);
        void connectNow_(void);
        void openTerminal_(void);
        void closeTab_(void);
        void selectTab_(const int tab);
        void sendQuality_(void); // quality, frame rate and the scale the terminal needs
        void note_(const QString& text);
        void finish_(const QString& reason, const int code);
        _nodiscard gui::VtScreen* currentVt_(void) const; // nullptr on the screen tab

    public:
        // ---------- Pre-Function -------- //
        void start(const client::Options& options, const bool screen); // no host = the connect page

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int exitCode(void) const {return this->_exitCode;};

        // ---------- Constructor --------- //
        explicit ClientTui(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~ClientTui() override = default;

    signals:
        void quit(void);
};

} // namespace end
#endif /* CLIENTTUI_H */
