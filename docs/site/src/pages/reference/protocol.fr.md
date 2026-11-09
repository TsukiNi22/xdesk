+++
title = "Protocole"
summary = "Le format réseau entre un client et l'hôte : le port, les messages de contrôle JSON, les trames binaires, la vidéo UDP chiffrée, octet par octet."
section = "reference"
order = 3
kind = "protocol"
keywords = "protocole websocket json binaire trame tuile jpeg uni palette copy vidéo h264 curseur souris touche texte terminal udp datagramme aes-gcm hello ack nack lane handshake"
+++

Lu depuis `include/xdesk/net/Protocol.hpp`, `Datagram.hpp` et les écritures de l'encodeur. Version du protocole `1` (`XDESK_PROTOCOL_VERSION`, augmentée à chaque changement incompatible).

## Trames {#framing}

Un seul port TLS 1.3 (`47800` par défaut) sert le HTTPS (`/`, `/api/info`, `/app/xdesk.apk`, `/app/version`) et la websocket `/ws` (RFC 6455) ; les sockets vidéo supplémentaires des viewers web ouvrent `/lane`.

- une **trame texte** est un objet JSON `{"t": "<type>", ...}` (contrôle), 64 Kio au plus ;
- une **trame binaire** est `[u8 type][payload]`, tous les entiers en **big endian**, 32 Mio au plus (une image 4K complète) ;
- la découverte sur le LAN est un broadcast UDP sur le port `47801`.

## Handshake {#handshake}

:::html
<ol class="seq">
  <li><div><span class="dir">client → hôte</span><br>TLS 1.3, groupe <code>X25519MLKEM768</code> en premier (clients natifs : groupes post-quantiques seulement), certificat vérifié avec l'empreinte épinglée.</div></li>
  <li><div><span class="dir">client → hôte</span><br><code>hello</code> : <code>v kind name os device nonce udp</code>.</div></li>
  <li><div><span class="dir">hôte → client</span><br><code>challenge</code> : <code>host version nonce salt kdf known</code>.</div></li>
  <li><div><span class="dir">client → hôte</span><br><code>auth</code> : <code>method = code</code> (web) ou <code>proof</code> (natif, <code>HMAC(Argon2id(code, sel), transcript)</code>), <code>device</code>, <code>remember</code>.</div></li>
  <li><div><span class="dir">hôte → client</span><br><code>pending</code> pendant que la personne devant le PC décide (appareil inconnu), puis <code>welcome</code> (<code>session perms screen capture device proof</code>) ou <code>denied</code> (<code>reason</code>).</div></li>
</ol>
:::

## Messages JSON {#json}

| t | sens | champs |
|---|---|---|
| `hello` | C → S | v kind name os device nonce udp (udp : l'appli Android a sa propre socket UDP) |
| `challenge` | S → C | host version nonce salt kdf known |
| `auth` | C → S | method = code (web) ou proof, device, remember |
| `pending / welcome / denied` | S → C | session perms screen capture device proof / reason |
| `ping / pong` | ↔ | ts |
| `welcome.udp` / `udp_off` | S → C / C → S | port connection down up (clés de la vidéo UDP) / le viewer n'a pas pu l'atteindre |
| `welcome.lanes / lanes` | S → C | token max (jeton des sockets supplémentaires) / n sockets à garder pour la vidéo |
| `lane` | C → S (premier message de /lane) | session token index |
| `quality / mode / refresh` | C → S | quality scale fps / view ou control |
| `term_open / term_resize / term_close / term_exit` | ↔ | id cols rows code |
| `perms / screen / info / bye / clip` | S → C (clip ↔) | perms, w h, text, reason, texte du presse-papiers |
| `focus_request` | C → S | contrôle mis en pause par le panneau de l'hôte : rendu après 1 min sauf refus |
| `input` | S → C | state (loading, ready, paused, none), error, wait, asked |

## Messages binaires {#binary}

:::message Frame 0x10
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x10` |
| id | u32 | 4 | id de l'image |
| width | u16 | 2 | largeur de l'écran |
| height | u16 | 2 | hauteur de l'écran |
| count | u16 | 2 | nombre de tuiles |
| tiles | tile | var | `count` tuiles, voir plus bas |
Envoyée quand la fenêtre de la session est ouverte ; acquittée par un `FrameAck` à l'arrivée. La première image contient toutes les tuiles, les suivantes seulement les tuiles changées.
:::

:::message FrameAck 0x11
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x11` |
| id | u32 | 4 | id de l'image |
Les acquittements dimensionnent les images en vol et mesurent le ping et le débit.
:::

:::message Cursor 0x12
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x12` |
| x | i16 | 2 |  |
| y | i16 | 2 |  |
| visible | u8 | 1 | 0 / 1 |
Le curseur distant, dessiné par le viewer.
:::

:::message Video 0x13
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x13` |
| id | u32 | 4 | id de l'image |
| width | u16 | 2 | arrondie au pair supérieur |
| height | u16 | 2 | arrondie au pair supérieur |
| flags | u8 | 1 | 1 = image clé |
| data | h264 | var | access unit annex B de toute l'image |
Seulement aux clients annonçant `h264` dans `enc`, tant que leurs images gardent leur ordre ; acquittée comme une image.
:::

:::message MouseMove 0x20
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x20` |
| x | u16 | 2 | absolue |
| y | u16 | 2 | absolue |
Pixels de l'écran de l'hôte ; ignoré sans le droit de contrôle.
:::

:::message MouseButton 0x21
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x21` |
| button | u8 | 1 | voir les codes |
| down | u8 | 1 | 1 appuyé, 0 relâché |

:::

:::message MouseWheel 0x22
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x22` |
| dx | i16 | 2 | 1/120 de cran |
| dy | i16 | 2 | 1/120 de cran |

:::

:::message Key 0x23
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x23` |
| down | u8 | 1 | 1 appuyé, 0 relâché |
| length | u8 | 1 | longueur de code |
| code | ascii | var | `KeyboardEvent.code` : `KeyA`, `ShiftLeft`... |
Codes physiques de touches : la disposition de l'hôte s'applique.
:::

:::message Text 0x24
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x24` |
| text | utf8 | var | le reste du message |
Un clavier virtuel n'envoie que ce qui a changé dans son champ (touches Backspace + les nouvelles lettres).
:::

:::message TermData 0x31
| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | `0x31` |
| id | u8 | 1 | id du terminal |
| data | bytes | var | octets bruts |
C → S : l'entrée du terminal `id` ; S → C : sa sortie.
:::

## Tuiles d'une Frame {#tiles}

Chaque tuile d'une `Frame` commence par `u16 x, u16 y, u16 w, u16 h, u8 enc`, puis :

| enc | nom | payload |
|---|---|---|
| `1` | Jpeg | u32 longueur, données JPEG |
| `2` | Solid | u8 r, u8 g, u8 b : toute la tuile d'une seule couleur |
| `3` | Palette | u8 n (1-64), n × (u8 r, g, b), u32 longueur, flux zlib de w × h index u8 (sans perte ; clients annonçant palette) |
| `4` | Copy | u16 sx, u16 sy : le rectangle w × h en (sx, sy) de l'image telle qu'elle était avant ce message (défilement ; clients annonçant copy) |

## Vidéo UDP chiffrée {#udp}

Viewers natifs et appli Android, sur le port du serveur. Un datagramme est un en-tête de 13 octets en clair mais authentifié, le payload AES-256-GCM et son tag de 16 octets. Deux clés aléatoires de 256 bits par session (une par sens) arrivent dans le `welcome.udp` à l'intérieur de TLS ; nonce = sens + séquence (jamais réutilisé) ; une fenêtre de 2048 séquences écarte les rejeux ; l'hôte ne répond qu'à une adresse qui a envoyé un datagramme authentique.

| champ | type | taille | description |
|---|---|---|---|
| type | u8 | 1 | 1 Hello ... 7 Pong |
| connection | u32 | 4 | id de connexion de la session |
| sequence | u64 | 8 | par sens, authentifiée |
| payload | AES-256-GCM | var | les champs ci-dessous |
| tag | octets | 16 | tag GCM |

| type | nom | sens | payload |
|---|---|---|---|
| `1` | Hello | V → H | vide : le viewer prouve les clés depuis son adresse |
| `2` | HelloAck | H → V | vide |
| `3` | Data | H → V | u32 image, u16 index (morceaux de données d'abord, puis une parité par groupe de 8), u16 nombre de données, u32 taille de l'image, octets (1150 par morceau) |
| `4` | Ack | V → H | u32 image (complète) |
| `5` | Nack | V → H | u32 image, u16 n, n × u16 index manquants |
| `6` | Ping | V → H | u64 ms (garde le NAT ouvert) |
| `7` | Pong | H → V | u64 ms (le même) |

Retour sur la websocket quand l'UDP est bloqué (2,5 s de hellos sans réponse) ou muet (4 s) ; une image abandonnée après 3 s demande une image complète.

## Codes {#codes}

Boutons de souris : `0` gauche, `1` milieu, `2` droit, `3` précédent, `4` suivant. Types de client : `0` gui, `1` tty, `2` web, `3` apk (un type inconnu est traité comme web, le plus restreint).
