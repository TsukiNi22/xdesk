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
##  @file DxgiCapture.hpp

File Description:
##  Windows screen capture with the DXGI Desktop Duplication API (GPU copy, changed frames only)
\**************************************************************/

#ifndef DXGICAPTURE_H
    #define DXGICAPTURE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #include <windows.h> // xstyle: ignore CPP-INCLUDE-ORDER (Windows headers before libutils: _unused clash)
    #include <d3d11.h>                  // ID3D11Device, ID3D11Texture2D
    #include <dxgi1_2.h>                // IDXGIOutputDuplication
    #define _Attribute
    #include <utils/utils.hpp>              // _hot, _cold, _nodiscard
    #include "../IScreenCapture.hpp"        // xdesk::platform::IScreenCapture
    #include <QString>                      // QString
    #include <QImage>                       // QImage

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class DxgiCapture final: public IScreenCapture {
    private:
        ID3D11Device* _device = nullptr;
        ID3D11DeviceContext* _context = nullptr;
        IDXGIOutputDuplication* _duplication = nullptr;
        ID3D11Texture2D* _staging = nullptr; // cpu readable copy
        QImage _last; // last frame (returned again when nothing changed)
        QSize _size;
        QPoint _origin; // desktop coordinates of the output (cursor mapping)
        QString _error = "";

        // ---------- Pre-Function -------- //
        _cold bool open_(void); // device + duplication of the primary output
        _cold void close_(void);

    public:
        // ---------- Pre-Function -------- //
        _cold bool start(void) override;
        _cold void stop(void) override;
        _hot _nodiscard QImage grab(void) override;
        _cold _nodiscard QSize size(void) const override;
        _hot _nodiscard QPoint cursor(void) const override;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard QString name(void) const override  {return "dxgi";};
        _cold _nodiscard QString error(void) const override {return this->_error;};

        // ---------- Constructor --------- //
        DxgiCapture() = default;

        // ----------- Destructor --------- //
        ~DxgiCapture() override {this->close_();};
};

} // namespace end
#endif /* DXGICAPTURE_H */
