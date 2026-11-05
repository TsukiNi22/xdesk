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
##  @file XTestInjector.hpp

File Description:
##  X11 input injection with the XTest extension (no special rights needed on X11)
\**************************************************************/

#ifndef XTESTINJECTOR_H
    #define XTESTINJECTOR_H

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

class XTestInjector final: public IInputInjector {
    private:
        void* _display = nullptr; // Display* (X11 headers kept out of the header: macros None, Bool...)
        QSize _screen; // captured size
        QSize _root; // X root window size
        QSet<int> _keys; // X keycodes down
        QSet<int> _buttons; // X buttons down
        int _wheelY = 0; // remainder (120 = one notch)
        int _wheelX = 0;
        QString _error = "";

        // ---------- Pre-Function -------- //
        _hot void keycode_(const int keycode, const bool down);
        _hot void click_(const int button, const int count);

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
        _cold _nodiscard QString name(void) const override  {return "xtest";};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ------------ Operator ---------- //
        XTestInjector& operator=(const XTestInjector& other) = delete;
        XTestInjector& operator=(XTestInjector&& other) = delete;

        // ---------- Constructor --------- //
        XTestInjector() = default;
        XTestInjector(const XTestInjector& other) = delete;
        XTestInjector(XTestInjector&& other) = delete;

        // ----------- Destructor --------- //
        ~XTestInjector() override;
};

} // namespace end
#endif /* XTESTINJECTOR_H */
