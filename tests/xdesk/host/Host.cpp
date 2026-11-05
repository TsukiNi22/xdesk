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
##  @file Host.cpp

File Description:
##  Unit tests of the Desktop class (init from the arguments, run)
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/core/Config.hpp"
#include "xdesk/host/Host.hpp"
#include <QCoreApplication>
#include <gtest/gtest.h>
#include <QSslSocket>

// Host on a free port of the loopback, LAN only, no discovery (an application is needed for the sockets and the thread)
static xdesk::core::Config& test_config(void)
{
    static int argc = 1;
    static char name[] = "unit_tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication* app = QCoreApplication::instance() ? nullptr : new QCoreApplication(argc, argv);
    static xdesk::core::Config config;

    (void)app;
    config.load();
    config.settings().bind = "127.0.0.1";
    config.settings().port = 47977;
    config.settings().discovery = false;
    config.settings().exposure = xdesk::core::Exposure::Lan;
    return config;
}

static bool listening(const quint16 port)
{
    QSslSocket socket;

    socket.connectToHost("127.0.0.1", port);
    return socket.waitForConnected(1000);
}

/* Sharing */
TEST(Host, StartWithoutSharingListensNowhere)
{
    xdesk::host::Host host(test_config());

    host.start(false);
    EXPECT_TRUE(host.isRunning());
    EXPECT_FALSE(host.isSharing());
    EXPECT_TRUE(host.exposure().endpoints().isEmpty());
    EXPECT_FALSE(listening(47977));
    host.stop();
}

TEST(Host, SharingTurnsOnAndOffWithoutRestart)
{
    xdesk::host::Host host(test_config());

    host.start(false);
    host.setSharing(true);
    EXPECT_TRUE(host.isSharing());
    EXPECT_TRUE(listening(47977));
    EXPECT_FALSE(host.exposure().endpoints().isEmpty());
    host.setSharing(false);
    EXPECT_FALSE(host.isSharing());
    EXPECT_FALSE(listening(47977));
    EXPECT_TRUE(host.exposure().endpoints().isEmpty());

    // Again: the server and the capture thread start a second time
    host.setSharing(true);
    EXPECT_TRUE(host.isSharing());
    EXPECT_TRUE(listening(47977));
    host.stop();
    EXPECT_FALSE(host.isRunning());
}

TEST(Host, SharingCodeChangesAtEachStart)
{
    xdesk::host::Host host(test_config());
    QString first;

    host.start(false);
    host.setSharing(true);
    first = host.code().display();
    host.setSharing(false);
    host.setSharing(true);
    EXPECT_NE(host.code().display(), first);
    host.stop();
}

TEST(Host, SetSharingBeforeStartDoesNothing)
{
    xdesk::host::Host host(test_config());

    EXPECT_NO_THROW(host.setSharing(true));
    EXPECT_FALSE(host.isSharing());
}
