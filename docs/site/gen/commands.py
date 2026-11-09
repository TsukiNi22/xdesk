"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
Edition:
##  09/11/2026 by Tsukini

File Name:
##  commands.py

File Description:
##  Commands page of the documentation site, generated from the ArgParser of src/xdesk/Desktop-init.cpp
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Import #####
# Import that can be checked
try:
    from pathlib import Path # Used to read the sources
    import re # Used to read the flags
except ImportError as e:
    raise SystemExit(f"Import Error ({__file__}): {e}")

##### Const #####
COMMAND = re.compile(r'"  (xdesk[^"]*?)\s{2,}([^"\\]+)(?:\\n)?"')
FLAG = re.compile(r'setFlag\("[\w-]+", \{"(\w*)", "\w*", "([\w-]+)", "(\w*)"\}, (\{\{"(\w+)"[^}]*\}\}|\{\}), "([^"]+)"\)')
TEXT = {
    "en": {"title": "Commands and flags", "summary": "Every command and flag of xdesk with its environment variable, read from its argument parser.",
        "intro": "Generated from the argument parser of `src/xdesk/Desktop-init.cpp` (version {version}). With no command, `xdesk` shares the PC (`host`).",
        "commands": "Commands", "flags": "Flags", "chead": "| Command | Does |", "fhead": "| Flag | Value | Environment | Effect |",
        "help": "| `-h`, `--help` | - | - | Display the help and exit |"},
    "fr": {"title": "Commandes et flags", "summary": "Toutes les commandes et tous les flags de xdesk avec leur variable d'environnement, lus depuis son analyseur d'arguments.",
        "intro": "Généré depuis l'analyseur d'arguments de `src/xdesk/Desktop-init.cpp` (version {version}, descriptions du programme, en anglais). Sans commande, `xdesk` partage le PC (`host`).",
        "commands": "Commandes", "flags": "Flags", "chead": "| Commande | Fait |", "fhead": "| Flag | Valeur | Environnement | Effet |",
        "help": "| `-h`, `--help` | - | - | Affiche l'aide et quitte |"},
}

##### Tools #####
def source(ctx: "Context") -> str: # The file declaring the arguments
    return (ctx.site / ".." / ".." / "src" / "xdesk" / "Desktop-init.cpp").resolve().read_text(encoding="utf-8")

def escape(text: str) -> str: # Markdown text of a description (no emphasis, no table cell break)
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    return re.sub(r"([\\`*_|\[\]])", r"\\\1", text)

def commands(ctx: "Context") -> list[tuple[str, str]]: # (usage, description)
    return [(m.group(1).strip(), m.group(2).strip()) for m in COMMAND.finditer(source(ctx))]

def flags(ctx: "Context") -> list[tuple[str, str, str, str, str]]: # (short, long, env, value, description)
    return [(m.group(1), m.group(2), m.group(3), m.group(5) or "", m.group(6)) for m in FLAG.finditer(source(ctx))]

##### Program #####
def pages(ctx: "Context") -> list[dict]:
    cmds, opts = commands(ctx), flags(ctx)
    if not cmds or not opts:
        raise SystemExit("Error: no command / flag read from src/xdesk/Desktop-init.cpp (format changed?)")
    page = {"slug": "commands", "section": "reference", "order": 1, "kind": ""}
    for lang in ctx.langs:
        text = TEXT[lang]
        md = [text["intro"].format(version=ctx.version or "-"), "", f"## {text['commands']} {{#commands}}", "", text["chead"], "|---|---|"]
        md += [f"| `{escape(u)}` | {escape(d)} |" for u, d in cmds]
        md += ["", f"## {text['flags']} {{#flags}}", "", text["fhead"], "|---|---|---|---|"]
        md += [f"| {('`-' + s + '`, ') if s else ''}`--{n}` | {('`' + v + '`') if v else '-'} | {('`' + e + '`') if e else '-'} | {escape(d)} |"
            for s, n, e, v, d in sorted(opts, key=lambda o: o[1])]
        md += [text["help"], ""]
        page[lang] = {"title": text["title"], "summary": text["summary"], "md": "\n".join(md),
            "keywords": " ".join(["--" + o[1] for o in opts] + [c[0].split()[1] if len(c[0].split()) > 1 else "host" for c in cmds])}
    return [page]

def search_entries(ctx: "Context") -> list[dict]:
    out = [{"k": "command", "l": lang, "t": u, "d": d, "u": "commands.html#commands", "x": ""} for u, d in commands(ctx) for lang in ctx.langs]
    out += [{"k": "option", "l": lang, "t": "--" + n, "d": d, "u": "commands.html#flags", "x": f"-{s} {e}" if s else e}
        for s, n, e, _, d in flags(ctx) for lang in ctx.langs]
    return out
