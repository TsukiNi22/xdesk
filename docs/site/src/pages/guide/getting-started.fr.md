+++
title = "Premiers pas"
summary = "Installer xdesk sous Linux ou Windows, puis partager un PC pour la première fois."
section = "guide"
order = 1
keywords = "installer installation dnf apt pacman my-universe paquets windows zip install.ps1 install.sh premier partage code"
+++

xdesk est un seul binaire : le même `xdesk` partage un PC (l'**hôte**) ou s'y connecte (le **client**).

## Installer sous Linux {#linux}

Paquets RPM (Fedora), DEB (Debian / Ubuntu) et Arch signés. Tous les dépôts de paquets de TsukiNi22 d'un coup, avec [my-universe](https://tsukini22.github.io/my-universe/) :

```bash "Terminal"
curl -fsSL https://tsukini22.github.io/my-universe/install.sh | bash -s
sudo dnf install xdesk          # ou : sudo apt install xdesk / sudo pacman -S xdesk
```

Ou seulement le dépôt de xdesk, avec son `setup.sh` :

```bash "Terminal"
curl -fsSL https://raw.githubusercontent.com/TsukiNi22/xdesk/main/setup.sh | bash -s -- --install
```

| Argument de `setup.sh` | Effet |
|---|---|
| `--install` | Installe aussi le paquet (par défaut seulement le dépôt et la clé GPG) |
| `--pre` | Le paquet de pré-release (`xdesk-pre`) |
| `--windows` | Les paquets Windows (`mingw64-xdesk`, sysroot MinGW, rpm / deb seulement) |
| `--no-sudo` | Lancer sans `sudo` (déjà root) |

Ou depuis les sources (dépendances, libutils, compilation, `~/.local/bin/xdesk`, entrées de menu) :

```bash "Terminal"
git clone https://github.com/TsukiNi22/xdesk.git && cd xdesk
scripts/install.sh --deps --apk --uinput   # --apk : embarquer l'appli Android, --uinput : contrôle à distance sous Wayland
scripts/install.sh --autostart             # panneau de l'hôte à la connexion (tray)
```

## Installer sous Windows {#windows}

Depuis le `xdesk-windows-x64.zip` d'une [release](https://github.com/TsukiNi22/xdesk/releases), dans PowerShell :

```powershell "PowerShell"
powershell -ExecutionPolicy Bypass -File install.ps1             # %LOCALAPPDATA%\Programs\xdesk, PATH, menu Démarrer, pare-feu
powershell -ExecutionPolicy Bypass -File install.ps1 -Autostart
powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall
```

`install.ps1` télécharge lui-même la dernière release quand il n'est pas lancé depuis le zip extrait (`-Zip <fichier>` pour un zip local).

> [!WARNING]
> Le build Windows est vérifié avec MinGW mais pas encore lancé sur un vrai Windows. [Limites connues](page:limits#platforms).

## Partager ce PC {#first-share}

1. Lancer `xdesk` : le panneau s'ouvre (fenêtre Qt, ou panneau dans le terminal sans affichage).
2. Appuyer sur **Start sharing** : le partage est arrêté à chaque lancement.
3. Donner le **code à usage unique** (`K7F-29Q-XM4`) ou montrer le **QR code**.
4. Accepter l'appareil quand le panneau le demande (un appareil inconnu est refusé après 30 s).

Sur l'autre PC : `xdesk connect <adresse> -c <code>`, ou ouvrir `https://<adresse>:47800` dans un navigateur. Ensuite : [partager un PC](page:share-a-pc#panel), [se connecter](page:connect#clients).
