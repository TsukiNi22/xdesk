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
##  @file UInputInjector.hpp

File Description:
##  Linux input injection through /dev/uinput (works on Wayland and X11): absolute pointer + keyboard
\**************************************************************/

#ifndef UINPUTINJECTOR_H
    #define UINPUTINJECTOR_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>              // _hot, _cold, _nodiscard
    #include "../IInputInjector.hpp"        // xdesk::platform::IInputInjector
    #include <QString>                      // QString
    #include <QSize>                        // QSize
    #include <QSet>                         // QSet

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class UInputInjector final: public IInputInjector {
    private:
        int _pointer = -1; // fd of the virtual tablet (ABS_X / ABS_Y + buttons + wheels)
        int _keyboard = -1; // fd of the virtual keyboard
        QSize _screen; // captured size (coordinates space)
        QSet<int> _keys; // evdev keys down
        QSet<int> _buttons; // BTN_* down
        int _wheelY = 0; // hi-res remainder (120 = one notch)
        int _wheelX = 0;
        QString _error = "";

        // ---------- Pre-Function -------- //
        _hot void emit_(const int fd, const int type, const int code, const int value);
        _hot void sync_(const int fd);
        _hot void keyEvdev_(const int evdev, const bool down);

    public:
        // ---------- Pre-Function -------- //
        _cold bool start(const QSize& screen) override;
        _hot void mouseMove(const int x, const int y) override;
        _hot void mouseButton(const net::MouseButton button, const bool down) override;
        _hot void mouseWheel(const int dx, const int dy) override;
        _hot void key(const QString& code, const bool down) override;
        _hot void text(const QString& text) override;
        _cold void releaseAll(void) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QString name(void) const override  {return "uinput";};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ------------ Operator ---------- //
        UInputInjector& operator=(const UInputInjector& other) = delete;
        UInputInjector& operator=(UInputInjector&& other) = delete;

        // ---------- Constructor --------- //
        UInputInjector() = default;
        UInputInjector(const UInputInjector& other) = delete;
        UInputInjector(UInputInjector&& other) = delete;

        // ----------- Destructor --------- //
        ~UInputInjector() override; // devices destroyed (keys released)
};

} // namespace end
#endif /* UINPUTINJECTOR_H */
