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
#include <QDateTime>
#include <QString>

xdesk::security::PairingCode::PairingCode(QObject* parent)
: QObject(parent)
{
    this->_timer.setSingleShot(true);
    QObject::connect(&this->_timer, &QTimer::timeout, this, &PairingCode::rotate);
    this->rotate();
}

void xdesk::security::PairingCode::configure(const int length, const int lifetime)
{
    this->_length = length;
    this->_lifetime = lifetime;
    this->rotate();
}

void xdesk::security::PairingCode::rotate(void)
{
    this->_code = normalize_code(random_code(this->_length));
    this->_salt = random_bytes(16);
    this->_key = argon2id(this->_code.toUtf8(), this->_salt);
    this->_timer.stop();
    if (this->_lifetime > 0) {
        this->_expires = QDateTime::currentMSecsSinceEpoch() + static_cast<qint64>(this->_lifetime) * 1000;
        this->_timer.start(this->_lifetime * 1000);
    } else this->_expires = 0;
    emit this->changed(this->display());
}

bool xdesk::security::PairingCode::checkPlain(const QString& code) const
{
    return constant_equal(normalize_code(code).toUtf8(), this->_code.toUtf8());
}

bool xdesk::security::PairingCode::checkProof(const QByteArray& transcript, const QByteArray& proof) const
{
    return constant_equal(hmac_sha256(this->_key, transcript), proof);
}

QByteArray xdesk::security::PairingCode::serverProof(const QByteArray& transcript) const
{
    return hmac_sha256(this->_key, transcript);
}

QString xdesk::security::PairingCode::display(void) const
{
    QString out;

    for (qsizetype i = 0; i < this->_code.size(); ++i) {
        if (i > 0 && i % 3 == 0) out.append('-');
        out.append(this->_code[i]);
    }
    return out;
}

int xdesk::security::PairingCode::secondsLeft(void) const
{
    if (this->_expires == 0) return -1;
    return static_cast<int>(std::max<qint64>(0, (this->_expires - QDateTime::currentMSecsSinceEpoch()) / 1000));
}
