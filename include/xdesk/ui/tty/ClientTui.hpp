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
##  Terminal client: remote shell passthrough, or the remote screen drawn with truecolor half blocks
\**************************************************************/

#ifndef CLIENTTUI_H
    #define CLIENTTUI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../../client/Client.hpp"  // xdesk::client::Client
    #include "Ansi.hpp"                 // xdesk::ui::tty::Terminal
    #include <QElapsedTimer>            // QElapsedTimer
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <QTimer>                   // QTimer

namespace xdesk::ui::tty { // namespace start
//----------------------------------------------------------------//
/* CLASS */

/*
 * Keys: everything goes to the remote, except the prefix ctrl+] followed by
 *   q quit | s screen / terminal view | c control on / off (screen view) | ctrl+] send a real ctrl+]
*/
class ClientTui: public QObject {
    Q_OBJECT

    private:
        client::Client _client;
        Terminal _term;
        bool _screen = false; // screen view instead of the terminal
        bool _control = false; // screen view: inputs sent
        bool _prefix = false; // ctrl+] typed
        std::uint8_t _terminal = 0; // remote terminal id (0 = not opened)
        QString _line = ""; // fingerprint answer typed
        bool _verifying = false;
        QTimer _frameTimer; // screen view: 15 fps max
        bool _dirty = false;
        int _exitCode = 0;

        // ---------- Pre-Function -------- //
        void onInput_(const QByteArray& data);
        void screenInput_(const QByteArray& data);
        void drawScreen_(void);
        void status_(const QString& text);
        void finish_(const QString& reason, const int code);

    public:
        // ---------- Pre-Function -------- //
        void start(const client::Options& options, const bool screen);

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
