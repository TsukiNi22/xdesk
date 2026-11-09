+++
title = "Fichiers et logs"
summary = "Où xdesk garde ses réglages, son identité, son registre d'appareils, son journal d'audit et les transcriptions des terminaux."
section = "reference"
order = 4
keywords = "fichiers dossiers config données logs audit jsonl transcription cert.pem key.pem devices.json registry.key known_hosts running.json paths"
+++

`xdesk paths` affiche les trois dossiers de ce PC.

## Dossiers {#folders}

| Fichier | Contenu |
|---|---|
| `~/.config/xdesk/config.json` | [les réglages](page:settings#network) |
| `~/.config/xdesk/known_hosts.json` | hôtes épinglés par les clients natifs : empreinte, id et secret de l'appareil |
| `~/.local/share/xdesk/cert.pem`, `key.pem` | l'identité de l'hôte (ECDSA P-256 auto-signé), nouvelle avec `xdesk --regen-cert` |
| `~/.local/share/xdesk/devices.json`, `registry.key` | appareils de confiance / connus / bannis, signés avec une clé HMAC (`0600`) |
| `~/.local/share/xdesk/running.json` | l'hôte en cours (un arrêt brutal pendant une session à distance est détecté au démarrage suivant) |
| `~/.local/share/xdesk/tailscale/` | le certificat du nom Tailscale (`tailscale cert`) |
| `~/.local/share/xdesk/logs/` | le journal d'audit et les transcriptions des terminaux |

Sous Windows, le dossier de config est `%LOCALAPPDATA%\xdesk` (`xdesk paths` affiche les autres).

## Journal d'audit {#audit}

`logs/audit-<jour>.jsonl` : un objet JSON par ligne, `ts level cat session msg` + `data` : connexions, décisions, entrées, commandes des terminaux. La page des logs des panneaux le lit en direct (niveau, catégorie, recherche, détails).

## Transcriptions des terminaux {#transcripts}

`logs/terminal-<date>-<session>-<id>.log` : tout ce qu'un terminal distant a affiché, avec les octets tapés. La page des sessions les reflète en direct.

> [!NOTE]
> Le registre est vérifié avec sa clé HMAC à chaque démarrage : modifié hors de xdesk, il est refusé et chaque appareil de confiance doit être réappairé.
