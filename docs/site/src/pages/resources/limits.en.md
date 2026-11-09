+++
title = "Known limits"
summary = "What does not work yet, or only under conditions, on each platform."
section = "resources"
order = 3
keywords = "limits wayland pipewire uinput windows mingw web client certificate binding known issues"
+++

## Platforms {#platforms}

> [!WARNING]
> - **Wayland**: the real time capture uses the PipeWire library of the system (loaded at run time; without it: portal screenshots, about 1 FPS). The capture starts with the first viewer (the portal asks the screen share then, once) and stops 15 s after the last one. The remote control needs `/dev/uinput` (`scripts/install.sh --uinput`), else the panel shows `Input: unavailable` and the sessions stay view only.
> - **Windows**: the code is checked with MinGW but not yet run on a real Windows.
> - **Web client**: it cannot read the certificate, so it sends the code inside TLS without the Argon2id binding.

## Capture and input backends {#backends}

| Platform | Capture | Input | Terminals |
|---|---|---|---|
| Windows | DXGI Desktop Duplication | `SendInput` (scancodes + unicode) | ConPTY |
| Linux X11, macOS | Qt | `uinput` or XTest (Linux) | `forkpty` |
| Linux Wayland | xdg-desktop-portal ScreenCast + PipeWire | `uinput` | `forkpty` |

## Network {#network}

- The TCP tunnels (`bore`, `ngrok`, `cloudflared`) keep the video on TCP: no UDP through them.
- Browsers have no raw UDP; WebRTC data channels are not implemented. Extra websockets give back throughput on lossy paths, not a lower ping.
