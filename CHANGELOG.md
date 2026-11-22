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

## v1.9.2 - 2026-11-22
### Fixed
- Zoom: the part of the screen sent to a zoomed viewer was computed on the box of the canvas element instead of the frame drawn letterboxed in it, so the host sent the tiles of another area (old pixels where the viewer looked): the view is the part of the drawn frame seen through the viewport, the resolution asked follows the drawn frame too
- Zoom: no copy tile (scroll) from a tile changed while it was out of the view (the viewer did not have its content)
- A new frame size (a zoom asks another resolution) keeps the old picture scaled in the web client and the native viewers instead of black while the tiles of the new size come

## v1.9.1 - 2026-11-22
### Changed
- The default clipboard right needs the control right: a session that only watches never gets what is copied on the host (it can still be given in the rights of the session)
- Documentation: top-off, only the shown part, video at the link bitrate, cursor drawn by the viewers, security notes of the clipboard (README, site)
### Fixed
- Security: the X11 clipboard bridge ignores the X errors (a local client destroying its window while asking the clipboard made Xlib exit the host, banning its sessions at the next start) and stops alone when Xwayland goes away
- Security: the web client inflates a `TermPacked` never past its announced size (16 MiB at most); the native viewers decode a cursor png only when its header says 512 x 512 or less

## v1.9.0 - 2026-11-22
### Changed
- Top-off sooner and sharper: 150 ms back to the quality of the stream, 500 ms up to 90-95 (was 300 ms / 1 s, +20)
### Added
- Video sized for the link (the rate control of Chrome Remote Desktop): once the link of a viewer is the bottleneck, the GPU encoders run in CBR at 80 % of its measured bandwidth with about two frames of buffer (changes at most every 2 s, by 30 %: live on NVENC / Quick Sync, reopened on VA-API), so a large move never builds a queue; openh264 stays at a quantizer (its rate control does not hold low targets on screen content)
- Large updates sized for the link: an update the link cannot carry in two frame intervals is sent lighter at once (jpeg size model per quality, learnt on the tiles sent), then topped off once still
- Cursor drawn by the viewers (Wayland: the portal cursor as metadata, no more in the frames): its shape is sent when it changes (`CursorShape`, png + hotspot), the web client, the app and the Qt viewer show their own cursor with that shape while controlling (no round trip), the remote one only when the mouse is elsewhere; `{"t": "cursor", "embedded"}` tells when the host draws it in its frames

## v1.8.0 - 2026-11-22
### Changed
- Resolution "Native" is now "Auto (native max)": never above the host, never above what the display shows
### Added
- Top-off of a still screen (the way of Chrome Remote Desktop): a tile unchanged for 300 ms goes back to the quality of the stream, after 1 s it is sent again sharper (+20, 4:4:4 chroma: no color bleeding around text), within a byte budget taken from the measured bandwidth and only while the link has no queue; the stream keeps encoding while tiles wait, even without a new capture
- Only what the viewer shows: a pinch zoom (web, app) or a 1:1 view larger than the window (web, Qt) tells the host the part shown (`view`), only its tiles are sent (one tile of margin), the rest once shown; the resolution asked follows the pixels the display shows (a 4K host on a phone gets about the phone resolution, a zoom asks more: the zoomed part in more detail)

## v1.7.1 - 2026-11-22
### Fixed
- Qt host panel: a page change no longer resizes / moves the window (it was grown to each page, never shrunk back, and grown again after the user made it smaller); the window is sized once at its first show for the largest page (90 % of the screen at most), the pages scroll inside it
- Qt viewer: the 1:1 mode no longer forces the window to the size of the remote screen (a 4K host made it larger than the screen, kept on the terminal tabs): the screen scrolls inside its tab (the wheel scrolls it while viewing only)

## v1.7.0 - 2026-11-22
### Changed
- Documentation: GPU video, `xdesk bench` results, shared clipboard, text line, gestures, keyboard navigation, `TermPacked` (README, site); the commands page shows `[` `]` and `|` of the usages as they are
### Added
- Video encoder per viewer from the bench: while its link is the bottleneck, the encoder giving the most frames on its measured bandwidth and ping is used (the fastest with room on the link, the smallest frames on a slow one), looked at again when the bandwidth moves by 30 %

## v1.6.0 - 2026-11-22
### Added
- Compressed terminal output (`TermPacked`, zlib): 3.6x on a build log, for the viewers off the LAN (the bench: zlib pays below about 440 Mb/s, the tiles do not compress)

## v1.5.0 - 2026-11-21
### Changed
- The automatic resolution never goes under 1080p (native first: the quality goes down, then the frame rate, 1080p last), unless the viewer asked less
- Faster BGRX -> YUV conversion (no bound check inside the rows): 4K in 5 ms on every core
### Added
- Hardware video: the large moves are encoded by the GPU (NVENC, Quick Sync, VA-API through the FFmpeg 8 libraries loaded at run time, headers vendored in `third_party/ffmpeg`) in AV1, HEVC or H.264 (constrained baseline, like openh264), up to 4K at about 60 fps (openh264 stays the fallback, up to 1920 x 1200); the codec travels in bits 1-2 of the flags of the `Video` message and the clients list `hevc` / `av1` in `enc` when they decode them (WebCodecs in the browsers and the app, libavcodec in the native viewers); settings `hardwareVideo` and `videoCodec` (auto, h264, hevc, av1)
- `xdesk bench [heights]`: every way of sending the screen (tiles, tiles + palette + copy, openh264, each GPU encoder of the computer) measured on synthetic scenes (typing, scroll, dragged window, full screen video) at 1080p / 4K (time, bytes, PSNR), played on six simulated networks (LAN to 2 Mb/s, 250 ms, 5 % loss), plus the compression of the terminal output; saved to `bench.json` in the data folder

## v1.4.0 - 2026-11-21
### Added
- Keyboard navigation: Tab / arrows / Home / End / Enter / Escape in the web client and the app (tabs, toolbar, menu, special keys, dialogs) and in the Qt windows (Enter clicks the focused button, arrows along the toolbars, the toolbar buttons reachable with Tab); while the remote screen has the focus every key goes to the remote computer (Tab too), Ctrl+Alt+Shift gives the keys back to the page / window

## v1.3.0 - 2026-11-21
### Changed
- Touch gestures of Chrome Remote Desktop: three fingers tap = middle click, three fingers swipe up = keyboard, down = special keys, a gray circle under the finger when a hold starts a drag (trackpad) or a right click (direct), the two fingers scroll goes on after the fingers leave (fling)

## v1.2.0 - 2026-11-21
### Added
- Shared clipboard both ways: a copy on the host reaches the viewers that keep the automatic clipboard on (default, can be turned off), a copy on a viewer reaches the host (web: when the page gets the focus back and at Ctrl+V through the paste event; app: the Android clipboard; Qt viewer; terminal client through OSC 52), buttons to force one way; on Wayland the host goes through the X11 selection of Xwayland (libX11 / libXfixes loaded at run time) since a background Wayland client cannot see the clipboard; host setting `allowClipboard` (on)

## v1.1.0 - 2026-11-21
### Changed
- Documentation site in the better-html-doc style ([tsukini22.github.io/xdesk/docs](https://tsukini22.github.io/xdesk/docs/)): user guide, concepts (pairing, screen stream), commands and settings generated from the sources, protocol, files, security and tamper protection, EN / FR with search; sources in `docs/site/`; the README points to it, the my-universe setup replaces the notes of the private repository
### Added
- Text line above the special keys (web client and app): Send types the text as it is, Send with an empty line sends Enter, an erase key next to it (held: repeats), like Chrome Remote Desktop

## v1.0.0 - 2026-11-09
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
- Video over UDP for the Qt viewer and the terminal client (setting `udp`, on by default): AES-256-GCM datagrams on the port of the server with two per-session keys sent inside the TLS session, replay window, nothing sent to an address that did not prove the keys, xor parity per 8 pieces while the path loses, nacks and probes, frames completing in any order with per-tile frame ids on the viewer, back to TCP when UDP is blocked or silent; UPnP maps UDP too; the transport shown per session in both panels
- Extra sockets for the video of the web viewers (setting `lanes`, on by default): up to 4 websockets joined on `/lane` with a token of the session (frames and their acks only), each frame on the least loaded one, opened while one connection holds the frames back and kept only when they help (one socket on a LAN, the link itself full = given back), `XDESK_LANES=1..4` to force; measured with Linux TCP at 1-3 % loss: x2 to x4 of throughput
- Android app: video over the encrypted UDP of the native viewers (`UdpVideo` in Java, frames handed to the page through a message port of the pinned origin), back to TCP when UDP does not get through
- Web client: frames drawn in any order of arrival (each 64×64 tile keeps the id of the frame that drew it, a late frame of the old size dropped), acks on the socket that brought the frame
- Stream flow control: frames in flight sized from the ping and the bandwidth measured by the acks (2 on a LAN, up to 8 for a far viewer: 30 fps at 120 ms of ping instead of 14), JPEG quality lowered while a standing queue builds up then raised again (a saturated 4 Mbit/s link: 24 fps instead of 8 for about the same lag; losses and jitter of a 4G link no longer lower it), frames paced on the slots of the asked rate (30 fps instead of 27)
- Stream: tiles hashed and encoded on every core, lighter JPEG while a large part of the screen moves then the same tiles sent again sharp once still, nothing computed while the capture has no new frame, PipeWire buffers reused and converted once, frames acked on arrival and decoded on every core by the native client, video paused while a client shows a terminal (the capture only starts once a client looks at the screen)
- Web terminal: xterm.js WebGL renderer, Unicode 11 widths, Nerd / powerline fonts when installed, styles allowed by the CSP (colors and attributes kept)
- Phones and tablets (web client and Android app): trackpad mode (slide = pointer, tap = click, hold then slide = drag) or direct touch, two fingers tap = right click, two fingers slide = scroll, pinch = zoom (the view follows the pointer), three fingers tap = keyboard; special keys bar (Esc, Tab, sticky Ctrl / Alt / Shift, Win (tap = the key itself, hold = sticky for a combination), arrows, Home / End / PgUp / PgDn, Del, Enter, F1-F12, Ins, PrtSc, Ctrl+Alt+Del, Paste, keyboard) for the screen and the terminals, held keys repeat, soft keyboard letters combined with the sticky modifiers; session tools in a menu on small screens
- Terminal panel and client: every row, value arrow, banner, button and box clickable (a second click opens / changes), close buttons; colors lowered to 256 / 16 / none, ascii look-alikes without a UTF-8 locale and no mouse reports on the consoles that have none (`TERM`, `COLORTERM`, `NO_COLOR`, the locale, `XDESK_TTY` to force), a terminal that cannot draw (`TERM=dumb`) refused with a message
- Scripts: `install.sh`, `install.ps1`, `bundle_windows.sh`, `build_apk.sh`
- Unit tests: crypto, pairing code, device registry, protocol, frame encoder (motion then sharp), command line, sharing switch of the host, certificate chosen by name (SNI), terminal keys / mouse / canvas / QR code

### Changed
- Automatic resolution (setting `autoResolution`, on by default; graphical and terminal panels): a viewer whose link cannot follow even at the lowest quality (a standing queue or stalls at the floor) gets the next lower resolution (1, 0.75, 0.56, 0.42, 0.32 of the asked one, at most one step per 1.5 s), raised again one step after 3 s of calm at full quality
- Web client, Android app and Qt viewer: resolution asked to the host next to the quality (native, 2160p ... 144p, never above the screen of the host, the scale of the viewer down to 0.05) and the resolution of the frames received shown (ex: `720p`)
- Palette tiles (`TileEncoding::Palette`, for the clients whose hello lists `palette` in `enc`: web client, Android app, Qt viewer, terminal client): a tile of 64 colors at most (text, ui, terminals) sent lossless as a palette + zlib indices when that is under a third of its pixels; measured on a 1080p desktop of text: 47 % fewer bytes for a complete frame (714 -> 378 KiB) and 38 % for a scroll (377 -> 234 KiB), sharp text, for twice the encoding time (8 -> 16 ms), so only for the viewers whose link is the bottleneck (ping over 20 ms, a queue, a lowered quality)
- Scroll detection (`TileEncoding::Copy`, for the clients whose hello lists `copy` in `enc`: web client, Android app, Qt viewer): the changed tiles found a few rows higher or lower in the previous frame are sent as copies of what the viewer already has (line hashes of each tile column, vote of the offsets, exact check of the pixels), only the revealed lines are encoded (a scroll of 37 rows of an editor: 21 KiB instead of 237 KiB); only while the frames of the viewer keep their order (one TCP socket: no UDP, no extra lane), a complete frame when that stops after copies were sent
- H.264 video (setting `h264`, on by default; graphical and terminal panels): while the link of a viewer is the bottleneck (ping over 20 ms, a queue, a lowered quality), a large move with no scroll found (a video, a dragged window) goes as one H.264 frame of the whole screen (`Binary::Video`, for the clients whose hello lists `h264` in `enc`: browsers and Android app with WebCodecs, Qt viewer with openh264), then the screen is sent again sharp in tiles once still; openh264 2.6+ loaded at run time (headers vendored in `third_party/openh264`, BSD), screen content real time mode, constant quantizer from the quality, no B frame, up to 1920×1200 (a 1080p frame that moves entirely: 47 KiB instead of 714 KiB of JPEG tiles); the viewers decoding it get no extra socket (the frames must keep their order), a broken frame asks a refresh and waits for a key frame; packages suggest `openh264`, `install.sh` installs it when it can
- Automatic resolution never under 720p (the text stays readable) unless the viewer asked less
- Android app: the video over the udp of the app is off by default (each frame crossed the ui thread of android as base64 text: latency, cpu of the phone); the websockets (+ extra sockets) carry it, `udp = 1` in the storage of the page turns it back on
- Android app: the connect screen starts on the last address tried (even a failed one, a bad link or a QR code), the 5 last addresses listed under it (tap = connect, long press = remove)
- Android app: version `x.y.z.<commits>` (version code `x.y.z * 1000 + commits`), so a new apk of the same version is offered as an update by the host
- Web viewer and host: 60 fps by default (the flow control still lowers it when the link cannot follow)
- Confirmations: only lifting a ban asks a typed word (`unban`), trust and forget are a yes / no, untrust and the settings need nothing more (all of them stay refused while a remote session has control or a terminal)
- Host window: grows to show the whole page (up to the screen, then the page scrolls), settings columns stacked when the window is too narrow (no horizontal scroll)
- Terminal panel and client restyled: rounded cards titled on their border (sharing with the code and a colored countdown, reach, addresses, system; sessions, devices, log; settings in two columns), selected rows with an accent bar, `● on` / `○ off` toggles, state marks (`✓ trusted`, `⊘ banned`, `● control`), underlined current page, connect page in cards; small terminals stack the cards and drop the ones that do not fit

### Fixed
- A device that came back under a new id (a browser keeps one id per site address) left a second record; a ban now also bans the other records of the same machine and removes their trust
- A protected file changed by the local user (a rebuild) banned the remote sessions with a terminal: only a session acting at that moment is blamed
- Remote shells started without a locale lost the accents and symbols
- The terminal panel aborted when the QR code was asked with no address to show
- Terminal numbers kept growing: a new terminal takes the smallest free number (1 again after a close or a reconnection)
- Phone keyboards: a suggestion or an autocorrection typed the whole word (or the text before) again in the screen and the terminals; only the difference is sent now, every letter at once (no wait for the end of the word)
- Remote control paused by the focus of the host panel: the terminal panel (`--tty`) pauses it too while its terminal has the focus (focus reports, not over ssh); a viewer stuck there (nobody can take the focus off the panel) asks its control back from the banner of the screen (`focus_request`), granted after the delay of the settings (`focusRequestDelay`, 60 s by default; `focusRequest` = off: no request) unless the person at the computer refuses (banner with Refuse in the graphical panel, a key / click in the terminal panel) or uses the panel; only that viewer gets it, a key / click in the panel pauses it again, and a request whose viewer leaves before the end is dropped (the panel keeps the focus)
- A host stopped by a signal (logout, shutdown, `systemctl stop`, a local `kill`) left an unclean stop: the device and the address of a viewer with control were banned at the next start; SIGTERM / SIGINT / SIGHUP now stop it cleanly, and only a remote session acting at that moment (keyboard / mouse in the last 5 s, a command in its terminal) is blamed (SIGKILL / a crash stay unclean)
- Host panel: the base line of the tab bars was cut by the selected tab
- Banned devices and addresses got an empty response (the address was dropped before TLS) and a refused connection a bare "Refused by the host": they are told they are banned (with the reason and until when) and the web client, the app and the viewer show it; a refusal or a timeout of the local approval says what happened
- Text typed by a viewer on Wayland (uinput) used the keys of the US layout: on an azerty host "a" came out "q", "m" came out ","; the layout of the session (GNOME input source, else localectl / `XKB_DEFAULT_*`) is compiled with libxkbcommon (loaded at run time, no build dependency) and each character is typed with its own key and shift / AltGr, an accented letter without a key through its dead key, anything else through the unicode input (ctrl+shift+u)
- Setting "Automatic bans" (`banOnTamper`, on by default) now also covers the 1 h ban of an address typing too many wrong codes (off: the code is renewed, nobody banned)
- Phones: a double tap was two single clicks (the second finger lands 15-30 host pixels away, more than the double click distance of the desktop; the jitter of the finger moved the trackpad pointer), a tap of 450-550 ms did nothing, a hold in direct touch right clicked at once and could not drag: the second tap clicks on the first point, the trackpad's second tap holds the button with the finger (double click, tap then slide = drag), a slow tap clicks, a direct hold right clicks when lifted and drags when it slides; in both modes the second tap holds the button while the finger stays (double click + hold / slide selects by words, as with a mouse)
- Phones: the tooltips of the buttons showed on a tap and stayed on screen (some phones report a hover: no native tooltip in the app, with a coarse pointer or after the first touch, the titles added later included; no sticky hover)
- Web client: nothing told why the first taps did nothing (no frame yet, input of the host opening with its capture), and the host said "Remote control unavailable" while its capture was only starting: the host sends the state of its input (`input`: loading / ready / paused / none + reason), the screen shows "Loading the screen", "Getting the controls ready", the reason when unavailable, then "Controls ready"; the remote control paused while the host panel has the focus (on purpose: nobody drives the panel from afar) said nothing: the viewer shows "Control paused: the xdesk panel is in use on the computer" (the banner as wide as its text up to 560px, not squeezed on several lines)
- Clicks and keys of a viewer did nothing without a word when the host had no input backend: the viewer is told why, and the backend is tried again when it asks for control (`/dev/uinput` rights given meanwhile); the uinput error shows its real reason
- A device unbanned in the panel kept the ban of its address (its viewer got a bare connection reset), an address unbanned kept its devices banned: both buttons lift the device, the other records of the machine and the address together now, and a banned address that connects is logged once
- A device trusted again after a ban still asked the code at each connection without saying why (the ban erases its key): its state shows `trusted · code once`, that one code needs no approval and gives it a new key
- `scripts/install.sh --uinput`: the remote control works at once for the user at the screen (`uaccess`), no new login needed

[v1.9.2]: https://github.com/TsukiNi22/xdesk/compare/v1.9.1...v1.9.2
[v1.9.1]: https://github.com/TsukiNi22/xdesk/compare/v1.9.0...v1.9.1
[v1.9.0]: https://github.com/TsukiNi22/xdesk/compare/v1.8.0...v1.9.0
[v1.8.0]: https://github.com/TsukiNi22/xdesk/compare/v1.7.1...v1.8.0
[v1.7.1]: https://github.com/TsukiNi22/xdesk/compare/v1.7.0...v1.7.1
[v1.7.0]: https://github.com/TsukiNi22/xdesk/compare/v1.6.0...v1.7.0
[v1.6.0]: https://github.com/TsukiNi22/xdesk/compare/v1.5.0...v1.6.0
[v1.5.0]: https://github.com/TsukiNi22/xdesk/compare/v1.4.0...v1.5.0
[v1.4.0]: https://github.com/TsukiNi22/xdesk/compare/v1.3.0...v1.4.0
[v1.3.0]: https://github.com/TsukiNi22/xdesk/compare/v1.2.0...v1.3.0
[v1.2.0]: https://github.com/TsukiNi22/xdesk/compare/v1.1.0...v1.2.0
[v1.1.0]: https://github.com/TsukiNi22/xdesk/compare/v1.0.0...v1.1.0
[v1.0.0]: https://github.com/TsukiNi22/xdesk/releases/tag/v1.0.0
