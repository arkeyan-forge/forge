#!/usr/bin/env python3
"""Trap Team's part of resources/collection/catalogue.json, from the game's own tables.

usage: python3 -I tools/trapteam_catalogue.py [<toy_tables.json>]     (default private/trapteam/toy_tables.json,
       made by `python3 -I tools/toy_tables.py <Trap Team disc>`); rewrites the catalogue in place.

What it sets (names only, no picture and no game data beyond what the catalogue already holds for the other games):
- the game's `portraits`: each picture of the game's collection screen (`<type>_<variant>_<name>.png`) under "<type>" (the
  plain figure: the newest year's, variant 0x3000, 0x2000, 0x1000 or 0) or "<type>:0x<variant>"; the traps' `<type>_<variant>`
  (a trap's "<type>" is its first shape's);
- the game's `villains`: the 46 of its table (id, name, element, model, vault index); their pictures are
  `<id>_villaincaptured` and `<id>_villainescaped`;
- each trap's variants: its shapes as the game lists them (variant 0x3000 + the shape number), the names the catalogue
  already had for a shape kept (public lists: [?]), the other shapes called "Shape <n>", and the Legendary and Ultimate Kaos
  traps the game's variant table names;
- it reports any toy whose name or element differs from the game's table (none are changed).
"""
import json
import os
import re
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
catalogue_path = os.path.join(root, "resources", "collection", "catalogue.json")
tables_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "private", "trapteam", "toy_tables.json")
tables = json.load(open(tables_path))
text = open(catalogue_path).read()
catalogue = json.loads(text)
game = next(g for g in catalogue["games"] if g["id"] == "trapteam")
toys = {t["type"]: t for t in catalogue["toys"]}

# Portraits: the plain figure (a year's base variant), else the variant.
PLAIN = (0x3000, 0x2000, 0x1000, 0)
found = {}  # type -> {variant: file}
for picture in tables["pictures"]:
    m = re.fullmatch(r"(skylander_toys|traps)/(\d+)_(\d+)(?:_([\w-]+))?", picture)
    if m:
        variant = int(m[3])
        # The game's own file name for Nitro Magna Charge (top) swaps two digits: 9128 for its variant 9218 (0x2402, what
        # the figure carries and the other games' maps use).
        if (int(m[2]), variant) == (2010, 9128):
            variant = 0x2402
        found.setdefault(int(m[2]), {})[variant] = picture.split("/")[1].lower()
portraits = {}
for type_, variants in sorted(found.items()):
    for variant, name in sorted(variants.items()):
        if variant in PLAIN and variant == next(p for p in PLAIN if p in variants):
            portraits[str(type_)] = name
        elif variant not in PLAIN:
            portraits["%d:0x%04X" % (type_, variant)] = name
for trap in tables["traps"]:  # a trap has no plain figure: the card of the type shows its first shape
    portraits.setdefault(str(trap["type"]), found[trap["type"]][min(found[trap["type"]])])
game["portraits"] = portraits

game["villains"] = [{"id": v["id"], "name": v["name"], "element": "none" if v["element"] == "kaos" else v["element"],
                     "model": v["model"], "vault": v["vault"]} for v in tables["villains"]]

# Traps: the shapes the game lists, then its Legendary and Ultimate traps (a variant table row's deco is the shape it
# redecorates: the picture 0x3400 | deco, or 0x3500 | deco for Kaos's).
for trap in tables["traps"]:
    toy = toys[trap["type"]]
    old = {int(v["id"], 16): v for v in toy.get("variants", [])}
    redecorated = {v["deco"] for v in tables["variants"]
                   if v["year"] == 2014 and v["type"] == trap["type"] and v["deco"] is not None}
    variants = []
    for shape in trap["shapes"]:
        if shape in redecorated and shape not in (6, 4, 8) and not (0x3000 + shape) in found.get(trap["type"], {}):
            continue  # Kaos's shape 31 is the Ultimate figure, which has no plain version
        variants.append(old.get(0x3000 + shape) or
                        {"id": "0x%04X" % (0x3000 + shape), "name": "Shape %d" % shape, "kind": "shape"})
    for v in tables["variants"]:
        if v["year"] == 2014 and v["type"] == trap["type"] and v["deco"] is not None:
            ids = [k for k in found.get(trap["type"], {}) if k >= 0x3400 and k & 0xFF == v["deco"]]
            shape = next((x for x in variants if x["id"] == "0x%04X" % (0x3000 + v["deco"])), None)
            if ids:
                legendary = v["kind"] == "Legendary"
                name = "Legendary " + shape["name"] if legendary and shape else v["name"]
                variants.append({"id": "0x%04X" % ids[0], "name": name, "kind": "legendary" if legendary else "alt_deco"})
    toy["variants"] = variants

for group in ("toys", "items", "traps"):
    for row in tables[group]:
        toy = toys.get(row["type"])
        if toy is None:
            print("not in the catalogue: %d %s" % (row["type"], row["name"]))
        else:
            plain = lambda s: re.sub(r"[^a-z]", "", s.lower())
            if row.get("name") and not row["name"].startswith("level=") and plain(row["name"]) != plain(toy["name"]):
                print("name differs: %d catalogue %r, game %r" % (row["type"], toy["name"], row["name"]))
            if row.get("element") not in (None, "kaos") and row["element"] != toy["element"]:
                print("element differs: %d catalogue %s, game %s" % (row["type"], toy["element"], row["element"]))

# One game or toy per line, as the file is.
head, rest = text.split('  "games": [\n', 1)
games_text, toys_text = rest.split('  ],\n  "toys": [\n', 1)
out = [head, '  "games": [\n']
out.append(",\n".join("    " + json.dumps(g, ensure_ascii=False) for g in catalogue["games"]) + "\n")
out.append('  ],\n  "toys": [\n')
out.append(",\n".join("    " + json.dumps(t, ensure_ascii=False) for t in catalogue["toys"]) + "\n")
out.append("  ]\n}\n")
open(catalogue_path, "w").write("".join(out))
print("trapteam: %d portraits, %d villains, %d traps" % (len(portraits), len(game["villains"]), len(tables["traps"])))
