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
##  @file Certificate.hpp

File Description:
##  Self-signed TLS identity of the host (ECDSA P-256, generated once, pinned by the clients)
\**************************************************************/

#ifndef CERTIFICATE_H
    #define CERTIFICATE_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>  // _cold, _nodiscard
    #include <QSslCertificate>  // QSslCertificate
    #include <QByteArray>       // QByteArray
    #include <QSslKey>          // QSslKey
    #include <QString>          // QString

namespace xdesk::security { // namespace start
//----------------------------------------------------------------//
/* CLASS */

class Certificate {
    private:
        QString _dir; // folder of cert.pem / key.pem
        QSslCertificate _cert; // host certificate
        QSslKey _key; // private key (0600)
        QByteArray _fingerprint; // sha256 of the DER certificate (tls-server-end-point binding)

    public:
        // ---------- Pre-Function -------- //
        void load(const bool regenerate = false); // read, or generate when missing / expired / asked
        _cold _nodiscard static QByteArray fingerprintOf(const QSslCertificate& cert); // sha256 of the DER

        // ------------ Function ---------- //
        /* getter */
        _cold _nodiscard inline const QSslCertificate& certificate(void) const {return this->_cert;};
        _cold _nodiscard inline const QSslKey& key(void) const                 {return this->_key;};
        _cold _nodiscard inline const QByteArray& fingerprint(void) const      {return this->_fingerprint;};

        // ------------ Operator ---------- //
        Certificate& operator=(const Certificate& other) = delete;
        Certificate& operator=(Certificate&& other) = delete;

        // ---------- Constructor --------- //
        explicit Certificate(const QString& dir): _dir{dir} { /* Nothing */ }; // load() reads it
        Certificate(const Certificate& other) = delete;
        Certificate(Certificate&& other) = delete;

        // ----------- Destructor --------- //
        ~Certificate() = default;
};

} // namespace end
#endif /* CERTIFICATE_H */
