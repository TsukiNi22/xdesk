+++
title = "Development"
summary = "Build modes, CMake options, the layout of the sources, the tests, the workflows and this site."
section = "resources"
order = 2
keywords = "development build debug asan optimized cmake options sources layout tests ctest gtest workflows release docs site"
+++

## Build {#build}

```bash "Terminal"
make                                                 # configure & build into build/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug         # debug build (-g -ggdb3)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Asan          # AddressSanitizer build
cmake --build build --target release                 # Debug, Asan & Optimized builds
cmake --build build --target package_release         # RPM / DEB / Arch packages of every variant
cmake --build build --target get_unregistered_files  # .cpp files missing from SRC / SRC_SHARED
```

| Option | Default | Content |
|---|---|---|
| `XDESK_PACKAGE_BIN` | `ON` | build and package the executable |
| `XDESK_PACKAGE_LIB` | `OFF` | build and package the library |
| `XDESK_PACKAGE_DEV` | `OFF` | package the headers and the CMake config |
| `XDESK_STABLE_RELEASE` | `OFF` | package channel: stable or `-pre` |
| `BUILD_TESTS` | `OFF` | build the unit tests |

The optional backends are detected by CMake (`xdesk backends: dbus=... pipewire=... xtest=...`).

## Sources {#layout}

| Path | Content |
|---|---|
| `src/xdesk/core/` | `Config` (settings, folders), `AuditLog` |
| `src/xdesk/security/` | `Crypto`, `Certificate`, `PairingCode`, `DeviceRegistry`, `IntegrityGuard` |
| `src/xdesk/net/` | `Protocol`, `WebSocket`, `Server`, `Exposure`, `Datagram` |
| `src/xdesk/host/` | `Host` (auth, rights, bans, terminals, tamper reaction), `Session`, `Stream` |
| `src/xdesk/client/` | `Client` (pinning, proof, frames, inputs, terminals) |
| `src/xdesk/platform/` | `FrameEncoder`, capture, input injection, pseudo terminals, `Keymap` |
| `src/xdesk/ui/` | `gui/` (Qt panel, viewer, VT emulator), `tty/` (terminal panel and client) |
| `web/`, `android/` | web client (embedded in the binary), Android app |
| `docs/site/` | sources of this site (Markdown + `gen/` generators) |

## Tests {#tests}

```bash "Terminal"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel $(nproc)
(cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure)
```

One test file per module in `tests/xdesk/`: crypto (RFC 4231 HMAC, Argon2id, transcript binding), pairing codes, registry, protocol reader / writer, frame encoder (tiles, scroll, H.264), command line.

## This site {#docs}

```bash "Terminal"
python3 ~/.claude/skills/better-html-doc/scripts/build_site.py --site docs/site --out docs
```

The pages are written in `docs/` and published by `Docs - Pages (CD)`; the commands and settings pages are generated from `src/xdesk/Desktop-init.cpp` and `include/xdesk/core/Config.hpp`.

## Workflows and releases {#workflows}

| Workflow | Does |
|---|---|
| `Dispatch (CI/CD)` | on every push of `main` or a tag `v*` (not `*.md` / `*.txt` / `docs/` only): decides what runs |
| `Unit Tests (CI)` | Linux and Windows (under `wine`) tests, always first |
| `Build (CI)` | build check when no package is built |
| `Build - Packages (CI/CD)` | Linux and `mingw64-` packages, signed and published on `gh-pages` |
| `Release - GitHub (CD)` | the GitHub release of a tag, from its section of `CHANGELOG.md` |
| `Docs - Pages (CD)` | copies `docs/` on `gh-pages` |

> [!NOTE]
> A push that only changes `*.txt` files (a `CMakeLists.txt`) does not start the CI: run `Unit Tests (CI)` by hand.
