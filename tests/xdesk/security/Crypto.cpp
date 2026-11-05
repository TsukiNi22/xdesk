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
##  @file Crypto.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xdesk/security/Crypto.hpp"
#include <QRegularExpression>
#include <gtest/gtest.h>
#include <QSet>

/* random */
TEST(Crypto, RandomBytesHaveTheAskedSizeAndDiffer)
{
    EXPECT_EQ(xdesk::security::random_bytes(32).size(), 32);
    EXPECT_NE(xdesk::security::random_bytes(32), xdesk::security::random_bytes(32));
    EXPECT_TRUE(xdesk::security::random_bytes(0).isEmpty());
}

TEST(Crypto, RandomCodeIsGroupedCrockfordBase32)
{
    const QRegularExpression format("^[0-9A-HJKMNP-TV-Z]{3}-[0-9A-HJKMNP-TV-Z]{3}-[0-9A-HJKMNP-TV-Z]{3}$");
    QSet<QString> codes;

    for (int i = 0; i < 200; ++i) {
        const QString code = xdesk::security::random_code(9);
        EXPECT_TRUE(format.match(code).hasMatch()) << code.toStdString();
        codes.insert(code);
    }
    EXPECT_GT(codes.size(), 195); // 45 bits: no collision expected
}

TEST(Crypto, NormalizeCodeAcceptsWhatHumansType)
{
    EXPECT_EQ(xdesk::security::normalize_code("k7f-29q-xm4"), "K7F29QXM4");
    EXPECT_EQ(xdesk::security::normalize_code(" K7F 29Q XM4 "), "K7F29QXM4");
    EXPECT_EQ(xdesk::security::normalize_code("O0-Il"), "0011");
}

/* hash */
TEST(Crypto, HmacSha256MatchesRfc4231)
{
    // RFC 4231 test case 2
    const QByteArray mac = xdesk::security::hmac_sha256("Jefe", "what do ya want for nothing?");

    EXPECT_EQ(mac.toHex(), "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
}

TEST(Crypto, ConstantEqualComparesContentAndSize)
{
    EXPECT_TRUE(xdesk::security::constant_equal("abc", "abc"));
    EXPECT_FALSE(xdesk::security::constant_equal("abc", "abd"));
    EXPECT_FALSE(xdesk::security::constant_equal("abc", "abcd"));
    EXPECT_FALSE(xdesk::security::constant_equal("", "")); // an empty secret never matches
}

TEST(Crypto, Argon2idIsDeterministicAndSalted)
{
    const QByteArray salt = QByteArray(16, 'a');
    const QByteArray key = xdesk::security::argon2id("K7F29QXM4", salt);

    EXPECT_EQ(key.size(), 32);
    EXPECT_EQ(key, xdesk::security::argon2id("K7F29QXM4", salt));
    EXPECT_NE(key, xdesk::security::argon2id("K7F29QXM4", QByteArray(16, 'b')));
    EXPECT_NE(key, xdesk::security::argon2id("K7F29QXM5", salt));
}

/* auth */
TEST(Crypto, TranscriptBindsRoleNoncesAndCertificate)
{
    const QByteArray a = QByteArray(32, '1');
    const QByteArray b = QByteArray(32, '2');
    const QByteArray fp = QByteArray(32, '3');
    const QByteArray client = xdesk::security::auth_transcript(a, b, fp, "client");

    EXPECT_TRUE(client.startsWith("xdesk-auth-v1|client"));
    EXPECT_EQ(client.size(), 20 + 96);
    EXPECT_NE(client, xdesk::security::auth_transcript(a, b, fp, "server"));
    EXPECT_NE(client, xdesk::security::auth_transcript(a, b, QByteArray(32, '4'), "client")); // other certificate (man in the middle)
}

TEST(Crypto, FingerprintTexts)
{
    const QByteArray fp = QByteArray::fromHex("0102030405060708090a");

    EXPECT_EQ(xdesk::security::fingerprint_text(fp), "01:02:03:04:05:06:07:08:09:0A");
    EXPECT_EQ(xdesk::security::fingerprint_short(fp), "0102 0304 0506 0708");
}
