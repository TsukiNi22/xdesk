+++
title = "Getting started"
summary = "Install xdesk on Linux or Windows, then share a PC for the first time."
section = "guide"
order = 1
keywords = "install setup dnf apt pacman my-universe packages windows zip install.ps1 install.sh first share code"
+++

xdesk is one binary: the same `xdesk` shares a PC (the **host**) or connects to one (the **client**).

## Install on Linux {#linux}

Signed RPM (Fedora), DEB (Debian / Ubuntu) and Arch packages. Every package repository of TsukiNi22 at once, with [my-universe](https://tsukini22.github.io/my-universe/):

```bash "Terminal"
curl -fsSL https://tsukini22.github.io/my-universe/install.sh | bash -s
sudo dnf install xdesk          # or: sudo apt install xdesk / sudo pacman -S xdesk
```

Or only the repository of xdesk, with its `setup.sh`:

```bash "Terminal"
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xdesk/main/setup.sh | bash -s -- --install
```

| Argument of `setup.sh` | Effect |
|---|---|
| `--install` | Also install the package (by default only the repository and the GPG key) |
| `--pre` | The pre-release package (`xdesk-pre`) |
| `--windows` | The Windows packages (`mingw64-xdesk`, MinGW sysroot, rpm / deb only) |
| `--no-sudo` | Run without `sudo` (already root) |

From the sources instead (dependencies, libutils, build, `~/.local/bin/xdesk`, menu entries):

```bash "Terminal"
git clone https://github.com/TsukiNi22/xdesk.git && cd xdesk
scripts/install.sh --deps --apk --uinput   # --apk: embed the Android app, --uinput: remote control on Wayland
scripts/install.sh --autostart             # host panel at login (tray)
```

## Install on Windows {#windows}

From the `xdesk-windows-x64.zip` of a [release](https://github.com/TsukiNi22/xdesk/releases), in PowerShell:

```powershell "PowerShell"
powershell -ExecutionPolicy Bypass -File install.ps1             # %LOCALAPPDATA%\Programs\xdesk, PATH, Start menu, firewall
powershell -ExecutionPolicy Bypass -File install.ps1 -Autostart
powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
```

`install.ps1` downloads the latest release itself when it is not run from the extracted zip (`-Zip <file>` for a local one).

> [!WARNING]
> The Windows build is checked with MinGW but not yet run on a real Windows. [Known limits](page:limits#platforms).

## Share this PC {#first-share}

1. Run `xdesk`: the panel opens (Qt window, or a terminal panel without display).
2. Press **Start sharing**: sharing starts off at each launch.
3. Give the **one time code** (`K7F-29Q-XM4`) or show the **QR code**.
4. Accept the device when the panel asks (an unknown device is refused after 30 s).

On the other PC: `xdesk connect <address> -c <code>`, or open `https://<address>:47800` in a browser. Next: [share a PC](page:share-a-pc#panel), [connect](page:connect#clients).
