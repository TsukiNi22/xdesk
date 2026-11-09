+++
title = "L'atteindre de l'extérieur"
summary = "Les modes d'exposition : LAN, Tailscale, UPnP, bore, ngrok, cloudflared, manuel ; ce que chacun ouvre et qui peut l'utiliser."
section = "guide"
order = 4
keywords = "exposition lan tailscale upnp bore ngrok cloudflared manuel redirection de port tunnel internet découverte hsts certificat"
+++

Le LAN (et le PC lui-même) est toujours ouvert pendant le partage ; le mode d'exposition **ajoute un chemin**. Une connexion arrivant par un chemin que le mode n'ouvre pas est coupée avant TLS et notée une fois dans les logs. Changer de mode coupe les sessions du chemin qu'il ferme.

## Modes {#modes}

| Mode | Ajoute | Comment | De bout en bout |
|---|---|---|---|
| `lan` | rien | toutes les interfaces locales + découverte UDP (`xdesk scan`, port `47801`) | oui |
| `tailscale` | tes appareils du tailnet | `https://100.x.y.z:<port>`, ou le nom MagicDNS pour `xdesk connect` | oui (WireGuard + TLS) |
| `upnp` | internet | `upnpc` ouvre le port sur la box, affiche l'adresse publique | oui |
| `bore` | internet (relais) | `bore local <port> --to bore.pub` (TCP, sans compte) | oui |
| `ngrok` | internet (relais) | `ngrok tcp <port>` (compte) | oui |
| `cloudflared` | internet (relais, navigateurs seulement) | `cloudflared tunnel --url https://localhost:<port>` | **non : le relais termine TLS** |
| `manual` | internet | port redirigé à la main, adresse publique dans les réglages | oui |

```bash "Terminal"
xdesk host --expose tailscale -p 47900   # ce lancement seulement : exposition et port
```

Les outils ne sont utilisés que s'ils sont installés ; le panneau montre l'état du tunnel et chaque adresse, avec des boutons copier et QR code. Les tunnels TCP (`bore`, `ngrok`, `cloudflared`) gardent la vidéo en TCP ; avec `manual`, redirige aussi l'UDP pour les viewers natifs.

## Tailscale et navigateurs {#tailscale}

Chaque nom `*.ts.net` est dans la liste HSTS préchargée des navigateurs : un certificat auto-signé y est refusé sans aucun moyen de l'accepter. Le chemin Tailscale reçoit donc un vrai certificat avec `tailscale cert` (renouvelé par xdesk, gardé dans `<data>/tailscale/`), choisi par le serveur selon le nom demandé par le navigateur (SNI) : `https://<pc>.<tailnet>.ts.net:<port>` s'ouvre sans avertissement. Les clients natifs n'envoient pas de nom et gardent le certificat auto-signé épinglé.

> [!IMPORTANT]
> Il faut, une fois : HTTPS activé dans la console d'admin Tailscale (page DNS ; le nom est alors publié dans les logs publics de certificats) et le droit de lancer `tailscale cert` (`sudo tailscale set --operator=$USER`). Sans cela l'adresse du navigateur reste l'IP `100.x.y.z` et le panneau dit pourquoi.

Les adresses du LAN utilisent le nom `<pc>.local` quand avahi / Bonjour le publie, l'IP sinon.
