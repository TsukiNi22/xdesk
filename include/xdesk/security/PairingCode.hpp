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
##  @file PairingCode.hpp

File Description:
##  Rotating one-time pairing code shown on the host
\**************************************************************/

#ifndef PAIRINGCODE_H
    #define PAIRINGCODE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <QByteArray>       // QByteArray
    #include <QObject>          // QObject
    #include <QString>          // QString
    #include <QTimer>           // QTimer

namespace xdesk::security { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class PairingCode: public QObject {
    Q_OBJECT

    private:
        QString _code; // current code, normalized (no separators)
        QByteArray _salt; // argon2id salt of the current code (16 bytes, sent in the challenge)
        QByteArray _key; // argon2id(code, salt), cached: computed once per code
        int _length = 9; // characters
        int _lifetime = 300; // seconds, 0 = only rotated after use
        qint64 _expires = 0; // ms
        QTimer _timer; // rotation

    public:
        // ---------- Pre-Function -------- //
        void rotate(void); // new code + salt, emit changed
        void configure(const int length, const int lifetime); // rotate with the new settings
        _cold _nodiscard bool checkPlain(const QString& code) const; // web clients (code sent inside TLS)
        _cold _nodiscard bool checkProof(const QByteArray& transcript, const QByteArray& proof) const; // native clients: HMAC(argon2id(code), transcript)
        _cold _nodiscard QByteArray serverProof(const QByteArray& transcript) const; // proves to the client that the host knows the code
        _cold _nodiscard QString display(void) const; // "K7F-29Q-XM4"
        _cold _nodiscard int secondsLeft(void) const; // -1 = no expiry

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const QByteArray& salt(void) const {return this->_salt;};

        // ---------- Constructor --------- //
        explicit PairingCode(QObject* parent = nullptr);

        // ----------- Destructor --------- //
        ~PairingCode() override = default;

    signals:
        void changed(const QString& display);
};

} // namespace end
#endif /* PAIRINGCODE_H */
