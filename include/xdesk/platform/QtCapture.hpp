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
##  @file QtCapture.hpp

File Description:
##  Screen capture with QScreen (X11, Windows GDI, macOS): grab done on the GUI thread
\**************************************************************/

#ifndef QTCAPTURE_H
    #define QTCAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>          // _hot, _cold, _nodiscard
    #include "IScreenCapture.hpp"       // xdesk::platform::IScreenCapture
    #include <QString>                  // QString
    #include <QImage>                   // QImage
    #include <QPoint>                   // QPoint
    #include <QSize>                    // QSize

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

/*
 * QScreen::grabWindow only works on the GUI thread: grab() posts the work to it and waits (bounded wait, so a busy or
 * stopping GUI thread never dead locks the capture thread: a null image is returned instead)
*/
class QtCapture final: public IScreenCapture {
    private:
        QSize _size; // pixels of the last frame
        QPoint _cursor = {-1, -1}; // last cursor position (frame pixels)
        QString _error = "";
        bool _started = false;

    public:
        // ---------- Pre-Function -------- //
        _cold bool start(void) override;
        _cold void stop(void) override;
        _hot _nodiscard QImage grab(void) override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QSize size(void) const override    {return this->_size;};
        _hot _nodiscard QPoint cursor(void) const override  {return this->_cursor;};
        _cold _nodiscard QString name(void) const override  {return "qt";};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ---------- Constructor --------- //
        QtCapture() = default;

        // ----------- Destructor --------- //
        ~QtCapture() override = default;
};

} // namespace end
#endif /* QTCAPTURE_H */
