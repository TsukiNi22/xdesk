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
##  @file Lanes.cpp

File Description:
##  Extra sockets of the video: joined with the token of the session, frames on the least loaded one, acks back, strangers refused
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/Certificate.hpp"
#include "xdesk/security/Crypto.hpp"
#include "xdesk/net/WebSocket.hpp"
#include "xdesk/net/Datagram.hpp"
#include "xdesk/net/Protocol.hpp"
#include "xdesk/net/Server.hpp"
#include "xdesk/host/Lanes.hpp"
#include <QSslConfiguration>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <QEventLoop>
#include <QtEndian>
#include <QTimer>

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

// A host server with its lanes, three sockets forced
struct LaneHost {
    QTemporaryDir dir;
    xdesk::security::Certificate cert{dir.path()};
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
    xdesk::net::Server server;
    xdesk::host::Lanes* lanes = nullptr;
    QJsonObject offer;

    LaneHost(void)
    {
        ensure_app();
        this->cert.load();
        this->tls.setLocalCertificate(this->cert.certificate());
        this->tls.setPrivateKey(this->cert.key());
        this->server.listen("127.0.0.1", 0, this->tls);
        qputenv("XDESK_LANES", "3");
        this->lanes = new xdesk::host::Lanes();
        this->offer = this->lanes->offer("s1");
        qunsetenv("XDESK_LANES");
        QObject::connect(&this->server, &xdesk::net::Server::newLane, this->lanes, &xdesk::host::Lanes::join);
    };

    ~LaneHost()
    {
        delete this->lanes;
    };

    // A viewer socket of /lane sending its join message once open
    xdesk::net::WebSocket* open(const int index, const QString& token)
    {
        xdesk::net::WebSocket* ws = xdesk::net::WebSocket::connectTo("127.0.0.1", this->server.port(), "/lane", QSslConfiguration::defaultConfiguration());
        QObject::connect(ws, &xdesk::net::WebSocket::opened, ws, [ws, index, token]() {
            ws->sendText(QString::fromUtf8(QJsonDocument(QJsonObject{{"t", "lane"}, {"session", "s1"}, {"token", token}, {"index", index}}).toJson(QJsonDocument::Compact)));
        });
        return ws;
    };
};

static bool wait_for(const std::function<bool(void)>& done, const int ms)
{
    QEventLoop loop;
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&loop, &done]() {if (done()) loop.quit();});
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    poll.start(10);
    loop.exec();
    return done();
}

static QByteArray frame(const std::uint32_t id)
{
    QByteArray message = xdesk::security::random_bytes(200);

    message[0] = static_cast<char>(xdesk::net::Binary::Frame);
    qToBigEndian(id, message.data() + 1);
    return message;
}

//----------------------------------------------------------------//
/* TESTS */

TEST(Lanes, JoinedSocketsCarryFramesAndTheirAcks)
{
    LaneHost host;
    int wanted = 0;
    QList<std::uint32_t> acked;
    QList<QByteArray> received;

    QObject::connect(host.lanes, &xdesk::host::Lanes::wanted, host.lanes, [&wanted](const QString&, const int n) {wanted = n;});
    QObject::connect(host.lanes, &xdesk::host::Lanes::frameAcked, host.lanes, [&acked](const QString&, const std::uint32_t id) {acked.append(id);});
    ASSERT_TRUE(wait_for([&wanted]() {return wanted == 3;}, 2000)); // told to the viewer
    const QString token = host.offer.value("token").toString();
    xdesk::net::WebSocket* one = host.open(1, token);
    xdesk::net::WebSocket* two = host.open(2, token);
    for (xdesk::net::WebSocket* ws: {one, two})
        QObject::connect(ws, &xdesk::net::WebSocket::binaryReceived, ws, [ws, &received](const QByteArray& data) {
            QByteArray ack(5, '\0');
            received.append(data);
            ack[0] = static_cast<char>(xdesk::net::Binary::FrameAck);
            std::memcpy(ack.data() + 1, data.constData() + 1, 4);
            ws->sendBinary(ack);
        });
    ASSERT_TRUE(wait_for([&host]() {return host.lanes->describe("s1") == "tcp x3";}, 5000));

    // A loaded main socket: the frame takes a lane, its ack comes back on it
    EXPECT_TRUE(host.lanes->sendFrame("s1", frame(7), 1 << 20));
    ASSERT_TRUE(wait_for([&acked]() {return acked.contains(7);}, 3000));
    EXPECT_EQ(xdesk::net::frame_id(received.value(0)), 7U);

    // Idle sockets take the frames in turn (the main one too)
    int onMain = 0;
    for (std::uint32_t id = 10; id < 16; ++id) {
        onMain += !host.lanes->sendFrame("s1", frame(id), 0);
        wait_for([]() {return false;}, 30); // written to the network
    }
    EXPECT_GE(onMain, 1);
    EXPECT_LE(onMain, 3);
    EXPECT_FALSE(host.lanes->sendFrame("other", frame(20), 1 << 20)); // no lanes: main socket
    delete one;
    delete two;
}

TEST(Lanes, SocketsWithoutTheTokenAreRefused)
{
    LaneHost host;
    bool closed = false;

    ASSERT_TRUE(wait_for([&host]() {return host.lanes->describe("s1").isEmpty();}, 100));
    wait_for([]() {return false;}, 400); // the 3 sockets are announced
    xdesk::net::WebSocket* ws = host.open(1, QString::fromLatin1(xdesk::security::random_bytes(32).toBase64()));
    QObject::connect(ws, &xdesk::net::WebSocket::closed, ws, [&closed]() {closed = true;});
    EXPECT_TRUE(wait_for([&closed]() {return closed;}, 5000));
    EXPECT_TRUE(host.lanes->describe("s1").isEmpty());
    EXPECT_FALSE(host.lanes->sendFrame("s1", frame(1), 1 << 20)); // still on the main socket
    delete ws;
}
