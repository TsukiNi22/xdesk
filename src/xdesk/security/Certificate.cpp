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
##  @file Certificate.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xdesk/security/Certificate.hpp"
#include "xdesk/security/Crypto.hpp"
#include "xdesk/net/Exposure.hpp"
#include <QNetworkInterface>
#include <openssl/x509v3.h>
#include <QSslCertificate>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <QHostInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QSslKey>
#include <QString>
#include <memory>
#include <QFile>

// Content of a memory BIO
static QByteArray bio_content(BIO* bio)
{
    char* data = nullptr;
    const long size = BIO_get_mem_data(bio, &data);

    return QByteArray(data, static_cast<qsizetype>(size));
}

// Write a private file (0600)
static void write_private(const QString& path, const QByteArray& content)
{
    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Io, "Cannot write " + path.toStdString());
    file.write(content);
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.commit()) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Io, "Cannot write " + path.toStdString());
}

// ECDSA P-256 (every browser accepts it), 5 years, SAN = localhost + host name + every local address
static void generate(const QString& dir)
{
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(EVP_EC_gen("P-256"), EVP_PKEY_free);
    std::unique_ptr<X509, decltype(&X509_free)> cert(X509_new(), X509_free);
    std::unique_ptr<BIO, decltype(&BIO_free)> keyBio(BIO_new(BIO_s_mem()), BIO_free);
    std::unique_ptr<BIO, decltype(&BIO_free)> certBio(BIO_new(BIO_s_mem()), BIO_free);
    const QByteArray serial = xdesk::security::random_bytes(16);
    const QByteArray name = ("xdesk " + QHostInfo::localHostName()).toUtf8();
    QString san = "DNS:localhost,IP:127.0.0.1,IP:::1";
    X509_NAME* subject = nullptr;
    X509V3_CTX ctx;
    X509_EXTENSION* ext = nullptr;
    BIGNUM* bn = nullptr;

    if (!key || !cert || !keyBio || !certBio) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Certificate, "OpenSSL allocation failed");

    // Identity
    X509_set_version(cert.get(), 2);
    bn = BN_bin2bn(reinterpret_cast<const unsigned char*>(serial.constData()), static_cast<int>(serial.size()), nullptr);
    BN_clear_bit(bn, 127); // positive serial
    BN_to_ASN1_INTEGER(bn, X509_get_serialNumber(cert.get()));
    BN_free(bn);
    X509_gmtime_adj(X509_getm_notBefore(cert.get()), -3600);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60 * 24 * 365 * 5);
    X509_set_pubkey(cert.get(), key.get());
    subject = X509_get_subject_name(cert.get());
    X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>(name.constData()), -1, -1, 0);
    X509_NAME_add_entry_by_txt(subject, "O", MBSTRING_UTF8, reinterpret_cast<const unsigned char*>("xdesk"), -1, -1, 0);
    X509_set_issuer_name(cert.get(), subject);

    // Extensions
    if (!QHostInfo::localHostName().isEmpty()) san += ",DNS:" + QHostInfo::localHostName();
    for (const QString& address: xdesk::net::Exposure::lanAddresses())
        san += ",IP:" + address;
    if (!xdesk::net::Exposure::tailscaleAddress().isEmpty()) san += ",IP:" + xdesk::net::Exposure::tailscaleAddress();
    X509V3_set_ctx(&ctx, cert.get(), cert.get(), nullptr, nullptr, 0);
    for (const std::pair<int, QByteArray>& e: {
        std::pair<int, QByteArray>{NID_subject_alt_name, san.toUtf8()},
        std::pair<int, QByteArray>{NID_basic_constraints, "critical,CA:FALSE"},
        std::pair<int, QByteArray>{NID_key_usage, "critical,digitalSignature"},
        std::pair<int, QByteArray>{NID_ext_key_usage, "serverAuth"},
        std::pair<int, QByteArray>{NID_subject_key_identifier, "hash"}}) {
        ext = X509V3_EXT_conf_nid(nullptr, &ctx, e.first, e.second.constData());
        if (!ext) continue;
        X509_add_ext(cert.get(), ext, -1);
        X509_EXTENSION_free(ext);
    }
    if (X509_sign(cert.get(), key.get(), EVP_sha256()) == 0) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Certificate, "X509_sign failed");

    // Files
    PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr);
    PEM_write_bio_X509(certBio.get(), cert.get());
    write_private(dir + "/key.pem", bio_content(keyBio.get()));
    write_private(dir + "/cert.pem", bio_content(certBio.get()));
}

QByteArray xdesk::security::Certificate::fingerprintOf(const QSslCertificate& cert)
{
    return sha256(cert.toDer());
}

void xdesk::security::Certificate::load(const bool regenerate)
{
    QFile certFile(this->_dir + "/cert.pem");
    QFile keyFile(this->_dir + "/key.pem");
    QList<QSslCertificate> certs;

    if (regenerate || !certFile.exists() || !keyFile.exists()) generate(this->_dir);
    if (!certFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Certificate, "Cannot read the certificate in " + this->_dir.toStdString());
    certs = QSslCertificate::fromData(certFile.readAll(), QSsl::Pem);
    this->_key = QSslKey(keyFile.readAll(), QSsl::Ec, QSsl::Pem);
    if (certs.isEmpty() || this->_key.isNull()) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Certificate, "Invalid certificate / key in " + this->_dir.toStdString());
    this->_cert = certs.front();

    // Expired (or about to): new identity, the clients will be asked to confirm the new fingerprint
    if (this->_cert.expiryDate() < QDateTime::currentDateTimeUtc().addDays(7) && !regenerate) {
        this->load(true);
        return;
    }
    this->_fingerprint = fingerprintOf(this->_cert);
}
