+++
title = "Paquets, Windows et Android"
summary = "Les paquets Linux et Windows, le bundle Windows et l'appli Android, et comment chacun est construit."
section = "resources"
order = 1
keywords = "paquets rpm deb arch mingw64 windows zip bundle compilation croisée android apk build_apk pre release dépendances openh264"
+++

## Paquets Linux {#packages}

Les mêmes paquets existent en RPM (Fedora), DEB (Debian / Ubuntu) et Arch (pacman), signés et publiés sur la branche `gh-pages` ([les installer](page:getting-started#linux)).

| Paquet | Contenu |
|---|---|
| `xdesk` | installe le paquet par défaut (`xdesk-op`) |
| `xdesk-op` | l'exécutable compilé avec les options d'optimisation |
| `xdesk-db` | l'exécutable compilé avec les options de debug (pour le debug seulement) |
| `xdesk-as` | l'exécutable compilé avec AddressSanitizer (pour le debug seulement) |

> [!WARNING]
> Les trois variantes installent le même `/usr/bin/xdesk` : une seule à la fois.

Chaque paquet a une pré-release `<paquet>-pre` (tag `vX.Y.Z-<suffixe>` ou commit avec `[build]`), rendue obsolète par tout paquet stable de même version ou plus.

## Windows {#windows}

Les mêmes paquets préfixés par `mingw64-` (`mingw64-xdesk`...), installés dans le sysroot MinGW, sans la variante `-as`. Depuis Linux :

```bash "Terminal"
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/mingw-w64.cmake -DCMAKE_BUILD_TYPE=Optimized
cmake --build build-windows --parallel $(nproc)          # xdesk.exe à la racine
cmake --build build-windows --target package_release    # RPM / DEB mingw64-xdesk*
scripts/bundle_windows.sh                                # dist/xdesk-windows-x64.zip (exe + dll Qt / OpenSSL + install.ps1)
```

Il faut `mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-openssl mingw64-libjpeg-turbo mingw64-libutils` (Fedora). H.264 sous Windows : la `openh264-2.6.x-win64.dll` de Cisco à côté de `xdesk.exe`.

## Android {#android}

`scripts/build_apk.sh` construit `android/build/xdesk.apk` sans Gradle (aapt2, javac, d8, apksigner), version `x.y.z.<commits>` ; elle est embarquée dans le binaire à la compilation suivante et servie sur `/app/xdesk.apk`. L'appli est une WebView du client web avec épinglage du certificat, un scanner de QR code et une mise à jour proposée dès que l'hôte sert une version plus récente. Il faut le SDK Android (build-tools `>= 34`).

## Dépendances {#dependencies}

| Nom | Version | Fedora (`dnf`) |
|---|---|---|
| `clang++` / `cmake` | C++20 / `>= 3.20` | `clang cmake` |
| `libutils` | `>= 4.1.0` | `libutils` |
| Qt 6 (Core Gui Widgets Network) | `>= 6.4` | `qt6-qtbase-devel` |
| OpenSSL | `>= 3.2` (`3.5` pour ML-KEM) | `openssl-devel` |
| libjpeg-turbo | toute | `libjpeg-turbo-devel` |
| PipeWire (exécution, Wayland) | `0.3` / `1.x` | `pipewire-libs` |
| XTest (optionnel) | toute | `libXtst-devel` |
| openh264 (exécution, optionnel) | `2.6+` | `openh264` |

Les noms Debian / Ubuntu et Arch sont dans le [README](https://github.com/TsukiNi22/xdesk#dependencies).
