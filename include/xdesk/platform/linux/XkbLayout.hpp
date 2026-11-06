/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 06/11/2026 by @author Tsukini

File Name:
##  @file XkbLayout.hpp

File Description:
##  Keyboard layout of the session (libxkbcommon loaded at run time): the key + modifiers typing each character
\**************************************************************/

#ifndef XKBLAYOUT_H
    #define XKBLAYOUT_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include <optional>             // std::optional
    #include <QString>              // QString
    #include <QHash>                // QHash

namespace xdesk::platform { // namespace start
//----------------------------------------------------------------//
/* STRUCT */

// Physical key typing a character on the layout of the session
struct LayoutKey {
    int evdev = 0; // KEY_*
    bool shift = false;
    bool altgr = false; // ISO level 3 (right alt)
};

//----------------------------------------------------------------//
/* CLASS */

/*
 * The text of a viewer is typed with the keys of the layout of the host (an "a" is KEY_Q on azerty): the layout of the
 * session (GNOME input sources, else localectl / XKB_DEFAULT_*) compiled by libxkbcommon (loaded at run time, no build
 * dependency); characters without a key: the dead key + the base letter (e circumflex), else none (the caller falls back)
*/
class XkbLayout {
    private:
        QHash<uint, LayoutKey> _keys; // unicode -> the simplest key typing it
        QHash<uint, LayoutKey> _dead; // dead keysym (dead_circumflex...) -> key
        QString _name = ""; // "fr(oss)", empty = not loaded

    public:
        // ---------- Pre-Function -------- //
        _cold bool load(void); // layout of the session, false = unknown (libxkbcommon missing, no layout found)
        _cold bool load(const QString& layout, const QString& variant); // this one ("fr", "oss")
        _hot _nodiscard std::optional<LayoutKey> find(const uint ucs) const;
        _hot _nodiscard std::optional<LayoutKey> dead(const uint keysym) const;

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline bool isLoaded(void) const       {return !this->_keys.isEmpty();};
        _cold _nodiscard inline const QString& name(void) const {return this->_name;};
};

} // namespace end

#endif /* XKBLAYOUT_H */
