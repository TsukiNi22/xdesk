+++
title = "Protection contre l'altération"
summary = "Pourquoi une session connectée ne peut jamais modifier ou arrêter le programme par lequel elle passe, et ce qui arrive quand elle essaie."
section = "security"
order = 2
keywords = "altération protection intégrité garde ban binaire config données surveillé haché commande terminal kill taskkill systemctl durci ptrace dumpable débannir"
+++

Une session connectée ne doit jamais modifier ou arrêter le programme par lequel elle passe. Activé par défaut (`banOnTamper`).

## L'idée {#idea}

La personne devant le PC compte sur xdesk pour lui montrer tout ce que fait une session. Une session qui modifie xdesk, ses réglages ou son registre, ou qui le tue, pourrait se cacher ou l'enfermer dehors : elle est donc bannie aussitôt.

## Ce qui est surveillé {#watched}

| Surveillé | Comment | Réaction |
|---|---|---|
| le binaire, le dossier de config, le dossier de données | hachés et surveillés | une modification pendant qu'une session agit (une commande qui tourne dans un de ses terminaux, une entrée dans les 5 dernières s) bannit ces sessions ; une modification sans session active est notée comme locale (une recompilation, une mise à jour) |
| les lignes tapées dans un terminal distant | vérifiées **avant** que leur Entrée n'atteigne le shell : chemins de xdesk, son nom, `kill` / `taskkill` / `systemctl` de son pid... | la ligne n'est pas lancée, la session est bannie |
| les processus lancés depuis les terminaux distants | analysés (Linux : ligne de commande et fichiers ouverts) | ban |
| le processus de l'hôte | durci (Linux : non dumpable, non traçable ; Windows : droits de terminaison, mémoire et threads retirés) | un hôte tué pendant une session à distance bannit ces appareils au démarrage suivant |

## Actions locales seulement {#local}

- Les entrées à distance sont en pause pendant que l'utilisateur local se sert du panneau (Windows : le panneau est exclu de la capture).
- Débannir, faire confiance, oublier et les réglages sont refusés tant qu'une session à distance a le contrôle ou un terminal.
- Lever un ban demande de taper le mot `unban` ; `xdesk unban` refuse les pipes et les shells lancés par xdesk.

> [!WARNING]
> Ces vérifications arrêtent les moyens habituels de toucher à xdesk depuis un terminal distant, pas un attaquant déterminé avec un shell qui tourne sous ton utilisateur. La vraie protection est de ne pas donner le droit terminal à des personnes en qui tu n'as pas confiance.
