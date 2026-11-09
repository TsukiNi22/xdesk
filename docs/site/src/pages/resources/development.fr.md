+++
title = "Développement"
summary = "Modes de compilation, options CMake, organisation des sources, tests, workflows et ce site."
section = "resources"
order = 2
keywords = "développement compilation debug asan optimized options cmake sources organisation tests ctest gtest workflows release docs site"
+++

## Compiler {#build}

```bash "Terminal"
make                                                 # configurer et compiler dans build/
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug         # build debug (-g -ggdb3)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Asan          # build AddressSanitizer
cmake --build build --target release                 # builds Debug, Asan et Optimized
cmake --build build --target package_release         # paquets RPM / DEB / Arch de chaque variante
cmake --build build --target get_unregistered_files  # .cpp absents de SRC / SRC_SHARED
```

| Option | Défaut | Contenu |
|---|---|---|
| `XDESK_PACKAGE_BIN` | `ON` | compiler et empaqueter l'exécutable |
| `XDESK_PACKAGE_LIB` | `OFF` | compiler et empaqueter la bibliothèque |
| `XDESK_PACKAGE_DEV` | `OFF` | empaqueter les headers et la config CMake |
| `XDESK_STABLE_RELEASE` | `OFF` | canal des paquets : stable ou `-pre` |
| `BUILD_TESTS` | `OFF` | compiler les tests unitaires |

Les backends optionnels sont détectés par CMake (`xdesk backends: dbus=... pipewire=... xtest=...`).

## Sources {#layout}

| Chemin | Contenu |
|---|---|
| `src/xdesk/core/` | `Config` (réglages, dossiers), `AuditLog` |
| `src/xdesk/security/` | `Crypto`, `Certificate`, `PairingCode`, `DeviceRegistry`, `IntegrityGuard` |
| `src/xdesk/net/` | `Protocol`, `WebSocket`, `Server`, `Exposure`, `Datagram` |
| `src/xdesk/host/` | `Host` (auth, droits, bans, terminaux, réaction à l'altération), `Session`, `Stream` |
| `src/xdesk/client/` | `Client` (épinglage, preuve, images, entrées, terminaux) |
| `src/xdesk/platform/` | `FrameEncoder`, capture, injection des entrées, pseudo-terminaux, `Keymap` |
| `src/xdesk/ui/` | `gui/` (panneau Qt, viewer, émulateur VT), `tty/` (panneau et client terminal) |
| `web/`, `android/` | client web (embarqué dans le binaire), appli Android |
| `docs/site/` | sources de ce site (Markdown + générateurs `gen/`) |

## Tests {#tests}

```bash "Terminal"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build --parallel $(nproc)
(cd build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure)
```

Un fichier de test par module dans `tests/xdesk/` : crypto (HMAC RFC 4231, Argon2id, liaison du transcript), codes d'appairage, registre, lecture / écriture du protocole, encodeur d'images (tuiles, défilement, H.264), ligne de commande.

## Ce site {#docs}

```bash "Terminal"
python3 ~/.claude/skills/better-html-doc/scripts/build_site.py --site docs/site --out docs
```

Les pages sont écrites dans `docs/` et publiées par `Docs - Pages (CD)` ; les pages des commandes et des réglages sont générées depuis `src/xdesk/Desktop-init.cpp` et `include/xdesk/core/Config.hpp`.

## Workflows et releases {#workflows}

| Workflow | Fait |
|---|---|
| `Dispatch (CI/CD)` | à chaque push de `main` ou d'un tag `v*` (pas pour `*.md` / `*.txt` / `docs/` seuls) : décide de ce qui tourne |
| `Unit Tests (CI)` | tests Linux et Windows (sous `wine`), toujours en premier |
| `Build (CI)` | vérification du build quand aucun paquet n'est construit |
| `Build - Packages (CI/CD)` | paquets Linux et `mingw64-`, signés et publiés sur `gh-pages` |
| `Release - GitHub (CD)` | la release GitHub d'un tag, depuis sa section du `CHANGELOG.md` |
| `Docs - Pages (CD)` | copie `docs/` sur `gh-pages` |

> [!NOTE]
> Un push qui ne modifie que des fichiers `*.txt` (un `CMakeLists.txt`) ne lance pas la CI : lancer `Unit Tests (CI)` à la main.
