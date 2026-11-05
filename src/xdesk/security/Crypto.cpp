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
#include <openssl/core_names.h>
#include <openssl/opensslv.h>
#include <QSslConfiguration>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <QByteArray>
#include <QString>
#include <QList>
#include <array>

// Crockford base32 without the letters that look like digits (I L O U)
static constexpr const char CODE_ALPHABET[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

QByteArray xdesk::security::random_bytes(const int size)
{
    QByteArray out(size, '\0');

    if (size <= 0) return out;
    if (RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), size) != 1) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Crypto, "RAND_bytes failed");
    return out;
}

QString xdesk::security::random_id(const int bytes)
{
    return QString::fromLatin1(random_bytes(bytes).toHex());
}

QString xdesk::security::random_code(const int length)
{
    const QByteArray raw = random_bytes(length);
    QString code;

    // 256 % 32 == 0: no modulo bias
    for (int i = 0; i < length; ++i) {
        if (i > 0 && i % 3 == 0) code.append('-');
        code.append(QChar(CODE_ALPHABET[static_cast<unsigned char>(raw[i]) % 32]));
    }
    return code;
}

QString xdesk::security::normalize_code(const QString& code)
{
    QString out;

    for (const QChar c: code.toUpper()) {
        if (c == '-' || c.isSpace()) continue;
        if (c == 'O') out.append('0');
        else if (c == 'I' || c == 'L') out.append('1');
        else out.append(c);
    }
    return out;
}

QByteArray xdesk::security::sha256(const QByteArray& data)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int size = 0;

    if (EVP_Digest(data.constData(), static_cast<std::size_t>(data.size()), digest.data(), &size, EVP_sha256(), nullptr) != 1) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Crypto, "EVP_Digest(sha256) failed");
    return QByteArray(reinterpret_cast<const char*>(digest.data()), static_cast<qsizetype>(size));
}

QByteArray xdesk::security::hmac_sha256(const QByteArray& key, const QByteArray& message)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> out{};
    std::size_t size = 0;
    const char* digest = "SHA256";

    if (!EVP_Q_mac(nullptr, "HMAC", nullptr, digest, nullptr,
        key.constData(), static_cast<std::size_t>(key.size()),
        reinterpret_cast<const unsigned char*>(message.constData()), static_cast<std::size_t>(message.size()),
        out.data(), out.size(), &size)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Crypto, "HMAC-SHA256 failed");
    return QByteArray(reinterpret_cast<const char*>(out.data()), static_cast<qsizetype>(size));
}

bool xdesk::security::constant_equal(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size() || a.isEmpty()) return false;
    return CRYPTO_memcmp(a.constData(), b.constData(), static_cast<std::size_t>(a.size())) == 0;
}

QByteArray xdesk::security::argon2id(const QByteArray& password, const QByteArray& salt, const int length)
{
    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    EVP_KDF_CTX* ctx = nullptr;
    QByteArray out(length, '\0');
    QByteArray pass = password; // non const buffers for the params
    QByteArray saltCopy = salt;
    std::uint32_t iterations = XDESK_ARGON2_ITERATIONS;
    std::uint32_t memory = XDESK_ARGON2_MEMORY;
    std::uint32_t lanes = XDESK_ARGON2_LANES;
    std::uint32_t threads = 1;
    std::array<OSSL_PARAM, 7> params{};
    int result = 0;

    if (!kdf) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Crypto, "ARGON2ID is not available (OpenSSL >= 3.2 needed)");
    ctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!ctx) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Crypto, "EVP_KDF_CTX_new failed");
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, pass.data(), static_cast<std::size_t>(pass.size()));
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, saltCopy.data(), static_cast<std::size_t>(saltCopy.size()));
    params[2] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations);
    params[3] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memory);
    params[4] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes);
    params[5] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &threads);
    params[6] = OSSL_PARAM_construct_end();
    result = EVP_KDF_derive(ctx, reinterpret_cast<unsigned char*>(out.data()), static_cast<std::size_t>(length), params.data());
    EVP_KDF_CTX_free(ctx);
    OPENSSL_cleanse(pass.data(), static_cast<std::size_t>(pass.size()));
    if (result != 1) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Crypto, "Argon2id derivation failed");
    return out;
}

QByteArray xdesk::security::auth_transcript(const QByteArray& serverNonce, const QByteArray& clientNonce, const QByteArray& certFingerprint, const QString& role)
{
    QByteArray out = QByteArray("xdesk-auth-v1|") + role.toUtf8();

    // Same layout in web/app.js: label | role, then the raw bytes
    out.append(serverNonce);
    out.append(clientNonce);
    out.append(certFingerprint);
    return out;
}

QString xdesk::security::fingerprint_text(const QByteArray& fingerprint)
{
    return QString::fromLatin1(fingerprint.toHex(':').toUpper());
}

QString xdesk::security::fingerprint_short(const QByteArray& fingerprint)
{
    const QString hex = QString::fromLatin1(fingerprint.left(8).toHex().toUpper());
    QStringList groups;

    for (qsizetype i = 0; i < hex.size(); i += 4)
        groups.append(hex.mid(i, 4));
    return groups.join(' ');
}

void xdesk::security::configure_tls(QSslConfiguration& config, const bool strict_pq)
{
    // TLS 1.3 only: forward secrecy, no renegotiation, encrypted certificate
    config.setProtocol(QSsl::TlsV1_3OrLater);
    config.setBackendConfigurationOption("Groups", QByteArray(strict_pq ? XDESK_TLS_GROUPS_STRICT : XDESK_TLS_GROUPS));
    config.setSslOption(QSsl::SslOptionDisableSessionTickets, true);
    config.setSslOption(QSsl::SslOptionDisableCompression, true);
}

QString xdesk::security::tls_summary(void)
{
    return QString("TLS 1.3 / X25519MLKEM768 hybrid (%1)").arg(QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION)));
}
