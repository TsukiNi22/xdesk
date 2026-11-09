+++
title = "Limites connues"
summary = "Ce qui ne marche pas encore, ou seulement sous conditions, sur chaque plateforme."
section = "resources"
order = 3
keywords = "limites wayland pipewire uinput windows mingw client web liaison certificat problèmes connus"
+++

## Plateformes {#platforms}

> [!WARNING]
> - **Wayland** : la capture en temps réel utilise la bibliothèque PipeWire du système (chargée à l'exécution ; sans elle : captures du portail, environ 1 FPS). La capture démarre avec le premier viewer (le portail demande alors le partage d'écran, une fois) et s'arrête 15 s après le dernier. Le contrôle à distance demande `/dev/uinput` (`scripts/install.sh --uinput`), sinon le panneau affiche `Input: unavailable` et les sessions restent en lecture seule.
> - **Windows** : le code est vérifié avec MinGW mais pas encore lancé sur un vrai Windows.
> - **Client web** : il ne peut pas lire le certificat, il envoie donc le code dans TLS sans la liaison Argon2id.

## Backends de capture et d'entrée {#backends}

| Plateforme | Capture | Entrée | Terminaux |
|---|---|---|---|
| Windows | DXGI Desktop Duplication | `SendInput` (scancodes + unicode) | ConPTY |
| Linux X11, macOS | Qt | `uinput` ou XTest (Linux) | `forkpty` |
| Linux Wayland | xdg-desktop-portal ScreenCast + PipeWire | `uinput` | `forkpty` |

## Réseau {#network}

- Les tunnels TCP (`bore`, `ngrok`, `cloudflared`) gardent la vidéo en TCP : pas d'UDP à travers eux.
- Les navigateurs n'ont pas d'UDP brut ; les data channels WebRTC ne sont pas implémentés. Les websockets supplémentaires rendent du débit sur les chemins avec pertes, pas un ping plus bas.
