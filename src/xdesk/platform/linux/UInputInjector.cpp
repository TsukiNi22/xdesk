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
##  @file UInputInjector.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/linux/UInputInjector.hpp"
#include "xdesk/platform/Keymap.hpp"
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <algorithm>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <cerrno>

//----------------------------------------------------------------//
/* DEFINE */

#define ABS_RANGE 32767 // absolute axis range (mapped by the compositor on the whole screen)

//----------------------------------------------------------------//
/* TOOLS */

// Create one uinput device (setup done by the caller with UI_SET_*), -1 on error
_cold static int create_device(const int fd, const char* name, const std::uint16_t product)
{
    uinput_setup setup;

    std::memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0x1d6b; // Linux Foundation (virtual devices)
    setup.id.product = product;
    setup.id.version = 1;
    std::strncpy(setup.name, name, UINPUT_MAX_NAME_SIZE - 1);
    if (ioctl(fd, UI_DEV_SETUP, &setup) < 0) return -1;
    if (ioctl(fd, UI_DEV_CREATE) < 0) return -1;
    return fd;
}

_cold static int open_uinput(void)
{
    int fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    const int first = errno;

    if (fd >= 0) return fd;
    fd = ::open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0 && errno == ENOENT) errno = first; // the old path missing: the reason of the real one (no rights)
    return fd;
}

//----------------------------------------------------------------//
/* CLASS */

xdesk::platform::UInputInjector::~UInputInjector()
{
    this->releaseAll();
    for (const int fd: {this->_pointer, this->_keyboard}) {
        if (fd < 0) continue;
        (void)ioctl(fd, UI_DEV_DESTROY);
        ::close(fd);
    }
}

bool xdesk::platform::UInputInjector::start(const QSize& screen)
{
    this->_screen = screen;

    // Pointer: same shape as the virtual tablets of the VMs (absolute pointer for libinput)
    this->_pointer = open_uinput();
    if (this->_pointer < 0) {
        this->_error = QString("/dev/uinput not writable (%1): run scripts/install.sh --uinput").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    (void)ioctl(this->_pointer, UI_SET_EVBIT, EV_KEY);
    for (const int button: {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA})
        (void)ioctl(this->_pointer, UI_SET_KEYBIT, button);
    (void)ioctl(this->_pointer, UI_SET_EVBIT, EV_REL);
    for (const int rel: {REL_WHEEL, REL_HWHEEL, REL_WHEEL_HI_RES, REL_HWHEEL_HI_RES})
        (void)ioctl(this->_pointer, UI_SET_RELBIT, rel);
    (void)ioctl(this->_pointer, UI_SET_EVBIT, EV_ABS);
    for (const int axis: {ABS_X, ABS_Y}) {
        uinput_abs_setup abs;
        std::memset(&abs, 0, sizeof(abs));
        abs.code = static_cast<std::uint16_t>(axis);
        abs.absinfo.minimum = 0;
        abs.absinfo.maximum = ABS_RANGE;
        abs.absinfo.resolution = 1;
        (void)ioctl(this->_pointer, UI_ABS_SETUP, &abs);
    }
    (void)ioctl(this->_pointer, UI_SET_PROPBIT, INPUT_PROP_POINTER);
    if (create_device(this->_pointer, "xdesk virtual pointer", 0x0101) < 0) {
        this->_error = QString("uinput pointer creation failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        ::close(this->_pointer);
        this->_pointer = -1;
        return false;
    }

    // Keyboard: every key of the standard range
    this->_keyboard = open_uinput();
    if (this->_keyboard < 0) {
        this->_error = QString("/dev/uinput not writable (%1): run scripts/install.sh --uinput").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    (void)ioctl(this->_keyboard, UI_SET_EVBIT, EV_KEY);
    (void)ioctl(this->_keyboard, UI_SET_EVBIT, EV_REP);
    for (int keyCode = KEY_ESC; keyCode < KEY_MICMUTE; ++keyCode)
        (void)ioctl(this->_keyboard, UI_SET_KEYBIT, keyCode);
    if (create_device(this->_keyboard, "xdesk virtual keyboard", 0x0102) < 0) {
        this->_error = QString("uinput keyboard creation failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        ::close(this->_keyboard);
        this->_keyboard = -1;
        return false;
    }
    (void)this->_layout.load(); // not loaded: the text is typed through the US layout
    return true;
}

void xdesk::platform::UInputInjector::emit_(const int fd, const int type, const int code, const int value)
{
    input_event event;

    if (fd < 0) return;
    std::memset(&event, 0, sizeof(event));
    event.type = static_cast<std::uint16_t>(type);
    event.code = static_cast<std::uint16_t>(code);
    event.value = value;
    if (::write(fd, &event, sizeof(event)) < 0) _unlikely {
        this->_error = QString("uinput write failed: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
    }
}

void xdesk::platform::UInputInjector::sync_(const int fd)
{
    this->emit_(fd, EV_SYN, SYN_REPORT, 0);
}

void xdesk::platform::UInputInjector::mouseMove(const int x, const int y)
{
    const int w = std::max(1, this->_screen.width() - 1);
    const int h = std::max(1, this->_screen.height() - 1);
    const long long ax = static_cast<long long>(std::clamp(x, 0, w)) * ABS_RANGE / w;
    const long long ay = static_cast<long long>(std::clamp(y, 0, h)) * ABS_RANGE / h;

    this->emit_(this->_pointer, EV_ABS, ABS_X, static_cast<int>(ax));
    this->emit_(this->_pointer, EV_ABS, ABS_Y, static_cast<int>(ay));
    this->sync_(this->_pointer);
}

void xdesk::platform::UInputInjector::mouseButton(const net::MouseButton button, const bool down)
{
    int code = BTN_LEFT;

    switch (button) {
        case net::MouseButton::Left: code = BTN_LEFT;      break;
        case net::MouseButton::Middle: code = BTN_MIDDLE;  break;
        case net::MouseButton::Right: code = BTN_RIGHT;    break;
        case net::MouseButton::Back: code = BTN_SIDE;      break;
        case net::MouseButton::Forward: code = BTN_EXTRA;  break;
        default: return;
    }
    if (down) this->_buttons.insert(code);
    else this->_buttons.remove(code);
    this->emit_(this->_pointer, EV_KEY, code, down ? 1 : 0);
    this->sync_(this->_pointer);
}

void xdesk::platform::UInputInjector::mouseWheel(const int dx, const int dy)
{
    // Positive dy = up (REL_WHEEL > 0), positive dx = left (REL_HWHEEL > 0 = right)
    if (dy != 0) {
        this->_wheelY += dy;
        this->emit_(this->_pointer, EV_REL, REL_WHEEL_HI_RES, dy);
        if (this->_wheelY / 120 != 0) {
            this->emit_(this->_pointer, EV_REL, REL_WHEEL, this->_wheelY / 120);
            this->_wheelY %= 120;
        }
    }
    if (dx != 0) {
        this->_wheelX += -dx;
        this->emit_(this->_pointer, EV_REL, REL_HWHEEL_HI_RES, -dx);
        if (this->_wheelX / 120 != 0) {
            this->emit_(this->_pointer, EV_REL, REL_HWHEEL, this->_wheelX / 120);
            this->_wheelX %= 120;
        }
    }
    this->sync_(this->_pointer);
}

void xdesk::platform::UInputInjector::keyEvdev_(const int evdev, const bool down)
{
    if (down) this->_keys.insert(evdev);
    else this->_keys.remove(evdev);
    this->emit_(this->_keyboard, EV_KEY, evdev, down ? 1 : 0);
    this->sync_(this->_keyboard);
}

void xdesk::platform::UInputInjector::key(const QString& code, const bool down)
{
    const std::optional<KeyInfo> info = key_info(code);

    if (!info || info->evdev <= 0) return;
    this->keyEvdev_(info->evdev, down);
}

void xdesk::platform::UInputInjector::tap_(const LayoutKey& key)
{
    if (key.shift) this->keyEvdev_(KEY_LEFTSHIFT, true);
    if (key.altgr) this->keyEvdev_(KEY_RIGHTALT, true);
    this->keyEvdev_(key.evdev, true);
    this->keyEvdev_(key.evdev, false);
    if (key.altgr) this->keyEvdev_(KEY_RIGHTALT, false);
    if (key.shift) this->keyEvdev_(KEY_LEFTSHIFT, false);
}

bool xdesk::platform::UInputInjector::typeUnicode_(const uint ucs)
{
    const QString hex = QString::number(ucs, 16);

    for (const QChar digit: hex)
        if (!this->_layout.find(digit.unicode())) return false;
    this->keyEvdev_(KEY_LEFTCTRL, true);
    this->keyEvdev_(KEY_LEFTSHIFT, true);
    this->keyEvdev_(KEY_U, true);
    this->keyEvdev_(KEY_U, false);
    this->keyEvdev_(KEY_LEFTSHIFT, false);
    this->keyEvdev_(KEY_LEFTCTRL, false);
    for (const QChar digit: hex)
        this->tap_(*this->_layout.find(digit.unicode()));
    this->keyEvdev_(KEY_SPACE, true);
    this->keyEvdev_(KEY_SPACE, false);
    return true;
}

void xdesk::platform::UInputInjector::text(const QString& text)
{
    // Dead keys of xkb for the combining marks (a decomposed character: the dead key, then its base letter)
    static const QHash<uint, uint> DEAD = {{0x300, 0xfe50}, {0x301, 0xfe51}, {0x302, 0xfe52}, {0x303, 0xfe53}, {0x308, 0xfe57}, {0x30A, 0xfe58}, {0x327, 0xfe5b}};

    // The layout of the host: each character with the key that types it there (an "a" is KEY_Q on azerty)
    if (this->_layout.isLoaded()) {
        for (const uint ucs: text.toUcs4()) {
            const std::optional<LayoutKey> key = this->_layout.find(ucs);
            if (ucs == '\n' || ucs == '\r') {this->keyEvdev_(KEY_ENTER, true); this->keyEvdev_(KEY_ENTER, false); continue;}
            if (ucs == '\t') {this->keyEvdev_(KEY_TAB, true); this->keyEvdev_(KEY_TAB, false); continue;}
            if (key) {this->tap_(*key); continue;}
            const QString parts = QString::fromUcs4(reinterpret_cast<const char32_t*>(&ucs), 1).normalized(QString::NormalizationForm_D);
            const QList<uint> split = parts.toUcs4();
            if (split.size() == 2 && DEAD.contains(split[1]) && this->_layout.dead(DEAD.value(split[1])) && this->_layout.find(split[0])) {
                this->tap_(*this->_layout.dead(DEAD.value(split[1])));
                this->tap_(*this->_layout.find(split[0]));
                continue;
            }
            (void)this->typeUnicode_(ucs); // no key at all: the unicode input of gtk / ibus
        }
        return;
    }

    // uinput only knows physical keys: ascii through the US layout (layout of the session unknown)
    for (const QChar c: text) {
        const std::optional<TextKey> textKey = text_key(c);
        if (!textKey) continue;
        const std::optional<KeyInfo> info = key_info(textKey->code);
        if (!info) continue;
        if (textKey->shift) this->keyEvdev_(KEY_LEFTSHIFT, true);
        this->keyEvdev_(info->evdev, true);
        this->keyEvdev_(info->evdev, false);
        if (textKey->shift) this->keyEvdev_(KEY_LEFTSHIFT, false);
    }
}

void xdesk::platform::UInputInjector::releaseAll(void)
{
    for (const int keyCode: this->_keys)
        this->emit_(this->_keyboard, EV_KEY, keyCode, 0);
    if (!this->_keys.isEmpty()) this->sync_(this->_keyboard);
    for (const int button: this->_buttons)
        this->emit_(this->_pointer, EV_KEY, button, 0);
    if (!this->_buttons.isEmpty()) this->sync_(this->_pointer);
    this->_keys.clear();
    this->_buttons.clear();
}
