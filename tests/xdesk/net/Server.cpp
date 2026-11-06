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
##  @file Server.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/Certificate.hpp"
#include "xdesk/net/Server.hpp"
#include <QSslConfiguration>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <QTcpServer>
#include <QTcpSocket>
#include <QEventLoop>
#include <QSslSocket>
#include <QTimer>

//----------------------------------------------------------------//
/* DEFINE */

#define TEST_PORT 47980 // not one of the host tests (parallel ctest processes)
#define TEST_NAME "named.test"

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

// Identity of a test certificate in its own folder
struct TestIdentity {
    QTemporaryDir dir;
    xdesk::security::Certificate cert{dir.path()};
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();

    TestIdentity(void)
    {
        this->cert.load();
        this->tls.setLocalCertificate(this->cert.certificate());
        this->tls.setPrivateKey(this->cert.key());
    };
};

// sha256 of the certificate the server shows for this name ("" = no SNI, like the native clients)
static QByteArray served(const QString& name)
{
    QSslSocket socket;
    QEventLoop loop;
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();

    tls.setPeerVerifyMode(QSslSocket::VerifyNone);
    if (name.isEmpty()) tls.setSslOption(QSsl::SslOptionDisableServerNameIndication, true);
    socket.setSslConfiguration(tls);
    QObject::connect(&socket, &QSslSocket::encrypted, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QSslSocket::errorOccurred, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    socket.connectToHostEncrypted("127.0.0.1", TEST_PORT, name.isEmpty() ? QString("127.0.0.1") : name);
    loop.exec();
    return xdesk::security::Certificate::fingerprintOf(socket.peerCertificate());
}

// First bytes a TLS client sends (its ClientHello), read by a plain TCP server
static QByteArray client_hello(const QString& name)
{
    QTcpServer raw;
    QSslSocket client;
    QEventLoop loop;
    QByteArray hello;
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();

    if (!raw.listen(QHostAddress::LocalHost, 0)) return {};
    QObject::connect(&raw, &QTcpServer::newConnection, &loop, [&]() {
        QTcpSocket* socket = raw.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, &loop, [&, socket]() {
            hello += socket->readAll();
            if (hello.size() >= 5 && hello.size() >= 5 + ((static_cast<unsigned char>(hello[3]) << 8) | static_cast<unsigned char>(hello[4]))) loop.quit();
        });
    });
    tls.setPeerVerifyMode(QSslSocket::VerifyNone);
    client.setSslConfiguration(tls);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    client.connectToHostEncrypted("127.0.0.1", raw.serverPort(), name);
    loop.exec();
    return hello;
}

//----------------------------------------------------------------//
/* TEST */

TEST(TlsServer, ReadsTheNameOfARealClientHello)
{
    bool complete = false;

    ensure_app();
    const QByteArray hello = client_hello("Host.Tailnet.ts.net");
    ASSERT_GT(hello.size(), 5);
    EXPECT_EQ(xdesk::net::TlsServer::serverName(hello, complete), "host.tailnet.ts.net");
    EXPECT_TRUE(complete);
}

TEST(TlsServer, WaitsForTheRestOfASplitClientHello)
{
    bool complete = true;

    ensure_app();
    const QByteArray hello = client_hello("host.tailnet.ts.net");
    ASSERT_GT(hello.size(), 40);
    EXPECT_EQ(xdesk::net::TlsServer::serverName(hello.left(hello.size() / 2), complete), "");
    EXPECT_FALSE(complete);
    EXPECT_EQ(xdesk::net::TlsServer::serverName(hello.left(3), complete), "");
    EXPECT_FALSE(complete);
}

TEST(TlsServer, NotTlsIsCompleteWithoutName)
{
    bool complete = false;

    EXPECT_EQ(xdesk::net::TlsServer::serverName("GET / HTTP/1.1\r\n", complete), "");
    EXPECT_TRUE(complete);
}

TEST(TlsServer, NamedCertificateOnlyForItsName)
{
    ensure_app();
    TestIdentity pinned;
    TestIdentity named;
    xdesk::net::Server server;

    server.listen("127.0.0.1", TEST_PORT, pinned.tls);
    server.setNamedCertificate(TEST_NAME, named.tls);
    EXPECT_EQ(served(TEST_NAME), named.cert.fingerprint());
    EXPECT_EQ(served("other.test"), pinned.cert.fingerprint());
    EXPECT_EQ(served(""), pinned.cert.fingerprint()); // native clients: no SNI

    // Removed: everybody gets the pinned one again
    server.setNamedCertificate("", QSslConfiguration());
    EXPECT_EQ(served(TEST_NAME), pinned.cert.fingerprint());
    server.stop();
}
