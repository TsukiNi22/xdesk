# xdesk

> [!TIP]
> Documentation [TsukiNi22/external-desktop](https://tsukini22.github.io/external-desktop/docs/) (v1.0.0): technical documentation, protocol and the interactive project graph.

Remote desktop and remote terminal in **one binary** for Windows and Unix: run `xdesk` on a PC, give its one time code, and someone can see / control its screen or open terminals on it, from the native viewer (Qt or terminal), a browser or the Android app.
Every connection is end to end **TLS 1.3 with a post-quantum hybrid key exchange (X25519 + ML-KEM-768)**, the code is proven with **Argon2id** bound to the host certificate, and the person in front of the PC sees everything each session does, live, and decides who stays (trust, kick, ban).

### Table of Contents
 - [Features](#features)
 - [Security](#security)
 - [Dependencies](#dependencies)
 - [Quick Setup - 0 (scripts)](#quick-setup---0-scripts)
 - [Packages](#packages)
 - [Quick Setup - 1 (all)](#quick-setup---1-all)
 - [Quick Setup - 2 (Limited)](#quick-setup---2-limited)
 - [Usage](#usage)
 - [Exposure (LAN / external)](#exposure-lan--external)
 - [Phone & browser](#phone--browser)
 - [Windows (cross compilation)](#windows-cross-compilation)
 - [Unit tests](#unit-tests)
 - [Known limits](#known-limits)
 - [Pre-commit hook](#pre-commit-hook)
 - [Workflows/Release](#workflowsrelease)

## Features

| Feature | Content |
| ------- | ------- |
| Host panel | Qt6 window (light / dark, tray icon) or full screen terminal panel (`--tty`, automatic without display) |
| Pairing | One time code (`K7F-29Q-XM4`, Crockford base32, 45 bits), rotated after each use and on a timer, copy / new code |
| Approval | Accept / Accept & trust / Refuse prompt for unknown devices (30 s, refused by default), or always / never |
| Sessions | Live table: device, client (`gui`, `tty`, `web`, `apk`), address, mode, rights, FPS, latency, bandwidth, last input |
| Live view of a session | Every input (keys, clicks, wheel, text, clipboard), mirror of its terminals with what it types |
| Rights | Per session: see the screen, control keyboard / mouse, open terminals, clipboard - changed live |
| Devices | Trusted (reconnect without code), known, banned devices and banned addresses; unban / trust only locally |
| Tamper protection | A session touching xdesk (its binary, config, data, process) is banned at once (see [Security](#security)) |
| Audit log | `jsonl` per day (connections, decisions, inputs, terminal commands) + full terminal transcripts |
| Screen | Dirty 64x64 tiles (JPEG via libjpeg-turbo or solid color), max 2 frames in flight (latency over throughput), remote cursor |
| Capture | Windows DXGI Desktop Duplication, X11 / macOS through Qt, Wayland through the xdg-desktop-portal (PipeWire) |
| Input | Windows `SendInput` (scancodes + unicode), Linux `uinput` (Wayland and X11) or XTest, physical key codes (layout of the host) |
| Terminals | Linux / macOS `forkpty`, Windows ConPTY; up to 4 per session, resizable, `xterm-256color` |
| Clients | Qt viewer (fit / 1:1 / full screen, terminals in tabs), terminal client (shell passthrough or the screen in truecolor half blocks), web client, Android APK |
| Exposure | LAN (+ discovery), Tailscale, UPnP, `bore`, `cloudflared`, `ngrok`, manual port forward |

## Security

| Layer | What |
| ----- | ---- |
| Transport | TLS 1.3 only, `X25519MLKEM768` hybrid key exchange first (OpenSSL >= 3.5); native clients refuse anything not post-quantum |
| Host identity | Self-signed ECDSA P-256 certificate, pinned by the native clients on first use (fingerprint shown on both sides), a changed identity is refused |
| Code proof (native) | `HMAC(Argon2id(code, salt), transcript)` with `transcript = nonces + sha256(certificate)`: no offline brute force, no relay by a man in the middle (RFC 5929 binding) |
| Mutual auth | The host answers with its own proof: a fake host that does not know the code / the device secret is detected |
| Trusted devices | 256 bits device secret, same HMAC proof, given only when the host trusts the device |
| Brute force | `maxAuthFailures` wrong codes from one address -> 1 h ban + new code |
| Registry | `devices.json` signed with an HMAC key (`registry.key`, `0600`): edited outside xdesk -> refused, every trusted device must pair again |
| Web | Strict CSP (`default-src 'self'`), no-store, no inline script, `frame-ancestors 'none'` |

> [!IMPORTANT]
> **Tamper protection**: a connected session must never change or stop the program it is connected through.
> - the binary, the config folder and the data folder are hashed and watched: a change while a session controls the PC or has a terminal bans those sessions;
> - every line typed in a remote terminal is checked **before** its Enter reaches the shell (paths of xdesk, its name, `kill` / `taskkill` / `systemctl` of its pid...) and the processes started from the remote terminals are scanned (Linux: command line and open files);
> - the host process is hardened (Linux: not dumpable / not traceable; Windows: terminate, memory and thread rights removed), a host killed during a remote session bans those devices at the next start;
> - remote input is paused while the local user uses the panel (Windows: the panel is excluded from the capture);
> - unban, trust and settings are refused while a remote session has control or a terminal, and need a code typed on the PC (proof of presence); `xdesk unban` refuses pipes and shells started by xdesk.

> [!WARNING]
> A remote terminal runs with the rights of your user: the checks above stop the usual ways of touching xdesk, not a determined attacker with a shell. Give terminals only to people you trust (the right can be removed live).

## Dependencies

> [!CAUTION]
> The MIT license of this project ([LICENSE](LICENSE)) only applies to the code of `xdesk` itself, not to its dependencies (Qt 6: LGPLv3, OpenSSL: Apache 2.0, libjpeg-turbo: BSD-style, xterm.js: MIT).

| Name + Link | Status | Last Update |
| ----------- | ------ | ----------- |
| [libutils](https://github.com/TsukiNi22/libutils) | ![CD - Dispatch](https://github.com/TsukiNi22/libutils/actions/workflows/dispatch.yml/badge.svg) | ![](https://img.shields.io/github/last-commit/TsukiNi22/libutils) |

| Name | Version | Fedora (`dnf`) | Debian/Ubuntu (`apt`) | Arch (`pacman`) |
| ---- | ------- | -------------- | --------------------- | --------------- |
| `clang++` / `cmake` | C++20 / `>= 3.20` | `clang cmake` | `clang cmake` | `clang cmake` |
| `python3` | `>= 3.8` | `python3` | `python3` | `python` |
| `libutils` | `4.x` (`>= 4.1.0`) | `libutils` | `libutils` | `libutils` |
| Qt 6 (Core Gui Widgets Network) | `>= 6.4` | `qt6-qtbase-devel` | `qt6-base-dev` | `qt6-base` |
| OpenSSL | `>= 3.2` (`3.5` for ML-KEM) | `openssl-devel` | `libssl-dev` | `openssl` |
| libjpeg-turbo | any | `libjpeg-turbo-devel` | `libjpeg-turbo8-dev` | `libjpeg-turbo` |
| PipeWire **[OPTIONAL]** | `0.3` | `pipewire-devel` | `libpipewire-0.3-dev` | `pipewire` |
| XTest **[OPTIONAL]** | any | `libXtst-devel` | `libxtst-dev` | `libxtst` |
| GoogleTest (tests only) | any | `gtest-devel` | `libgtest-dev` | `gtest` |
| MinGW-w64 (Windows only) | GCC `>= 13` | `mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-openssl mingw64-libjpeg-turbo mingw64-libutils` | - | - |
| Android SDK (APK only) | build-tools `>= 34` | [Android SDK](https://developer.android.com/studio#command-line-tools-only) | same | same |
| `ccache` **[OPTIONAL]** | any | `ccache` | `ccache` | `ccache` |

> [!NOTE]
> The optional backends are detected by CMake (`xdesk backends: dbus=... pipewire=... xtest=...`): without PipeWire, a Wayland host captures through the portal screenshot API (a few frames per second); without XTest, the input goes through `uinput`.

## Quick Setup - 0 (scripts)

```bash
# Linux / macOS: dependencies + libutils + build + ~/.local/bin/xdesk + menu entries
git clone https://github.com/TsukiNi22/external-desktop.git && cd external-desktop
scripts/install.sh --deps --apk --uinput   # --apk: embed the android app, --uinput: remote control on Wayland
scripts/install.sh --autostart             # host panel at login (tray)
scripts/install.sh --uninstall
```

```powershell
# Windows (PowerShell): from the release zip (or run it from the extracted zip)
powershell -ExecutionPolicy Bypass -File install.ps1             # %LOCALAPPDATA%\Programs\xdesk, PATH, Start menu, firewall
powershell -ExecutionPolicy Bypass -File install.ps1 -Autostart
powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
```

| Script | Effect |
| ------ | ------ |
| `scripts/install.sh` | `--deps` (dnf / apt / pacman / brew + libutils), `--apk`, `--uinput` (udev rule + group `xdesk`), `--autostart`, `--prefix DIR`, `--uninstall` |
| `scripts/install.ps1` | `-Zip <file>`, `-Prefix`, `-Port`, `-Autostart`, `-NoFirewall`, `-Uninstall` (private repository: needs `gh auth login` to download the release) |
| `scripts/bundle_windows.sh` | Fedora: cross compiles `xdesk.exe` and packs every dll (Qt, plugins, OpenSSL) in `dist/xdesk-windows-x64.zip` |
| `scripts/build_apk.sh` | Builds `android/build/xdesk.apk` without Gradle (aapt2, javac, d8, apksigner); embedded in the binary at the next build |

## Packages

> [!NOTE]
> Every package has a pre-release/unstable version named `<package_name>-pre`.
> The `-pre` packages are marked as obsolete by any release/stable package (without `-pre`) of the same version or higher.
>
> The same packages exist as RPM (Fedora), DEB (Debian/Ubuntu) and Arch Linux (pacman, `.pkg.tar.zst`): same names, content and dependencies (on Arch the libraries are in `/usr/lib`).

| File Name | Content |
| --------- | ------- |
| `xdesk` | Install the default package (`xdesk-op`) |
| `xdesk-op` | Executable compiled with the optimization options |
| `xdesk-db` | Executable compiled with the debug options (intended for debugging only) |
| `xdesk-as` | Executable compiled with the asan options (intended for debugging only) |

> [!WARNING]
> The three variants install the same `/usr/bin/xdesk`: only one of them can be installed at a time.

**Windows** (MinGW-w64 cross compilation, installed in the MinGW sysroot: `/usr/x86_64-w64-mingw32/sys-root/mingw` on Fedora, `/usr/x86_64-w64-mingw32` on Debian/Ubuntu): the same packages prefixed with `mingw64-` (ex: `mingw64-xdesk`), without the `-as` variants (no AddressSanitizer with MinGW).

> [!NOTE]
> The content of the packages is chosen at the top of the `CMakeLists.txt`: `XDESK_KIND` (`binary`) sets the defaults of `XDESK_PACKAGE_BIN` (executable), `XDESK_PACKAGE_LIB` (library, `XDESK_SHARED_LIB` for a `.so`) and `XDESK_PACKAGE_DEV` (headers + CMake config), each one can be forced with `-D...=ON/OFF`.
> After a change, update `PACKAGE_BIN` / `PACKAGE_LIB` / `PACKAGE_DEV` in `.github/workflows/build.yml` and `build-packages.yml`.

## Quick Setup - 1 (all)
> Setup into `/usr/local`

### Clone the repository
```bash
git clone https://github.com/TsukiNi22/external-desktop.git
cd external-desktop
```

### Build & install
```bash
export BUILD_DIR=build
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Optimized
cmake --build $BUILD_DIR --parallel $(nproc)                       # ./xdesk at the root
sudo cmake --build $BUILD_DIR --target install --parallel $(nproc) # /usr/local/bin/xdesk
```

## Quick Setup - 2 (Limited)
> Setup into `/usr`

> [!WARNING]
> If during the installation using tools like `dnf`, `apt`... you see an `invalid hash` error, reset the cache of the tool or relaunch the `setup.sh` script.
>
> If the error still persists, it might be due to an error in the repository.

> [!WARNING]
> Restriction: `fedora-based (rpm)`, `debian-based (deb)`, `arch-based (pacman)`

> [!NOTE]
> The usage of `sudo` in the script can be removed with the `--no-sudo` argument.

Run the setup script directly, without cloning the repository. It registers the package repository (`gh-pages` branch) so the system package manager can install the packages:
1. Detect the distribution from `/etc/os-release` (`rpm`, `deb` or `arch` based).
2. `rpm`: add `/etc/yum.repos.d/xdesk.repo` and import the GPG key.
3. `deb`: add the GPG key in `/etc/apt/keyrings/xdesk.gpg`, the source in `/etc/apt/sources.list.d/xdesk.list`, then `apt-get update`.
4. `arch`: add the GPG key to the pacman keyring (`pacman-key --add` + `--lsign-key`), the `[xdesk]` repository in `/etc/pacman.conf` (`SigLevel = Required`), then `pacman -Sy`.
5. With `--install` only: install the package `xdesk` (`--pre` for the unstable one, `--windows` for `mingw64-xdesk`, rpm / deb only).

```bash
wget -qO- https://raw.githubusercontent.com/TsukiNi22/external-desktop/main/setup.sh | bash -s
```

or with `curl`:

```bash
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/external-desktop/main/setup.sh | bash -s
```

| Argument | Effect |
| -------- | ------ |
| `--no-sudo` | Run without `sudo` (the script must already run as root) |
| `--install` | Also install the package (by default only the mirror & GPG key are setup) |
| `--pre` | Select the pre-release/unstable package (`xdesk-pre`) |
| `--windows` | Select the Windows packages (`mingw64-xdesk`, MinGW sysroot, rpm / deb only) |
| `-h`, `--help` | Display the usage |

## Usage

```bash
xdesk                                        # share this PC: graphical panel (terminal panel without display)
xdesk host --tty                             # share this PC with the terminal panel
xdesk host --expose tailscale -p 47900       # this run only: exposure and port
xdesk connect 192.168.1.20 -c K7F-29Q-XM4    # control a PC (Qt viewer)
xdesk connect my-pc.ts.net --tty             # remote shell in this terminal (ctrl+] q to quit)
xdesk connect bore.pub:41234 --tty --screen  # the remote screen in this terminal (ctrl+] c control)
xdesk connect 192.168.1.20 --view-only       # never send keyboard / mouse
xdesk viewer                                 # Qt viewer with its connect page (LAN scan, known hosts)
xdesk scan                                   # hosts announced on the LAN
xdesk devices                                # known / trusted / banned devices and addresses
xdesk unban <device id | address>            # local unban (host stopped, typed confirmation)
xdesk paths                                  # config / data / logs folders
xdesk --regen-cert                           # new host identity (clients confirm the new fingerprint)
```

| Panel key (`--tty`) | Effect |
| ------------------- | ------ |
| `↑` `↓`, `tab` / `1` `2` `3` | Select a session, pages sessions / devices / logs |
| `a` `A` `r` | Accept / accept & trust / refuse a connection |
| `k` `b` | Kick / ban the selected session |
| `v` `c` `t` `p` | Toggle view / control / terminal / clipboard rights |
| `u` `T` `U` `f` `a` (devices) | Unban, trust, untrust, forget a device, unban the first address (typed confirmation) |
| `r`, `q q` | New code, quit |

| File | Content |
| ---- | ------- |
| `~/.config/xdesk/config.json` (`%LOCALAPPDATA%\xdesk`) | Settings (port, exposure, code, approval, rights, stream, theme) |
| `~/.config/xdesk/known_hosts.json` | Hosts pinned by the native clients (fingerprint, device id and secret) |
| `~/.local/share/xdesk/` | `cert.pem` / `key.pem`, `devices.json` + `registry.key`, `running.json` |
| `~/.local/share/xdesk/logs/` | `audit-<day>.jsonl`, `terminal-<date>-<session>-<id>.log` |

### Build

```bash
make                                                      # Configure & build into build/
export BUILD_DIR=build
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Debug         # Debug build (-g -ggdb3)
cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Asan          # AddressSanitizer build
cmake --build $BUILD_DIR --target release                 # Debug, Asan & Optimized builds
cmake --build $BUILD_DIR --target package_release         # RPM / DEB / Arch packages of every variant
cmake --build $BUILD_DIR --target get_unregistered_files  # .cpp files missing from the SRC / SRC_SHARED lists
```

| Option | Default | Content |
| ------ | ------- | ------- |
| `XDESK_KIND` | `binary` | Project kind (`binary`, `library`, `header-only`): defaults of the three options below |
| `XDESK_PACKAGE_BIN` | `ON` | Build and package the executable (`xdesk-op` / `-db` / `-as`) |
| `XDESK_PACKAGE_LIB` | `OFF` | Build and package the library (`libxdesk-op` / `-db` / `-as`) |
| `XDESK_PACKAGE_DEV` | `OFF` | Package the public headers and the CMake config (`libxdesk-dev`) |
| `XDESK_SHARED_LIB` | `OFF` | Shared library (`.so` / `.dll`) instead of a static one |
| `XDESK_STABLE_RELEASE` | `OFF` | Package channel: stable (`ON`) or pre-release `-pre` (`OFF`) |
| `BUILD_TESTS` | `OFF` | Build the unit tests (`unit_tests`, GoogleTest) |

| Path | Content |
| ---- | ------- |
| `src/xdesk/` | `main.cpp`, `Desktop-init.cpp` (arguments), `Desktop.cpp` (commands) |
| `src/xdesk/core/` | `Config` (settings, folders), `AuditLog` |
| `src/xdesk/security/` | `Crypto` (Argon2id, HMAC, TLS), `Certificate`, `PairingCode`, `DeviceRegistry`, `IntegrityGuard` |
| `src/xdesk/net/` | `Protocol`, `WebSocket` (RFC 6455 over TLS), `Server` (https + api + upgrade), `Exposure` (LAN, tunnels, discovery) |
| `src/xdesk/host/` | `Host` (auth, rights, bans, terminals, tamper reaction), `Session`, `Stream` (capture thread) |
| `src/xdesk/client/` | `Client` (pinning, proof, frames, inputs, terminals) |
| `src/xdesk/platform/` | `FrameEncoder`, capture (Qt, `linux/PortalCapture`, `windows/DxgiCapture`), input (`linux/UInputInjector`, `XTestInjector`, `windows/SendInputInjector`), terminals (`unix/PtyTerminal`, `windows/ConPtyTerminal`), `Keymap` |
| `src/xdesk/ui/` | `gui/` (Qt: `HostWindow`, `ViewerWindow`, `TerminalWidget` + `VtScreen` emulator, `Theme`), `tty/` (`HostTui`, `ClientTui`, `Ansi`) |
| `web/` | Web client (embedded in the binary), xterm.js in `web/vendor/` |
| `android/` | Android application (WebView with certificate pinning and auto update) |
| `scripts/` | `install.sh`, `install.ps1`, `bundle_windows.sh`, `build_apk.sh` |
| `include/xdesk/` | Headers (same tree) |
| `tests/xdesk/` | Unit tests (one file per module) |
| `cmake/config/exceptions/*.json` | Project exception codes (`utils::exception::ExternalCode`) |
| `cmake/scripts/` | Exception header generator (`generate_exception_header.py`, `const.py`), `check_files.sh` |
| `cmake/toolchain/mingw-w64.cmake` | Windows cross compilation toolchain |
| `cmake/package/` | CMake package config (`find_package(xdesk)`) and the CPack prefix of the Windows packages |
| `docs/` | HTML documentation (technical page, project graph) |
| `include/exception/generated_external_exception_header.hpp` | Generated at build time from the JSON files (not committed) |

> [!NOTE]
> Every new `.cpp` must be added to the `SRC` (executable only) or `SRC_SHARED` (executable and library) list of the `CMakeLists.txt` (`get_unregistered_files` lists the missing ones).

## Exposure (LAN / external)

The LAN is always open while the host shares (and the PC itself, `localhost`); the mode only **adds one path**, and the
host refuses every connection that arrives through another one (ex: in `lan` mode a peer coming through Tailscale or
from the internet is dropped before TLS and logged once). Changing the mode drops the sessions that came through the
path it closes. The panel shows, for each path, who can use it and its address.

| Mode | Adds | How | Encryption |
| ---- | ---- | --- | ---------- |
| `lan` | nothing (LAN only) | Every local interface + UDP discovery (`xdesk scan`, port `47801`) | end to end |
| `tailscale` | your devices of the tailnet | The MagicDNS name of the host (`pc.tailnet.ts.net`, else `100.x.y.z`), nothing to open | end to end (WireGuard + TLS) |
| `upnp` | the internet | `upnpc` opens the port on the router, shows the public address | end to end |
| `bore` | the internet (relay) | `bore local <port> --to bore.pub` (TCP tunnel, no account) | end to end |
| `ngrok` | the internet (relay) | `ngrok tcp <port>` (account) | end to end |
| `cloudflared` | the internet (relay) | `cloudflared tunnel --url https://localhost:<port>` (`*.trycloudflare.com`, browsers only) | **the relay terminates TLS** |
| `manual` | the internet | Port forwarded by hand, public address given in the settings | end to end |

> [!NOTE]
> The tools (`upnpc`, `bore`, `cloudflared`, `ngrok`, `tailscale`) are used only when installed; the panel shows the state of the tunnel and every address (double click to copy).

## Phone & browser

- Open `https://<host>:<port>` (shown in the panel): the web client asks the code, then shows the screen (touch: tap = click, long press = right click, two fingers = scroll, keyboard button) and terminals (xterm.js).
- On Android the page proposes the **xdesk app** (`/app/xdesk.apk`): full screen, certificate pinned on first use, and an update offered as soon as the host serves a newer version.
- The certificate is self-signed: the browser asks to accept it once (compare the fingerprint with the host panel), or use a Tailscale address / tunnel with a real certificate.

## Windows (cross compilation)
> Build for Windows (x86_64) from Linux with the MinGW-w64 toolchain (libutils: `mingw64-libutils` packages in the MinGW sysroot)

```bash
export BUILD_DIR=build-windows
cmake -S . -B $BUILD_DIR -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Optimized
cmake --build $BUILD_DIR --parallel $(nproc)                 # xdesk.exe at the root / build-windows/libxdesk.a
cmake --build $BUILD_DIR --target package_release           # mingw64-xdesk* RPM/DEB (MinGW sysroot)
WINEPATH=/usr/x86_64-w64-mingw32/sys-root/mingw/bin wine ./xdesk.exe --help
scripts/bundle_windows.sh                                   # dist/xdesk-windows-x64.zip (exe + Qt / OpenSSL dll + install.ps1)
```

> [!NOTE]
> No AddressSanitizer with MinGW: the `Asan` mode is built like `Debug`. Portable code only: system calls through `utils::platform::`, `<windows.h>` before `<utils/utils.hpp>`.

## Unit tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel $(nproc)                       # ./unit_tests at the root
(cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure)
```

| Test file | Content |
| --------- | ------- |
| `tests/xdesk/security/Crypto.cpp` | random codes, normalization, HMAC (RFC 4231), Argon2id, transcript binding |
| `tests/xdesk/security/PairingCode.cpp` | plain / proof checks, rotation, length and expiry |
| `tests/xdesk/security/DeviceRegistry.cpp` | bans, address bans, a `devices.json` edited by hand is refused |
| `tests/xdesk/net/Protocol.cpp` | binary writer / reader, overflow detection |
| `tests/xdesk/platform/FrameEncoder.cpp` | complete then dirty frames, JPEG round trip, resize |
| `tests/xdesk/Desktop.cpp` | command line |

## Known limits

> [!WARNING]
> - Wayland: the real time capture needs `pipewire-devel` at build time (else portal screenshots, a few FPS); the capture only starts with the first viewer (the portal asks the screen share then, once: it remembers it) and stops 15 s after the last one; the remote control needs `/dev/uinput` (`scripts/install.sh --uinput`), else the panel shows `Input: unavailable` and the sessions stay view only.
> - Windows: the build needs `mingw64-qt6-qtbase` (`scripts/bundle_windows.sh`); the code is checked with MinGW but not yet run on a real Windows.
> - The web client cannot read the certificate: it sends the code inside TLS (no Argon2id binding) - prefer the native clients on untrusted networks.

## Pre-commit hook

A shared [xstyle](https://github.com/TsukiNi22/ai-utils) hook (`.githooks/pre-commit`) checks the staged lines at each `git commit` and refuses a `major` (or worse) issue. `setup.sh` already enabled it in this clone.

```bash
git config core.hooksPath .githooks   # once per new clone
git commit --no-verify                # skip the hook once (or XSTYLE_HOOK=0 git commit ...)
xstyle --fix --staged                 # fix what can be, then git add -u
```

## Workflows/Release
### Workflows
- `Dispatch (CI/CD)`: runs on every push (branch `main` or tag `v*`, not for `*.md` / `*.txt` / `docs/` only changes) and decides what to trigger from the ref / commit message
- `Unit Tests (CI)`: builds and runs the unit tests, always first (Linux in the `unit-tests` image, Windows in the `mingw64-unit-tests` image under `wine`)
- `Build (CI)`: only builds the project (Linux in the `ci` image, Windows in the `mingw64-ci` image) and checks the outputs, without producing packages
- `Build - Packages (CI/CD)`: builds the packages (RPM/DEB/Arch: Linux in the `ci` image, Windows `mingw64-*` in the `mingw64-ci` image), signs them and publishes them on the `gh-pages` repository
- `Release - GitHub (CD)`: creates the GitHub release of a tag from its section of the `CHANGELOG.md`, once the packages are published
- `Docs - Pages (CD)`: copies `docs/` on the `gh-pages` branch on every push changing it (https://tsukini22.github.io/external-desktop/docs/)
- `Sync Packages (CD)` (on `gh-pages`): rebuilds the metadata and signatures of the package repository after a manual push on the branch

> [!NOTE]
> `Build (CI)` only runs when `Dispatch (CI/CD)` decides **not** to build the packages, to check that the code still compiles.

> [!IMPORTANT]
> The workflows need the repository variable `RUNNER` (`gh variable set RUNNER --body <runner>`), the containers `ghcr.io/tsukini22/ci`, `unit-tests`, `mingw64-ci`, `mingw64-unit-tests`, `package`, `git`, and the secrets `GPG_PRIVATE_KEY` (base64), `GPG_KEY_ID`, `GPG_PASSPHRASE` to sign and publish the packages.

> [!TIP]
> A commit containing `[ignore]` skips the whole CI.

### Pre-Release (unstable)
The pre-release of the packages (`<package_name>-pre`) is triggered by:
- a tag with a suffix, matching `vx.x.x-<suffix>` (ex: `v1.0.1-pre`, `x` = `major`, `minor`, `fix`)
- or a commit containing `[build]`

> [!NOTE]
> Only the 5 last versions of each `-pre` package are kept in the package repository, every release is kept.

### Release (stable)
The release of the packages (channel `stable`) and the GitHub release are only triggered by a tag matching exactly `vx.x.x` (ex: `v1.0.0`), whose section must exist in the `CHANGELOG.md`.

### Downstream notification
`Dispatch (CI/CD)` has a commented `notify-downstream` job (`repository_dispatch` to another repository, like libutils -> docker-image), triggered after a package publication or by a commit containing `[release]`.
