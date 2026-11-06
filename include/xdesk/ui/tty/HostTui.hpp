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
##  Terminal panel of the host: everything of the graphical panel (overview, sessions, devices, logs, settings) with keys and mouse
\**************************************************************/

#ifndef HOSTTUI_H
    #define HOSTTUI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _cold, _nodiscard
    #include "../../host/Host.hpp"      // xdesk::host::Host, Session
    #include "../gui/VtScreen.hpp"      // xdesk::ui::gui::VtScreen
    #include "Ansi.hpp"                 // xdesk::ui::tty::Terminal, Canvas, Style
    #include <QJsonObject>              // QJsonObject
    #include <QStringList>              // QStringList
    #include <functional>               // std::function
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <QTimer>                   // QTimer
    #include <QHash>                    // QHash
    #include <QList>                    // QList

namespace xdesk::ui::tty { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class Page {
    Overview,
    Sessions,
    Devices,
    Logs,
    Settings,
};

enum class Modal {
    None,
    Help, // keys of every page
    Text, // lines (log entry, QR code)
    Input, // a typed value (search, setting)
    Confirm, // y / n, or a word to type
    Mirror, // remote terminal of a session, read only
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * Keys (? shows them all): 1-5 / tab pages, arrows select, enter opens, q quit
 * Mouse: click on the tabs, the rows and the buttons of the bottom bar, wheel scrolls
*/
class HostTui: public QObject {
    Q_OBJECT

    private:
        host::Host& _host;
        Terminal _term;
        Canvas _canvas;
        QTimer _redraw; // 4 Hz (countdown, stats) + on every event
        QList<Hit> _hits; // clickable areas of the last frame
        Page _page = Page::Overview;
        QString _flash = ""; // message of the status line
        qint64 _flashAt = 0; // ms
        bool _flashBad = false;
        QString _tamper = ""; // red banner until dismissed
        QString _approval = ""; // pending session asking (overlay)

        /* selections */
        int _endpoint = 0;
        int _session = 0;
        int _device = 0; // devices, then the banned addresses
        int _field = 0; // settings row
        int _logRow = -1; // selected entry of the filtered list (-1 = follow the newest)

        /* sessions */
        QHash<QString, QStringList> _activity; // session -> last lines
        QHash<QString, gui::VtScreen*> _mirrors; // "session/id" -> remote terminal
        QHash<QString, QByteArray> _typed; // "session/id" -> line being typed
        QString _mirror = ""; // mirror shown
        int _mirrorScroll = 0;

        /* logs */
        QList<QJsonObject> _logs; // newest last
        int _logLevel = 0; // index of LOG_LEVELS
        int _logCategory = 0; // index of LOG_CATEGORIES
        QString _logSearch = "";

        /* settings */
        core::Settings _draft; // edited values (saved with s)

        /* modal */
        Modal _modal = Modal::None;
        QString _modalTitle = "";
        QStringList _modalLines;
        int _modalScroll = 0;
        bool _qrApp = true; // QR modal: app link | browser link
        QString _input = ""; // typed text
        std::function<void(const QString&)> _inputDone;
        QString _confirmWord = ""; // "" = y / n
        std::function<void(void)> _confirmAction;

        // ---------- Pre-Function -------- //
        void draw_(void);
        void drawHeader_(void);
        void drawOverview_(const QRect& area);
        void drawSessions_(const QRect& area);
        void drawDevices_(const QRect& area);
        void drawLogs_(const QRect& area);
        void drawSettings_(const QRect& area);
        void drawFooter_(void);
        void drawModal_(void);
        void drawApproval_(void);
        void onInput_(const QByteArray& data);
        void onKey_(const QString& key, const QByteArray& raw);
        void onMouse_(const Mouse& mouse);
        void pageKey_(const QString& key);
        void move_(const int delta); // selection of the page
        void flash_(const QString& text, const bool bad = false);
        void ask_(const QString& title, const QString& initial, const std::function<void(const QString&)>& done);
        void confirm_(const QString& question, const QString& word, const std::function<void(void)>& action); // word "" = y / n
        void showText_(const QString& title, const QStringList& lines);
        void showQr_(void); // QR code of the selected address
        void copy_(const QString& text, const QString& what); // OSC 52
        void toggleSharing_(void);
        void setExposure_(const int delta);
        void saveSettings_(void);
        void changeField_(const int delta, const bool edit); // ← → / enter on a setting
        void addLog_(const QJsonObject& entry);
        QRect card_(const QRect& area, const QString& title, const QString& note = ""); // card of the panel (returns its inside)
        void select_(const QRect& row); // selected row of a list
        _nodiscard QList<host::Session*> ordered_(void) const;
        _nodiscard host::Session* currentSession_(void) const;
        _nodiscard QList<QJsonObject> filteredLogs_(void) const;
        _nodiscard gui::VtScreen* mirror_(const QString& session, const std::uint8_t id);
        _nodiscard QStringList mirrorsOf_(const QString& session) const; // keys, by id
        _nodiscard bool locked_(void); // flash when locked

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
