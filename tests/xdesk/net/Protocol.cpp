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
##  @file Protocol.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/net/Protocol.hpp"
#include <gtest/gtest.h>

TEST(Protocol, WriterAndReaderRoundTrip)
{
    xdesk::net::ByteWriter w(xdesk::net::Binary::Key);
    QByteArray data;

    w.u8(1).u16(0xBEEF).u32(0xDEADBEEF).i16(-1234).bytes("KeyA");
    data = w.data();
    ASSERT_EQ(static_cast<std::uint8_t>(data[0]), 0x23);
    xdesk::net::ByteReader r(data);
    EXPECT_EQ(r.u8(), 1);
    EXPECT_EQ(r.u16(), 0xBEEF);
    EXPECT_EQ(r.u32(), 0xDEADBEEFu);
    EXPECT_EQ(r.i16(), -1234);
    EXPECT_EQ(r.rest(), "KeyA");
    EXPECT_TRUE(r.ok());
}

TEST(Protocol, ReaderFlagsOverflows)
{
    const QByteArray data = QByteArray::fromHex("2001");
    xdesk::net::ByteReader r(data);

    (void)r.u16(); // only one byte left
    EXPECT_FALSE(r.ok());
    xdesk::net::ByteReader r2(data);
    EXPECT_TRUE(r2.bytes(5).isEmpty());
    EXPECT_FALSE(r2.ok());
    xdesk::net::ByteReader r3(data);
    EXPECT_TRUE(r3.bytes(-1).isEmpty());
    EXPECT_FALSE(r3.ok());
}

TEST(Protocol, ClientKindNames)
{
    EXPECT_EQ(xdesk::net::client_kind_name(xdesk::net::ClientKind::Apk), "apk");
    EXPECT_EQ(xdesk::net::client_kind_from("tty"), xdesk::net::ClientKind::Tty);
    EXPECT_EQ(xdesk::net::client_kind_from("anything"), xdesk::net::ClientKind::Web); // the most restricted
}
