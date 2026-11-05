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
    #include "TerminalWidget.hpp"           // xdesk::ui::gui::TerminalWidget
    #include <QStackedWidget>               // QStackedWidget
    #include <QListWidget>                  // QListWidget
    #include <QMainWindow>                  // QMainWindow
    #include <QPushButton>                  // QPushButton
    #include <QTabWidget>                   // QTabWidget
    #include <QLineEdit>                    // QLineEdit
    #include <QCheckBox>                    // QCheckBox
    #include <QSpinBox>                     // QSpinBox
    #include <QToolBar>                     // QToolBar
    #include <QSlider>                      // QSlider
    #include <QLabel>                       // QLabel
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
        QToolBar* _toolbar = nullptr;
        QAction* _controlAction = nullptr;
        QLabel* _latency = nullptr;
        QSlider* _quality = nullptr;
        QHash<std::uint8_t, TerminalWidget*> _terminals;

        // ---------- Pre-Function -------- //
        _nodiscard QWidget* buildConnect_(void);
        _nodiscard QWidget* buildViewer_(void);
        void scan_(void);
        void loadRecent_(void);
        void startConnection_(void);
        void openTerminal_(void);
        void onState_(const client::State state);

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
