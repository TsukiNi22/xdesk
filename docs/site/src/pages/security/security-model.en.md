+++
title = "Security model"
summary = "The layers that protect a connection, who can reach the host, and the honest limits."
section = "security"
order = 1
keywords = "security tls 1.3 post-quantum ml-kem x25519 certificate pinning argon2id hmac mutual auth brute force registry csp web limits threat model"
+++

Every connection is end to end TLS 1.3 with a post-quantum key exchange, and the person in front of the PC sees and decides everything.

## Layers {#layers}

| Layer | What |
|---|---|
| Transport | TLS 1.3 only, `X25519MLKEM768` hybrid key exchange first (OpenSSL >= 3.5); native clients refuse anything not post-quantum |
| Host identity | self-signed ECDSA P-256 certificate, pinned by the native clients on first use; a changed identity is refused |
| Code proof (native) | `HMAC(Argon2id(code, salt), transcript)`, transcript = nonces + `sha256(certificate)`: no offline brute force, no relay by a man in the middle |
| Mutual auth | the host answers with its own proof: a fake host is detected |
| Trusted devices | 256-bit device secret, same HMAC proof, given only when the host trusts the device |
| Brute force | `maxAuthFailures` wrong codes from one address: 1 h ban and a new code |
| Registry | `devices.json` signed with an HMAC key (`registry.key`, `0600`) |
| Web | strict CSP (`default-src 'self'`), no-store, no inline script, `frame-ancestors 'none'` |
| UDP video | AES-256-GCM, per-session keys sent inside TLS, replay window, no answer to an unauthenticated address |

[How the code proof works](page:how-pairing-works#how).

## Who can reach the host {#reach}

Only the LAN and the PC itself, plus the one path the [exposure mode](page:exposure#modes) adds; a peer coming through another path is dropped before TLS. Sharing starts off at each launch, and each start gives a new code.

## Limits {#limits}

> [!WARNING]
> A remote terminal runs with the rights of your user. The [tamper protection](page:tamper-protection#idea) stops the usual ways of touching xdesk, not a determined attacker with a shell: give terminals only to people you trust (the right can be removed live).

- The web client cannot read the certificate: it sends the code inside TLS without the Argon2id binding. Prefer the native clients on an untrusted network.
- `cloudflared` terminates TLS at the relay.
- The Tailscale certificate publishes the MagicDNS name in the public certificate logs.
