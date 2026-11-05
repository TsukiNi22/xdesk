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
##  @file Crypto.hpp

File Description:
##  Cryptographic tools: random, codes, HMAC, Argon2id, post-quantum TLS 1.3 configuration
\**************************************************************/

#ifndef CRYPTO_H
    #define CRYPTO_H

    //----------------------------------------------------------------//
    /* INCLUDE */

    /* type */
    #define _Attribute
    #include <utils/utils.hpp>      // _hot, _cold, _nodiscard
    #include <QSslConfiguration>    // QSslConfiguration
    #include <QByteArray>           // QByteArray
    #include <QString>              // QString

    //----------------------------------------------------------------//
    /* DEFINE */

    /* tls */
    #define XDESK_TLS_GROUPS "X25519MLKEM768:SecP256r1MLKEM768:X25519:secp256r1" // hybrid post-quantum key exchange first
    #define XDESK_TLS_GROUPS_STRICT "X25519MLKEM768:SecP256r1MLKEM768" // native peers: post-quantum only

    /* argon2id (pairing code proof) */
    #define XDESK_ARGON2_MEMORY 65536 // KiB
    #define XDESK_ARGON2_ITERATIONS 3
    #define XDESK_ARGON2_LANES 1

namespace xdesk::security { // namespace start
//----------------------------------------------------------------//
/* PROTOTYPE */

/* random */
_hot _nodiscard QByteArray random_bytes(const int size); // CSPRNG (OpenSSL RAND_bytes), throws Crypto
_cold _nodiscard QString random_id(const int bytes = 12); // hex id
_cold _nodiscard QString random_code(const int length); // Crockford base32 without I L O U, grouped by 3 ("K7F-29Q-XM4")
_cold _nodiscard QString normalize_code(const QString& code); // upper case, separators removed, O -> 0, I / L -> 1

/* hash */
_hot _nodiscard QByteArray sha256(const QByteArray& data);
_hot _nodiscard QByteArray hmac_sha256(const QByteArray& key, const QByteArray& message);
_hot _nodiscard bool constant_equal(const QByteArray& a, const QByteArray& b); // timing safe comparison
_cold _nodiscard QByteArray argon2id(const QByteArray& password, const QByteArray& salt, const int length = 32); // XDESK_ARGON2_* cost, throws Crypto

/* auth */
_cold _nodiscard QByteArray auth_transcript(const QByteArray& serverNonce, const QByteArray& clientNonce, const QByteArray& certFingerprint, const QString& role); // "xdesk-auth-v1|role" + nonces + tls-server-end-point binding
_cold _nodiscard QString fingerprint_text(const QByteArray& fingerprint); // "AB:CD:..." (sha256 of the DER certificate)
_cold _nodiscard QString fingerprint_short(const QByteArray& fingerprint); // first 8 bytes, grouped (shown next to the code)

/* tls */
_cold void configure_tls(QSslConfiguration& config, const bool strict_pq = false); // TLS 1.3 only, ML-KEM hybrid groups, no session tickets
_cold _nodiscard QString tls_summary(void); // "TLS 1.3 / X25519MLKEM768 (OpenSSL x.y.z)"

} // namespace end
#endif /* CRYPTO_H */
