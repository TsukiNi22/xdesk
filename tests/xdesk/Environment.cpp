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
##  @file Environment.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include <QTemporaryDir>
#include <gtest/gtest.h>
#include <memory>

// Every test runs with its own configuration / data folders (never the real ones of the user)
class XdgEnvironment: public ::testing::Environment {
    private:
        std::unique_ptr<QTemporaryDir> _dir;

    public:
        void SetUp(void) override
        {
            this->_dir = std::make_unique<QTemporaryDir>();
            qputenv("XDG_CONFIG_HOME", (this->_dir->path() + "/config").toUtf8());
            qputenv("XDG_DATA_HOME", (this->_dir->path() + "/data").toUtf8());
            qputenv("APPDATA", (this->_dir->path() + "/config").toUtf8());
            qputenv("LOCALAPPDATA", (this->_dir->path() + "/data").toUtf8());
        };
        void TearDown(void) override {this->_dir.reset();};
};

static ::testing::Environment* const XDG_ENVIRONMENT = ::testing::AddGlobalTestEnvironment(new XdgEnvironment());
