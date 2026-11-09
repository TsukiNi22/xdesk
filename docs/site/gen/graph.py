"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
Edition:
##  09/11/2026 by Tsukini

File Name:
##  graph.py

File Description:
##  Search entry of the interactive project graph (docs/graph.html, html-doc), kept next to the site
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

##### Program #####
def pages(ctx: "Context") -> list[dict]:
    return [] # graph.html is not built by the site: only found by the search

def search_entries(ctx: "Context") -> list[dict]:
    text = {"en": ("Project graph", "Files, classes, functions, calls and tests of the repository, in 2D / 3D"),
        "fr": ("Graphe du projet", "Fichiers, classes, fonctions, appels et tests du dépôt, en 2D / 3D")}
    return [{"k": "page", "l": lang, "t": text[lang][0], "d": text[lang][1], "u": "graph.html", "x": "graph"} for lang in ctx.langs]
