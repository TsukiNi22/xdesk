/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 21/11/2026 by @author Tsukini

File Name:
##  @file Clipboard.hpp

File Description:
##  Text clipboard of this computer, watched and set without the focus (Qt, or the X11 bridge of Xwayland on Wayland)
\**************************************************************/

#ifndef CLIPBOARD_H
    #define CLIPBOARD_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <memory>           // std::unique_ptr

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

struct X11Clipboard; // X11 bridge (only in the .cpp)

/*
 * A Wayland client only sees / owns the clipboard while it has the focus: there, the clipboard goes through the X11
 * selection of Xwayland (GNOME, KDE and wlroots keep both in sync), libX11 + libXfixes loaded at run time
 * changed() is never emitted for a text given to setText (no echo back to who sent it)
*/
class Clipboard: public QObject {
    Q_OBJECT

    private:
        std::unique_ptr<X11Clipboard> _x11; // the bridge, when used
        QString _backend = "none"; // "qt", "x11", "none"
        QString _last = ""; // last text seen or set (no duplicate signal)

        // ---------- Pre-Function -------- //
        _cold void onQt_(void); // QClipboard::dataChanged (reads the text, then received_)

    public:
        // ---------- Pre-Function -------- //
        _cold void setText(const QString& text); // the local clipboard (1 MiB at most)
        _cold void received_(const QString& text); // a change seen by the bridge thread (main thread, queued)
        _cold _nodiscard QString text(void) const; // the current text ("" = none / unknown)

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline QString backend(void) const {return this->_backend;};
        _cold _nodiscard inline bool available(void) const  {return this->_backend != "none";};

        // ---------- Constructor --------- //
        explicit Clipboard(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~Clipboard() override;

    signals:
        void changed(const QString& text); // the local clipboard got a new text (copied on this computer)
};

} // namespace end
#endif /* CLIPBOARD_H */
