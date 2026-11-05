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
##  @file VtScreen.hpp

File Description:
##  VT100 / xterm subset emulator: parser, screen grid, scrollback (no painting)
\**************************************************************/

#ifndef VTSCREEN_H
    #define VTSCREEN_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <QVector>          // QVector
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <cstdint>          // std::int32_t, std::uint8_t
    #include <QPoint>           // QPoint
    #include <QList>            // QList

    //----------------------------------------------------------------//
    /* DEFINE */

    /* colors */
    #define VT_DEFAULT_COLOR -1 // default foreground / background of the theme
    #define VT_TRUECOLOR 0x1000000 // VT_TRUECOLOR + 0xRRGGBB = 24 bits color (0 - 255 = palette)

    /* limits */
    #define VT_SCROLLBACK 5000 // lines kept above the screen
    #define VT_MAX_PARAMS 32 // csi parameters

namespace xdesk::ui::gui { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum CellAttr: std::uint8_t {
    Bold = 1,
    Italic = 2,
    Underline = 4,
    Inverse = 8,
    Dim = 16,
    Strike = 32,
    Wide = 64, // first half of a double width character
    WideTail = 128, // second half (nothing drawn)
};

//----------------------------------------------------------------//
/* STRUCT */

struct Cell {
    char32_t ch = U' ';
    std::int32_t fg = VT_DEFAULT_COLOR; // VT_DEFAULT_COLOR | palette index | VT_TRUECOLOR + rgb
    std::int32_t bg = VT_DEFAULT_COLOR;
    std::uint8_t attr = 0; // CellAttr bits

    _hot _nodiscard inline bool sameStyle(const Cell& other) const {return this->fg == other.fg && this->bg == other.bg && (this->attr & 0x3F) == (other.attr & 0x3F);};
};

//----------------------------------------------------------------//
/* TYPEDEF */

using Line = QVector<xdesk::ui::gui::Cell>;

//----------------------------------------------------------------//
/* CLASS */

class VtScreen: public QObject {
    Q_OBJECT

    private:
        /* grid */
        int _cols = 80;
        int _rows = 24;
        QVector<Line> _main; // primary screen
        QVector<Line> _alt; // alternate screen (1049)
        QList<Line> _scrollback; // lines scrolled out of the primary screen (oldest first)
        bool _altActive = false;

        /* cursor */
        int _x = 0;
        int _y = 0;
        int _savedX = 0;
        int _savedY = 0;
        Cell _pen; // current style (ch unused)
        Cell _savedPen;
        bool _wrapPending = false; // last column written: wrap on the next character

        /* modes */
        int _top = 0; // scrolling region (DECSTBM), inclusive
        int _bottom = 23;
        bool _originMode = false;
        bool _autoWrap = true;
        bool _insertMode = false;
        bool _appCursor = false; // DECCKM
        bool _appKeypad = false;
        bool _bracketedPaste = false;
        bool _cursorVisible = true;
        int _mouseMode = 0; // 0 none, 1000 click, 1002 drag, 1003 any
        bool _mouseSgr = false; // 1006
        bool _lineDrawing = false; // G0 = DEC special graphics
        QVector<bool> _tabs;
        QString _title = "";

        /* parser */
        enum class Parse {Ground, Escape, Csi, Osc, OscEscape, Charset, Ignore};
        Parse _state = Parse::Ground;
        QByteArray _params; // csi parameters text
        QByteArray _intermediate; // csi private marker / intermediates
        QByteArray _osc; // osc text
        char32_t _utf8 = 0; // codepoint being decoded
        int _utf8Left = 0; // continuation bytes expected

        // ---------- Pre-Function -------- //
        _hot void put_(const char32_t ch);
        _hot void control_(const char c);
        void escape_(const char c);
        void csi_(const char final);
        void osc_(void);
        void sgr_(const QVector<int>& params);
        void privateMode_(const QVector<int>& params, const bool set);
        void lineFeed_(void);
        void scrollUp_(const int top, const int bottom, const int count);
        void scrollDown_(const int top, const int bottom, const int count);
        void eraseInLine_(const int mode);
        void eraseInDisplay_(const int mode);
        void clamp_(void);
        _nodiscard Line blankLine_(void) const;
        _nodiscard QVector<Line>& screen_(void);
        _nodiscard static int charWidth_(const char32_t ch);

    public:
        // ---------- Pre-Function -------- //
        _hot void feed(const QByteArray& data); // pty output
        void resize(const int cols, const int rows); // truncate / pad, no reflow
        void reset(void); // RIS
        _cold _nodiscard const Line& line(const int row, const int scroll = 0) const; // row of the view scrolled up by scroll lines
        _cold _nodiscard QString text(const QPoint& from, const QPoint& to, const int scroll = 0) const; // selection text (view coordinates)

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int cols(void) const             {return this->_cols;};
        _cold _nodiscard inline int rows(void) const             {return this->_rows;};
        _cold _nodiscard inline int scrollbackSize(void) const   {return this->_altActive ? 0 : static_cast<int>(this->_scrollback.size());};
        _cold _nodiscard inline QPoint cursor(void) const        {return QPoint(this->_x, this->_y);};
        _cold _nodiscard inline bool cursorVisible(void) const   {return this->_cursorVisible;};
        _cold _nodiscard inline bool appCursor(void) const       {return this->_appCursor;};
        _cold _nodiscard inline bool bracketedPaste(void) const  {return this->_bracketedPaste;};
        _cold _nodiscard inline int mouseMode(void) const        {return this->_mouseMode;};
        _cold _nodiscard inline bool mouseSgr(void) const        {return this->_mouseSgr;};
        _cold _nodiscard inline const QString& title(void) const {return this->_title;};

        // ---------- Constructor --------- //
        explicit VtScreen(const int cols = 80, const int rows = 24, QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~VtScreen() override = default;

    signals:
        void changed(void); // something to repaint
        void titleChanged(const QString& title);
        void bell(void);
        void reply(const QByteArray& data); // answers to send back to the pty (DSR, DA)
};

} // namespace end
#endif /* VTSCREEN_H */
