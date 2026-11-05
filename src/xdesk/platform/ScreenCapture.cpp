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
##  @file ScreenCapture.cpp

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
#include "xdesk/platform/IScreenCapture.hpp"
#include "xdesk/platform/QtCapture.hpp"
#ifdef _WIN32
    #include "xdesk/platform/windows/DxgiCapture.hpp"
#endif
#ifdef XDESK_HAS_DBUS
    #include "xdesk/platform/linux/PortalCapture.hpp"
#endif
#include <QtGlobal>
#include <iostream>
#include <QString>

std::unique_ptr<xdesk::platform::IScreenCapture> xdesk::platform::create_screen_capture(void)
{
    std::unique_ptr<IScreenCapture> capture = nullptr;

    #ifdef _WIN32
        // Desktop Duplication (GPU copy, only the changed frames) then GDI through Qt
        capture = std::make_unique<DxgiCapture>();
        if (capture->start()) return capture;
        std::cerr << "xdesk: capture: dxgi: " << capture->error().toStdString() << std::endl;
    #elif defined(__linux__)
        const bool wayland = qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");

        // Wayland: no client can read the screen, only the portal (the compositor asks the user once)
        if (wayland) {
            #ifdef XDESK_HAS_DBUS
                capture = std::make_unique<PortalCapture>();
                if (capture->start()) return capture;
                std::cerr << "xdesk: capture: portal: " << capture->error().toStdString() << std::endl;
            #else
                std::cerr << "xdesk: capture: Wayland session but xdesk was built without Qt DBus (portal)" << std::endl;
            #endif
        }
    #endif
    capture = std::make_unique<QtCapture>();
    if (capture->start()) return capture;
    std::cerr << "xdesk: capture: qt: " << capture->error().toStdString() << std::endl;
    return nullptr;
}
