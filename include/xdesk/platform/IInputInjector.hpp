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
##  @file IInputInjector.hpp

File Description:
##  Interface of the input injection backends (Linux uinput / XTest, Windows SendInput, macOS CGEvent)
\**************************************************************/

#ifndef IINPUTINJECTOR_H
    #define IINPUTINJECTOR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include "../net/Protocol.hpp"  // xdesk::net::MouseButton
    #include <QString>              // QString
    #include <memory>               // std::unique_ptr
    #include <QSize>                // QSize

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

/*
 * Coordinates are absolute pixels of the captured screen (IScreenCapture::size)
 * Key codes are the KeyboardEvent.code names ("KeyA", "Digit1", "ShiftLeft", "ArrowUp", "F5"...)
 * releaseAll() is called when a session stops controlling (no stuck modifier)
*/
class IInputInjector {
    public:
        // ---------- Pre-Function -------- //
        _cold virtual bool start(const QSize& screen) = 0; // false = backend unavailable (error() says why)
        _hot virtual void mouseMove(const int x, const int y) = 0;
        _hot virtual void mouseButton(const net::MouseButton button, const bool down) = 0;
        _hot virtual void mouseWheel(const int dx, const int dy) = 0; // 1/120 of a notch
        _hot virtual void key(const QString& code, const bool down) = 0;
        _hot virtual void text(const QString& text) = 0; // unicode text (best effort on Linux uinput: ascii)
        _cold virtual void releaseAll(void) = 0;
        _cold _nodiscard virtual QString name(void) const = 0; // "uinput", "xtest", "sendinput"...
        _cold _nodiscard virtual QString error(void) const = 0;

        // ----------- Destructor --------- //
        virtual ~IInputInjector() = default;
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard std::unique_ptr<IInputInjector> create_input_injector(const QSize& screen); // best backend (started), nullptr when none works

} // namespace end
#endif /* IINPUTINJECTOR_H */
