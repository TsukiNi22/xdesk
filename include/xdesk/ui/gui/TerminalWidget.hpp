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
##  @file TerminalWidget.hpp

File Description:
##  Terminal widget: paints a VtScreen, keyboard to escape sequences, selection, copy / paste, scrollback
\**************************************************************/

#ifndef TERMINALWIDGET_H
    #define TERMINALWIDGET_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include "VtScreen.hpp"     // xdesk::ui::gui::VtScreen
    #include <QScrollBar>       // QScrollBar
    #include <QByteArray>       // QByteArray
    #include <QWidget>          // QWidget
    #include <QPoint>           // QPoint
    #include <QFont>            // QFont

namespace xdesk::ui::gui { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class TerminalWidget: public QWidget {
    Q_OBJECT

    private:
        VtScreen _screen;
        QScrollBar* _scrollBar = nullptr;
        QFont _font;
        int _cellWidth = 8;
        int _cellHeight = 16;
        int _ascent = 12;
        int _scroll = 0; // lines scrolled up into the scrollback
        bool _readOnly = false; // mirror of a remote session (host panel)
        bool _selecting = false;
        QPoint _selFrom = {-1, -1}; // view cells
        QPoint _selTo = {-1, -1};
        bool _repaintQueued = false;

        // ---------- Pre-Function -------- //
        void updateMetrics_(void);
        void updateScrollBar_(void);
        _nodiscard QPoint cellAt_(const QPoint& pos) const;
        _nodiscard bool selected_(const int x, const int y) const;
        _nodiscard QColor color_(const std::int32_t color, const bool foreground) const;
        void sendMouse_(QMouseEvent* event, const int button, const bool press, const bool motion);

    protected:
        // ---------- Pre-Function -------- //
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseDoubleClickEvent(QMouseEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        bool focusNextPrevChild(const bool next) override; // tab goes to the terminal
        void inputMethodEvent(QInputMethodEvent* event) override;

    public:
        // ---------- Pre-Function -------- //
        void feed(const QByteArray& data);
        void setReadOnly(const bool read_only);
        void setGridSize(const int cols, const int rows); // mirror: size of the remote terminal
        void copySelection(void);
        void paste(void);
        void clear(void);
        _cold _nodiscard static QByteArray keyToBytes(const QKeyEvent* event, const bool app_cursor); // escape sequence of a key

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline VtScreen& screen(void) {return this->_screen;};
        _cold _nodiscard inline int cols(void) const   {return this->_screen.cols();};
        _cold _nodiscard inline int rows(void) const   {return this->_screen.rows();};

        // ---------- Constructor --------- //
        explicit TerminalWidget(QWidget* parent = nullptr);

        // ----------- Destructor --------- //
        ~TerminalWidget() override = default;

    signals:
        void input(const QByteArray& data); // bytes for the pty
        void sizeChanged(const int cols, const int rows);
        void titleChanged(const QString& title);
};

} // namespace end
#endif /* TERMINALWIDGET_H */
