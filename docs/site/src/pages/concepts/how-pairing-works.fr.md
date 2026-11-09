+++
title = "Comment marche l'appairage"
summary = "Comment un code de 9 caractères prouve qui tu es sans jamais être envoyé, et pourquoi un homme du milieu ou un devineur n'obtient rien."
section = "concepts"
order = 1
kind = "explain"
level = "simple"
keywords = "appairage code preuve argon2id hmac transcript nonce sel certificat épinglage empreinte mitm force brute appareil de confiance secret handshake"
+++

:::simple tldr
**En une phrase :** le client prouve qu'il connaît le code sans l'envoyer, et la preuve ne marche que sur la connexion pour laquelle elle a été faite : personne au milieu ne peut la réutiliser.
:::

:::technical tldr
**Définition :** une preuve proche d'un PAKE au-dessus de TLS 1.3 : `preuve = HMAC(Argon2id(code, sel), transcript)`, le transcript étant lié aux deux nonces et à `sha256(certificat)` (tls-server-end-point, RFC 5929) ; l'hôte répond avec sa propre preuve (authentification mutuelle).
:::

## L'idée {#idea}

:::simple
Deux personnes doivent vérifier qu'elles connaissent le même mot de passe, dans une pièce où quelqu'un écoute peut-être. Au lieu de le dire, chacune écrit un calcul qui ne marche qu'avec le mot de passe **et** la date du jour et le numéro de la pièce. L'espion n'apprend rien, et le papier ne sert à rien dans une autre pièce.
:::

:::technical
Un secret partagé de faible entropie (45 bits) authentifie les deux côtés d'un canal déjà chiffré. Le secret est étiré avec Argon2id (`m = 64 Mio`, `t = 3`, `p = 1`, sel de 16 octets par code) pour que chaque essai coûte cher, puis sert de clé HMAC sur un transcript qui contient les deux nonces (pas de rejeu) et l'empreinte du certificat vu par le client (pas de relais : un homme du milieu présente un autre certificat, la preuve ne correspond plus).
:::

## Comment ça marche {#how}

:::html
<figure class="stepper" data-steps="7">
  <ol class="captions">
    <li><b>TLS.</b> Le client ouvre TLS 1.3 avec l’échange de clés hybride <code>X25519MLKEM768</code> et vérifie le certificat avec l’empreinte qu’il a épinglée (première fois : affichée des deux côtés).</li>
    <li><b>hello.</b> Le client envoie son type, son nom, son OS, l’id de l’appareil et un nonce aléatoire.</li>
    <li><b>challenge.</b> L’hôte répond avec son propre nonce et le sel Argon2id de 16 octets du code actuel.</li>
    <li><b>auth.</b> Le client envoie <code>HMAC(Argon2id(code, sel), transcript)</code> : le transcript contient les deux nonces et le SHA-256 du certificat qu’il voit. Le code lui-même ne circule jamais.</li>
    <li><b>pending.</b> La preuve est bonne ; l’appareil est inconnu, la personne devant le PC est donc sollicitée (30 s, refusé par défaut).</li>
    <li><b>welcome.</b> Accepté : la session reçoit ses droits, et l’hôte prouve qu’il connaît aussi le code, pour démasquer un faux hôte.</li>
    <li><b>Session.</b> Images, entrées et terminaux circulent sur la même connexion TLS (la vidéo peut passer en UDP chiffré).</li>
  </ol>
  <svg viewBox="0 0 760 360" role="img" aria-label="handshake entre un client et l'hôte">
    <defs><marker id="fr-hs-arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path class="arrow" d="M0 0L10 5L0 10z"/></marker></defs>
    <rect id="fr-hs-client" class="box-alt" x="40" y="10" width="160" height="40" rx="8"/><text class="lbl" x="120" y="35" text-anchor="middle">client</text>
    <rect id="fr-hs-host" class="box-alt" x="560" y="10" width="160" height="40" rx="8"/><text class="lbl" x="640" y="35" text-anchor="middle">hôte</text>
    <path class="edge dash" d="M120 50V350" data-free=""/>
    <path class="edge dash" d="M640 50V350" data-free=""/>
    <rect id="fr-hs-c0" class="box" x="116" y="76" width="8" height="8" data-show="0-"/>
    <rect id="fr-hs-h0" class="box" x="636" y="76" width="8" height="8" data-show="0-"/>
    <path id="fr-hs-m0" class="edge-accent" data-from="fr-hs-c0" data-to="fr-hs-h0" marker-end="url(#fr-hs-arr)" data-show="0-" data-hl="0"/>
    <text class="lbl" x="380" y="72" text-anchor="middle" data-show="0-">TLS 1.3, X25519MLKEM768</text>
    <rect id="fr-hs-c1" class="box" x="116" y="116" width="8" height="8" data-show="1-"/>
    <rect id="fr-hs-h1" class="box" x="636" y="116" width="8" height="8" data-show="1-"/>
    <path id="fr-hs-m1" class="edge" data-from="fr-hs-c1" data-to="fr-hs-h1" marker-end="url(#fr-hs-arr)" data-show="1-" data-hl="1"/>
    <text class="lbl" x="380" y="112" text-anchor="middle" data-show="1-">hello (nonce, appareil)</text>
    <rect id="fr-hs-c2" class="box" x="116" y="156" width="8" height="8" data-show="2-"/>
    <rect id="fr-hs-h2" class="box" x="636" y="156" width="8" height="8" data-show="2-"/>
    <path id="fr-hs-m2" class="edge" data-from="fr-hs-h2" data-to="fr-hs-c2" marker-end="url(#fr-hs-arr)" data-show="2-" data-hl="2"/>
    <text class="lbl" x="380" y="152" text-anchor="middle" data-show="2-">challenge (nonce, sel)</text>
    <rect id="fr-hs-c3" class="box" x="116" y="196" width="8" height="8" data-show="3-"/>
    <rect id="fr-hs-h3" class="box" x="636" y="196" width="8" height="8" data-show="3-"/>
    <path id="fr-hs-m3" class="edge" data-from="fr-hs-c3" data-to="fr-hs-h3" marker-end="url(#fr-hs-arr)" data-show="3-" data-hl="3"/>
    <text class="lbl" x="380" y="192" text-anchor="middle" data-show="3-">auth (preuve)</text>
    <rect id="fr-hs-c4" class="box" x="116" y="236" width="8" height="8" data-show="4-"/>
    <rect id="fr-hs-h4" class="box" x="636" y="236" width="8" height="8" data-show="4-"/>
    <path id="fr-hs-m4" class="edge" data-from="fr-hs-h4" data-to="fr-hs-c4" marker-end="url(#fr-hs-arr)" data-show="4-" data-hl="4"/>
    <text class="lbl" x="380" y="232" text-anchor="middle" data-show="4-">pending (approbation)</text>
    <rect id="fr-hs-c5" class="box" x="116" y="276" width="8" height="8" data-show="5-"/>
    <rect id="fr-hs-h5" class="box" x="636" y="276" width="8" height="8" data-show="5-"/>
    <path id="fr-hs-m5" class="edge" data-from="fr-hs-h5" data-to="fr-hs-c5" marker-end="url(#fr-hs-arr)" data-show="5-" data-hl="5"/>
    <text class="lbl" x="380" y="272" text-anchor="middle" data-show="5-">welcome (droits, preuve)</text>
    <rect id="fr-hs-c6" class="box" x="116" y="316" width="8" height="8" data-show="6-"/>
    <rect id="fr-hs-h6" class="box" x="636" y="316" width="8" height="8" data-show="6-"/>
    <path id="fr-hs-m6" class="edge-accent" data-from="fr-hs-c6" data-to="fr-hs-h6" marker-end="url(#fr-hs-arr)" data-show="6-" data-hl="6"/>
    <text class="lbl" x="380" y="312" text-anchor="middle" data-show="6-">images, entrées, terminaux</text>
  </svg>
</figure>
:::

## Quelle solidité {#strength}

:::simple
Il y a environ **35 000 milliards** de codes possibles. Une adresse a 5 essais, puis elle est bannie une heure et le code change ; le code change aussi toutes les 5 minutes. Deviner est sans espoir.
:::

:::technical
| Grandeur | Valeur |
|---|---|
| Espace des codes | 32^9 = 2^45 = 35 184 372 088 832 |
| Essais en ligne par adresse | 5 (`maxAuthFailures`), puis un ban d'1 h et un nouveau code |
| Chance que 5 essais tombent sur un code | 5 / 2^45 ≈ 1,42e-13 |
| Durée de vie du code | 300 s (`codeLifetime`), et un seul usage |
| Attaque hors ligne | impossible : un observateur ne voit que TLS ; la preuve demande le transcript d'une session TLS dont il ne fait pas partie |
:::

## Appareils de confiance {#trusted}

« Accepter et faire confiance » donne à l'appareil un **secret de 256 bits**, utilisé avec la même preuve HMAC à la place du code : il se reconnecte sans code. Le registre (`devices.json`) est signé avec une clé HMAC (`registry.key`, mode `0600`) : modifié hors de xdesk, il est refusé et chaque appareil de confiance doit être réappairé.

## Pièges {#pitfalls}

- Les **navigateurs** ne peuvent pas lire le certificat : ils envoient le code dans TLS (`method = code`), sans la liaison. Préfère les clients natifs sur un réseau non sûr.
- **cloudflared** termine TLS au relais : le relais voit le trafic. Tous les autres modes d'exposition sont de bout en bout.
- **`xdesk --regen-cert`** crée une nouvelle identité : les clients natifs la refusent tant que la nouvelle empreinte n'est pas confirmée.

## Vérifie-toi {#quiz}

:::html
<details class="quiz"><summary>Un attaquant relaie la connexion TLS entre le client et l'hôte. Pourquoi la preuve échoue-t-elle ?</summary><p>Le client voit le certificat de l'attaquant : son SHA-256 est dans le transcript, la preuve est donc calculée pour cette connexion, pas pour celle de l'hôte.</p></details>
<details class="quiz"><summary>Pourquoi Argon2id et pas un simple hash du code ?</summary><p>Un code de 45 bits est petit : chaque essai doit coûter 64 Mio de mémoire et trois passes, pour qu'en essayer beaucoup soit lent.</p></details>
<details class="quiz"><summary>Contre quoi protège la preuve de l'hôte dans le welcome ?</summary><p>Un faux hôte : sans le code (ou le secret de l'appareil) il ne peut pas la produire.</p></details>
:::

## Glossaire {#glossary}

| Terme | Sens |
|---|---|
| Nonce | une valeur aléatoire utilisée une fois, pour qu'un vieux message ne puisse pas être rejoué |
| Sel | des octets aléatoires mélangés au code avant le hash (16 octets par code) |
| Transcript | le label `xdesk-auth-v1` et le rôle, les deux nonces, l'empreinte du certificat |
| Épinglage | retenir l'empreinte de l'hôte à la première connexion et refuser toute autre |
| ML-KEM-768 | l'encapsulation de clé post-quantique du groupe TLS hybride |

Sources : `include/xdesk/security/Crypto.hpp` (`XDESK_ARGON2_*`, `auth_transcript`), `PairingCode.hpp`, RFC 5929 (liaisons de canal), RFC 9106 (Argon2).
