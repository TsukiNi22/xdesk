/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 17/10/2026 by @author Tsukini

File Name:
##  @file Desktop.cpp

File Description:
##  Unit tests of the Desktop class (init from the arguments, run)
\**************************************************************/

#define _Exception
#define _Verbose
#include <utils/utils.hpp>
#include "xdesk/Desktop.hpp"
#include <gtest/gtest.h>

/* Init */
TEST(Desktop, InitWithoutArgument)
{
    xdesk::Desktop core;
    const char* const argv[] = {"xdesk", nullptr};

    EXPECT_NO_THROW(core.init(1, argv));
}

TEST(Desktop, InitSetsTheVerboseLevel)
{
    xdesk::Desktop core;
    const char* const argv[] = {"xdesk", "--verbose", "none", nullptr};

    EXPECT_NO_THROW(core.init(3, argv));
    EXPECT_EQ(utils::verbose::verbose.load(), utils::verbose::Verbose::None);
}

TEST(Desktop, InitRefusesAnInvalidVerboseLevel)
{
    xdesk::Desktop core;
    const char* const argv[] = {"xdesk", "--verbose", "loud", nullptr};

    EXPECT_THROW(core.init(3, argv), utils::exception::IException);
}

TEST(Desktop, ConnectWithoutAddressOpensTheConnectPage)
{
    xdesk::Desktop core;
    const char* const argv[] = {"xdesk", "connect", nullptr};

    EXPECT_NO_THROW(core.init(2, argv)); // the viewer / terminal client asks the address
}

TEST(Desktop, ConnectTakesItsAddressAndFlags)
{
    xdesk::Desktop core;
    const char* const argv[] = {"xdesk", "connect", "192.168.1.20", "--port", "47801", "--code", "K7F-29Q-XM4", "--tty", nullptr};

    EXPECT_NO_THROW(core.init(8, argv));
}

TEST(Desktop, InitRefusesAnUnknownExposure)
{
    xdesk::Desktop core;
    const char* const argv[] = {"xdesk", "--expose", "carrier-pigeon", nullptr};

    EXPECT_THROW(core.init(3, argv), utils::exception::IException);
}

/* Run */
TEST(Desktop, RunPrintsThePaths)
{
    xdesk::Desktop core;
    const char* const argv[] = {"xdesk", "paths", nullptr};

    ASSERT_NO_THROW(core.init(2, argv));
    EXPECT_NO_THROW(core.run());
    EXPECT_EQ(core.exit(), 0);
}
