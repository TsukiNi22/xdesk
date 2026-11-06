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
##  @file XkbLayout.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/platform/linux/XkbLayout.hpp"
#include <QRegularExpression>
#include <linux/input.h>
#include <QStringList>
#include <QLibrary>
#include <QProcess>
#include <cstdint>
#include <cstddef>

//----------------------------------------------------------------//
/* DEFINE */

#define EVDEV_OFFSET 8 // xkb keycode = evdev code + 8

//----------------------------------------------------------------//
/* STRUCT */

// struct xkb_rule_names of xkbcommon.h (the library is loaded at run time)
struct XkbRuleNames {
    const char* rules = nullptr;
    const char* model = nullptr;
    const char* layout = nullptr;
    const char* variant = nullptr;
    const char* options = nullptr;
};

//----------------------------------------------------------------//
/* TYPEDEF */

using XkbContextNew = void* (*)(int);
using XkbKeymapNewFromNames = void* (*)(void*, const XkbRuleNames*, int);
using XkbKeymapKeycode = std::uint32_t (*)(void*);
using XkbKeymapNumLevels = std::uint32_t (*)(void*, std::uint32_t, std::uint32_t);
using XkbKeymapSymsByLevel = int (*)(void*, std::uint32_t, std::uint32_t, std::uint32_t, const std::uint32_t**);
using XkbKeymapModsForLevel = std::size_t (*)(void*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t*, std::size_t);
using XkbKeymapModIndex = std::uint32_t (*)(void*, const char*);
using XkbKeysymToUtf32 = std::uint32_t (*)(std::uint32_t);
using XkbUnref = void (*)(void*);

//----------------------------------------------------------------//
/* TOOLS */

// Layout of the session: the GNOME input source in use, else the system one; "fr+oss" -> <fr, oss>
_cold static QPair<QString, QString> session_layout(void)
{
    static const QRegularExpression XKB_SOURCE("'xkb',\\s*'([^']+)'");
    QString source = "";

    for (const QString& key: {"mru-sources", "sources"}) {
        QProcess gsettings;
        gsettings.start("gsettings", {"get", "org.gnome.desktop.input-sources", key});
        if (!gsettings.waitForFinished(1500)) {gsettings.kill(); continue;}
        const QRegularExpressionMatch match = XKB_SOURCE.match(QString::fromUtf8(gsettings.readAllStandardOutput()));
        if (match.hasMatch()) {source = match.captured(1); break;}
    }
    if (!source.isEmpty()) return {source.section('+', 0, 0), source.section('+', 1)};
    if (!qEnvironmentVariableIsEmpty("XKB_DEFAULT_LAYOUT")) return {qEnvironmentVariable("XKB_DEFAULT_LAYOUT"), qEnvironmentVariable("XKB_DEFAULT_VARIANT")};
    QProcess localectl;
    localectl.start("localectl", {"status"});
    if (localectl.waitForFinished(1500)) {
        QString layout = "";
        QString variant = "";
        for (const QString& line: QString::fromUtf8(localectl.readAllStandardOutput()).split('\n')) {
            if (line.trimmed().startsWith("X11 Layout:")) layout = line.section(':', 1).trimmed().section(',', 0, 0);
            if (line.trimmed().startsWith("X11 Variant:")) variant = line.section(':', 1).trimmed().section(',', 0, 0);
        }
        if (!layout.isEmpty()) return {layout, variant};
    } else localectl.kill();
    return {"", ""};
}

//----------------------------------------------------------------//
/* CLASS */

bool xdesk::platform::XkbLayout::load(void)
{
    const QPair<QString, QString> layout = session_layout();

    return this->load(layout.first, layout.second);
}

bool xdesk::platform::XkbLayout::load(const QString& layoutId, const QString& variantId)
{
    QLibrary library("xkbcommon", 0);
    const QPair<QString, QString> layout = {layoutId, variantId};
    const QByteArray layoutName = layout.first.toUtf8();
    const QByteArray variantName = layout.second.toUtf8();
    XkbRuleNames names;
    void* context = nullptr;
    void* keymap = nullptr;

    this->_keys.clear();
    this->_dead.clear();
    this->_name.clear();
    if (layout.first.isEmpty() || !library.load()) return false;
    const XkbContextNew contextNew = reinterpret_cast<XkbContextNew>(library.resolve("xkb_context_new"));
    const XkbKeymapNewFromNames keymapNew = reinterpret_cast<XkbKeymapNewFromNames>(library.resolve("xkb_keymap_new_from_names"));
    const XkbKeymapKeycode minKeycode = reinterpret_cast<XkbKeymapKeycode>(library.resolve("xkb_keymap_min_keycode"));
    const XkbKeymapKeycode maxKeycode = reinterpret_cast<XkbKeymapKeycode>(library.resolve("xkb_keymap_max_keycode"));
    const XkbKeymapNumLevels numLevels = reinterpret_cast<XkbKeymapNumLevels>(library.resolve("xkb_keymap_num_levels_for_key"));
    const XkbKeymapSymsByLevel symsByLevel = reinterpret_cast<XkbKeymapSymsByLevel>(library.resolve("xkb_keymap_key_get_syms_by_level"));
    const XkbKeymapModsForLevel modsForLevel = reinterpret_cast<XkbKeymapModsForLevel>(library.resolve("xkb_keymap_key_get_mods_for_level"));
    const XkbKeymapModIndex modIndex = reinterpret_cast<XkbKeymapModIndex>(library.resolve("xkb_keymap_mod_get_index"));
    const XkbKeysymToUtf32 toUtf32 = reinterpret_cast<XkbKeysymToUtf32>(library.resolve("xkb_keysym_to_utf32"));
    const XkbUnref keymapUnref = reinterpret_cast<XkbUnref>(library.resolve("xkb_keymap_unref"));
    const XkbUnref contextUnref = reinterpret_cast<XkbUnref>(library.resolve("xkb_context_unref"));
    if (!contextNew || !keymapNew || !minKeycode || !maxKeycode || !numLevels || !symsByLevel || !modsForLevel || !modIndex || !toUtf32 || !keymapUnref || !contextUnref) return false;

    names.rules = "evdev";
    names.model = "pc105";
    names.layout = layoutName.constData();
    names.variant = variantName.isEmpty() ? nullptr : variantName.constData();
    context = contextNew(0);
    if (!context) return false;
    keymap = keymapNew(context, &names, 0);
    if (!keymap) {contextUnref(context); return false;}

    // Every key, every level of the first group: the character (or the dead key) it types and the modifiers it needs
    const std::uint32_t shiftBit = 1u << modIndex(keymap, "Shift");
    const std::uint32_t altgrBit = 1u << modIndex(keymap, "Mod5");
    for (std::uint32_t keycode = minKeycode(keymap); keycode <= maxKeycode(keymap); ++keycode) {
        const int evdev = static_cast<int>(keycode) - EVDEV_OFFSET;
        if (evdev <= 0 || evdev >= KEY_MICMUTE) continue;
        for (std::uint32_t level = 0; level < numLevels(keymap, keycode, 0); ++level) {
            const std::uint32_t* syms = nullptr;
            std::uint32_t masks[8] = {};
            const std::size_t count = modsForLevel(keymap, keycode, 0, level, masks, 8);
            if (symsByLevel(keymap, keycode, 0, level, &syms) != 1 || !syms) continue;
            // The simplest modifiers giving this level, only shift / altgr (never ctrl, alt, caps lock)
            int best = -1;
            for (std::size_t i = 0; i < count; ++i) {
                if ((masks[i] & ~(shiftBit | altgrBit)) != 0) continue;
                if (best < 0 || __builtin_popcount(masks[i]) < __builtin_popcount(masks[best])) best = static_cast<int>(i);
            }
            if (best < 0 && level != 0) continue;
            const LayoutKey key{evdev, best >= 0 && (masks[best] & shiftBit) != 0, best >= 0 && (masks[best] & altgrBit) != 0};
            const std::uint32_t sym = syms[0];
            const int cost = (key.shift ? 1 : 0) + (key.altgr ? 2 : 0);
            if (sym >= 0xfe50 && sym <= 0xfe62) {
                if (!this->_dead.contains(sym)) this->_dead.insert(sym, key);
                continue;
            }
            const std::uint32_t ucs = toUtf32(sym);
            if (ucs == 0 || ucs < 0x20) continue;
            const std::optional<LayoutKey> known = this->find(ucs);
            if (!known || (known->shift ? 1 : 0) + (known->altgr ? 2 : 0) > cost) this->_keys.insert(ucs, key);
        }
    }
    keymapUnref(keymap);
    contextUnref(context);
    this->_name = layout.first + (layout.second.isEmpty() ? QString() : "(" + layout.second + ")");
    return !this->_keys.isEmpty();
}

std::optional<xdesk::platform::LayoutKey> xdesk::platform::XkbLayout::find(const uint ucs) const
{
    const QHash<uint, LayoutKey>::const_iterator it = this->_keys.constFind(ucs);

    if (it == this->_keys.constEnd()) return std::nullopt;
    return it.value();
}

std::optional<xdesk::platform::LayoutKey> xdesk::platform::XkbLayout::dead(const uint keysym) const
{
    const QHash<uint, LayoutKey>::const_iterator it = this->_dead.constFind(keysym);

    if (it == this->_dead.constEnd()) return std::nullopt;
    return it.value();
}
