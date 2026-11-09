+++
title = "Packages, Windows and Android"
summary = "The Linux and Windows packages, the Windows bundle and the Android app, and how each one is built."
section = "resources"
order = 1
keywords = "packages rpm deb arch mingw64 windows zip bundle cross compilation android apk build_apk pre release dependencies openh264"
+++

## Linux packages {#packages}

The same packages exist as RPM (Fedora), DEB (Debian / Ubuntu) and Arch (pacman), signed and published on the `gh-pages` branch ([install them](page:getting-started#linux)).

| Package | Content |
|---|---|
| `xdesk` | installs the default package (`xdesk-op`) |
| `xdesk-op` | the executable compiled with the optimization options |
| `xdesk-db` | the executable compiled with the debug options (debugging only) |
| `xdesk-as` | the executable compiled with AddressSanitizer (debugging only) |

> [!WARNING]
> The three variants install the same `/usr/bin/xdesk`: only one of them at a time.

Every package has a pre-release `<package>-pre` (tag `vX.Y.Z-<suffix>` or a commit with `[build]`), obsoleted by any stable package of the same version or higher.

## Windows {#windows}

The same packages prefixed with `mingw64-` (`mingw64-xdesk`...), installed in the MinGW sysroot, without the `-as` variant. From Linux:

```bash "Terminal"
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Optimized
cmake --build build-windows --parallel $(nproc)          # xdesk.exe at the root
cmake --build build-windows --target package_release    # mingw64-xdesk* RPM / DEB
scripts/bundle_windows.sh                                # dist/xdesk-windows-x64.zip (exe + Qt / OpenSSL dll + install.ps1)
```

Needs `mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-openssl mingw64-libjpeg-turbo mingw64-libutils` (Fedora). H.264 on Windows: the `openh264-2.6.x-win64.dll` of Cisco next to `xdesk.exe`.

## Android {#android}

`scripts/build_apk.sh` builds `android/build/xdesk.apk` without Gradle (aapt2, javac, d8, apksigner), version `x.y.z.<commits>`; it is embedded in the binary at the next build and served at `/app/xdesk.apk`. The app is a WebView of the web client with certificate pinning, a QR code scanner and an update offered as soon as the host serves a newer version. Needs the Android SDK (build-tools `>= 34`).

## Dependencies {#dependencies}

| Name | Version | Fedora (`dnf`) |
|---|---|---|
| `clang++` / `cmake` | C++20 / `>= 3.20` | `clang cmake` |
| `libutils` | `>= 4.1.0` | `libutils` |
| Qt 6 (Core Gui Widgets Network) | `>= 6.4` | `qt6-qtbase-devel` |
| OpenSSL | `>= 3.2` (`3.5` for ML-KEM) | `openssl-devel` |
| libjpeg-turbo | any | `libjpeg-turbo-devel` |
| PipeWire (run time, Wayland) | `0.3` / `1.x` | `pipewire-libs` |
| XTest (optional) | any | `libXtst-devel` |
| openh264 (run time, optional) | `2.6+` | `openh264` |

The Debian / Ubuntu and Arch names are in the [README](https://github.com/TsukiNi22/xdesk#dependencies).
