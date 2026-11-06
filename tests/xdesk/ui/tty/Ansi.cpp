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
