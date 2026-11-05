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
##  @file SendInputInjector.hpp

File Description:
##  Windows input injection with SendInput (absolute mouse on the virtual desktop, scancodes, unicode text)
\**************************************************************/

#ifndef SENDINPUTINJECTOR_H
    #define SENDINPUTINJECTOR_H

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

class SendInputInjector final: public IInputInjector {
    private:
        QSize _screen; // captured screen (primary output)
        QSet<QString> _keys; // keys down (releaseAll)
        QSet<int> _buttons; // buttons down
        QString _error = "";

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
        _cold _nodiscard QString name(void) const override  {return "sendinput";};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ---------- Constructor --------- //
        SendInputInjector() = default;

        // ----------- Destructor --------- //
        ~SendInputInjector() override {this->releaseAll();};
};

} // namespace end
#endif /* SENDINPUTINJECTOR_H */
