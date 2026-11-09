+++
title = "Files and logs"
summary = "Where xdesk keeps its settings, identity, device registry, audit log and terminal transcripts."
section = "reference"
order = 4
keywords = "files folders config data logs audit jsonl transcript cert.pem key.pem devices.json registry.key known_hosts running.json paths"
+++

`xdesk paths` prints the three folders of this PC.

## Folders {#folders}

| File | Content |
|---|---|
| `~/.config/xdesk/config.json` | [the settings](page:settings#network) |
| `~/.config/xdesk/known_hosts.json` | hosts pinned by the native clients: fingerprint, device id and secret |
| `~/.local/share/xdesk/cert.pem`, `key.pem` | the host identity (self-signed ECDSA P-256), new with `xdesk --regen-cert` |
| `~/.local/share/xdesk/devices.json`, `registry.key` | trusted / known / banned devices, signed with an HMAC key (`0600`) |
| `~/.local/share/xdesk/running.json` | the running host (an unclean stop during a remote session is detected at the next start) |
| `~/.local/share/xdesk/tailscale/` | the certificate of the Tailscale name (`tailscale cert`) |
| `~/.local/share/xdesk/logs/` | the audit log and the terminal transcripts |

On Windows the config folder is `%LOCALAPPDATA%\xdesk` (`xdesk paths` prints the others).

## Audit log {#audit}

`logs/audit-<day>.jsonl`: one JSON object per line, `ts level cat session msg` + `data`: connections, decisions, inputs, terminal commands. The logs page of the panels reads it live (level, category, search, details).

## Terminal transcripts {#transcripts}

`logs/terminal-<date>-<session>-<id>.log`: everything a remote terminal printed, with the bytes typed. The sessions page mirrors them live.

> [!NOTE]
> The registry is checked with its HMAC key at each start: edited outside xdesk, it is refused and every trusted device must pair again.
