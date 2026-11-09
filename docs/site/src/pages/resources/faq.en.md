+++
title = "FAQ"
summary = "Short answers to the usual questions about xdesk."
section = "resources"
order = 4
keywords = "faq questions code changed browser certificate warning wayland control unavailable unban port firewall"
+++

## The code I gave no longer works {#code}

It changes after each use, every 5 minutes (`codeLifetime`) and at each start of the sharing. Give the one shown now, or trust the device so it reconnects without code. [The code](page:share-a-pc#code).

## The browser warns about the certificate {#certificate}

The host certificate is self-signed: accept it once after comparing the fingerprint with the panel, or use the [Tailscale name](page:exposure#tailscale), which has a real certificate.

## The viewer sees the screen but cannot control it {#control}

Check the **control** right of the session in the panel. On Wayland, the remote control needs `/dev/uinput` (`scripts/install.sh --uinput`). While the person at the PC uses the panel, remote input is paused.

## How do I lift a ban? {#unban}

Only on the host PC: the devices page of the panel, or `xdesk unban <device id | address>` with the host stopped, typing the word `unban`.

## Which port must I open? {#port}

`47800` TCP (HTTPS + websocket) and the same port in UDP for the video of the native viewers; `47801` UDP for the LAN discovery. On Windows, `install.ps1` opens `47800` TCP and `47801` UDP on the private networks.
