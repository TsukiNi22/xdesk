+++
title = "Connect to a PC"
summary = "The four clients: the Qt viewer, the terminal client, a browser and the Android app, with their keys and gestures."
section = "guide"
order = 3
keywords = "connect client viewer qt tty terminal browser web phone android apk touch gestures keys scan known hosts fingerprint"
+++

Every client speaks to the same TLS port of the host (`47800` by default) and gets the rights the host gives it.

## The clients {#clients}

| Client | Start it | Shows |
|---|---|---|
| Qt viewer (`gui`) | `xdesk connect <address> -c <code>`, `xdesk viewer` | the screen (fit / 1:1 / full screen), terminals in tabs |
| Terminal client (`tty`) | `xdesk connect <address> --tty` | terminals, and the screen in truecolor half blocks (`--screen`) |
| Browser (`web`) | `https://<address>:<port>` | the screen, terminals (xterm.js) |
| Android app (`apk`) | the QR code, or `/app/xdesk.apk` offered by the web page | the web client full screen, certificate pinned |

```bash "Terminal"
xdesk connect 192.168.1.20 -c K7F-29Q-XM4    # Qt viewer
xdesk connect my-pc.ts.net --tty             # remote terminal in this terminal
xdesk connect bore.pub:41234 --tty --screen  # starts on the remote screen
xdesk connect --tty                          # connect page: known hosts, LAN scan, address and code
xdesk connect 192.168.1.20 --view-only       # never send keyboard / mouse
xdesk scan                                   # hosts announced on the LAN
```

The native clients pin the certificate of the host on first use (the fingerprint is shown on both sides) and refuse a changed identity. [Why](page:how-pairing-works#idea).

## Keys of the terminal client {#tty-keys}

After `ctrl+]`:

| Key | Effect |
|---|---|
| `s` / `0`, `1`-`9`, `n` `p` | screen tab, terminal N, next / previous tab |
| `t`, `w` | new terminal, close this one |
| `c`, `d` | control on / off, ctrl+alt+del |
| `+` `-`, `f`, `r` | image quality, frame rate, refresh |
| `pgup` `pgdn` | scroll back (the wheel too) |
| `q`, `?`, `ctrl+]` | disconnect, keys, a real ctrl+] |

`XDESK_TTY=ascii,16` forces what the terminal shows (`ascii`, `unicode`, `mono`, `16`, `256`, `truecolor`, `no-mouse`).

## Browser and phone {#browser}

Scan the QR code of the panel with the camera: the app or the browser opens with the code filled in. Or open the address shown in the panel and type the code (or Scan).

| Gesture (trackpad mode) | Does |
|---|---|
| slide | move the pointer |
| tap / double tap | click / double click |
| tap then slide, hold then slide | drag |
| two fingers tap / slide | right click / scroll |
| pinch | zoom |
| three fingers tap | keyboard |

Direct touch mode: tap = click there, slide = drag, hold then lift = right click. A bar gives Esc, Tab, sticky Ctrl / Alt / Shift, Win, arrows, F1-F12, Ctrl+Alt+Del and Paste.

> [!WARNING]
> The certificate is self-signed: the browser asks to accept it once (compare the fingerprint with the panel). A browser cannot read the certificate, so it sends the code inside TLS without the Argon2id binding: prefer the native clients on an untrusted network, or the [Tailscale name](page:exposure#tailscale) (real certificate).
