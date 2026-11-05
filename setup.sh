#!/bin/bash
# Package repository setup of xdesk (becomes the setup.sh of a generated project): registers the gh-pages
# mirror (RPM / DEB / Arch, signed) in the system package manager (--install also installs the package).
set -euo pipefail

BASE_URL="https://tsukini22.github.io/external-desktop"
KEY_NAME="RPM-GPG-KEY-tsukini"
PACKAGE="xdesk"
SUDO="sudo"
INSTALL=false
CHANNEL=""
PREFIX=""

# =========================
# Parse arguments
# =========================
usage() {
    echo "Usage: $0 [--no-sudo] [--install] [--pre] [--windows]"
    echo "  --no-sudo      Run without sudo (requires the script to be run as root already)"
    echo "  --install      Also install the package (by default only the mirror & gpg key are setup)"
    echo "  --pre          Select the pre-release/unstable package (xdesk-pre)"
    echo "  --windows      Select the Windows packages (mingw64-xdesk, MinGW sysroot) instead of the Linux ones (rpm / deb only)"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --no-sudo)
            SUDO=""
            ;;
        --install)
            INSTALL=true
            ;;
        --pre)
            CHANNEL="-pre"
            ;;
        --windows)
            PREFIX="mingw64-"
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Error: unknown argument '$arg'" >&2
            usage
            ;;
    esac
done
PACKAGE="${PREFIX}${PACKAGE}${CHANNEL}"

if [[ -z "$SUDO" ]] && [[ "$(id -u)" -ne 0 ]]; then
    echo "Error: --no-sudo requires the script to be run as root." >&2
    exit 1
fi

# =========================
# Requirements
# =========================
require() {
    for cmd in "$@"; do
        if ! command -v "$cmd" > /dev/null 2>&1; then
            echo "Error: '$cmd' is required but not installed." >&2
            exit 1
        fi
    done
}
require curl

# =========================
# Find the os
# =========================
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    OS_ID="${ID:-unknown}"
    OS_ID_LIKE="${ID_LIKE:-}"
    OS_VERSION="${VERSION_ID:-unknown}"
else
    echo "Error: /etc/os-release not found, cannot detect OS." >&2
    exit 1
fi

is_rpm_based() {
    [[ "$OS_ID" =~ ^(fedora|rhel|centos|rocky|almalinux)$ ]] || [[ "$OS_ID_LIKE" =~ (fedora|rhel) ]]
}

is_deb_based() {
    [[ "$OS_ID" =~ ^(debian|ubuntu)$ ]] || [[ "$OS_ID_LIKE" =~ (debian|ubuntu) ]]
}

is_arch_based() {
    [[ "$OS_ID" =~ ^(arch|manjaro|endeavouros|cachyos)$ ]] || [[ "$OS_ID_LIKE" =~ arch ]]
}

# =========================
# RPM
# =========================
install_rpm_repo() {
    echo "Detected RPM-based system (ID=$OS_ID). Setting up xdesk repo..."

    # The mirror is only built for a fixed release (see: .github/workflows/build-packages.yml)
    if [[ "$OS_ID" = "fedora" ]] && ! curl -fsSL -o /dev/null "$BASE_URL/fedora/$OS_VERSION/$(uname -m)/repodata/repomd.xml"; then
        echo "Warning: no packages published for fedora $OS_VERSION ($(uname -m)), the installation will probably fail." >&2
    fi

    $SUDO curl -fsSL -o /etc/yum.repos.d/xdesk.repo \
        "$BASE_URL/xdesk.repo"

    $SUDO rpm --import "$BASE_URL/$KEY_NAME"

    echo "xdesk repo installed."
}

install_rpm_package() {
    local pm="dnf"
    command -v dnf > /dev/null 2>&1 || pm="yum"

    echo "Installing $PACKAGE..."
    $SUDO "$pm" install -y "$PACKAGE"
}

# =========================
# DEB
# =========================
install_deb_repo() {
    echo "Detected DEB-based system (ID=$OS_ID). Setting up xdesk repo..."
    require gpg dpkg

    $SUDO install -m 0755 -d /etc/apt/keyrings

    curl -fsSL "$BASE_URL/$KEY_NAME" \
        | gpg --dearmor | $SUDO tee /etc/apt/keyrings/xdesk.gpg > /dev/null
    $SUDO chmod a+r /etc/apt/keyrings/xdesk.gpg

    ARCH="$(dpkg --print-architecture)"

    echo "deb [arch=${ARCH} signed-by=/etc/apt/keyrings/xdesk.gpg] ${BASE_URL}/debian stable main" \
        | $SUDO tee /etc/apt/sources.list.d/xdesk.list > /dev/null

    $SUDO apt-get update

    echo "xdesk repo installed."
}

install_deb_package() {
    echo "Installing $PACKAGE..."
    $SUDO apt-get install -y "$PACKAGE"
}

# =========================
# Arch (pacman)
# =========================
PACMAN_CONF="${PACMAN_CONF:-/etc/pacman.conf}"

install_arch_repo() {
    echo "Detected Arch-based system (ID=$OS_ID). Setting up xdesk repo..."
    require gpg pacman-key

    local key_file key_id
    key_file="$(mktemp)"
    curl -fsSL -o "$key_file" "$BASE_URL/$KEY_NAME"

    # Trust the signing key in the pacman keyring (packages & database are signed)
    key_id="$(gpg --with-colons --import-options show-only --import "$key_file" | awk -F: '/^fpr:/ {print $10; exit}')"
    $SUDO pacman-key --add "$key_file"
    $SUDO pacman-key --lsign-key "$key_id"
    rm -f "$key_file"

    if ! grep -q '^\[xdesk\]' "$PACMAN_CONF"; then
        printf '\n[xdesk]\nSigLevel = Required\nServer = %s/arch/$arch\n' "$BASE_URL" \
            | $SUDO tee -a "$PACMAN_CONF" > /dev/null
    fi

    $SUDO pacman -Sy

    echo "xdesk repo installed."
}

install_arch_package() {
    echo "Installing $PACKAGE..."
    $SUDO pacman -S --noconfirm "$PACKAGE"
}

# =========================
# Dispatch
# =========================
if is_rpm_based; then
    install_rpm_repo
    if $INSTALL; then install_rpm_package; fi
elif is_deb_based; then
    install_deb_repo
    if $INSTALL; then install_deb_package; fi
elif is_arch_based; then
    if [[ -n "$PREFIX" ]]; then
        echo "Error: no Windows (mingw64-) packages for Arch based distributions (no MinGW OpenSSL package)." >&2
        exit 1
    fi
    install_arch_repo
    if $INSTALL; then install_arch_package; fi
else
    echo "Error: unsupported distribution (ID=$OS_ID, ID_LIKE=$OS_ID_LIKE)." >&2
    echo "Supported: Fedora/RHEL-based (rpm), Debian/Ubuntu-based (deb) and Arch-based (pacman)." >&2
    exit 1
fi

if $INSTALL; then
    echo "$PACKAGE installed."
elif is_rpm_based; then
    echo "You can now run: $SUDO dnf install $PACKAGE"
elif is_arch_based; then
    echo "You can now run: $SUDO pacman -S $PACKAGE"
else
    echo "You can now run: $SUDO apt install $PACKAGE"
fi
