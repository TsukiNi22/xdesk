+++
title = "Comment l'écran est envoyé"
summary = "Pourquoi un écran immobile ne coûte rien, comment le défilement et la vidéo sont traités, et comment le flux s'adapte à un lien lent ou avec pertes."
section = "concepts"
order = 2
kind = "explain"
level = "simple"
keywords = "écran flux tuiles 64x64 jpeg uni palette copy défilement h264 openh264 mouvement images en vol ack ping débit udp aes-gcm parité nack lanes websockets"
+++

:::simple tldr
**En une phrase :** xdesk n'envoie que les parties de l'écran qui ont changé, réutilise ce que le viewer a déjà quand tu fais défiler, et envoie des images plus légères pendant que le lien est lent, puis nettes à nouveau.
:::

:::technical tldr
**Définition :** un encodeur de tuiles modifiées par session (tuiles 64×64 hachées, tuiles JPEG / unies / palette / copy, H.264 pour les grands mouvements sur un lien goulot) piloté par un contrôle de flux à acquittements (images en vol dimensionnées depuis le ping et le débit mesurés), sur des websockets TLS, de l'UDP chiffré pour les viewers natifs, ou jusqu'à 4 websockets pour les viewers web sur les chemins avec pertes.
:::

## L'idée {#idea}

:::simple
Imagine un tableau blanc filmé pour quelqu'un au loin. Si un seul mot change, tu n'envoies pas une nouvelle photo de tout le tableau : tu envoies le coin avec le mot. Si le texte remonte, tu dis « monte cette partie » et tu n'envoies que la nouvelle ligne en bas.
:::

:::technical
L'hôte garde, par session, le hash de chaque tuile que le viewer possède. Une capture ne produit que les tuiles dont le hash a changé ; une capture sans nouveauté ne produit rien. L'encodage d'une tuile est choisi selon son contenu (unie, palette, JPEG) et l'historique (copie d'une région précédente), et le rythme selon les acquittements du viewer.
:::

## Comment ça marche {#how}

:::html
<figure class="stepper" data-steps="6">
  <ol class="captions">
    <li><b>Première image.</b> Le viewer n’a encore rien : toutes les tuiles de 64×64 sont envoyées.</li>
    <li><b>Hash.</b> Quelque chose bouge : les tuiles sont hachées et 3 sur 32 diffèrent de ce qu’a le viewer.</li>
    <li><b>Encodage.</b> Seules ces 3 partent : une tuile d’une seule couleur en 3 octets, les tuiles changées d’une rangée fusionnées en un JPEG (ou une palette sans perte pour du texte).</li>
    <li><b>Défilement.</b> La page défile d’une rangée : les tuiles déplacées sont retrouvées plus haut dans l’image précédente et envoyées comme tuiles <i>copy</i> ; seule la rangée révélée est encodée.</li>
    <li><b>Mouvement.</b> Plus d’un quart des tuiles changent (une vidéo) : un JPEG plus léger, ou une image H.264 de tout l’écran tant que le lien du viewer est le goulot.</li>
    <li><b>Immobile.</b> L’écran ne bouge plus : les mêmes tuiles sont renvoyées, nettes.</li>
  </ol>
  <svg viewBox="0 0 760 210" role="img" aria-label="tuiles de l'écran envoyées à chaque étape">
    <rect id="fr-tl-panel" class="group" x="350" y="35" width="390" height="150" rx="8"/>
    <rect id="fr-tl-t00" class="box" x="30" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t10" class="box" x="68" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t20" class="box" x="106" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t30" class="box" x="144" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t40" class="box" x="182" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t50" class="box" x="220" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t60" class="box" x="258" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t70" class="box" x="296" y="40" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t01" class="box" x="30" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t11" class="box" x="68" y="78" width="34" height="34" rx="3" data-hl="0,2,5" data-warn="4,1"/>
    <rect id="fr-tl-t21" class="box" x="106" y="78" width="34" height="34" rx="3" data-hl="0,2,5" data-warn="4,1"/>
    <rect id="fr-tl-t31" class="box" x="144" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t41" class="box" x="182" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t51" class="box" x="220" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t61" class="box" x="258" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t71" class="box" x="296" y="78" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t02" class="box" x="30" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t12" class="box" x="68" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t22" class="box" x="106" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t32" class="box" x="144" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t42" class="box" x="182" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t52" class="box" x="220" y="116" width="34" height="34" rx="3" data-hl="0,2,5" data-warn="4,1"/>
    <rect id="fr-tl-t62" class="box" x="258" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t72" class="box" x="296" y="116" width="34" height="34" rx="3" data-hl="0,5" data-warn="4"/>
    <rect id="fr-tl-t03" class="box" x="30" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-t13" class="box" x="68" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-t23" class="box" x="106" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-t33" class="box" x="144" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-t43" class="box" x="182" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-t53" class="box" x="220" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-t63" class="box" x="258" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-t73" class="box" x="296" y="154" width="34" height="34" rx="3" data-hl="0,3,5" data-warn="4"/>
    <rect id="fr-tl-copy" class="group" x="27" y="37" width="306" height="116" rx="6" data-show="3"/>
    <text class="lbl" x="182" y="102" text-anchor="middle" data-show="3">copy</text>
    <text class="lbl" x="370" y="70" data-show="0">32 tuiles envoyées</text>
    <text class="small" x="370" y="96" data-show="0">tout l’écran</text>
    <text class="lbl" x="370" y="70" data-show="1">3 tuiles changées</text>
    <text class="small" x="370" y="96" data-show="1">29 identiques : rien envoyé</text>
    <text class="lbl" x="370" y="70" data-show="2">3 tuiles envoyées</text>
    <text class="small" x="370" y="96" data-show="2">1 unie (3 octets), 2 en un JPEG</text>
    <text class="lbl" x="370" y="70" data-show="3">24 tuiles copy + 8 encodées</text>
    <text class="small" x="370" y="96" data-show="3">copy = u16 sx, sy (4 octets)</text>
    <text class="lbl" x="370" y="70" data-show="4">JPEG plus léger / H.264</text>
    <text class="small" x="370" y="96" data-show="4">qualité baissée pendant le mouvement</text>
    <text class="lbl" x="370" y="70" data-show="5">tuiles nettes à nouveau</text>
    <text class="small" x="370" y="96" data-show="5">qualité revenue au réglage</text>
  </svg>
</figure>
:::

## À quelle vitesse {#flow}

:::simple
xdesk n'envoie pas plus vite que le viewer ne peut recevoir : il attend que le viewer dise « reçu » avant d'en envoyer plus. Sur un LAN 2 images peuvent être en route, pour un viewer lointain jusqu'à 8. Quand une file se forme, la qualité baisse jusqu'à ce qu'elle disparaisse.
:::

:::technical
Le thread de capture tourne toutes les 4 ms. Une session reçoit une image quand son plafond de FPS le permet et que sa fenêtre est ouverte : sans file, les images dont son rythme a besoin pendant un ping (2 sur un LAN, jusqu'à 8 au loin) ; avec une file, seulement ce que le chemin contient (débit × ping) et une qualité JPEG plus basse jusqu'à ce qu'elle se vide, pour qu'aucun retard ne s'accumule. Un viewer dont le lien ne suit pas même à la qualité la plus basse reçoit une résolution plus basse (`autoResolution`), remontée une fois le lien calme. Aucune image n'est envoyée pendant que le viewer affiche un terminal.
:::

## En UDP et sockets supplémentaires {#transports}

Le viewer Qt, le client terminal et l'appli Android reçoivent la vidéo en **UDP** sur le port du serveur : AES-256-GCM avec une clé par sens envoyée dans TLS, morceaux de 1150 octets, une parité xor pour 8 morceaux tant que des pertes sont vues, nacks, et des images qui se complètent dans n'importe quel ordre. Contrôle, entrées et terminaux restent sur TLS ; la vidéo revient sur TLS quand l'UDP est bloqué.

| Lien (émulé, 25 s) | TCP + TLS | UDP + GCM + parité |
|---|---|---|
| LAN câble | 30 fps, p95 4.9 ms | 30 fps, p95 13 ms (prototype Python) |
| Fibre, 40 ms | 30 fps, p99 69 ms | 30 fps, p99 50 ms |
| 4G, 80 ms, 1 % de perte | 27 fps, p95 163 ms | 27 fps, p95 160 ms |
| 160 ms, 3 % de perte, 6 Mbit/s | 17 fps, 15 blocages | 23 fps, 4 blocages |
| 300 ms, 8 % de perte, 2 Mbit/s | 5.4 fps, p50 506 ms | 7.8 fps, p50 243 ms |

Les navigateurs n'ont pas d'UDP brut : sur un chemin lointain avec pertes, un viewer web peut recevoir **jusqu'à 4 websockets** pour la vidéo, chacune avec sa propre fenêtre TCP, gardées seulement si elles aident. Débit utile en Mbit/s :

| Chemin (ping, perte, lien) | 1 socket | 2 | 3 | 4 |
|---|---|---|---|---|
| LAN, 2 ms, 300 Mbit/s | 286 | 290 | 293 | 292 |
| 80 ms, 1 %, 30 Mbit/s | 2.5 | 5.8 | 7.1 | 10.1 |
| 160 ms, 3 %, 6 Mbit/s | 0.9 | 1.4 | 2.0 | 3.2 |
| 300 ms, 8 %, 2 Mbit/s | 0.4 | 0.6 | 1.1 | 1.3 |

[Le format octet par octet de chaque message](page:protocol#udp).

## Pièges {#pitfalls}

- **Wayland** sans la bibliothèque PipeWire retombe sur les captures du portail : environ 1 image par seconde.
- **H.264** demande `openh264` à l'exécution (`libopenh264.so.8`, ou la dll de Cisco à côté de `xdesk.exe` sous Windows) ; sans elle, les grands mouvements restent en JPEG.
- Les **sockets supplémentaires** ne baissent pas le ping : elles rendent seulement le débit qu'un chemin avec pertes prend à une connexion.

## Vérifie-toi {#quiz}

:::html
<details class="quiz"><summary>Rien ne bouge sur l'écran de l'hôte. Combien est envoyé ?</summary><p>Rien : une capture sans nouvelle image est sautée, et les tuiles inchangées ne sont jamais envoyées.</p></details>
<details class="quiz"><summary>Pourquoi les tuiles copy ne servent-elles que tant que les images gardent leur ordre ?</summary><p>Une copie lit l'image telle qu'elle était avant le message : si une image plus ancienne arrivait après une plus récente, les pixels copiés seraient faux.</p></details>
:::

## Glossaire {#glossary}

| Terme | Sens |
|---|---|
| Tuile | un carré de 64×64 de l'écran, l'unité hachée et envoyée |
| Ack | le viewer qui dit à l'hôte qu'une image est arrivée |
| Images en vol | images envoyées mais pas encore acquittées |
| Parité | un xor de 8 morceaux qui reconstruit un morceau perdu sans le redemander |
| Lane | une websocket supplémentaire qui ne transporte que des images |

Sources : `src/xdesk/platform/FrameEncoder.cpp`, `src/xdesk/host/Stream.cpp`, `include/xdesk/net/Datagram.hpp` ; les mesures viennent de liens émulés avec netem.
