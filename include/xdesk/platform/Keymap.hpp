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
##  @file Keymap.hpp

File Description:
##  KeyboardEvent.code -> physical key of each platform (Linux evdev, Windows scancode), ascii text -> keys
\**************************************************************/

#ifndef KEYMAP_H
    #define KEYMAP_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _hot, _nodiscard
    #include <optional>         // std::optional
    #include <QString>          // QString

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

struct KeyInfo {
    int evdev = 0; // linux input-event-codes KEY_* (X11 keycode = evdev + 8)
    unsigned short scancode = 0; // Windows set 1 scancode, 0 = use the virtual key
    bool extended = false; // Windows KEYEVENTF_EXTENDEDKEY (E0 prefix)
    unsigned short virtualKey = 0; // Windows VK_* when there is no usable scancode (Pause)
};

struct TextKey {
    QString code; // KeyboardEvent.code of the key
    bool shift = false; // shift needed (US layout)
};

//----------------------------------------------------------------//
/* PROTOTYPE */

_hot _nodiscard std::optional<KeyInfo> key_info(const QString& code); // nullopt = unknown code
_hot _nodiscard std::optional<TextKey> text_key(const QChar character); // US layout, ascii only
_hot _nodiscard QString code_from_evdev(const int evdev); // linux KEY_* -> KeyboardEvent.code ("" = unknown)
_hot _nodiscard QString code_from_scancode(const unsigned short scancode, const bool extended); // Windows set 1 -> KeyboardEvent.code

} // namespace end
#endif /* KEYMAP_H */
