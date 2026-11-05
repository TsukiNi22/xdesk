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
- Screen stream: dirty tiles (libjpeg-turbo / solid), frames in flight limit, remote cursor; capture DXGI (Windows), Qt (X11 / macOS), xdg-desktop-portal (Wayland), started with the first viewer only
- Input: `SendInput` (Windows), `uinput` / XTest (Linux), physical key codes
- Terminals: `forkpty` (Unix), ConPTY (Windows)
- Exposure: LAN + discovery, Tailscale (IP for the browsers: the `*.ts.net` names are HSTS preloaded, MagicDNS name for the native clients), UPnP, `bore`, `cloudflared`, `ngrok`, manual; the LAN is always open, each mode adds one path and the host refuses the peers coming through another one
- Sharing switch: the host starts with the sharing off, Start (green) / Stop (red) sharing at the bottom of the panel, in the tray and with `s` in the terminal panel, without quitting xdesk; a new code at each start
- QR code of an address + the code (`xdesk://` link with the pinned fingerprint for the Android app, `https://` link for a browser), scanners in the Android app and in the web client (BarcodeDetector, else jsQR)
- Look: own "graphite + signal green" theme (square corners, small capitals, monospace for codes and addresses), Auto / Light / Dark switch in the host panel, the viewer and the web client, the Auto mode follows the system live
- Scripts: `install.sh`, `install.ps1`, `bundle_windows.sh`, `build_apk.sh`
- Unit tests: crypto, pairing code, device registry, protocol, frame encoder, command line, sharing switch of the host

[v1.0.0]: https://github.com/TsukiNi22/external-desktop/releases/tag/v1.0.0
