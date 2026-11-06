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
##  @file Datagram.cpp

File Description:
##  Encrypted datagrams (forgery, tampering, replay), pieces and parity, the udp video between a host and a viewer
\**************************************************************/

#include "xdesk/security/Crypto.hpp"
#include "xdesk/client/UdpLink.hpp"
#include "xdesk/host/UdpServer.hpp"
#include "xdesk/net/Protocol.hpp"
#include "xdesk/net/Datagram.hpp"
#include <QCoreApplication>
#include <gtest/gtest.h>
#include <QEventLoop>
#include <algorithm>
#include <QtEndian>
#include <QTimer>
#include <random>

//----------------------------------------------------------------//
/* TOOLS */

static void ensure_app(void)
{
    static int argc = 1;
    static char name[] = "unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication* app = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);

    (void)app;
}

// A Binary::Frame message of the given size (random bytes after the id)
static QByteArray fake_frame(const std::uint32_t id, const int size)
{
    QByteArray frame = xdesk::security::random_bytes(size);

    frame[0] = static_cast<char>(xdesk::net::Binary::Frame);
    qToBigEndian(id, frame.data() + 1);
    return frame;
}

// The two ends of one connection
struct Pair {
    QByteArray down = xdesk::security::random_bytes(32);
    QByteArray up = xdesk::security::random_bytes(32);
    xdesk::net::DatagramCrypto host{77, down, up, true};
    xdesk::net::DatagramCrypto viewer{77, up, down, false};
};

//----------------------------------------------------------------//
/* TESTS */

TEST(Datagram, SealedDatagramsOpenOnlyOnTheOtherSide)
{
    Pair pair;
    xdesk::net::Datagram type = xdesk::net::Datagram::Hello;
    QByteArray payload;
    const QByteArray packet = pair.host.seal(xdesk::net::Datagram::Data, "pixels");

    ASSERT_FALSE(packet.isEmpty());
    EXPECT_FALSE(packet.contains("pixels")); // encrypted
    EXPECT_FALSE(pair.host.open(packet, type, payload)); // its own direction: another nonce / key
    ASSERT_TRUE(pair.viewer.open(packet, type, payload));
    EXPECT_EQ(type, xdesk::net::Datagram::Data);
    EXPECT_EQ(payload, "pixels");
    EXPECT_EQ(xdesk::net::DatagramCrypto::connectionOf(packet), 77U);
}

TEST(Datagram, TamperedForgedAndReplayedDatagramsAreDropped)
{
    Pair pair;
    xdesk::net::DatagramCrypto stranger(77, xdesk::security::random_bytes(32), xdesk::security::random_bytes(32), true);
    xdesk::net::Datagram type = xdesk::net::Datagram::Hello;
    QByteArray payload;
    const QByteArray packet = pair.host.seal(xdesk::net::Datagram::Data, "frame");
    QByteArray tampered = packet;
    QByteArray retyped = packet;

    tampered[tampered.size() - 20] = static_cast<char>(tampered[tampered.size() - 20] ^ 1);
    retyped[0] = static_cast<char>(xdesk::net::Datagram::Ack); // the header is authenticated too
    EXPECT_FALSE(pair.viewer.open(tampered, type, payload));
    EXPECT_FALSE(pair.viewer.open(retyped, type, payload));
    EXPECT_FALSE(pair.viewer.open(stranger.seal(xdesk::net::Datagram::Data, "fake"), type, payload)); // another key
    EXPECT_FALSE(pair.viewer.open(packet.left(20), type, payload));
    ASSERT_TRUE(pair.viewer.open(packet, type, payload)); // the failures did not burn its sequence
    EXPECT_FALSE(pair.viewer.open(packet, type, payload)); // replay
}

TEST(Datagram, LateDatagramsPassOnceInsideTheWindow)
{
    Pair pair;
    xdesk::net::Datagram type = xdesk::net::Datagram::Hello;
    QByteArray payload;
    QList<QByteArray> packets;

    for (int i = 0; i < XDESK_UDP_WINDOW + 10; ++i)
        packets.append(pair.host.seal(xdesk::net::Datagram::Data, QByteArray::number(i)));
    ASSERT_TRUE(pair.viewer.open(packets[100], type, payload));
    EXPECT_TRUE(pair.viewer.open(packets[50], type, payload)); // reordered
    EXPECT_FALSE(pair.viewer.open(packets[50], type, payload));
    ASSERT_TRUE(pair.viewer.open(packets.last(), type, payload));
    EXPECT_FALSE(pair.viewer.open(packets[0], type, payload)); // older than the window
    EXPECT_TRUE(pair.viewer.open(packets[packets.size() - 2], type, payload));
}

TEST(Datagram, ParityRebuildsOneLostPiecePerGroup)
{
    const QByteArray frame = fake_frame(9, 30 * XDESK_UDP_CHUNK + 123);
    QList<QByteArray> pieces = xdesk::net::split_frame(9, frame, true);
    xdesk::net::FrameAssembler assembler;
    std::mt19937 shuffle(4);
    QByteArray out;
    std::uint32_t id = 0;
    int result = 0;

    ASSERT_EQ(pieces.size(), 31 + 4); // 31 data, 4 groups
    // One data piece lost in each group, the others in any order
    for (int group = 0; group < 4; ++group)
        pieces.removeAt(group * (XDESK_UDP_GROUP - 1) + 3);
    std::shuffle(pieces.begin(), pieces.end(), shuffle);
    for (const QByteArray& piece: pieces)
        result = std::max(result, assembler.add(piece, 0, out, id));
    EXPECT_EQ(result, 1);
    EXPECT_EQ(id, 9U);
    EXPECT_EQ(out, frame);
    EXPECT_TRUE(assembler.missing(1000, 50).isEmpty());
}

TEST(Datagram, MissingPiecesAreAskedThenGivenUp)
{
    const QByteArray frame = fake_frame(3, 10 * XDESK_UDP_CHUNK);
    QList<QByteArray> pieces = xdesk::net::split_frame(3, frame, false);
    xdesk::net::FrameAssembler assembler;
    QByteArray out;
    std::uint32_t id = 0;

    pieces.removeAt(7);
    pieces.removeAt(2);
    for (const QByteArray& piece: pieces)
        EXPECT_EQ(assembler.add(piece, 0, out, id), 0);
    EXPECT_TRUE(assembler.missing(2, 50).isEmpty()); // reordering may still bring them
    const QList<QPair<std::uint32_t, QList<std::uint16_t>>> due = assembler.missing(100, 50);
    ASSERT_EQ(due.size(), 1);
    EXPECT_EQ(due[0].second, (QList<std::uint16_t>{2, 7}));
    EXPECT_TRUE(assembler.missing(120, 50).isEmpty()); // asked: again only one ping later
    EXPECT_EQ(assembler.expired(XDESK_UDP_GIVE_UP + 10), QList<std::uint32_t>{3});
    EXPECT_EQ(assembler.add(pieces[0], XDESK_UDP_GIVE_UP + 20, out, id), 2); // a late piece of a given up frame
}

TEST(Datagram, LyingPiecesAreRefused)
{
    const QList<QByteArray> pieces = xdesk::net::split_frame(5, fake_frame(5, 3000), false);
    xdesk::net::FrameAssembler assembler;
    QByteArray bigger = pieces[0];
    QByteArray shorter = pieces[0];
    QByteArray out;
    std::uint32_t id = 0;

    qToBigEndian(static_cast<std::uint32_t>(XDESK_MAX_MESSAGE + 1), bigger.data() + 8);
    shorter.chop(10);
    EXPECT_EQ(assembler.add(bigger, 0, out, id), -1);
    EXPECT_EQ(assembler.add(shorter, 0, out, id), -1);
    EXPECT_EQ(assembler.add(QByteArray(5, 'x'), 0, out, id), -1);
    EXPECT_EQ(assembler.add(pieces[0], 0, out, id), 0);
}

TEST(Datagram, ViewerGetsItsFramesOverUdp)
{
    ensure_app();
    xdesk::host::UdpServer server;
    xdesk::client::UdpLink link;
    QList<QByteArray> received;
    QList<std::uint32_t> acked;
    QEventLoop loop;

    ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));
    const QJsonObject offer = server.offer("s1");
    QObject::connect(&link, &xdesk::client::UdpLink::frame, &loop, [&received](const QByteArray& frame) {received.append(frame);});
    QObject::connect(&server, &xdesk::host::UdpServer::acked, &loop, [&acked, &loop](const QString& session, const std::uint32_t id) {
        EXPECT_EQ(session, "s1");
        acked.append(id);
        if (acked.size() == 2) loop.quit();
    });
    QObject::connect(&server, &xdesk::host::UdpServer::activeChanged, &loop, [&server](const QString& session, const bool active) {
        if (!active) return;
        EXPECT_TRUE(server.sendFrame(session, fake_frame(1, 5000)));
        EXPECT_TRUE(server.sendFrame(session, fake_frame(2, 200000)));
    });
    EXPECT_FALSE(server.sendFrame("s1", fake_frame(1, 10))); // not reached yet: tcp
    ASSERT_TRUE(link.start(QHostAddress::LocalHost, offer));
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    ASSERT_EQ(received.size(), 2);
    EXPECT_EQ(xdesk::net::frame_id(received[0]), 1U);
    EXPECT_EQ(received[1].size(), 200000);
    std::sort(acked.begin(), acked.end()); // a lost ack comes back later: any order
    EXPECT_EQ(acked, (QList<std::uint32_t>{1, 2}));
    EXPECT_TRUE(link.isActive());
    EXPECT_TRUE(server.describe("s1").startsWith("udp")); // "udp + parity" once the path loses
    server.remove("s1");
    EXPECT_FALSE(server.sendFrame("s1", fake_frame(3, 10)));
}

TEST(Datagram, WrongKeysNeverReachTheVideo)
{
    ensure_app();
    xdesk::host::UdpServer server;
    xdesk::client::UdpLink link;
    QJsonObject offer;
    bool active = false;
    QEventLoop loop;

    ASSERT_TRUE(server.listen(QHostAddress::LocalHost, 0));
    offer = server.offer("s1");
    offer["up"] = QString::fromLatin1(xdesk::security::random_bytes(32).toBase64()); // a viewer without the real key
    QObject::connect(&server, &xdesk::host::UdpServer::activeChanged, &loop, [&active](const QString&, const bool on) {active = on;});
    QObject::connect(&link, &xdesk::client::UdpLink::activeChanged, &loop, [&loop](const bool on) {if (!on) loop.quit();});
    ASSERT_TRUE(link.start(QHostAddress::LocalHost, offer));
    QTimer::singleShot(6000, &loop, &QEventLoop::quit);
    loop.exec();
    EXPECT_FALSE(active); // its hellos were dropped
    EXPECT_FALSE(link.isActive()); // and it gave up (tcp)
}
