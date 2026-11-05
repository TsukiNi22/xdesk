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
##  @file DeviceRegistry.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/DeviceRegistry.hpp"
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <QFile>

static xdesk::security::Device make_device(const QString& id)
{
    xdesk::security::Device d;

    d.id = id;
    d.name = "Phone " + id;
    d.secret = QByteArray(32, 's');
    d.trusted = true;
    return d;
}

TEST(DeviceRegistry, BanUnbanAndPersistence)
{
    QTemporaryDir dir;
    {
        xdesk::security::DeviceRegistry registry(dir.path());
        registry.load();
        (void)registry.upsert(make_device("a"));
        registry.ban("a", "test");
        EXPECT_TRUE(registry.isBanned("a"));
        EXPECT_FALSE(registry.find("a")->trusted); // a ban drops the trust and the secret
        EXPECT_TRUE(registry.find("a")->secret.isEmpty());
    }
    xdesk::security::DeviceRegistry again(dir.path());
    again.load();
    EXPECT_FALSE(again.wasTampered());
    EXPECT_TRUE(again.isBanned("a"));
    again.unban("a");
    EXPECT_FALSE(again.isBanned("a"));
}

TEST(DeviceRegistry, AddressBansExpire)
{
    QTemporaryDir dir;
    xdesk::security::DeviceRegistry registry(dir.path());

    registry.load();
    registry.banAddress("10.0.0.1", "forever");
    registry.banAddress("10.0.0.2", "short", -1); // negative = no expiry given -> until a local unban
    EXPECT_TRUE(registry.addressBan("10.0.0.1").has_value());
    EXPECT_FALSE(registry.addressBan("10.0.0.3").has_value());
    registry.unbanAddress("10.0.0.1");
    EXPECT_FALSE(registry.addressBan("10.0.0.1").has_value());
}

TEST(DeviceRegistry, AFileEditedOutsideXdeskIsRefused)
{
    QTemporaryDir dir;
    {
        xdesk::security::DeviceRegistry registry(dir.path());
        registry.load();
        (void)registry.upsert(make_device("trusted"));
        xdesk::security::Device banned = make_device("banned");
        (void)registry.upsert(banned);
        registry.ban("banned", "test");
    }

    // Someone flips the ban by hand
    QFile file(dir.path() + "/devices.json");
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QByteArray content = file.readAll();
    file.close();
    content.replace("\"banned\": true", "\"banned\": false");
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
    file.close();

    xdesk::security::DeviceRegistry registry(dir.path());
    registry.load();
    EXPECT_TRUE(registry.wasTampered());
    EXPECT_FALSE(registry.find("trusted").has_value()); // trusted devices reset: they have to pair again
}
