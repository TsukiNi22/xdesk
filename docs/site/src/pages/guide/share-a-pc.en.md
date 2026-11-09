+++
title = "Share a PC"
summary = "The host panel: the sharing switch, the code and its QR code, the approval, the sessions and their rights, the devices."
section = "guide"
order = 2
keywords = "host panel share sharing code qr approval accept trust refuse sessions rights devices ban unban tty keys tray"
+++

The person in front of the PC keeps the control: they see every session live and decide who stays.

## The panel {#panel}

`xdesk` (or `xdesk host`) opens the panel: a Qt window (Auto / Light / Dark, tray icon) or, with `--tty` or without a display, the same panel in the terminal. Both have 5 pages: **overview, sessions, devices, logs, settings**.

**Sharing starts off.** Start / Stop sharing (bottom left, the tray, `s` in the terminal panel) starts or stops the server, the exposure and the capture without quitting; each start gives a new code.

## The code {#code}

| | |
|---|---|
| Format | 9 characters of Crockford base32 (`K7F-29Q-XM4`): 45 bits |
| Rotation | after each use, every 300 s (`codeLifetime`), at each start of the sharing |
| Wrong codes | 5 from one address (`maxAuthFailures`): that address is banned 1 h and the code changes |
| QR code | `xdesk://host:port?fp=..&code=..` for the Android app (pins the certificate), `https://host:port/?code=..` for a browser |

## Approval {#approval}

An unknown device waits for **Accept / Accept & trust / Refuse** (refused after 30 s). The `approval` setting: `untrusted` (every device not trusted yet, default), `always`, `never` (the code is enough). A **trusted** device reconnects without code, with its 256-bit device secret.

## Sessions and rights {#sessions}

The sessions page shows each one live: device, client (`gui`, `tty`, `web`, `apk`), address, mode, rights, FPS, latency, bandwidth, last input. Opening one shows every input it sends and mirrors its terminals.

| Right | Allows |
|---|---|
| view | see the screen |
| control | keyboard and mouse |
| terminal | open terminals (4 at most per session) |
| clipboard | read and write the clipboard |

Rights change live. While you use the panel, remote input is paused; a viewer can ask its control back (granted after 60 s unless refused).

## Devices {#devices}

Trusted, known and banned devices and banned addresses, one record per machine. Trust, untrust, forget and unban are only possible **on this PC**, and refused while a remote session has control or a terminal. Lifting a ban asks the word `unban` typed.

```bash "Terminal"
xdesk devices                  # known / trusted / banned devices and addresses
xdesk unban <device id | address>  # host stopped, the word unban typed
```

## Keys of the terminal panel {#keys}

| Key (`?` shows them) | Effect |
|---|---|
| `1`-`5`, `tab` | pages overview / sessions / devices / logs / settings |
| `s`, `q` | start / stop sharing, quit |
| `a` `t` `r` | accept / accept & trust / refuse a connection |
| `n` `y` `enter` `c` `e` (overview) | new code, copy the code, QR code, copy its link, exposure |
| `v` `c` `t` `p` `m` `k` `b` `K` (sessions) | rights view / control / terminal / clipboard, watch its terminals, disconnect, ban, disconnect all |
| `T` `U` `f` `u` (devices) | trust, untrust, forget, unban |
| `l` `c` `/` `enter` (logs) | level, category, search, details |
