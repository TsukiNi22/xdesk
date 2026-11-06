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
##  @file Ansi.hpp

File Description:
##  Terminal tools of the TUIs: raw mode, keys, mouse, cell canvas drawn by changed rows, ANSI sequences
\**************************************************************/

#ifndef ANSI_H
    #define ANSI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _hot, _cold, _nodiscard
    #include "../gui/VtScreen.hpp"      // xdesk::ui::gui::Line, Cell
    #include <QStringList>              // QStringList
    #include <functional>               // std::function
    #include <QByteArray>               // QByteArray
    #include <QVector>                  // QVector
    #include <QObject>                  // QObject
    #include <QString>                  // QString
    #include <cstdint>                  // std::uint8_t
    #include <QColor>                   // QColor
    #include <QPoint>                   // QPoint
    #include <QTimer>                   // QTimer
    #include <QList>                    // QList
    #include <QRect>                    // QRect
    #include <QSize>                    // QSize

namespace xdesk::ui::tty { // namespace start
//----------------------------------------------------------------//
/* ENUM */

enum class ColorDepth {
    None, // no color: the attributes only (bold, inverse...)
    Basic, // 16 colors
    Palette, // 256 colors
    True, // 24-bit
};

//----------------------------------------------------------------//
/* STRUCT */

// What the terminal can show (TERM, COLORTERM, NO_COLOR, the locale; XDESK_TTY overrides them)
struct Capabilities {
    bool ansi = true; // cursor moves and the alternate screen (false: TERM=dumb, a console without VT)
    ColorDepth colors = ColorDepth::True;
    bool unicode = true; // box drawing, blocks and arrows (false: ascii look-alikes)
    bool mouse = true; // xterm mouse reports (SGR 1006)
};

// Look of a cell (invalid color = the default one of the terminal)
struct Style {
    QColor fg;
    QColor bg;
    bool bold = false;
    bool dim = false;
    bool underline = false;
    bool inverse = false;
    _hot _nodiscard inline bool operator==(const Style& other) const {return this->fg == other.fg && this->bg == other.bg && this->bold == other.bold && this->dim == other.dim && this->underline == other.underline && this->inverse == other.inverse;};
};

struct TextCell {
    char32_t ch = U' '; // 0 = second half of a wide character (nothing written)
    Style style;
};

// Clickable area of a frame
struct Hit {
    QRect area; // cells
    std::function<void(void)> action; // left click
};

// SGR mouse report (1006)
struct Mouse {
    int button = 0; // 0 left, 1 middle, 2 right
    int col = 0; // 0 based
    int row = 0;
    bool press = false; // false = release
    bool motion = false;
    int wheel = 0; // 1 up, -1 down, 0 none
    int raw = 0; // button byte as sent
};

//----------------------------------------------------------------//
/* PROTOTYPE */

/* sequences */
_hot _nodiscard QByteArray move(const int row, const int col); // 1 based
_hot _nodiscard QByteArray fg(const QColor& color); // truecolor
_hot _nodiscard QByteArray bg(const QColor& color);
_hot _nodiscard QByteArray reset(void);
_hot _nodiscard QByteArray bold(void);
_hot _nodiscard QByteArray dim(void);
_hot _nodiscard QByteArray inverse(void);
_hot _nodiscard QString fit(const QString& text, const int width); // cut with an ellipsis / pad with spaces
_cold _nodiscard inline QByteArray osc52(const QString& text) {return "\x1b]52;c;" + text.toUtf8().toBase64() + "\x07";}; // copy to the clipboard of the terminal (works over ssh)

/* capabilities */
_cold _nodiscard Capabilities detect_capabilities(void); // of the terminal of this process
_cold _nodiscard Capabilities parse_capabilities(const QString& term, const QString& colorterm, const QString& program, const QString& locale, const bool noColor, const QString& overrides); // TERM, COLORTERM, TERM_PROGRAM, locale, NO_COLOR set, XDESK_TTY ("ascii", "unicode", "mono", "16", "256", "truecolor", "no-mouse", "mouse", "ansi")
_hot _nodiscard char ascii_of(const char32_t ch); // ascii look-alike of a character ('?' when none)
_hot _nodiscard QByteArray sgr_color(const QColor& color, const ColorDepth depth, const bool background); // color sequence for this depth ("" for None)

/* input */
_hot _nodiscard QList<QByteArray> split_keys(const QByteArray& data); // one entry per key / mouse report / paste
_hot _nodiscard QString key_name(const QByteArray& key); // "up", "enter", "ctrl+s", "shift+tab", "f1", "paste", "mouse", or the typed text
_hot _nodiscard bool parse_mouse(const QByteArray& key, Mouse& mouse); // SGR report -> mouse
_hot _nodiscard QByteArray paste_text(const QByteArray& key); // content of a bracketed paste

/* drawing */
_hot _nodiscard QColor vt_color(const std::int32_t color); // VtScreen color -> QColor (invalid = default)
_cold _nodiscard QStringList qr_lines(const QString& text, const bool unicode = true); // QR code in half blocks (dark on light), quiet zone included (ascii: "##" per light module, one line per module)

//----------------------------------------------------------------//
/* CLASS */

// Grid of cells drawn by the TUIs (one frame), turned into rows of ANSI bytes
class Canvas {
    private:
        int _w = 0;
        int _h = 0;
        QVector<TextCell> _cells;

    public:
        // ---------- Pre-Function -------- //
        void resize(const int w, const int h); // cleared
        void clear(const Style& style = Style());
        int text(const int x, const int y, const QString& text, const Style& style, const int max = -1); // returns the column after the text (cut at max cells)
        void fill(const QRect& area, const Style& style, const char32_t ch = U' ');
        void box(const QRect& area, const QString& title, const Style& border, const Style& inside); // rounded frame
        void vtLine(const int x, const int y, const gui::Line& line, const int max); // a row of a VtScreen
        _hot _nodiscard QByteArray row(const int y, const Capabilities& caps = Capabilities()) const; // ANSI bytes of a row (starts and ends with a reset), colors / characters lowered to what the terminal shows

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline int width(void) const                 {return this->_w;};
        _cold _nodiscard inline int height(void) const                {return this->_h;};
        _hot _nodiscard inline TextCell& at(const int x, const int y) {return this->_cells[static_cast<qsizetype>(y) * this->_w + x];};
};

// The terminal of this process: raw mode, alternate screen, keyboard bytes, size changes, frames drawn by changed rows
class Terminal: public QObject {
    Q_OBJECT

    private:
        QSize _size; // cols x rows
        Capabilities _caps;
        QTimer _poll; // size polling (no signal handler needed)
        bool _raw = false;
        bool _alternate = false;
        void* _reader = nullptr; // reader thread (Windows) / socket notifier (Unix)
        QVector<QByteArray> _shown; // rows on the screen (only the changed ones are written again)

    public:
        // ---------- Pre-Function -------- //
        void enableRaw(void); // no echo, no line buffering, no signals (ctrl+c is a byte)
        void enterAlternate(void); // alternate screen + hidden cursor + bracketed paste
        void setMouse(const bool enabled, const bool motion = false); // SGR mouse reporting (clicks + wheel, every motion too when asked)
        void restore(void); // everything back (called by the destructor too)
        void present(const Canvas& canvas, const QPoint& cursor = QPoint(-1, -1)); // changed rows only (synchronized output), cursor shown there (-1 = hidden)
        _hot void write(const QByteArray& data);
        _cold _nodiscard QSize size(void) const; // cols x rows
        _cold _nodiscard static bool isTty(void); // stdin and stdout are terminals

        // ------------ Function ---------- //
        _cold inline void invalidate(void) {this->_shown.clear();}; // next present writes every row
        _cold _nodiscard inline const Capabilities& caps(void) const {return this->_caps;};

        // ---------- Constructor --------- //
        explicit Terminal(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Terminal() override;

    signals:
        void input(const QByteArray& data);
        void resized(const QSize& size);
};

} // namespace end
#endif /* ANSI_H */
