#!/usr/bin/env bash
#
# Edition:
##  05/11/2026 by Tsukini
#
# File Name:
##  bundle_windows.sh
#
# File Description:
##  Cross compile xdesk.exe on Fedora (MinGW-w64) and pack it with every dll it needs: dist/xdesk-windows-x64.zip
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${MINGW_SYSROOT:-/usr/x86_64-w64-mingw32/sys-root/mingw}"
BUILD="$ROOT/build-windows"
DIST="$ROOT/dist/xdesk-windows-x64"
OBJDUMP="${OBJDUMP:-x86_64-w64-mingw32-objdump}"

die() { echo "bundle_windows: $*" >&2; exit 1; }
[ -d "$SYSROOT/bin" ] || die "MinGW sysroot not found ($SYSROOT)"
for pkg in mingw64-qt6-qtbase mingw64-openssl mingw64-libjpeg-turbo mingw64-libutils; do
    rpm -q "$pkg" > /dev/null 2>&1 || die "missing $pkg: sudo dnf install mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-openssl mingw64-libjpeg-turbo (+ mingw64-libutils from the libutils mirror)"
done

# Build
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain/mingw-w64.cmake" -DCMAKE_BUILD_TYPE=Optimized \
    -DQt6_DIR="$SYSROOT/lib/cmake/Qt6" -DCMAKE_PREFIX_PATH="$SYSROOT"
cmake --build "$BUILD" --parallel
[ -f "$ROOT/xdesk.exe" ] || die "xdesk.exe not built"

# The exe, the Qt plugins it loads at run time, then every dll they import (recursively, from the sysroot)
rm -rf "$DIST"
mkdir -p "$DIST/platforms" "$DIST/tls" "$DIST/styles" "$DIST/imageformats"
cp "$ROOT/xdesk.exe" "$DIST/"
QT_PLUGINS="$SYSROOT/lib/qt6/plugins"
cp "$QT_PLUGINS/platforms/qwindows.dll" "$DIST/platforms/"
cp "$QT_PLUGINS/tls/qopensslbackend.dll" "$DIST/tls/" 2> /dev/null || cp "$QT_PLUGINS"/tls/*openssl*.dll "$DIST/tls/"
cp "$QT_PLUGINS"/styles/*.dll "$DIST/styles/" 2> /dev/null || true
cp "$QT_PLUGINS/imageformats/qjpeg.dll" "$DIST/imageformats/" 2> /dev/null || true
declare -A seen=()
queue=("$DIST/xdesk.exe" "$DIST"/platforms/*.dll "$DIST"/tls/*.dll)
while [ ${#queue[@]} -gt 0 ]; do
    file="${queue[0]}"
    queue=("${queue[@]:1}")
    for dll in $("$OBJDUMP" -p "$file" | sed -n 's/^\s*DLL Name: //p'); do
        [ -n "${seen[$dll]:-}" ] && continue
        seen[$dll]=1
        if [ -f "$SYSROOT/bin/$dll" ]; then
            cp "$SYSROOT/bin/$dll" "$DIST/"
            queue+=("$DIST/$dll")
        fi
    done
done
# OpenSSL is loaded at run time by the tls plugin (not imported)
cp "$SYSROOT"/bin/libssl-*.dll "$SYSROOT"/bin/libcrypto-*.dll "$DIST/" 2> /dev/null || true
cp "$ROOT/scripts/install.ps1" "$ROOT/LICENSE" "$DIST/"
printf '[Paths]\nPlugins = .\n' > "$DIST/qt.conf"

(cd "$ROOT/dist" && rm -f xdesk-windows-x64.zip && zip -qr xdesk-windows-x64.zip xdesk-windows-x64)
echo "bundle_windows: $ROOT/dist/xdesk-windows-x64.zip ($(du -h "$ROOT/dist/xdesk-windows-x64.zip" | cut -f1))"
