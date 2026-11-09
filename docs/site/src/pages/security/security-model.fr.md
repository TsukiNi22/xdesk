+++
title = "Modèle de sécurité"
summary = "Les couches qui protègent une connexion, qui peut atteindre l'hôte, et les limites honnêtes."
section = "security"
order = 1
keywords = "sécurité tls 1.3 post-quantique ml-kem x25519 certificat épinglage argon2id hmac authentification mutuelle force brute registre csp web limites modèle de menace"
+++

Chaque connexion est en TLS 1.3 de bout en bout avec un échange de clés post-quantique, et la personne devant le PC voit et décide tout.

## Couches {#layers}

| Couche | Quoi |
|---|---|
| Transport | TLS 1.3 seulement, échange de clés hybride `X25519MLKEM768` en premier (OpenSSL >= 3.5) ; les clients natifs refusent tout ce qui n'est pas post-quantique |
| Identité de l'hôte | certificat ECDSA P-256 auto-signé, épinglé par les clients natifs à la première connexion ; une identité changée est refusée |
| Preuve du code (natif) | `HMAC(Argon2id(code, sel), transcript)`, transcript = nonces + `sha256(certificat)` : pas de force brute hors ligne, pas de relais par un homme du milieu |
| Authentification mutuelle | l'hôte répond avec sa propre preuve : un faux hôte est détecté |
| Appareils de confiance | secret d'appareil de 256 bits, même preuve HMAC, donné seulement quand l'hôte fait confiance à l'appareil |
| Force brute | `maxAuthFailures` mauvais codes depuis une adresse : ban d'1 h et nouveau code |
| Registre | `devices.json` signé avec une clé HMAC (`registry.key`, `0600`) |
| Web | CSP stricte (`default-src 'self'`), no-store, aucun script inline, `frame-ancestors 'none'` |
| Vidéo UDP | AES-256-GCM, clés par session envoyées dans TLS, fenêtre anti-rejeu, aucune réponse à une adresse non authentifiée |

[Comment marche la preuve du code](page:how-pairing-works#how).

## Qui peut atteindre l'hôte {#reach}

Seulement le LAN et le PC lui-même, plus le chemin que le [mode d'exposition](page:exposure#modes) ajoute ; un pair arrivant par un autre chemin est coupé avant TLS. Le partage démarre arrêté à chaque lancement, et chaque démarrage donne un nouveau code.

## Limites {#limits}

> [!WARNING]
> Un terminal distant tourne avec les droits de ton utilisateur. La [protection contre l'altération](page:tamper-protection#idea) arrête les moyens habituels de toucher à xdesk, pas un attaquant déterminé avec un shell : ne donne les terminaux qu'à des personnes de confiance (le droit peut être retiré en direct).

- Le client web ne peut pas lire le certificat : il envoie le code dans TLS sans la liaison Argon2id. Préfère les clients natifs sur un réseau non sûr.
- `cloudflared` termine TLS au relais.
- Le certificat Tailscale publie le nom MagicDNS dans les logs publics de certificats.
