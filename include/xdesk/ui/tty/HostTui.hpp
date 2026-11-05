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
##  @file HostTui.hpp

File Description:
##  Terminal panel of the host: code, endpoints, live sessions, approvals, rights, devices, logs
\**************************************************************/

#ifndef HOSTTUI_H
    #define HOSTTUI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../../host/Host.hpp"      // xdesk::host::Host
    #include "Ansi.hpp"                 // xdesk::ui::tty::Terminal
    #include <QStringList>              // QStringList
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <QTimer>                   // QTimer
    #include <QHash>                    // QHash

namespace xdesk::ui::tty { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class Page {
    Dashboard,
    Devices,
    Logs,
};

//----------------------------------------------------------------//
/* CLASS */

class HostTui: public QObject {
    Q_OBJECT

    private:
        host::Host& _host;
        Terminal _term;
        QTimer _redraw; // 5 Hz
        Page _page = Page::Dashboard;
        int _selected = 0; // row of the sessions / devices list
        QString _approval = ""; // pending session shown in the prompt
        QString _prompt = ""; // question of a typed prompt ("" = none)
        QString _promptAnswer = ""; // expected answer
        QString _promptTyped = "";
        std::function<void(void)> _promptAction; // run when the typed answer matches
        QString _flash = ""; // message of the status line
        QHash<QString, QStringList> _activity; // session -> last lines
        bool _quitAsked = false;

        // ---------- Pre-Function -------- //
        void draw_(void);
        void onInput_(const QByteArray& data);
        void ask_(const QString& question, const QString& answer, const std::function<void(void)>& action);
        _nodiscard QList<host::Session*> ordered_(void) const;

    public:
        // ---------- Constructor --------- //
        explicit HostTui(host::Host& host, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~HostTui() override = default;

    signals:
        void quit(void);
};

} // namespace end
#endif /* HOSTTUI_H */
