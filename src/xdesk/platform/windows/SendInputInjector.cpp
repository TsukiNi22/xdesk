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
##  @file SendInputInjector.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#include <windows.h> // xstyle: ignore CPP-INCLUDE-ORDER (Windows headers before libutils: _unused clash)
#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/windows/SendInputInjector.hpp"
#include "xdesk/platform/Keymap.hpp"
#include <optional>

bool xdesk::platform::SendInputInjector::start(const QSize& screen)
{
    this->_screen = screen;
    return true;
}

void xdesk::platform::SendInputInjector::mouseMove(const int x, const int y)
{
    INPUT input{};

    // Absolute coordinates are 0..65535 over the primary screen
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>((static_cast<long long>(x) * 65535) / std::max(1, this->_screen.width() - 1));
    input.mi.dy = static_cast<LONG>((static_cast<long long>(y) * 65535) / std::max(1, this->_screen.height() - 1));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &input, sizeof(INPUT));
}

void xdesk::platform::SendInputInjector::mouseButton(const net::MouseButton button, const bool down)
{
    INPUT input{};

    input.type = INPUT_MOUSE;
    switch (button) {
        case net::MouseButton::Left:    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;     break;
        case net::MouseButton::Right:   input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;   break;
        case net::MouseButton::Middle:  input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
        case net::MouseButton::Back:    input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; input.mi.mouseData = XBUTTON1; break;
        case net::MouseButton::Forward: input.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; input.mi.mouseData = XBUTTON2; break;
    }
    if (down) this->_buttons.insert(static_cast<int>(button));
    else this->_buttons.remove(static_cast<int>(button));
    SendInput(1, &input, sizeof(INPUT));
}

void xdesk::platform::SendInputInjector::mouseWheel(const int dx, const int dy)
{
    INPUT inputs[2] = {};
    UINT count = 0;

    if (dy != 0) {
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_WHEEL;
        inputs[count++].mi.mouseData = static_cast<DWORD>(dy);
    }
    if (dx != 0) {
        inputs[count].type = INPUT_MOUSE;
        inputs[count].mi.dwFlags = MOUSEEVENTF_HWHEEL;
        inputs[count++].mi.mouseData = static_cast<DWORD>(dx);
    }
    if (count > 0) SendInput(count, inputs, sizeof(INPUT));
}

void xdesk::platform::SendInputInjector::key(const QString& code, const bool down)
{
    const std::optional<KeyInfo> info = key_info(code);
    INPUT input{};

    if (!info.has_value()) return;
    input.type = INPUT_KEYBOARD;

    // Scancodes: the layout of the host applies (like a physical keyboard)
    if (info->scancode != 0) {
        input.ki.wScan = info->scancode;
        input.ki.dwFlags = KEYEVENTF_SCANCODE | (info->extended ? KEYEVENTF_EXTENDEDKEY : 0) | (down ? 0 : KEYEVENTF_KEYUP);
    } else {
        input.ki.wVk = info->virtualKey;
        input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    }
    if (down) this->_keys.insert(code);
    else this->_keys.remove(code);
    SendInput(1, &input, sizeof(INPUT));
}

void xdesk::platform::SendInputInjector::text(const QString& text)
{
    // UTF-16 units sent as unicode key strokes (any character, whatever the layout)
    for (const QChar c: text) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wScan = c.unicode();
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
    }
}

void xdesk::platform::SendInputInjector::releaseAll(void)
{
    const QSet<QString> keys = this->_keys;
    const QSet<int> buttons = this->_buttons;

    for (const QString& code: keys)
        this->key(code, false);
    for (const int button: buttons)
        this->mouseButton(static_cast<net::MouseButton>(button), false);
}
