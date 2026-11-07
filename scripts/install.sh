#!/usr/bin/env bash
#
# Edition:
##  05/11/2026 by Tsukini
#
# File Name:
##  install.sh
#
# File Description:
##  Build and install xdesk on Linux / macOS (dependencies, libutils, binary, menu entries, uinput access)
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
BUILD="$ROOT/build-release"
DEPS=0
UINPUT=0
AUTOSTART=0
UNINSTALL=0
APK=0
SUDO=""

usage() {
    cat << HELP
Usage: scripts/install.sh [options]
  --deps         install the build dependencies (dnf / apt / pacman / brew, asks for sudo) and libutils
  --apk          build the android application too (needs the Android SDK) and embed it
  --uinput       let your user drive the mouse / keyboard on Wayland (udev rule + group "xdesk", sudo)
  --autostart    start the host panel at login (tray)
  --prefix DIR   install folder (default: ~/.local -> ~/.local/bin/xdesk)
  --uninstall    remove xdesk from the prefix, the menu entries and the autostart
  -h, --help     this help
HELP
}

while [ $# -gt 0 ]; do
    case "$1" in
        --deps) DEPS=1 ;;
        --apk) APK=1 ;;
        --uinput) UINPUT=1 ;;
        --autostart) AUTOSTART=1 ;;
        --prefix) PREFIX="$2"; shift ;;
        --uninstall) UNINSTALL=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "install.sh: unknown option $1" >&2; usage; exit 2 ;;
    esac
    shift
done
[ "$(id -u)" -ne 0 ] && command -v sudo > /dev/null && SUDO="sudo"

say() { printf '\033[1;34mxdesk\033[0m %s\n' "$*"; }
die() { printf '\033[1;31mxdesk\033[0m %s\n' "$*" >&2; exit 1; }

# ===== Uninstall =====
if [ "$UNINSTALL" -eq 1 ]; then
    rm -f "$PREFIX/bin/xdesk" "$HOME/.local/share/applications/xdesk.desktop" "$HOME/.local/share/applications/xdesk-viewer.desktop" \
        "$HOME/.config/autostart/xdesk.desktop" "$HOME/.local/share/icons/hicolor/scalable/apps/xdesk.svg"
    say "removed (your settings, devices and logs stay in ~/.config/xdesk and ~/.local/share/xdesk)"
    exit 0
fi

# ===== Dependencies =====
install_libutils() {
    if [ -f /usr/include/utils/utils.hpp ] || [ -f /usr/local/include/utils/utils.hpp ]; then return; fi
    say "installing libutils (TsukiNi22/libutils)"
    if command -v dnf > /dev/null || command -v apt-get > /dev/null; then
        curl -fsSL https://raw.githubusercontent.com/TsukiNi22/libutils/main/setup.sh | $SUDO bash
        if command -v dnf > /dev/null; then $SUDO dnf install -y libutils; else $SUDO apt-get install -y libutils; fi
    else
        local tmp
        tmp="$(mktemp -d)"
        git clone --depth 1 https://github.com/TsukiNi22/libutils.git "$tmp/libutils"
        cmake -S "$tmp/libutils" -B "$tmp/libutils/build" -DCMAKE_BUILD_TYPE=Optimized
        cmake --build "$tmp/libutils/build" --parallel
        $SUDO cmake --build "$tmp/libutils/build" --target install
        rm -rf "$tmp"
    fi
}

if [ "$DEPS" -eq 1 ]; then
    say "installing the build dependencies"
    if command -v dnf > /dev/null; then
        $SUDO dnf install -y cmake clang ccache python3 qt6-qtbase-devel openssl-devel libjpeg-turbo-devel gtest-devel pipewire-devel libXtst-devel git curl
    elif command -v apt-get > /dev/null; then
        $SUDO apt-get update
        $SUDO apt-get install -y cmake clang ccache python3 qt6-base-dev libssl-dev libjpeg-turbo8-dev libgtest-dev libpipewire-0.3-dev libxtst-dev git curl \
            || $SUDO apt-get install -y cmake clang ccache python3 qt6-base-dev libssl-dev libjpeg-dev libgtest-dev libpipewire-0.3-dev libxtst-dev git curl
    elif command -v pacman > /dev/null; then
        $SUDO pacman -S --needed --noconfirm cmake clang ccache python qt6-base openssl libjpeg-turbo gtest pipewire libxtst git curl
    elif command -v brew > /dev/null; then
        brew install cmake llvm ccache python qt openssl@3 jpeg-turbo googletest git
    else
        die "unknown package manager: install cmake, clang, Qt 6 (Core Gui Widgets Network), OpenSSL >= 3.2, libjpeg-turbo yourself"
    fi
    # openh264 (run time, optional): H.264 video of the large moves for the far viewers, tiles only without it
    if command -v dnf > /dev/null; then $SUDO dnf install -y openh264 || say "openh264 not installed: tiles only for the far viewers"
    elif command -v apt-get > /dev/null; then $SUDO apt-get install -y libopenh264-8 || say "openh264 2.6 not installed: tiles only for the far viewers"
    elif command -v pacman > /dev/null; then $SUDO pacman -S --needed --noconfirm openh264 || say "openh264 not installed: tiles only for the far viewers"
    fi
    install_libutils
fi

# ===== Build =====
command -v cmake > /dev/null || die "cmake missing (run with --deps)"
if [ "$APK" -eq 1 ]; then
    say "building the android application"
    bash "$ROOT/scripts/build_apk.sh"
fi
say "building xdesk (Optimized)"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Optimized > /dev/null
cmake --build "$BUILD" --parallel
[ -x "$ROOT/xdesk" ] || die "build failed"

# ===== Install =====
mkdir -p "$PREFIX/bin" "$HOME/.local/share/applications" "$HOME/.local/share/icons/hicolor/scalable/apps"
install -m 0755 "$ROOT/xdesk" "$PREFIX/bin/xdesk"
install -m 0644 "$ROOT/docs/logo.svg" "$HOME/.local/share/icons/hicolor/scalable/apps/xdesk.svg"
cat > "$HOME/.local/share/applications/xdesk.desktop" << DESKTOP
[Desktop Entry]
Type=Application
Name=xdesk - share this PC
Comment=Let someone see / control this PC with a one time code
Exec=$PREFIX/bin/xdesk host --gui
Icon=xdesk
Categories=Network;RemoteAccess;
Terminal=false
DESKTOP
cat > "$HOME/.local/share/applications/xdesk-viewer.desktop" << DESKTOP
[Desktop Entry]
Type=Application
Name=xdesk viewer
Comment=Connect to a PC shared with xdesk
Exec=$PREFIX/bin/xdesk viewer --gui
Icon=xdesk
Categories=Network;RemoteAccess;
Terminal=false
DESKTOP
if [ "$AUTOSTART" -eq 1 ]; then
    mkdir -p "$HOME/.config/autostart"
    sed 's|Exec=.*|Exec='"$PREFIX"'/bin/xdesk host --gui|' "$HOME/.local/share/applications/xdesk.desktop" > "$HOME/.config/autostart/xdesk.desktop"
    say "the host panel starts at login (set startMinimized in the settings to start in the tray)"
fi

# ===== uinput (mouse / keyboard on Wayland) =====
if [ "$UINPUT" -eq 1 ]; then
    say "giving the group xdesk access to /dev/uinput (sudo)"
    getent group xdesk > /dev/null || $SUDO groupadd --system xdesk
    $SUDO usermod -aG xdesk "$USER"
    # uaccess: the user logged in at the screen gets it at once (no new login), the group covers the other sessions
    $SUDO rm -f /etc/udev/rules.d/70-xdesk-uinput.rules
    echo 'KERNEL=="uinput", SUBSYSTEM=="misc", GROUP="xdesk", MODE="0660", TAG+="uaccess", OPTIONS+="static_node=uinput"' | $SUDO tee /etc/udev/rules.d/60-xdesk-uinput.rules > /dev/null
    echo uinput | $SUDO tee /etc/modules-load.d/xdesk-uinput.conf > /dev/null
    $SUDO modprobe uinput || true
    $SUDO udevadm control --reload-rules && $SUDO udevadm trigger /dev/uinput || true
    say "the remote control works now on Wayland too (other users of the group xdesk: after a new login)"
fi

case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) say "add $PREFIX/bin to your PATH" ;;
esac
say "installed: $("$PREFIX/bin/xdesk" --version)"
say "share this PC: xdesk   ·   connect: xdesk connect <address> (or xdesk viewer)"
