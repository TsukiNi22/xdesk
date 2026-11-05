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
##  @file InputInjector.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/IInputInjector.hpp"
#ifdef _WIN32
    #include "xdesk/platform/windows/SendInputInjector.hpp"
#elif defined(__linux__)
    #include "xdesk/platform/linux/UInputInjector.hpp"
    #ifdef XDESK_HAS_XTEST
        #include "xdesk/platform/linux/XTestInjector.hpp"
    #endif
#endif
#include <QtGlobal>
#include <QString>

std::unique_ptr<xdesk::platform::IInputInjector> xdesk::platform::create_input_injector(const QSize& screen, QString& error)
{
    std::unique_ptr<IInputInjector> injector = nullptr;

    #ifdef _WIN32
        injector = std::make_unique<SendInputInjector>();
        if (injector->start(screen)) return injector;
        error = injector->error();
        return nullptr;
    #elif defined(__linux__)
        const QString session = qEnvironmentVariable("XDG_SESSION_TYPE");
        const bool wayland = session == "wayland" || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");

        // X11: XTest needs no special rights, uinput is the fallback (and the only way on Wayland)
        #ifdef XDESK_HAS_XTEST
            if (!wayland && qEnvironmentVariableIsSet("DISPLAY")) {
                injector = std::make_unique<XTestInjector>();
                if (injector->start(screen)) return injector;
                error = "xtest: " + injector->error() + "; ";
            }
        #else
            (void)wayland;
        #endif
        injector = std::make_unique<UInputInjector>();
        if (injector->start(screen)) return injector;
        error += injector->error();
        return nullptr;
    #else
        // macOS: no backend yet (CGEvent planned), the session stays view only
        (void)screen;
        error = "no input backend on this platform";
        return nullptr;
    #endif
}
