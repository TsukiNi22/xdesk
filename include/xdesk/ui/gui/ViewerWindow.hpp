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
##  @file ViewerWindow.hpp

File Description:
##  Native viewer: connect page, fingerprint check, remote screen canvas, inputs, terminals
\**************************************************************/

#ifndef VIEWERWINDOW_H
    #define VIEWERWINDOW_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>              // _cold, _nodiscard
    #include "../../client/Client.hpp"      // xdesk::client::Client, Options
    #include "../../platform/Clipboard.hpp" // xdesk::platform::Clipboard
    #include "TerminalWidget.hpp"           // xdesk::ui::gui::TerminalWidget
    #include <QStackedWidget>               // QStackedWidget
    #include <QListWidget>                  // QListWidget
    #include <QMainWindow>                  // QMainWindow
    #include <QPushButton>                  // QPushButton
    #include <QScrollArea>                  // QScrollArea
    #include <QTabWidget>                   // QTabWidget
    #include <QLineEdit>                    // QLineEdit
    #include <QComboBox>                    // QComboBox
    #include <QCheckBox>                    // QCheckBox
    #include <QSpinBox>                     // QSpinBox
    #include <QToolBar>                     // QToolBar
    #include <QSlider>                      // QSlider
    #include <QLabel>                       // QLabel
    #include <QTimer>                       // QTimer
    #include <QHash>                        // QHash
    #include <QSet>                         // QSet

namespace xdesk::ui::gui { // namespace start
//----------------------------------------------------------------//
/* CLASS */

// Remote screen: paints the canvas of the client, sends the inputs in frame coordinates
class ScreenView: public QWidget {
    Q_OBJECT

    private:
        client::Client& _client;
        bool _fit = true; // scale to fit | 1:1
        bool _control = false; // inputs sent
        QPoint _cursor = {-1, -1}; // remote cursor (frame coordinates)
        bool _cursorVisible = false;
        QSet<QString> _pressed; // keys down (released on focus loss)

        // ---------- Pre-Function -------- //
        _nodiscard QRect target_(void) const; // where the canvas is drawn
        void place_(void); // size of the view in its scroll area (fit: fills it, 1:1: the host screen)
        _nodiscard QPoint toFrame_(const QPointF& pos) const;

    protected:
        // ---------- Pre-Function -------- //
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void focusInEvent(QFocusEvent* event) override; // says how to get the keyboard back while controlling
        void releaseKeyboard_(void); // Ctrl+Alt+Shift: remote keys up, the focus to the next widget
        bool focusNextPrevChild(const bool next) override; // tab goes to the remote
        bool event(QEvent* event) override;

    public:
        // ---------- Pre-Function -------- //
        void setFit(const bool fit);
        void setControl(const bool control);
        void setCursor(const QPoint& position, const bool visible);
        _cold _nodiscard static QString keyCode(const QKeyEvent* event); // Qt key -> KeyboardEvent.code

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline bool isFit(void) const {return this->_fit;};

        // ---------- Constructor --------- //
        explicit ScreenView(client::Client& client, QWidget* parent = nullptr);

        // ----------- Destructor --------- //
        ~ScreenView() override = default;
};

class ViewerWindow: public QMainWindow {
    Q_OBJECT

    private:
        client::Client _client;
        QStackedWidget* _pages = nullptr; // connect | waiting | viewer | closed
        QLineEdit* _host = nullptr;
        QSpinBox* _port = nullptr;
        QLineEdit* _code = nullptr;
        QCheckBox* _remember = nullptr;
        QCheckBox* _viewOnly = nullptr;
        QListWidget* _found = nullptr; // LAN scan + recent hosts
        QPushButton* _connect = nullptr;
        QLabel* _connectError = nullptr;
        QLabel* _waiting = nullptr;
        QLabel* _closedTitle = nullptr;
        QLabel* _closedText = nullptr;
        QTabWidget* _tabs = nullptr; // screen + terminals
        ScreenView* _screen = nullptr;
        QScrollArea* _screenArea = nullptr; // the screen tab: scrolls the 1:1 view
        QToolBar* _toolbar = nullptr;
        QAction* _controlAction = nullptr;
        QAction* _clipAuto = nullptr; // shared clipboard on (default): a copy on one side lands on the other
        platform::Clipboard* _clipboard = nullptr; // clipboard of this computer
        QLabel* _latency = nullptr;
        QSlider* _quality = nullptr;
        QComboBox* _resolution = nullptr; // native, 1080p ... 144p (height asked to the host)
        QLabel* _received = nullptr; // resolution of the frames received
        QTimer _viewTimer; // the view sent once the scroll / resize settles
        QRectF _viewSent = QRectF(0.0, 0.0, 1.0, 1.0); // part of the host screen the host was told
        QString _qualitySent = ""; // last quality message (not sent again unchanged)
        QHash<std::uint8_t, TerminalWidget*> _terminals;

        // ---------- Pre-Function -------- //
        void addClipboard_(void); // clipboard button of the toolbar (automatic sync + one way actions)
        void updateView_(void); // tells the host the part of its screen shown (1:1 larger than the window), the resolution worth sending
        bool eventFilter(QObject* watched, QEvent* event) override; // resizes of the screen area
        _nodiscard QWidget* buildConnect_(void);
        _nodiscard QWidget* buildViewer_(void);
        void scan_(void);
        void loadRecent_(void);
        void startConnection_(void);
        void openTerminal_(void);
        void sendQuality_(void); // quality + scale of the asked resolution
        void onState_(const client::State state);
        Q_INVOKABLE void retheme_(void); // icons drawn again in the new colours (called by Theme::apply)

    public:
        // ---------- Pre-Function -------- //
        void connectNow(const client::Options& options); // skip the connect page

        // ---------- Constructor --------- //
        explicit ViewerWindow(QWidget* parent = nullptr);

        // ----------- Destructor --------- //
        ~ViewerWindow() override = default;
};

} // namespace end
#endif /* VIEWERWINDOW_H */
