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
##  @file IScreenCapture.hpp

File Description:
##  Interface of the screen capture backends (Qt / X11, Wayland portal + PipeWire, Windows DXGI / GDI)
\**************************************************************/

#ifndef ISCREENCAPTURE_H
    #define ISCREENCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _cold, _nodiscard
    #include <QString>          // QString
    #include <QImage>           // QImage
    #include <QPoint>           // QPoint
    #include <memory>           // std::unique_ptr
    #include <QSize>            // QSize

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

/*
 * Called from the capture thread only (one instance, never shared between threads)
 * grab() returns the latest frame (QImage::Format_RGB32 / ARGB32), a null image when nothing new can be read
*/
class IScreenCapture {
    public:
        // ---------- Pre-Function -------- //
        _cold virtual bool start(void) = 0; // false = backend unavailable (error() says why)
        _cold virtual void stop(void) = 0;
        _hot _nodiscard virtual QImage grab(void) = 0; // latest frame (may block until the next one, max ~1 frame)
        _cold _nodiscard virtual QSize size(void) const = 0; // pixels of the captured screen (the input space)
        _hot _nodiscard virtual QPoint cursor(void) const = 0; // cursor position in the captured screen (-1, -1 = unknown)
        _cold _nodiscard virtual QString name(void) const = 0; // "qt", "x11", "portal-pipewire", "dxgi"...
        _cold _nodiscard virtual QString error(void) const = 0; // last error

        // ----------- Destructor --------- //
        virtual ~IScreenCapture() = default;
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_cold _nodiscard std::unique_ptr<IScreenCapture> create_screen_capture(void); // best backend of the platform (started), nullptr when none works

} // namespace end
#endif /* ISCREENCAPTURE_H */
