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
##  @file DxgiCapture.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/windows/DxgiCapture.hpp"
#include <windows.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <QImage>

// Release a COM pointer and reset it
template<typename T>
static void release(T*& pointer)
{
    if (pointer) pointer->Release();
    pointer = nullptr;
}

bool xdesk::platform::DxgiCapture::open_(void)
{
    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIOutput* output = nullptr;
    IDXGIOutput1* output1 = nullptr;
    DXGI_OUTPUT_DESC desc{};
    D3D_FEATURE_LEVEL level;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &this->_device, &level, &this->_context);

    if (FAILED(hr)) {this->_error = "D3D11CreateDevice failed"; return false;}
    hr = this->_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
    if (SUCCEEDED(hr)) hr = dxgiDevice->GetAdapter(&adapter);
    if (SUCCEEDED(hr)) hr = adapter->EnumOutputs(0, &output); // primary output
    if (SUCCEEDED(hr)) hr = output->GetDesc(&desc);
    if (SUCCEEDED(hr)) hr = output->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&output1));
    if (SUCCEEDED(hr)) hr = output1->DuplicateOutput(this->_device, &this->_duplication);
    release(output1);
    release(output);
    release(adapter);
    release(dxgiDevice);
    if (FAILED(hr)) {
        this->_error = hr == E_ACCESSDENIED ? "Desktop duplication refused (secure desktop / UAC prompt)" : "DuplicateOutput failed (0x" + QString::number(static_cast<unsigned long>(hr), 16) + ")";
        return false;
    }
    this->_origin = QPoint(desc.DesktopCoordinates.left, desc.DesktopCoordinates.top);
    this->_size = QSize(desc.DesktopCoordinates.right - desc.DesktopCoordinates.left, desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top);
    return true;
}

void xdesk::platform::DxgiCapture::close_(void)
{
    release(this->_staging);
    release(this->_duplication);
    release(this->_context);
    release(this->_device);
}

bool xdesk::platform::DxgiCapture::start(void)
{
    this->close_();
    return this->open_();
}

void xdesk::platform::DxgiCapture::stop(void)
{
    this->close_();
}

QSize xdesk::platform::DxgiCapture::size(void) const
{
    return this->_size;
}

QPoint xdesk::platform::DxgiCapture::cursor(void) const
{
    POINT p{};

    if (!GetCursorPos(&p)) return {-1, -1};
    return QPoint(p.x - this->_origin.x(), p.y - this->_origin.y());
}

QImage xdesk::platform::DxgiCapture::grab(void)
{
    DXGI_OUTDUPL_FRAME_INFO info{};
    IDXGIResource* resource = nullptr;
    ID3D11Texture2D* texture = nullptr;
    D3D11_TEXTURE2D_DESC desc{};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = S_OK;

    if (!this->_duplication && !this->open_()) return this->_last;
    hr = this->_duplication->AcquireNextFrame(16, &info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return this->_last; // nothing changed
    if (hr == DXGI_ERROR_ACCESS_LOST) {this->close_(); this->open_(); return this->_last;} // mode change, UAC desktop
    if (FAILED(hr)) return this->_last;

    // GPU texture -> staging texture -> QImage (BGRA = Format_RGB32 on little endian)
    if (SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture)))) {
        texture->GetDesc(&desc);
        if (!this->_staging) {
            D3D11_TEXTURE2D_DESC staging = desc;
            staging.Usage = D3D11_USAGE_STAGING;
            staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging.BindFlags = 0;
            staging.MiscFlags = 0;
            staging.MipLevels = 1;
            staging.ArraySize = 1;
            staging.SampleDesc.Count = 1;
            this->_device->CreateTexture2D(&staging, nullptr, &this->_staging);
        }
        if (this->_staging) {
            this->_context->CopyResource(this->_staging, texture);
            if (SUCCEEDED(this->_context->Map(this->_staging, 0, D3D11_MAP_READ, 0, &mapped))) {
                this->_last = QImage(static_cast<const uchar*>(mapped.pData), static_cast<int>(desc.Width), static_cast<int>(desc.Height), static_cast<qsizetype>(mapped.RowPitch), QImage::Format_RGB32).copy();
                this->_context->Unmap(this->_staging, 0);
                this->_size = this->_last.size();
            }
        }
        release(texture);
    }
    release(resource);
    this->_duplication->ReleaseFrame();
    return this->_last;
}
