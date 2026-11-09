+++
title = "Reach it from outside"
summary = "The exposure modes: LAN, Tailscale, UPnP, bore, ngrok, cloudflared, manual; what each one opens and who can use it."
section = "guide"
order = 4
keywords = "exposure lan tailscale upnp bore ngrok cloudflared manual port forward tunnel internet discovery hsts certificate"
+++

The LAN (and the PC itself) is always open while the host shares; the exposure mode **adds one path**. A connection arriving through a path the mode does not open is dropped before TLS and logged once. Changing the mode drops the sessions of the path it closes.

## Modes {#modes}

| Mode | Adds | How | End to end |
|---|---|---|---|
| `lan` | nothing | every local interface + UDP discovery (`xdesk scan`, port `47801`) | yes |
| `tailscale` | your devices of the tailnet | `https://100.x.y.z:<port>`, or the MagicDNS name for `xdesk connect` | yes (WireGuard + TLS) |
| `upnp` | the internet | `upnpc` opens the port on the router, shows the public address | yes |
| `bore` | the internet (relay) | `bore local <port> --to bore.pub` (TCP, no account) | yes |
| `ngrok` | the internet (relay) | `ngrok tcp <port>` (account) | yes |
| `cloudflared` | the internet (relay, browsers only) | `cloudflared tunnel --url https://localhost:<port>` | **no: the relay terminates TLS** |
| `manual` | the internet | port forwarded by hand, public address in the settings | yes |

```bash "Terminal"
xdesk host --expose tailscale -p 47900   # this run only: exposure and port
```

The tools are used only when installed; the panel shows the state of the tunnel and every address, each with copy and QR code buttons. The TCP tunnels (`bore`, `ngrok`, `cloudflared`) keep the video on TCP; with `manual`, forward UDP too for the native viewers.

## Tailscale and browsers {#tailscale}

Every `*.ts.net` name is on the HSTS preload list of the browsers: a self-signed certificate is refused there without any way to accept it. So the Tailscale path gets a real certificate with `tailscale cert` (renewed by xdesk, kept in `<data>/tailscale/`), picked by the server from the name the browser asks (SNI): `https://<pc>.<tailnet>.ts.net:<port>` opens without warning. The native clients send no name and keep the pinned self-signed certificate.

> [!IMPORTANT]
> Needs, once: HTTPS enabled in the Tailscale admin console (DNS page; the name is then published in the public certificate logs) and the right to run `tailscale cert` (`sudo tailscale set --operator=$USER`). Without them the browser address stays the `100.x.y.z` IP and the panel says why.

LAN addresses use the `<pc>.local` name when avahi / Bonjour publishes it, the IP otherwise.
