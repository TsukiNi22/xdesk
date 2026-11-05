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
##  Terminal tools of the TUIs: raw mode, stdin reader, size, ANSI sequences
\**************************************************************/

#ifndef ANSI_H
    #define ANSI_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <QColor>           // QColor
    #include <QTimer>           // QTimer
    #include <QSize>            // QSize

namespace xdesk::ui::tty { // namespace start
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

//----------------------------------------------------------------//
/* CLASS */

// The terminal of this process: raw mode, alternate screen, keyboard bytes, size changes
class Terminal: public QObject {
    Q_OBJECT

    private:
        QSize _size; // cols x rows
        QTimer _poll; // size polling (no signal handler needed)
        bool _raw = false;
        bool _alternate = false;
        void* _reader = nullptr; // reader thread (Windows) / socket notifier (Unix)

    public:
        // ---------- Pre-Function -------- //
        void enableRaw(void); // no echo, no line buffering, no signals (ctrl+c is a byte)
        void enterAlternate(void); // alternate screen + hidden cursor
        void setMouse(const bool enabled); // SGR mouse reporting (any motion)
        void restore(void); // everything back (called by the destructor too)
        _hot void write(const QByteArray& data);
        _cold _nodiscard QSize size(void) const; // cols x rows
        _cold _nodiscard static bool isTty(void); // stdin and stdout are terminals

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
