"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
Edition:
##  09/11/2026 by Tsukini

File Name:
##  settings.py

File Description:
##  Settings page of the documentation site, generated from the Settings struct of include/xdesk/core/Config.hpp
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can't be in the try
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent)) # The generators are loaded from their file, not as a package

# Import that can be checked
try:
    import re # Used to read the struct
    from commands import escape # Same Markdown escape as the commands page
except ImportError as e:
    raise SystemExit(f"Import Error ({__file__}): {e}")

##### Const #####
FIELD = re.compile(r"^\s+([\w:]+) (\w+) = (.+?); // (.+)$")
GROUP = re.compile(r"^\s+/\* (\w+) \*/$")
CLAMP = re.compile(r'(\w+) = std::clamp\(o\.value\("(\w+)"\)\.\w+\([^)]*\), ([\d.]+), ([\d.]+)\)')
TYPES = {"std::uint16_t": "int", "int": "int", "double": "number", "bool": "bool", "QString": "string", "Exposure": "string", "Approval": "string"}
GROUPS = {"network": ("Network", "Réseau"), "access": ("Access", "Accès"), "stream": ("Stream", "Flux"), "ui": ("Interface", "Interface")}
TEXT = {
    "en": {"title": "Settings", "summary": "Every key of config.json with its type, default and range, read from the Settings struct of the sources.",
        "intro": "`~/.config/xdesk/config.json` (Windows: `%LOCALAPPDATA%\\xdesk`), written by the settings page of the panels (version {version}, generated from `include/xdesk/core/Config.hpp`). A value out of its range is brought back into it when the file is read.",
        "head": "| Key | Type | Default | Range | Meaning |"},
    "fr": {"title": "Réglages", "summary": "Toutes les clés de config.json avec leur type, valeur par défaut et plage, lues depuis la struct Settings des sources.",
        "intro": "`~/.config/xdesk/config.json` (Windows : `%LOCALAPPDATA%\\xdesk`), écrit par la page des réglages des panneaux (version {version}, généré depuis `include/xdesk/core/Config.hpp`, descriptions du programme, en anglais). Une valeur hors de sa plage y est ramenée à la lecture du fichier.",
        "head": "| Clé | Type | Défaut | Plage | Sens |"},
}

##### Tools #####
def root(ctx: "Context") -> Path: # The repository
    return (ctx.site / ".." / "..").resolve()

def settings(ctx: "Context") -> list[tuple[str, str, str, str, str, str]]: # (group, key, type, default, range, meaning)
    header = (root(ctx) / "include" / "xdesk" / "core" / "Config.hpp").read_text(encoding="utf-8")
    body = header[header.index("struct Settings {"):]
    body = body[:body.index("};")]
    loader = (root(ctx) / "src" / "xdesk" / "core" / "Config.cpp").read_text(encoding="utf-8")
    ranges = {m.group(2): f"{m.group(3)} - {m.group(4)}" for m in CLAMP.finditer(loader)}
    out, group = [], ""
    for line in body.splitlines():
        if GROUP.match(line):
            group = GROUP.match(line).group(1)
        match = FIELD.match(line)
        if match:
            kind, key, default, meaning = match.groups()
            default = default.split("::")[-1].lower() if "::" in default else default.strip('"')
            out.append((group, key, TYPES.get(kind, kind), default, ranges.get(key, ""), meaning))
    return out

def cell(text: str) -> str: # `value`, - when empty
    return f"`{escape(text)}`" if text else "-"

##### Program #####
def pages(ctx: "Context") -> list[dict]:
    rows = settings(ctx)
    if not rows:
        raise SystemExit("Error: no setting read from include/xdesk/core/Config.hpp (format changed?)")
    page = {"slug": "settings", "section": "reference", "order": 2, "kind": ""}
    for lang in ctx.langs:
        text = TEXT[lang]
        md = [text["intro"].format(version=ctx.version or "-"), ""]
        for gid, (title_en, title_fr) in GROUPS.items():
            group = [r for r in rows if r[0] == gid]
            if not group:
                continue
            md += [f"## {title_en if lang == 'en' else title_fr} {{#{gid}}}", "", text["head"], "|---|---|---|---|---|"]
            md += [f"| `{k}` | {t} | {cell(d)} | {cell(r)} | {escape(m)} |" for _, k, t, d, r, m in group]
            md.append("")
        page[lang] = {"title": text["title"], "summary": text["summary"], "md": "\n".join(md), "keywords": " ".join(r[1] for r in rows)}
    return [page]

def search_entries(ctx: "Context") -> list[dict]:
    return [{"k": "settings", "l": lang, "t": key, "d": meaning, "u": f"settings.html#{group}", "x": f"{kind} {default}"}
        for group, key, kind, default, _, meaning in settings(ctx) for lang in ctx.langs]
