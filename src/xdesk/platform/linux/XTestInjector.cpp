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
##  @file XTestInjector.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/linux/XTestInjector.hpp"
#include "xdesk/platform/Keymap.hpp"
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstdlib>

//----------------------------------------------------------------//
/* DEFINE */

#define DISPLAY (static_cast<::Display*>(this->_display))
#define KEYCODE_OFFSET 8 // X keycode = evdev code + 8 (evdev / xkb servers)

//----------------------------------------------------------------//
/* CLASS */

xdesk::platform::XTestInjector::~XTestInjector()
{
    if (!this->_display) return;
    this->releaseAll();
    XCloseDisplay(DISPLAY);
}

bool xdesk::platform::XTestInjector::start(const QSize& screen)
{
    int eventBase = 0;
    int errorBase = 0;
    int major = 0;
    int minor = 0;

    this->_screen = screen;
    this->_display = XOpenDisplay(nullptr);
    if (!this->_display) {
        this->_error = "cannot open the X display";
        return false;
    }
    if (!XTestQueryExtension(DISPLAY, &eventBase, &errorBase, &major, &minor)) {
        this->_error = "XTest extension missing";
        XCloseDisplay(DISPLAY);
        this->_display = nullptr;
        return false;
    }
    XTestGrabControl(DISPLAY, True); // keep working while another client grabs the server
    this->_root = QSize(DisplayWidth(DISPLAY, DefaultScreen(DISPLAY)), DisplayHeight(DISPLAY, DefaultScreen(DISPLAY)));
    return true;
}

void xdesk::platform::XTestInjector::mouseMove(const int x, const int y)
{
    int rx = x;
    int ry = y;

    if (!this->_display) return;
    if (this->_screen.isValid() && this->_screen != this->_root) {
        rx = static_cast<int>(static_cast<long long>(x) * this->_root.width() / std::max(1, this->_screen.width()));
        ry = static_cast<int>(static_cast<long long>(y) * this->_root.height() / std::max(1, this->_screen.height()));
    }
    XTestFakeMotionEvent(DISPLAY, -1, rx, ry, CurrentTime);
    XFlush(DISPLAY);
}

void xdesk::platform::XTestInjector::mouseButton(const net::MouseButton button, const bool down)
{
    int xbutton = 1;

    if (!this->_display) return;
    switch (button) {
        case net::MouseButton::Left: xbutton = 1;    break;
        case net::MouseButton::Middle: xbutton = 2;  break;
        case net::MouseButton::Right: xbutton = 3;   break;
        case net::MouseButton::Back: xbutton = 8;    break;
        case net::MouseButton::Forward: xbutton = 9; break;
        default: return;
    }
    if (down) this->_buttons.insert(xbutton);
    else this->_buttons.remove(xbutton);
    XTestFakeButtonEvent(DISPLAY, static_cast<unsigned int>(xbutton), down ? True : False, CurrentTime);
    XFlush(DISPLAY);
}

void xdesk::platform::XTestInjector::click_(const int button, const int count)
{
    for (int i = 0; i < count; ++i) {
        XTestFakeButtonEvent(DISPLAY, static_cast<unsigned int>(button), True, CurrentTime);
        XTestFakeButtonEvent(DISPLAY, static_cast<unsigned int>(button), False, CurrentTime);
    }
}

void xdesk::platform::XTestInjector::mouseWheel(const int dx, const int dy)
{
    // X11 wheel = buttons 4 (up) 5 (down) 6 (left) 7 (right), one click per notch
    if (!this->_display) return;
    this->_wheelY += dy;
    this->_wheelX += dx;
    if (this->_wheelY / 120 != 0) {
        this->click_(this->_wheelY > 0 ? 4 : 5, std::abs(this->_wheelY / 120));
        this->_wheelY %= 120;
    }
    if (this->_wheelX / 120 != 0) {
        this->click_(this->_wheelX > 0 ? 6 : 7, std::abs(this->_wheelX / 120));
        this->_wheelX %= 120;
    }
    XFlush(DISPLAY);
}

void xdesk::platform::XTestInjector::keycode_(const int keycode, const bool down)
{
    if (keycode <= 0) return;
    if (down) this->_keys.insert(keycode);
    else this->_keys.remove(keycode);
    XTestFakeKeyEvent(DISPLAY, static_cast<unsigned int>(keycode), down ? True : False, CurrentTime);
}

void xdesk::platform::XTestInjector::key(const QString& code, const bool down)
{
    const std::optional<KeyInfo> info = key_info(code);

    if (!this->_display || !info) return;
    this->keycode_(info->evdev + KEYCODE_OFFSET, down);
    XFlush(DISPLAY);
}

void xdesk::platform::XTestInjector::text(const QString& text)
{
    if (!this->_display) return;
    for (const uint ucs: text.toUcs4()) {
        const KeySym keysym = (ucs < 0x100) ? static_cast<KeySym>(ucs) : static_cast<KeySym>(0x01000000 | ucs);
        const KeyCode keycode = XKeysymToKeycode(DISPLAY, keysym);
        bool shift = false;

        if (ucs == '\n' || ucs == '\r') {
            this->keycode_(28 + KEYCODE_OFFSET, true);
            this->keycode_(28 + KEYCODE_OFFSET, false);
            continue;
        }
        if (keycode == 0) continue; // not on the current layout
        shift = XkbKeycodeToKeysym(DISPLAY, keycode, 0, 0) != keysym && XkbKeycodeToKeysym(DISPLAY, keycode, 0, 1) == keysym;
        if (shift) this->keycode_(42 + KEYCODE_OFFSET, true);
        this->keycode_(keycode, true);
        this->keycode_(keycode, false);
        if (shift) this->keycode_(42 + KEYCODE_OFFSET, false);
    }
    XFlush(DISPLAY);
}

void xdesk::platform::XTestInjector::releaseAll(void)
{
    if (!this->_display) return;
    for (const int keycode: this->_keys)
        XTestFakeKeyEvent(DISPLAY, static_cast<unsigned int>(keycode), False, CurrentTime);
    for (const int button: this->_buttons)
        XTestFakeButtonEvent(DISPLAY, static_cast<unsigned int>(button), False, CurrentTime);
    this->_keys.clear();
    this->_buttons.clear();
    XFlush(DISPLAY);
}
