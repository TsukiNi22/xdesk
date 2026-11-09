+++
title = "Se connecter à un PC"
summary = "Les quatre clients : le viewer Qt, le client terminal, un navigateur et l'appli Android, avec leurs touches et gestes."
section = "guide"
order = 3
keywords = "connecter client viewer qt tty terminal navigateur web téléphone android apk tactile gestes touches scan hôtes connus empreinte"
+++

Chaque client parle au même port TLS de l'hôte (`47800` par défaut) et reçoit les droits que l'hôte lui donne.

## Les clients {#clients}

| Client | Le lancer | Montre |
|---|---|---|
| Viewer Qt (`gui`) | `xdesk connect <adresse> -c <code>`, `xdesk viewer` | l'écran (ajusté / 1:1 / plein écran), les terminaux en onglets |
| Client terminal (`tty`) | `xdesk connect <adresse> --tty` | les terminaux, et l'écran en demi-blocs truecolor (`--screen`) |
| Navigateur (`web`) | `https://<adresse>:<port>` | l'écran, les terminaux (xterm.js) |
| Appli Android (`apk`) | le QR code, ou `/app/xdesk.apk` proposé par la page web | le client web en plein écran, certificat épinglé |

```bash "Terminal"
xdesk connect 192.168.1.20 -c K7F-29Q-XM4    # viewer Qt
xdesk connect my-pc.ts.net --tty             # terminal distant dans ce terminal
xdesk connect bore.pub:41234 --tty --screen  # démarre sur l'écran distant
xdesk connect --tty                          # page de connexion : hôtes connus, scan du LAN, adresse et code
xdesk connect 192.168.1.20 --view-only       # n'envoie jamais clavier / souris
xdesk scan                                   # hôtes annoncés sur le LAN
```

Les clients natifs épinglent le certificat de l'hôte à la première connexion (l'empreinte est affichée des deux côtés) et refusent une identité changée. [Pourquoi](page:how-pairing-works#idea).

## Touches du client terminal {#tty-keys}

Après `ctrl+]` :

| Touche | Effet |
|---|---|
| `s` / `0`, `1`-`9`, `n` `p` | onglet de l'écran, terminal N, onglet suivant / précédent |
| `t`, `w` | nouveau terminal, fermer celui-ci |
| `c`, `d` | contrôle on / off, ctrl+alt+del |
| `+` `-`, `f`, `r` | qualité d'image, fréquence, rafraîchir |
| `pgup` `pgdn` | remonter l'historique (la molette aussi) |
| `q`, `?`, `ctrl+]` | déconnecter, touches, un vrai ctrl+] |

`XDESK_TTY=ascii,16` force ce que le terminal affiche (`ascii`, `unicode`, `mono`, `16`, `256`, `truecolor`, `no-mouse`).

## Navigateur et téléphone {#browser}

Scanne le QR code du panneau avec l'appareil photo : l'appli ou le navigateur s'ouvre avec le code rempli. Ou ouvre l'adresse affichée dans le panneau et tape le code (ou Scan).

| Geste (mode trackpad) | Fait |
|---|---|
| glisser | déplacer le pointeur |
| tap / double tap | clic / double clic |
| tap puis glisser, appui long puis glisser | glisser-déposer |
| tap / glisser à deux doigts | clic droit / défilement |
| pincer | zoom |
| tap à trois doigts | clavier |

Mode toucher direct : tap = clic à cet endroit, glisser = glisser-déposer, appui long puis relâcher = clic droit. Une barre donne Échap, Tab, Ctrl / Alt / Maj collants, Win, flèches, F1-F12, Ctrl+Alt+Suppr et Coller.

> [!WARNING]
> Le certificat est auto-signé : le navigateur demande de l'accepter une fois (compare l'empreinte avec le panneau). Un navigateur ne peut pas lire le certificat, il envoie donc le code dans TLS sans la liaison Argon2id : préfère les clients natifs sur un réseau non sûr, ou le [nom Tailscale](page:exposure#tailscale) (vrai certificat).
