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
#include <linux/input.h>
#include <gtest/gtest.h>

// Key of a character on a layout (fails the test when it has none)
static xdesk::platform::LayoutKey key_of(const xdesk::platform::XkbLayout& layout, const uint ucs)
{
    const std::optional<xdesk::platform::LayoutKey> key = layout.find(ucs);

    EXPECT_TRUE(key.has_value()) << "no key for U+" << std::hex << ucs;
    return key.value_or(xdesk::platform::LayoutKey{});
}

TEST(XkbLayout, AzertyTypesTheLettersWhereTheyAre)
{
    xdesk::platform::XkbLayout layout;

    if (!layout.load("fr", "")) GTEST_SKIP() << "libxkbcommon / xkeyboard-config missing";
    EXPECT_EQ(key_of(layout, 'a').evdev, KEY_Q);
    EXPECT_FALSE(key_of(layout, 'a').shift);
    EXPECT_EQ(key_of(layout, 'q').evdev, KEY_A);
    EXPECT_EQ(key_of(layout, 'm').evdev, KEY_SEMICOLON);
    EXPECT_EQ(key_of(layout, 'z').evdev, KEY_W);
    EXPECT_EQ(key_of(layout, 'A').evdev, KEY_Q);
    EXPECT_TRUE(key_of(layout, 'A').shift);
    EXPECT_EQ(key_of(layout, '1').evdev, KEY_1);
    EXPECT_TRUE(key_of(layout, '1').shift); // the digits need shift on azerty
    EXPECT_EQ(key_of(layout, 0xE9).evdev, KEY_2); // e acute
    EXPECT_FALSE(key_of(layout, 0xE9).shift);
    EXPECT_TRUE(key_of(layout, '@').altgr);
    EXPECT_FALSE(layout.find(0xEA).has_value()); // e circumflex: a dead key + e
    EXPECT_TRUE(layout.dead(0xfe52).has_value()); // dead circumflex
}

TEST(XkbLayout, QwertyKeepsTheUsPositions)
{
    xdesk::platform::XkbLayout layout;

    if (!layout.load("us", "")) GTEST_SKIP() << "libxkbcommon / xkeyboard-config missing";
    EXPECT_EQ(key_of(layout, 'a').evdev, KEY_A);
    EXPECT_EQ(key_of(layout, 'm').evdev, KEY_M);
    EXPECT_FALSE(key_of(layout, '1').shift);
    EXPECT_TRUE(key_of(layout, '!').shift);
}
