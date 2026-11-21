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
##  @file PortalCapture.hpp

File Description:
##  Wayland screen capture: xdg-desktop-portal ScreenCast + PipeWire stream (Screenshot portal fallback)
\**************************************************************/

#ifndef PORTALCAPTURE_H
    #define PORTALCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>              // _hot, _cold, _nodiscard
    #include "../IScreenCapture.hpp"        // xdesk::platform::IScreenCapture
    #include <QElapsedTimer>                // QElapsedTimer
    #include <QString>                      // QString
    #include <QImage>                       // QImage
    #include <QPoint>                       // QPoint
    #include <memory>                       // std::unique_ptr
    #include <QSize>                        // QSize

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct PipeWireState; // PipeWire objects + latest frame (PortalCapture.cpp: no PipeWire header here)

//----------------------------------------------------------------//
/* CLASS */

/*
 * ScreenCast: CreateSession -> SelectSources (monitor, cursor as metadata when offered else embedded, persist_mode 2 + restore token in
 * <data>/portal.token: the user approves once) -> Start -> OpenPipeWireRemote -> PipeWire stream (shared memory
 * buffers, BGRx / RGBx), only the latest frame is kept (no queue = no added latency)
 * Without PipeWire (not compiled in): Screenshot portal, about 1 frame per second
*/
class PortalCapture final: public IScreenCapture {
    private:
        QString _session = ""; // portal session object path
        unsigned int _node = 0; // PipeWire node of the monitor
        std::unique_ptr<PipeWireState> _pw; // nullptr in screenshot mode
        bool _screenshot = false; // fallback mode
        unsigned int _cursorMode = 2; // portal cursor_mode: 4 = metadata (drawn by the viewers), 2 = embedded in the frames, 1 = hidden
        QImage _last; // screenshot mode: last image
        QElapsedTimer _lastShot; // screenshot mode: rate limit
        QSize _size;
        QString _error = "";

        // ---------- Pre-Function -------- //
        _cold bool startScreenCast_(void);
        _cold bool startPipeWire_(const int fd);
        _cold bool startScreenshot_(void);
        _cold _nodiscard QImage screenshot_(void);
        _cold void closeSession_(void);

    public:
        // ---------- Pre-Function -------- //
        _cold bool start(void) override;
        _cold void stop(void) override;
        _hot _nodiscard QImage grab(void) override;
        _hot _nodiscard QPoint cursor(void) const override; // from the cursor metadata (-1 = embedded in the frames / hidden)
        _cold _nodiscard quint32 cursorShape(QImage& shape, QPoint& hotspot) const override; // bitmap of the cursor metadata

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QSize size(void) const override    {return this->_size;};
        _cold _nodiscard bool cursorEmbedded(void) const override {return this->_screenshot || this->_cursorMode == 2;};
        _cold _nodiscard QString name(void) const override  {return this->_screenshot ? "portal-screenshot" : "portal-pipewire";};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ------------ Operator ---------- //
        PortalCapture& operator=(const PortalCapture& other) = delete;
        PortalCapture& operator=(PortalCapture&& other) = delete;

        // ---------- Constructor --------- //
        PortalCapture(void);
        PortalCapture(const PortalCapture& other) = delete;
        PortalCapture(PortalCapture&& other) = delete;

        // ----------- Destructor --------- //
        ~PortalCapture() override; // stream and portal session closed
};

} // namespace end
#endif /* PORTALCAPTURE_H */
