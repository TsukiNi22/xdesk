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
##  @file Keymap.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/Keymap.hpp"
#include <QHash>

//----------------------------------------------------------------//
/* STRUCT */

struct KeyEntry {
    const char* code; // KeyboardEvent.code
    int evdev; // KEY_*
    unsigned short scancode; // set 1, 0xE0xx = extended
    unsigned short virtualKey; // VK_* fallback
};

//----------------------------------------------------------------//
/* DATA */

// Main block: the set 1 scancodes are the evdev codes (1 - 88)
static const KeyEntry KEYS[] = {
    /* letters */
    {"KeyA", 30, 0x1E, 0}, {"KeyB", 48, 0x30, 0}, {"KeyC", 46, 0x2E, 0}, {"KeyD", 32, 0x20, 0},
    {"KeyE", 18, 0x12, 0}, {"KeyF", 33, 0x21, 0}, {"KeyG", 34, 0x22, 0}, {"KeyH", 35, 0x23, 0},
    {"KeyI", 23, 0x17, 0}, {"KeyJ", 36, 0x24, 0}, {"KeyK", 37, 0x25, 0}, {"KeyL", 38, 0x26, 0},
    {"KeyM", 50, 0x32, 0}, {"KeyN", 49, 0x31, 0}, {"KeyO", 24, 0x18, 0}, {"KeyP", 25, 0x19, 0},
    {"KeyQ", 16, 0x10, 0}, {"KeyR", 19, 0x13, 0}, {"KeyS", 31, 0x1F, 0}, {"KeyT", 20, 0x14, 0},
    {"KeyU", 22, 0x16, 0}, {"KeyV", 47, 0x2F, 0}, {"KeyW", 17, 0x11, 0}, {"KeyX", 45, 0x2D, 0},
    {"KeyY", 21, 0x15, 0}, {"KeyZ", 44, 0x2C, 0},

    /* digits */
    {"Digit1", 2, 0x02, 0}, {"Digit2", 3, 0x03, 0}, {"Digit3", 4, 0x04, 0}, {"Digit4", 5, 0x05, 0},
    {"Digit5", 6, 0x06, 0}, {"Digit6", 7, 0x07, 0}, {"Digit7", 8, 0x08, 0}, {"Digit8", 9, 0x09, 0},
    {"Digit9", 10, 0x0A, 0}, {"Digit0", 11, 0x0B, 0},

    /* punctuation */
    {"Minus", 12, 0x0C, 0}, {"Equal", 13, 0x0D, 0}, {"BracketLeft", 26, 0x1A, 0}, {"BracketRight", 27, 0x1B, 0},
    {"Semicolon", 39, 0x27, 0}, {"Quote", 40, 0x28, 0}, {"Backquote", 41, 0x29, 0}, {"Backslash", 43, 0x2B, 0},
    {"Comma", 51, 0x33, 0}, {"Period", 52, 0x34, 0}, {"Slash", 53, 0x35, 0}, {"IntlBackslash", 86, 0x56, 0},
    {"IntlRo", 89, 0x73, 0}, {"IntlYen", 124, 0x7D, 0},

    /* editing */
    {"Escape", 1, 0x01, 0}, {"Backspace", 14, 0x0E, 0}, {"Tab", 15, 0x0F, 0}, {"Enter", 28, 0x1C, 0},
    {"Space", 57, 0x39, 0}, {"CapsLock", 58, 0x3A, 0}, {"Insert", 110, 0xE052, 0}, {"Delete", 111, 0xE053, 0},
    {"Home", 102, 0xE047, 0}, {"End", 107, 0xE04F, 0}, {"PageUp", 104, 0xE049, 0}, {"PageDown", 109, 0xE051, 0},
    {"ArrowUp", 103, 0xE048, 0}, {"ArrowDown", 108, 0xE050, 0}, {"ArrowLeft", 105, 0xE04B, 0}, {"ArrowRight", 106, 0xE04D, 0},
    {"PrintScreen", 99, 0xE037, 0}, {"ScrollLock", 70, 0x46, 0}, {"Pause", 119, 0, 0x13}, {"ContextMenu", 127, 0xE05D, 0},

    /* modifiers */
    {"ShiftLeft", 42, 0x2A, 0}, {"ShiftRight", 54, 0x36, 0}, {"ControlLeft", 29, 0x1D, 0}, {"ControlRight", 97, 0xE01D, 0},
    {"AltLeft", 56, 0x38, 0}, {"AltRight", 100, 0xE038, 0}, {"MetaLeft", 125, 0xE05B, 0}, {"MetaRight", 126, 0xE05C, 0},
    {"OSLeft", 125, 0xE05B, 0}, {"OSRight", 126, 0xE05C, 0},

    /* function */
    {"F1", 59, 0x3B, 0}, {"F2", 60, 0x3C, 0}, {"F3", 61, 0x3D, 0}, {"F4", 62, 0x3E, 0},
    {"F5", 63, 0x3F, 0}, {"F6", 64, 0x40, 0}, {"F7", 65, 0x41, 0}, {"F8", 66, 0x42, 0},
    {"F9", 67, 0x43, 0}, {"F10", 68, 0x44, 0}, {"F11", 87, 0x57, 0}, {"F12", 88, 0x58, 0},
    {"F13", 183, 0x64, 0}, {"F14", 184, 0x65, 0}, {"F15", 185, 0x66, 0}, {"F16", 186, 0x67, 0},

    /* numpad */
    {"NumLock", 69, 0x45, 0}, {"NumpadDivide", 98, 0xE035, 0}, {"NumpadMultiply", 55, 0x37, 0}, {"NumpadSubtract", 74, 0x4A, 0},
    {"NumpadAdd", 78, 0x4E, 0}, {"NumpadEnter", 96, 0xE01C, 0}, {"NumpadDecimal", 83, 0x53, 0}, {"NumpadEqual", 117, 0x59, 0},
    {"Numpad0", 82, 0x52, 0}, {"Numpad1", 79, 0x4F, 0}, {"Numpad2", 80, 0x50, 0}, {"Numpad3", 81, 0x51, 0},
    {"Numpad4", 75, 0x4B, 0}, {"Numpad5", 76, 0x4C, 0}, {"Numpad6", 77, 0x4D, 0}, {"Numpad7", 71, 0x47, 0},
    {"Numpad8", 72, 0x48, 0}, {"Numpad9", 73, 0x49, 0}, {"NumpadComma", 121, 0x7E, 0},

    /* media */
    {"AudioVolumeMute", 113, 0xE020, 0}, {"AudioVolumeDown", 114, 0xE02E, 0}, {"AudioVolumeUp", 115, 0xE030, 0},
    {"MediaPlayPause", 164, 0xE022, 0}, {"MediaStop", 166, 0xE024, 0}, {"MediaTrackNext", 163, 0xE019, 0},
    {"MediaTrackPrevious", 165, 0xE010, 0},
};

// US layout: unshifted / shifted characters of the main block
static const char* const TEXT_PLAIN = "abcdefghijklmnopqrstuvwxyz1234567890-=[];'`\\,./ ";
static const char* const TEXT_SHIFT = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+{}:\"~|<>?";
static const char* const TEXT_CODES[] = {
    "KeyA", "KeyB", "KeyC", "KeyD", "KeyE", "KeyF", "KeyG", "KeyH", "KeyI", "KeyJ", "KeyK", "KeyL", "KeyM",
    "KeyN", "KeyO", "KeyP", "KeyQ", "KeyR", "KeyS", "KeyT", "KeyU", "KeyV", "KeyW", "KeyX", "KeyY", "KeyZ",
    "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", "Digit7", "Digit8", "Digit9", "Digit0",
    "Minus", "Equal", "BracketLeft", "BracketRight", "Semicolon", "Quote", "Backquote", "Backslash",
    "Comma", "Period", "Slash", "Space",
};
static const char* const TEXT_SHIFT_CODES[] = {
    "KeyA", "KeyB", "KeyC", "KeyD", "KeyE", "KeyF", "KeyG", "KeyH", "KeyI", "KeyJ", "KeyK", "KeyL", "KeyM",
    "KeyN", "KeyO", "KeyP", "KeyQ", "KeyR", "KeyS", "KeyT", "KeyU", "KeyV", "KeyW", "KeyX", "KeyY", "KeyZ",
    "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6", "Digit7", "Digit8", "Digit9", "Digit0",
    "Minus", "Equal", "BracketLeft", "BracketRight", "Semicolon", "Quote", "Backquote", "Backslash",
    "Comma", "Period", "Slash",
};

//----------------------------------------------------------------//
/* FUNCTION */

static const QHash<QString, xdesk::platform::KeyInfo>& key_table(void)
{
    static const QHash<QString, xdesk::platform::KeyInfo> table = [](void) {
        QHash<QString, xdesk::platform::KeyInfo> keys;
        for (const KeyEntry& entry: KEYS) {
            xdesk::platform::KeyInfo info;
            info.evdev = entry.evdev;
            info.scancode = static_cast<unsigned short>(entry.scancode & 0xFF);
            info.extended = (entry.scancode & 0xFF00) == 0xE000;
            info.virtualKey = entry.virtualKey;
            keys.insert(QString::fromLatin1(entry.code), info);
        }
        return keys;
    }();
    return table;
}

std::optional<xdesk::platform::KeyInfo> xdesk::platform::key_info(const QString& code)
{
    const QHash<QString, KeyInfo>& table = key_table();
    auto it = table.constFind(code);

    if (it == table.constEnd()) return std::nullopt;
    return it.value();
}

std::optional<xdesk::platform::TextKey> xdesk::platform::text_key(const QChar character)
{
    const char c = character.toLatin1();

    if (character.unicode() > 0x7F || c == 0) return std::nullopt;
    if (c == '\n' || c == '\r') return TextKey{"Enter", false};
    if (c == '\t') return TextKey{"Tab", false};
    for (std::size_t i = 0; TEXT_PLAIN[i]; ++i)
        if (TEXT_PLAIN[i] == c) return TextKey{QString::fromLatin1(TEXT_CODES[i]), false};
    for (std::size_t i = 0; TEXT_SHIFT[i]; ++i)
        if (TEXT_SHIFT[i] == c) return TextKey{QString::fromLatin1(TEXT_SHIFT_CODES[i]), true};
    return std::nullopt;
}

QString xdesk::platform::code_from_evdev(const int evdev)
{
    for (const KeyEntry& entry: KEYS)
        if (entry.evdev == evdev) return QString::fromLatin1(entry.code);
    return "";
}

QString xdesk::platform::code_from_scancode(const unsigned short scancode, const bool extended)
{
    const unsigned short wanted = static_cast<unsigned short>((extended ? 0xE000 : 0) | (scancode & 0xFF));

    for (const KeyEntry& entry: KEYS)
        if (entry.scancode == wanted) return QString::fromLatin1(entry.code);
    return "";
}
