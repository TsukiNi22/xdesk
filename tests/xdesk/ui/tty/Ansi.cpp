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
##  @file Ansi.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/ui/tty/Ansi.hpp"
#include <gtest/gtest.h>

TEST(Ansi, SplitsKeysSequencesAndPastes)
{
    const QList<QByteArray> keys = xdesk::ui::tty::split_keys("a\x1b[A\x1b[<0;10;5M\xc3\xa9\x1b[200~x\ny\x1b[201~\x13\x1bOP");

    ASSERT_EQ(keys.size(), 7);
    EXPECT_EQ(xdesk::ui::tty::key_name(keys[0]), "a");
    EXPECT_EQ(xdesk::ui::tty::key_name(keys[1]), "up");
    EXPECT_EQ(xdesk::ui::tty::key_name(keys[2]), "mouse");
    EXPECT_EQ(xdesk::ui::tty::key_name(keys[3]), QString::fromUtf8("\xc3\xa9")); // one utf-8 character
    EXPECT_EQ(xdesk::ui::tty::key_name(keys[4]), "paste");
    EXPECT_EQ(xdesk::ui::tty::paste_text(keys[4]), "x\ny");
    EXPECT_EQ(xdesk::ui::tty::key_name(keys[5]), "ctrl+s");
    EXPECT_EQ(xdesk::ui::tty::key_name(keys[6]), "f1");
}

TEST(Ansi, ReadsSgrMouseReports)
{
    xdesk::ui::tty::Mouse mouse;

    ASSERT_TRUE(xdesk::ui::tty::parse_mouse("\x1b[<0;10;5M", mouse));
    EXPECT_EQ(mouse.col, 9);
    EXPECT_EQ(mouse.row, 4);
    EXPECT_TRUE(mouse.press);
    ASSERT_TRUE(xdesk::ui::tty::parse_mouse("\x1b[<65;1;1M", mouse));
    EXPECT_EQ(mouse.wheel, -1);
    ASSERT_TRUE(xdesk::ui::tty::parse_mouse("\x1b[<2;3;4m", mouse));
    EXPECT_FALSE(mouse.press);
    EXPECT_EQ(mouse.button, 2);
    EXPECT_FALSE(xdesk::ui::tty::parse_mouse("\x1b[A", mouse));
}

TEST(Ansi, CanvasRowsCarryTheirStyleAndCutLongText)
{
    xdesk::ui::tty::Canvas canvas;
    xdesk::ui::tty::Style red;

    red.fg = QColor(255, 0, 0);
    red.bold = true;
    canvas.resize(8, 2);
    EXPECT_EQ(canvas.text(0, 0, "hello world", red, 6), 6);
    const QByteArray row = canvas.row(0);
    EXPECT_TRUE(row.contains("\x1b[0;1m\x1b[38;2;255;0;0m"));
    EXPECT_TRUE(row.contains(QString::fromUtf8("hello\xe2\x80\xa6").toUtf8())); // cut with an ellipsis
    EXPECT_TRUE(row.endsWith("\x1b[0m"));
    EXPECT_EQ(canvas.row(1), "\x1b[0m\x1b[0m        \x1b[0m"); // blank row: default style
}

TEST(Ansi, QrCodeIsSquareInHalfBlocks)
{
    const QStringList lines = xdesk::ui::tty::qr_lines("https://192.168.1.2:47800/?code=ABC123DEF");

    ASSERT_FALSE(lines.isEmpty());
    EXPECT_EQ(lines.first().size(), lines.last().size());
    EXPECT_EQ(lines.size(), (lines.first().size() + 1) / 2); // 2 modules per line
}

TEST(Ansi, CapabilitiesFollowTheTerminal)
{
    using xdesk::ui::tty::ColorDepth;
    const xdesk::ui::tty::Capabilities kitty = xdesk::ui::tty::parse_capabilities("xterm-kitty", "", "", "en_US.UTF-8", false, "");
    const xdesk::ui::tty::Capabilities tmux = xdesk::ui::tty::parse_capabilities("tmux-256color", "", "", "C.UTF-8", false, "");
    const xdesk::ui::tty::Capabilities console = xdesk::ui::tty::parse_capabilities("linux", "", "", "C", false, "");
    const xdesk::ui::tty::Capabilities dumb = xdesk::ui::tty::parse_capabilities("dumb", "truecolor", "", "", false, "");
    const xdesk::ui::tty::Capabilities forced = xdesk::ui::tty::parse_capabilities("xterm-256color", "truecolor", "", "en_US.UTF-8", true, "ascii, 16 no-mouse");

    EXPECT_EQ(kitty.colors, ColorDepth::True);
    EXPECT_TRUE(kitty.unicode && kitty.mouse && kitty.ansi);
    EXPECT_EQ(tmux.colors, ColorDepth::Palette);
    EXPECT_EQ(console.colors, ColorDepth::Basic);
    EXPECT_FALSE(console.unicode); // C locale: ascii
    EXPECT_FALSE(console.mouse);
    EXPECT_FALSE(dumb.ansi);
    EXPECT_EQ(dumb.colors, ColorDepth::None);
    EXPECT_EQ(forced.colors, ColorDepth::Basic); // XDESK_TTY wins over NO_COLOR
    EXPECT_FALSE(forced.unicode);
    EXPECT_FALSE(forced.mouse);
}

TEST(Ansi, RowsAreLoweredToTheTerminal)
{
    using xdesk::ui::tty::ColorDepth;
    xdesk::ui::tty::Canvas canvas;
    xdesk::ui::tty::Capabilities caps;
    xdesk::ui::tty::Style selected;

    selected.fg = QColor(255, 0, 0);
    selected.bg = QColor(34, 66, 54);
    canvas.resize(4, 1);
    (void)canvas.text(0, 0, QString::fromUtf8("\xe2\x96\xb8\xc3\xa9\xe2\x94\x80x"), selected); // ▸ é ─ x
    caps.colors = ColorDepth::Palette;
    EXPECT_TRUE(canvas.row(0, caps).contains("\x1b[38;5;196m"));
    caps.colors = ColorDepth::Basic;
    EXPECT_TRUE(canvas.row(0, caps).contains("\x1b[31m"));
    EXPECT_TRUE(canvas.row(0, caps).contains("\x1b[100m")); // dark gray selection: bright black
    caps.colors = ColorDepth::None;
    caps.unicode = false;
    EXPECT_EQ(canvas.row(0, caps), "\x1b[0m\x1b[0;7m>e-x\x1b[0m"); // inverse video, ascii look-alikes
}

TEST(Ansi, QrCodeInAsciiHasSquareModules)
{
    const QStringList lines = xdesk::ui::tty::qr_lines("https://192.168.1.2:47800/?code=ABC123DEF", false);

    ASSERT_FALSE(lines.isEmpty());
    EXPECT_EQ(lines.first().size(), lines.size() * 2); // 2 cells per module, 1 line per module
    EXPECT_TRUE(lines.first().startsWith("####")); // quiet zone (light)
}
