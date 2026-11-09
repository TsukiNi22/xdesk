+++
title = "FAQ"
summary = "Des réponses courtes aux questions habituelles sur xdesk."
section = "resources"
order = 4
keywords = "faq questions code changé navigateur avertissement certificat wayland contrôle indisponible débannir port pare-feu"
+++

## Le code que j'ai donné ne marche plus {#code}

Il change après chaque usage, toutes les 5 minutes (`codeLifetime`) et à chaque démarrage du partage. Donne celui affiché maintenant, ou fais confiance à l'appareil pour qu'il se reconnecte sans code. [Le code](page:share-a-pc#code).

## Le navigateur avertit à propos du certificat {#certificate}

Le certificat de l'hôte est auto-signé : accepte-le une fois après avoir comparé l'empreinte avec le panneau, ou utilise le [nom Tailscale](page:exposure#tailscale), qui a un vrai certificat.

## Le viewer voit l'écran mais ne peut pas le contrôler {#control}

Vérifie le droit **control** de la session dans le panneau. Sous Wayland, le contrôle à distance demande `/dev/uinput` (`scripts/install.sh --uinput`). Pendant que la personne devant le PC utilise le panneau, les entrées à distance sont en pause.

## Comment lever un ban ? {#unban}

Seulement sur le PC hôte : la page des appareils du panneau, ou `xdesk unban <id d'appareil | adresse>` avec l'hôte arrêté, en tapant le mot `unban`.

## Quel port dois-je ouvrir ? {#port}

`47800` en TCP (HTTPS + websocket) et le même port en UDP pour la vidéo des viewers natifs ; `47801` en UDP pour la découverte sur le LAN. Sous Windows, `install.ps1` ouvre `47800` en TCP et `47801` en UDP sur les réseaux privés.
