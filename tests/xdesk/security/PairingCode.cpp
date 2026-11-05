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
##  @file PairingCode.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#include <utils/utils.hpp>
#include "xdesk/security/PairingCode.hpp"
#include "xdesk/security/Crypto.hpp"
#include <gtest/gtest.h>

TEST(PairingCode, PlainCheckAcceptsTheDisplayedCodeOnly)
{
    xdesk::security::PairingCode code;
    const QString shown = code.display();

    EXPECT_TRUE(code.checkPlain(shown));
    EXPECT_TRUE(code.checkPlain(shown.toLower().remove('-')));
    EXPECT_FALSE(code.checkPlain("AAA-AAA-AAA"));
    EXPECT_FALSE(code.checkPlain(""));
}

TEST(PairingCode, ProofIsTheHmacOfTheArgon2Key)
{
    xdesk::security::PairingCode code;
    const QByteArray transcript = "transcript";
    const QByteArray key = xdesk::security::argon2id(xdesk::security::normalize_code(code.display()).toUtf8(), code.salt());

    EXPECT_TRUE(code.checkProof(transcript, xdesk::security::hmac_sha256(key, transcript)));
    EXPECT_FALSE(code.checkProof(transcript, xdesk::security::hmac_sha256(key, "other")));
    EXPECT_EQ(code.serverProof("server"), xdesk::security::hmac_sha256(key, "server"));
}

TEST(PairingCode, RotationInvalidatesTheOldCode)
{
    xdesk::security::PairingCode code;
    const QString old = code.display();
    const QByteArray oldSalt = code.salt();

    code.rotate();
    EXPECT_NE(code.salt(), oldSalt);
    EXPECT_FALSE(code.checkPlain(old) && old != code.display());
}

TEST(PairingCode, ConfigureChangesLengthAndExpiry)
{
    xdesk::security::PairingCode code;

    code.configure(12, 0);
    EXPECT_EQ(code.display().size(), 15); // 12 characters + 3 separators
    EXPECT_EQ(code.secondsLeft(), -1);
    code.configure(6, 120);
    EXPECT_EQ(code.display().size(), 7);
    EXPECT_GT(code.secondsLeft(), 100);
}
