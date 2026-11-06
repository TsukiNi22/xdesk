# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

| Scope | Content |
| ----- | ------- |
| `In Progress` | Work that hasn't been completed yet (typically listed under [pre-release]) |
| `Added`       | New features, files, or sections introduced in this release |
| `Removed`     | Existing features, files, or sections that have been deleted |
| `Changed`     | Existing features or sections that were modified (behavior, structure, or content) |
| `Fixed`       | Bugs or issues that have been resolved |

> [!NOTE]
> The `(unofficial)` version are sub-versions (typically fixes) that don't have associated tag.
> A package built at this version is not guaranteed!

---

## [pre-release] (empty)

## v1.0.0 - 2026-11-05
### Added
- First version of `xdesk` (binary), set up from [cpp_project_template](https://github.com/TsukiNi22/cpp_project_template)
- Host: Qt6 panel (overview, live sessions, devices, logs, settings, tray, approval prompt, tamper banner) and terminal panel (`--tty`)
- Clients: Qt viewer (fit / 1:1 / full screen, terminals in tabs, LAN scan, known hosts), terminal client (shell passthrough, screen in truecolor half blocks), web client (xterm.js), Android application (WebView, certificate pinning, auto update)
- Security: TLS 1.3 with the `X25519MLKEM768` hybrid post-quantum key exchange, one time pairing codes proven with Argon2id and bound to the certificate, mutual authentication, trusted devices, signed device registry, bans of devices and addresses
- Tamper protection: watched binary / config / data, checked terminal commands and processes, hardened host process, ban of the sessions touching xdesk, local only unban / trust / settings
- Audit log (`jsonl` per day) and terminal transcripts
- Screen stream: dirty tiles (libjpeg-turbo / solid), frames in flight limit, remote cursor; capture DXGI (Windows), Qt (X11 / macOS), xdg-desktop-portal ScreenCast + PipeWire video stream (Wayland: vendored headers, `libpipewire` loaded at run time, no build dependency), started with the first viewer only
- Input: `SendInput` (Windows), `uinput` / XTest (Linux), physical key codes
- Terminals: `forkpty` (Unix), ConPTY (Windows)
- Exposure: LAN + discovery (`<pc>.local` when published), Tailscale (`tailscale cert` certificate served by name through SNI: the `*.ts.net` names are HSTS preloaded; the IP when HTTPS is off on the tailnet; the native clients send no SNI and keep the pinned certificate), UPnP, `bore`, `cloudflared`, `ngrok`, manual; the LAN is always open, each mode adds one path and the host refuses the peers coming through another one
- Sharing switch: the host starts with the sharing off, Start (green) / Stop (red) sharing at the bottom of the panel, in the tray and with `s` in the terminal panel, without quitting xdesk; a new code at each start
- QR code of an address + the code (`xdesk://` link with the pinned fingerprint for the Android app, `https://` link for a browser), scanners in the Android app and in the web client (BarcodeDetector, else jsQR)
- Stream settings: resolution presets (native, 1440p ... 144p, `maxHeight`) and image quality presets instead of a bare slider and scale
- Look: own "graphite + signal green" theme (square corners, small capitals, monospace for codes and addresses), Auto / Light / Dark switch in the host panel, the viewer and the web client, the Auto mode follows the system live; lists and number fields with chevrons, tables without a focus frame on the cells (one accent bar on the selected row), settings columns of the same height
- Terminal panel of the host (`--tty`): the 5 pages of the graphical one (overview with the code countdown, addresses, QR code in half blocks and exposure; sessions with rights, live activity and their terminals watched live; devices and banned addresses; logs with level / category / search / details; every setting editable), approval and confirmation boxes, mouse (tabs, rows, buttons, wheel), copy through OSC 52, frames drawn by changed rows only
- Terminal client: connect page (known hosts, LAN scan, address, port, code, view only), remote terminals in tabs drawn by the `VtScreen` emulator (status bar, scroll back, application cursor keys, bracketed paste, remote mouse programs), screen tab in truecolor half blocks with control, ctrl+alt+del, quality and frame rate; `xdesk connect --tty` without an address opens the connect page
- Stream: tiles hashed and encoded on every core, lighter JPEG while a large part of the screen moves then the same tiles sent again sharp once still, nothing computed while the capture has no new frame, PipeWire buffers reused and converted once, frames acked on arrival and decoded on every core by the native client, video paused while a client shows a terminal (the capture only starts once a client looks at the screen)
- Web terminal: xterm.js WebGL renderer, Unicode 11 widths, Nerd / powerline fonts when installed, styles allowed by the CSP (colors and attributes kept)
- Phones and tablets (web client and Android app): trackpad mode (slide = pointer, tap = click, hold then slide = drag) or direct touch, two fingers tap = right click, two fingers slide = scroll, pinch = zoom (the view follows the pointer), three fingers tap = keyboard; special keys bar (Esc, Tab, sticky Ctrl / Alt / Shift / Win, arrows, Home / End / PgUp / PgDn, Del, Enter, F1-F12, Ins, PrtSc, Ctrl+Alt+Del, Paste, keyboard) for the screen and the terminals, held keys repeat, soft keyboard letters combined with the sticky modifiers; session tools in a menu on small screens
- Terminal panel and client: every row, value arrow, banner, button and box clickable (a second click opens / changes), close buttons; colors lowered to 256 / 16 / none, ascii look-alikes without a UTF-8 locale and no mouse reports on the consoles that have none (`TERM`, `COLORTERM`, `NO_COLOR`, the locale, `XDESK_TTY` to force), a terminal that cannot draw (`TERM=dumb`) refused with a message
- Scripts: `install.sh`, `install.ps1`, `bundle_windows.sh`, `build_apk.sh`
- Unit tests: crypto, pairing code, device registry, protocol, frame encoder (motion then sharp), command line, sharing switch of the host, certificate chosen by name (SNI), terminal keys / mouse / canvas / QR code

### Changed
- Confirmations: only lifting a ban asks a typed word (`unban`), trust and forget are a yes / no, untrust and the settings need nothing more (all of them stay refused while a remote session has control or a terminal)
- Host window: grows to show the whole page (up to the screen, then the page scrolls), settings columns stacked when the window is too narrow (no horizontal scroll)

### Fixed
- A device that came back under a new id (a browser keeps one id per site address) left a second record; a ban now also bans the other records of the same machine and removes their trust
- A protected file changed by the local user (a rebuild) banned the remote sessions with a terminal: only a session acting at that moment is blamed
- Remote shells started without a locale lost the accents and symbols
- The terminal panel aborted when the QR code was asked with no address to show
- Terminal numbers kept growing: a new terminal takes the smallest free number (1 again after a close or a reconnection)

[v1.0.0]: https://github.com/TsukiNi22/external-desktop/releases/tag/v1.0.0
