+++
title = "Partager un PC"
summary = "Le panneau de l'hôte : l'interrupteur du partage, le code et son QR code, l'approbation, les sessions et leurs droits, les appareils."
section = "guide"
order = 2
keywords = "hôte panneau partager partage code qr approbation accepter confiance refuser sessions droits appareils bannir débannir tty touches tray"
+++

La personne devant le PC garde la main : elle voit chaque session en direct et décide qui reste.

## Le panneau {#panel}

`xdesk` (ou `xdesk host`) ouvre le panneau : une fenêtre Qt (Auto / Clair / Sombre, icône dans le tray) ou, avec `--tty` ou sans affichage, le même panneau dans le terminal. Les deux ont 5 pages : **vue d'ensemble, sessions, appareils, logs, réglages**.

**Le partage démarre arrêté.** Start / Stop sharing (en bas à gauche, le tray, `s` dans le panneau terminal) démarre ou arrête le serveur, l'exposition et la capture sans quitter ; chaque démarrage donne un nouveau code.

## Le code {#code}

| | |
|---|---|
| Format | 9 caractères en base32 de Crockford (`K7F-29Q-XM4`) : 45 bits |
| Rotation | après chaque usage, toutes les 300 s (`codeLifetime`), à chaque démarrage du partage |
| Mauvais codes | 5 depuis une adresse (`maxAuthFailures`) : cette adresse est bannie 1 h et le code change |
| QR code | `xdesk://host:port?fp=..&code=..` pour l'appli Android (épingle le certificat), `https://host:port/?code=..` pour un navigateur |

## Approbation {#approval}

Un appareil inconnu attend **Accepter / Accepter et faire confiance / Refuser** (refusé après 30 s). Le réglage `approval` : `untrusted` (tout appareil sans confiance, par défaut), `always`, `never` (le code suffit). Un appareil **de confiance** se reconnecte sans code, avec son secret d'appareil de 256 bits.

## Sessions et droits {#sessions}

La page des sessions montre chacune en direct : appareil, client (`gui`, `tty`, `web`, `apk`), adresse, mode, droits, FPS, latence, débit, dernière entrée. En ouvrir une montre chaque entrée qu'elle envoie et le miroir de ses terminaux.

| Droit | Permet |
|---|---|
| view | voir l'écran |
| control | clavier et souris |
| terminal | ouvrir des terminaux (4 au plus par session) |
| clipboard | lire et écrire le presse-papiers |

Les droits changent en direct. Pendant que tu utilises le panneau, les entrées à distance sont en pause ; un viewer peut redemander le contrôle (accordé après 60 s sauf refus).

## Appareils {#devices}

Appareils de confiance, connus et bannis, et adresses bannies, un enregistrement par machine. Faire confiance, retirer la confiance, oublier et débannir ne sont possibles **que sur ce PC**, et refusés tant qu'une session à distance a le contrôle ou un terminal. Lever un ban demande de taper le mot `unban`.

```bash "Terminal"
xdesk devices                  # appareils et adresses connus / de confiance / bannis
xdesk unban <id d'appareil | adresse>  # hôte arrêté, le mot unban tapé
```

## Touches du panneau terminal {#keys}

| Touche (`?` les affiche) | Effet |
|---|---|
| `1`-`5`, `tab` | pages vue d'ensemble / sessions / appareils / logs / réglages |
| `s`, `q` | démarrer / arrêter le partage, quitter |
| `a` `t` `r` | accepter / accepter et faire confiance / refuser une connexion |
| `n` `y` `enter` `c` `e` (vue d'ensemble) | nouveau code, copier le code, QR code, copier son lien, exposition |
| `v` `c` `t` `p` `m` `k` `b` `K` (sessions) | droits voir / contrôle / terminal / presse-papiers, suivre ses terminaux, déconnecter, bannir, tout déconnecter |
| `T` `U` `f` `u` (appareils) | confiance, retirer la confiance, oublier, débannir |
| `l` `c` `/` `enter` (logs) | niveau, catégorie, recherche, détails |
