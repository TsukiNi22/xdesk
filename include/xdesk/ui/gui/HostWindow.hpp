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
##  @file HostWindow.hpp

File Description:
##  Host panel: pairing code, endpoints, live sessions, approvals, devices, logs, settings, tray
\**************************************************************/

#ifndef HOSTWINDOW_H
    #define HOSTWINDOW_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard
    #include "../../host/Host.hpp"          // xdesk::host::Host, Session
    #include "TerminalWidget.hpp"           // xdesk::ui::gui::TerminalWidget
    #include <QSystemTrayIcon>              // QSystemTrayIcon
    #include <QPlainTextEdit>               // QPlainTextEdit
    #include <QStackedWidget>               // QStackedWidget
    #include <QDoubleSpinBox>               // QDoubleSpinBox
    #include <QTableWidget>                 // QTableWidget
    #include <QProgressBar>                 // QProgressBar
    #include <QJsonObject>                  // QJsonObject
    #include <QListWidget>                  // QListWidget
    #include <QMainWindow>                  // QMainWindow
    #include <QTreeWidget>                  // QTreeWidget
    #include <QPushButton>                  // QPushButton
    #include <QTabWidget>                   // QTabWidget
    #include <QComboBox>                    // QComboBox
    #include <QLineEdit>                    // QLineEdit
    #include <QCheckBox>                    // QCheckBox
    #include <QSpinBox>                     // QSpinBox
    #include <QSlider>                      // QSlider
    #include <QLabel>                       // QLabel
    #include <QTimer>                       // QTimer
    #include <QHash>                        // QHash

namespace xdesk::ui::gui { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class HostWindow: public QMainWindow {
    Q_OBJECT

    private:
        host::Host& _host;
        QListWidget* _nav = nullptr;
        QStackedWidget* _pages = nullptr;
        QWidget* _banner = nullptr; // tamper alert
        QLabel* _bannerText = nullptr;
        QSystemTrayIcon* _tray = nullptr;
        QTimer _tick; // 1 Hz: countdown, lock state
        bool _quitting = false;
        bool _trayHintShown = false;

        /* overview */
        QLabel* _code = nullptr;
        QProgressBar* _codeBar = nullptr;
        QLabel* _codeTimer = nullptr;
        QLabel* _fingerprint = nullptr;
        QTableWidget* _endpoints = nullptr;
        QComboBox* _exposure = nullptr;
        QLabel* _exposureStatus = nullptr;
        QLabel* _statusCapture = nullptr;
        QLabel* _statusInput = nullptr;
        QLabel* _statusTls = nullptr;
        QLabel* _statusSessions = nullptr;

        /* sessions */
        QTableWidget* _sessions = nullptr;
        QWidget* _detail = nullptr;
        QLabel* _detailTitle = nullptr;
        QLabel* _detailInfo = nullptr;
        QCheckBox* _permView = nullptr;
        QCheckBox* _permControl = nullptr;
        QCheckBox* _permTerminal = nullptr;
        QCheckBox* _permClipboard = nullptr;
        QPlainTextEdit* _activity = nullptr;
        QTabWidget* _mirrorTabs = nullptr;
        QString _selected = ""; // session shown in the detail pane
        QHash<QString, QStringList> _activityLog; // session -> last lines
        QHash<QString, TerminalWidget*> _mirrors; // "session/id" -> mirror
        QHash<QString, QByteArray> _typed; // "session/id" -> line being typed

        /* devices */
        QTableWidget* _devices = nullptr;
        QTableWidget* _addresses = nullptr;
        QLabel* _lockInfo = nullptr;
        QList<QPushButton*> _localButtons; // disabled while locked

        /* logs */
        QTreeWidget* _logs = nullptr;
        QComboBox* _logLevel = nullptr;
        QComboBox* _logCategory = nullptr;
        QLineEdit* _logSearch = nullptr;

        /* settings */
        QSpinBox* _setPort = nullptr;
        QLineEdit* _setBind = nullptr;
        QComboBox* _setExposure = nullptr;
        QLineEdit* _setPublicHost = nullptr;
        QCheckBox* _setDiscovery = nullptr;
        QSpinBox* _setCodeLength = nullptr;
        QSpinBox* _setCodeLifetime = nullptr;
        QComboBox* _setApproval = nullptr;
        QSpinBox* _setMaxSessions = nullptr;
        QSpinBox* _setMaxFailures = nullptr;
        QSpinBox* _setIdle = nullptr;
        QCheckBox* _setAllowControl = nullptr;
        QCheckBox* _setAllowTerminal = nullptr;
        QCheckBox* _setAllowWeb = nullptr;
        QCheckBox* _setViewOnly = nullptr;
        QCheckBox* _setBanOnTamper = nullptr;
        QLineEdit* _setShell = nullptr;
        QSpinBox* _setFps = nullptr;
        QSlider* _setQuality = nullptr;
        QDoubleSpinBox* _setScale = nullptr;
        QComboBox* _setTheme = nullptr;
        QComboBox* _setLanguage = nullptr;
        QCheckBox* _setMinimized = nullptr;
        QPushButton* _settingsSave = nullptr;

        // ---------- Pre-Function -------- //
        _nodiscard QWidget* buildOverview_(void);
        _nodiscard QWidget* buildSessions_(void);
        _nodiscard QWidget* buildDevices_(void);
        _nodiscard QWidget* buildLogs_(void);
        _nodiscard QWidget* buildSettings_(void);
        void buildTray_(void);
        void refreshOverview_(void);
        void refreshSession_(host::Session* session);
        void refreshDetail_(void);
        void refreshDevices_(void);
        void refreshLock_(void);
        void loadSettings_(void);
        void saveSettings_(void);
        void selectSession_(const QString& id);
        void addLog_(const QJsonObject& entry);
        void filterLogs_(void);
        void pushActivity_(const QString& session, const QString& line);
        void showApproval_(host::Session* session);
        void showTamper_(const QString& session, const QString& reason);
        void quit_(void);
        _nodiscard int sessionRow_(const QString& id) const;
        _nodiscard TerminalWidget* mirror_(const QString& session, const std::uint8_t id);
        _nodiscard bool confirmPresence_(const QString& action); // typed random code

    protected:
        // ---------- Pre-Function -------- //
        void closeEvent(QCloseEvent* event) override;
        void changeEvent(QEvent* event) override;
        void showEvent(QShowEvent* event) override;

    public:
        // ---------- Constructor --------- //
        explicit HostWindow(host::Host& host, QWidget* parent = nullptr);

        // ----------- Destructor --------- //
        ~HostWindow() override = default;
};

} // namespace end
#endif /* HOSTWINDOW_H */
