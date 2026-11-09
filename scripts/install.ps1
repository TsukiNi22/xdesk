<#
Edition:
##  05/11/2026 by Tsukini

File Name:
##  install.ps1

File Description:
##  Install xdesk on Windows from its zip (release or local): files, PATH, Start menu, firewall, autostart
#>
[CmdletBinding()]
param(
    [string]$Zip = "",                  # local xdesk-windows-x64.zip (default: the folder of this script, else the latest release)
    [string]$Repo = "TsukiNi22/xdesk",
    [string]$Prefix = "$env:LOCALAPPDATA\Programs\xdesk",
    [int]$Port = 47800,
    [switch]$Autostart,                 # host panel at login (tray)
    [switch]$NoFirewall,                # do not open the port for the LAN
    [switch]$Uninstall
)
$ErrorActionPreference = "Stop"

function Say([string]$Text) { Write-Host "xdesk " -ForegroundColor Blue -NoNewline; Write-Host $Text }
$StartMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\xdesk"
$Startup = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\Startup\xdesk.lnk"

# ===== Uninstall =====
if ($Uninstall) {
    Get-Process xdesk -ErrorAction SilentlyContinue | Stop-Process -Force
    Remove-Item -Recurse -Force $Prefix, $StartMenu -ErrorAction SilentlyContinue
    Remove-Item -Force $Startup -ErrorAction SilentlyContinue
    $path = [Environment]::GetEnvironmentVariable("Path", "User") -split ";" | Where-Object { $_ -and $_ -ne $Prefix }
    [Environment]::SetEnvironmentVariable("Path", ($path -join ";"), "User")
    Start-Process -Verb RunAs -WindowStyle Hidden netsh -ArgumentList "advfirewall firewall delete rule name=xdesk" -ErrorAction SilentlyContinue
    Say "removed (settings and logs stay in $env:LOCALAPPDATA\xdesk)"
    exit 0
}

# ===== Files =====
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Zip -and (Test-Path (Join-Path $here "xdesk.exe"))) {
    $source = $here                     # run from the extracted zip
} else {
    if (-not $Zip) {
        Say "downloading the latest release of $Repo"
        $Zip = Join-Path $env:TEMP "xdesk-windows-x64.zip"
        if (Get-Command gh -ErrorAction SilentlyContinue) {
            gh release download --repo $Repo --pattern "xdesk-windows-x64.zip" --output $Zip --clobber
        } else {
            $release = Invoke-RestMethod "https://api.github.com/repos/$Repo/releases/latest"
            $asset = $release.assets | Where-Object { $_.name -eq "xdesk-windows-x64.zip" } | Select-Object -First 1
            if (-not $asset) { throw "No xdesk-windows-x64.zip in the latest release" }
            Invoke-WebRequest $asset.browser_download_url -OutFile $Zip
        }
    }
    $extract = Join-Path $env:TEMP "xdesk-install"
    Remove-Item -Recurse -Force $extract -ErrorAction SilentlyContinue
    Expand-Archive -Path $Zip -DestinationPath $extract -Force
    $source = (Get-ChildItem $extract -Recurse -Filter xdesk.exe | Select-Object -First 1).DirectoryName
}
Get-Process xdesk -ErrorAction SilentlyContinue | Stop-Process -Force
New-Item -ItemType Directory -Force $Prefix | Out-Null
Copy-Item -Recurse -Force (Join-Path $source "*") $Prefix
Say "files in $Prefix"

# ===== PATH =====
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if (($userPath -split ";") -notcontains $Prefix) {
    [Environment]::SetEnvironmentVariable("Path", "$userPath;$Prefix", "User")
    Say "added to the PATH (new terminals): xdesk connect <address>"
}

# ===== Shortcuts =====
$shell = New-Object -ComObject WScript.Shell
New-Item -ItemType Directory -Force $StartMenu | Out-Null
function Link([string]$Path, [string]$Arguments, [string]$Description) {
    $link = $shell.CreateShortcut($Path)
    $link.TargetPath = Join-Path $Prefix "xdesk.exe"
    $link.Arguments = $Arguments
    $link.WorkingDirectory = $Prefix
    $link.Description = $Description
    $link.WindowStyle = 7               # minimized console (the window is the Qt panel)
    $link.Save()
}
Link (Join-Path $StartMenu "xdesk - share this PC.lnk") "host --gui" "Let someone see / control this PC with a one time code"
Link (Join-Path $StartMenu "xdesk viewer.lnk") "viewer --gui" "Connect to a PC shared with xdesk"
if ($Autostart) { Link $Startup "host --gui" "xdesk host at login"; Say "starts at login" }

# ===== Firewall (LAN clients) =====
if (-not $NoFirewall) {
    $exe = Join-Path $Prefix "xdesk.exe"
    $rule = "advfirewall firewall add rule name=xdesk dir=in action=allow program=`"$exe`" protocol=TCP localport=$Port profile=private,domain"
    $discovery = "advfirewall firewall add rule name=xdesk dir=in action=allow program=`"$exe`" protocol=UDP localport=47801 profile=private,domain"
    try {
        Start-Process -Verb RunAs -Wait -WindowStyle Hidden netsh -ArgumentList "advfirewall firewall delete rule name=xdesk"
        Start-Process -Verb RunAs -Wait -WindowStyle Hidden netsh -ArgumentList $rule
        Start-Process -Verb RunAs -Wait -WindowStyle Hidden netsh -ArgumentList $discovery
        Say "port $Port opened for the private networks"
    } catch { Say "firewall not changed (administrator refused): LAN clients may be blocked" }
}
Say "installed: $(& (Join-Path $Prefix 'xdesk.exe') --version)"
