+++
title = "Protocol"
summary = "The wire format between a client and the host: the port, the JSON control messages, the binary frames, the encrypted UDP video, byte by byte."
section = "reference"
order = 3
kind = "protocol"
keywords = "protocol websocket json binary frame tile jpeg solid palette copy video h264 cursor mouse key text terminal udp datagram aes-gcm hello ack nack lane handshake"
+++

Read from `include/xdesk/net/Protocol.hpp`, `Datagram.hpp` and the writers of the encoder. Protocol version `1` (`XDESK_PROTOCOL_VERSION`, bumped on every incompatible change).

## Framing {#framing}

One TLS 1.3 port (`47800` by default) serves HTTPS (`/`, `/api/info`, `/app/xdesk.apk`, `/app/version`) and the websocket `/ws` (RFC 6455); the extra video sockets of the web viewers open `/lane`.

- a **text frame** is one JSON object `{"t": "<type>", ...}` (control), 64 KiB at most;
- a **binary frame** is `[u8 type][payload]`, every integer **big endian**, 32 MiB at most (a full 4K frame);
- the LAN discovery is a UDP broadcast on port `47801`.

## Handshake {#handshake}

:::html
<ol class="seq">
  <li><div><span class="dir">client → host</span><br>TLS 1.3, group <code>X25519MLKEM768</code> first (native clients: post-quantum groups only), certificate checked against the pinned fingerprint.</div></li>
  <li><div><span class="dir">client → host</span><br><code>hello</code>: <code>v kind name os device nonce udp</code>.</div></li>
  <li><div><span class="dir">host → client</span><br><code>challenge</code>: <code>host version nonce salt kdf known</code>.</div></li>
  <li><div><span class="dir">client → host</span><br><code>auth</code>: <code>method = code</code> (web) or <code>proof</code> (native, <code>HMAC(Argon2id(code, salt), transcript)</code>), <code>device</code>, <code>remember</code>.</div></li>
  <li><div><span class="dir">host → client</span><br><code>pending</code> while the person at the PC decides (unknown device), then <code>welcome</code> (<code>session perms screen capture device proof</code>) or <code>denied</code> (<code>reason</code>).</div></li>
</ol>
:::

## JSON messages {#json}

| t | direction | fields |
|---|---|---|
| `hello` | C → S | v kind name os device nonce udp (udp: the Android app has its own UDP socket) |
| `challenge` | S → C | host version nonce salt kdf known |
| `auth` | C → S | method = code (web) or proof, device, remember |
| `pending / welcome / denied` | S → C | session perms screen capture device proof / reason |
| `ping / pong` | ↔ | ts |
| `welcome.udp` / `udp_off` | S → C / C → S | port connection down up (keys of the UDP video) / the viewer could not reach it |
| `welcome.lanes / lanes` | S → C | token max (token of the extra sockets) / n sockets to keep for the video |
| `lane` | C → S (first message of /lane) | session token index |
| `quality / mode / refresh` | C → S | quality scale fps / view or control |
| `term_open / term_resize / term_close / term_exit` | ↔ | id cols rows code |
| `perms / screen / info / bye / clip` | S → C (clip ↔) | perms, w h, text, reason, clipboard text |
| `focus_request` | C → S | control paused by the host panel: given back after 1 min unless refused |
| `input` | S → C | state (loading, ready, paused, none), error, wait, asked |

## Binary messages {#binary}

:::message Frame 0x10
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x10` |
| id | u32 | 4 | frame id |
| width | u16 | 2 | screen width |
| height | u16 | 2 | screen height |
| count | u16 | 2 | number of tiles |
| tiles | tile | var | `count` tiles, see below |
Sent when the window of the session is open; acked by a `FrameAck` on arrival. The first frame holds every tile, the next ones only the changed tiles.
:::

:::message FrameAck 0x11
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x11` |
| id | u32 | 4 | frame id |
The acks size the frames in flight and measure the ping and the bandwidth.
:::

:::message Cursor 0x12
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x12` |
| x | i16 | 2 |  |
| y | i16 | 2 |  |
| visible | u8 | 1 | 0 / 1 |
The remote cursor, drawn by the viewer.
:::

:::message Video 0x13
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x13` |
| id | u32 | 4 | frame id |
| width | u16 | 2 | rounded up to even |
| height | u16 | 2 | rounded up to even |
| flags | u8 | 1 | 1 = key frame |
| data | h264 | var | annex B access unit of the whole frame |
Only to the clients listing `h264` in `enc`, while their frames keep their order; acked like a frame.
:::

:::message MouseMove 0x20
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x20` |
| x | u16 | 2 | absolute |
| y | u16 | 2 | absolute |
Pixels of the host screen; dropped without the control right.
:::

:::message MouseButton 0x21
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x21` |
| button | u8 | 1 | see the codes |
| down | u8 | 1 | 1 pressed, 0 released |

:::

:::message MouseWheel 0x22
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x22` |
| dx | i16 | 2 | 1/120 of a notch |
| dy | i16 | 2 | 1/120 of a notch |

:::

:::message Key 0x23
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x23` |
| down | u8 | 1 | 1 pressed, 0 released |
| length | u8 | 1 | length of code |
| code | ascii | var | `KeyboardEvent.code`: `KeyA`, `ShiftLeft`... |
Physical key codes: the layout of the host applies.
:::

:::message Text 0x24
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x24` |
| text | utf8 | var | the rest of the message |
A soft keyboard sends only what changed in its input (Backspace keys + the new letters).
:::

:::message TermData 0x31
| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | `0x31` |
| id | u8 | 1 | terminal id |
| data | bytes | var | raw bytes |
C → S: the input of terminal `id`; S → C: its output.
:::

## Tiles of a Frame {#tiles}

Each tile of a `Frame` starts with `u16 x, u16 y, u16 w, u16 h, u8 enc`, then:

| enc | name | payload |
|---|---|---|
| `1` | Jpeg | u32 length, JPEG data |
| `2` | Solid | u8 r, u8 g, u8 b: the whole tile of one color |
| `3` | Palette | u8 n (1-64), n × (u8 r, g, b), u32 length, zlib stream of w × h u8 indices (lossless; clients listing palette) |
| `4` | Copy | u16 sx, u16 sy: the w × h rectangle at (sx, sy) of the frame as it was before this message (scroll; clients listing copy) |

## Encrypted UDP video {#udp}

Native viewers and the Android app, on the port of the server. A datagram is a 13-byte header in clear but authenticated, the AES-256-GCM payload and its 16-byte tag. Two random 256-bit keys per session (one per direction) come in the `welcome.udp` inside TLS; nonce = direction + sequence (never reused); a window of 2048 sequences drops replays; the host only answers an address that sent an authentic datagram.

| field | type | size | description |
|---|---|---|---|
| type | u8 | 1 | 1 Hello ... 7 Pong |
| connection | u32 | 4 | connection id of the session |
| sequence | u64 | 8 | per direction, authenticated |
| payload | AES-256-GCM | var | the fields below |
| tag | bytes | 16 | GCM tag |

| type | name | direction | payload |
|---|---|---|---|
| `1` | Hello | V → H | empty: the viewer proves the keys from its address |
| `2` | HelloAck | H → V | empty |
| `3` | Data | H → V | u32 frame, u16 index (data pieces first, then one parity per group of 8), u16 data count, u32 frame size, bytes (1150 per piece) |
| `4` | Ack | V → H | u32 frame (complete) |
| `5` | Nack | V → H | u32 frame, u16 n, n × u16 missing indexes |
| `6` | Ping | V → H | u64 ms (keeps the NAT open) |
| `7` | Pong | H → V | u64 ms (the same) |

Back to the websocket when UDP is blocked (2.5 s of unanswered hellos) or silent (4 s); a frame given up after 3 s asks a complete one.

## Codes {#codes}

Mouse buttons: `0` left, `1` middle, `2` right, `3` back, `4` forward. Client kinds: `0` gui, `1` tty, `2` web, `3` apk (an unknown kind is treated as web, the most restricted).
