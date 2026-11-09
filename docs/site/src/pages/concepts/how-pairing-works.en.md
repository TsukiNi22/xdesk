+++
title = "How pairing works"
summary = "How a 9-character code proves who you are without ever being sent, and why a man in the middle or a guesser gets nothing."
section = "concepts"
order = 1
kind = "explain"
level = "simple"
keywords = "pairing code proof argon2id hmac transcript nonce salt certificate pinning fingerprint mitm brute force trusted device secret handshake"
+++

:::simple tldr
**In one sentence:** the client proves it knows the code without sending it, and the proof only works on the connection it was made for, so nobody in between can reuse it.
:::

:::technical tldr
**Definition:** a PAKE-like proof over TLS 1.3: `proof = HMAC(Argon2id(code, salt), transcript)`, with the transcript bound to both nonces and to `sha256(certificate)` (tls-server-end-point, RFC 5929); the host answers with its own proof (mutual authentication).
:::

## The idea {#idea}

:::simple
Two people meet and must check they know the same password, in a room where someone may listen. Instead of saying it, each one writes a sum that only works with the password **and** today's date and room number. The listener learns nothing, and the note is useless in another room.
:::

:::technical
A shared low-entropy secret (45 bits) authenticates both sides of an already encrypted channel. The secret is stretched with Argon2id (`m = 64 MiB`, `t = 3`, `p = 1`, 16-byte salt per code) so each guess is expensive, then used as an HMAC key over a transcript that includes both nonces (no replay) and the fingerprint of the certificate the client sees (no relay: a man in the middle presents another certificate, so the proof does not match).
:::

## How it works {#how}

:::html
<figure class="stepper" data-steps="7">
  <ol class="captions">
    <li><b>TLS.</b> The client opens TLS 1.3 with the hybrid <code>X25519MLKEM768</code> key exchange and checks the certificate against the fingerprint it pinned (first use: shown on both sides).</li>
    <li><b>hello.</b> The client sends its kind, name, OS, device id and a random nonce.</li>
    <li><b>challenge.</b> The host answers with its own nonce and the 16-byte Argon2id salt of the current code.</li>
    <li><b>auth.</b> The client sends <code>HMAC(Argon2id(code, salt), transcript)</code>: the transcript holds both nonces and the SHA-256 of the certificate it sees. The code itself never travels.</li>
    <li><b>pending.</b> The proof is right; the device is unknown, so the person at the PC is asked (30 s, refused by default).</li>
    <li><b>welcome.</b> Accepted: the session gets its rights, and the host proves it knows the code too, so a fake host is caught.</li>
    <li><b>Session.</b> Frames, inputs and terminals flow on the same TLS connection (the video may move to encrypted UDP).</li>
  </ol>
  <svg viewBox="0 0 760 360" role="img" aria-label="handshake between a client and the host">
    <defs><marker id="en-hs-arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path class="arrow" d="M0 0L10 5L0 10z"/></marker></defs>
    <rect id="en-hs-client" class="box-alt" x="40" y="10" width="160" height="40" rx="8"/><text class="lbl" x="120" y="35" text-anchor="middle">client</text>
    <rect id="en-hs-host" class="box-alt" x="560" y="10" width="160" height="40" rx="8"/><text class="lbl" x="640" y="35" text-anchor="middle">host</text>
    <path class="edge dash" d="M120 50V350" data-free=""/>
    <path class="edge dash" d="M640 50V350" data-free=""/>
    <rect id="en-hs-c0" class="box" x="116" y="76" width="8" height="8" data-show="0-"/>
    <rect id="en-hs-h0" class="box" x="636" y="76" width="8" height="8" data-show="0-"/>
    <path id="en-hs-m0" class="edge-accent" data-from="en-hs-c0" data-to="en-hs-h0" marker-end="url(#en-hs-arr)" data-show="0-" data-hl="0"/>
    <text class="lbl" x="380" y="72" text-anchor="middle" data-show="0-">TLS 1.3, X25519MLKEM768</text>
    <rect id="en-hs-c1" class="box" x="116" y="116" width="8" height="8" data-show="1-"/>
    <rect id="en-hs-h1" class="box" x="636" y="116" width="8" height="8" data-show="1-"/>
    <path id="en-hs-m1" class="edge" data-from="en-hs-c1" data-to="en-hs-h1" marker-end="url(#en-hs-arr)" data-show="1-" data-hl="1"/>
    <text class="lbl" x="380" y="112" text-anchor="middle" data-show="1-">hello (nonce, device)</text>
    <rect id="en-hs-c2" class="box" x="116" y="156" width="8" height="8" data-show="2-"/>
    <rect id="en-hs-h2" class="box" x="636" y="156" width="8" height="8" data-show="2-"/>
    <path id="en-hs-m2" class="edge" data-from="en-hs-h2" data-to="en-hs-c2" marker-end="url(#en-hs-arr)" data-show="2-" data-hl="2"/>
    <text class="lbl" x="380" y="152" text-anchor="middle" data-show="2-">challenge (nonce, salt)</text>
    <rect id="en-hs-c3" class="box" x="116" y="196" width="8" height="8" data-show="3-"/>
    <rect id="en-hs-h3" class="box" x="636" y="196" width="8" height="8" data-show="3-"/>
    <path id="en-hs-m3" class="edge" data-from="en-hs-c3" data-to="en-hs-h3" marker-end="url(#en-hs-arr)" data-show="3-" data-hl="3"/>
    <text class="lbl" x="380" y="192" text-anchor="middle" data-show="3-">auth (proof)</text>
    <rect id="en-hs-c4" class="box" x="116" y="236" width="8" height="8" data-show="4-"/>
    <rect id="en-hs-h4" class="box" x="636" y="236" width="8" height="8" data-show="4-"/>
    <path id="en-hs-m4" class="edge" data-from="en-hs-h4" data-to="en-hs-c4" marker-end="url(#en-hs-arr)" data-show="4-" data-hl="4"/>
    <text class="lbl" x="380" y="232" text-anchor="middle" data-show="4-">pending (approval)</text>
    <rect id="en-hs-c5" class="box" x="116" y="276" width="8" height="8" data-show="5-"/>
    <rect id="en-hs-h5" class="box" x="636" y="276" width="8" height="8" data-show="5-"/>
    <path id="en-hs-m5" class="edge" data-from="en-hs-h5" data-to="en-hs-c5" marker-end="url(#en-hs-arr)" data-show="5-" data-hl="5"/>
    <text class="lbl" x="380" y="272" text-anchor="middle" data-show="5-">welcome (perms, proof)</text>
    <rect id="en-hs-c6" class="box" x="116" y="316" width="8" height="8" data-show="6-"/>
    <rect id="en-hs-h6" class="box" x="636" y="316" width="8" height="8" data-show="6-"/>
    <path id="en-hs-m6" class="edge-accent" data-from="en-hs-c6" data-to="en-hs-h6" marker-end="url(#en-hs-arr)" data-show="6-" data-hl="6"/>
    <text class="lbl" x="380" y="312" text-anchor="middle" data-show="6-">frames, inputs, terminals</text>
  </svg>
</figure>
:::

## How strong {#strength}

:::simple
There are about **35 trillion** possible codes. One address gets 5 tries, then it is banned for an hour and the code changes; the code also changes every 5 minutes. Guessing is hopeless.
:::

:::technical
| Quantity | Value |
|---|---|
| Code space | 32^9 = 2^45 = 35,184,372,088,832 |
| Online tries per address | 5 (`maxAuthFailures`), then a 1 h ban and a new code |
| Chance that 5 tries hit one code | 5 / 2^45 ≈ 1.42e-13 |
| Code lifetime | 300 s (`codeLifetime`), and one use |
| Offline guessing | impossible: an observer sees only TLS; the proof needs the transcript of a TLS session it is not part of |
:::

## Trusted devices {#trusted}

"Accept & trust" gives the device a **256-bit secret**, used with the same HMAC proof instead of the code: it reconnects without code. The registry (`devices.json`) is signed with an HMAC key (`registry.key`, mode `0600`): edited outside xdesk, it is refused and every trusted device must pair again.

## Pitfalls {#pitfalls}

- **Browsers** cannot read the certificate: they send the code inside TLS (`method = code`), without the binding. Prefer the native clients on an untrusted network.
- **cloudflared** terminates TLS at the relay: the relay sees the traffic. Every other exposure mode is end to end.
- **`xdesk --regen-cert`** makes a new identity: the native clients refuse it until the new fingerprint is confirmed.

## Check yourself {#quiz}

:::html
<details class="quiz"><summary>An attacker relays the TLS connection between the client and the host. Why does the proof fail?</summary><p>The client sees the attacker's certificate: its SHA-256 is in the transcript, so the proof is computed for that connection, not for the host's.</p></details>
<details class="quiz"><summary>Why Argon2id and not a plain hash of the code?</summary><p>A 45-bit code is small: each guess must cost 64 MiB of memory and three passes, so trying many is slow.</p></details>
<details class="quiz"><summary>What does the host's own proof in the welcome protect against?</summary><p>A fake host: without the code (or the device secret) it cannot produce it.</p></details>
:::

## Glossary {#glossary}

| Term | Meaning |
|---|---|
| Nonce | a random value used once, so an old message can't be replayed |
| Salt | random bytes mixed with the code before hashing (16 bytes per code) |
| Transcript | the label `xdesk-auth-v1` and the role, both nonces, the certificate fingerprint |
| Pinning | remembering the host fingerprint at first use and refusing any other |
| ML-KEM-768 | the post-quantum key encapsulation of the hybrid TLS group |

Sources: `include/xdesk/security/Crypto.hpp` (`XDESK_ARGON2_*`, `auth_transcript`), `PairingCode.hpp`, RFC 5929 (channel bindings), RFC 9106 (Argon2).
