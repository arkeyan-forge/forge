#!/usr/bin/env python3
"""The toys and variants that SuperChargers' and Imaginators' own toy tables define and resources/collection/catalogue.json
lacked: adds them (and corrects the ids the tables contradict), keeping every existing id and the file's order.

usage: python3 -I tools/catalogue_tables.py [--check]
  reads private/{trapteam,superchargers,imaginators}/toy_tables.json (python3 -I tools/toy_tables.py <disc> writes them);
  rewrites the catalogue in place; --check changes nothing and exits 1 if the catalogue is not up to date.

Sources (docs/runtime/host/collection.md, "The games' own toy tables"):
- ids: the CVariantIdentifier of SuperChargers' and Imaginators' archives/permanent.pak ToyData (deco id, year code, flag
  bytes: tools/toy_tables.py toydata_tables). A variant is new when the catalogue has neither its id, nor its id with bit 8
  (0x0100: the retail SuperChargers carry it, the tables never: the owner's dumps of Dark Spitfire 0x4502, Legendary
  Hurricane Jet-Vac 0x4503 against the tables' 0x4402, 0x4403), nor, for a variant of any year (the table's year code -1),
  a variant with the same low 12 bits.
- names: Trap Team's own variant table (character/Init_Setup.bld, `2014,458,Dark,Dark`) for the Trap Team toys; else
  NAMES below, each with its source. Toy names and elements: the tables (Kaos's element, 11, is "none" here as for the
  other Kaos toys).
Left out (reported): the tables' templates and internal bodies (Template*, CYOS_*_Template, MediumGeneric_Base,
BlueFalcon_Base, the creation classes 4523-4534), and the toys with no collection picture nor series (Fusion, Synergy,
Unity 4500-4502, Undead Card 4517): nothing says a figure of them was made.
"""
import json
import os
import re
import sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
catalogue_path = os.path.join(root, "resources", "collection", "catalogue.json")


def load(game):
    path = os.path.join(root, "private", game, "toy_tables.json")
    if not os.path.exists(path):
        raise SystemExit(f"{path}: run python3 -I tools/toy_tables.py <the {game} disc> first")
    return json.load(open(path))


# Variants whose name the tables do not give (their text is "Special", "Series 2"...): (type, id) -> (name, kind, source).
NAMES = {
    (113, 0x3801): ("Sure Shot Shroomboom", "series", "tables: Series 2, year 2014 (Trap Team's release); public name"),
    (14, 0x3809): ("Tidal Wave Gill Grunt", "series", "tables: Series 4, year 2014; public name"),
    (3004, 0x3801): ("Hog Wild Fryno", "series", "tables: Series 2, Collection_Fryno_Reposed2014; public name"),
    (9, 0x3804): ("Event Eruptor", "event", "tables: deco 4 (Event in Swap Force's deco enum), year 2014 [? which event]"),
    (462, 0x3004): ("Event Snap Shot", "event", "tables: deco 4 (Event), year 2014 [? which event]"),
    (618, 0x540C): ("Heartbreaker Buckshot", "event", "tables: Collection_ArcherGoat_ValentinesDay; public name"),
    # deco 15 with the alternate-deco flag, no variant text, pictures *_Instant: Trap Team's table calls deco 15
    # "Virtual,Instant" (Snap Shot, Food Fight). The tablet editions [? no physical figure known].
    **{(t, 0x440F): (None, "other", "tables: deco 15, Collection_*_Instant") for t in (3224, 3228, 3231, 3232, 3237, 3412,
                                                                                    3415, 3425)},
}
# The catalogue's ids that the tables contradict (both from the public list before): (type, old id) -> new id. The
# tables give these figures' series by the deco (Series 2 deco 1, Series 3 deco 5, Series 4 deco 9: Fizzy Frenzy Pop Fizz
# and Full Blast Jet-Vac, Series 3, are 0x3805 in both).
CORRECTIONS = {(113, 0x3805): 0x3801, (14, 0x3805): 0x3809}
KINDS = {"Dark": "dark", "Legendary": "legendary", "Easter": "event", "Winterfest": "event"}
# The SuperChargers racing villains and their vehicles: the tables' toys 3440-3497 (CFullCharacterToyData,
# CVehicleToyData), each with a collection picture. Never sold [?]; added for the games that define them.
NEW_TOYS = range(3440, 3500)
# Pictures the tables name but neither disc has among its badges [V: ArkeyanArtTest].
NO_BADGE = {"collection_villainvehiclethreatpack_normal"}
LEFT_OUT = re.compile(r"Template|_Base$|^CYOS_")


def main():
    check = "--check" in sys.argv[1:]
    text = open(catalogue_path).read()
    catalogue = json.loads(text)
    toys = {t["type"]: t for t in catalogue["toys"]}
    before = count(catalogue)
    trapteam = load("trapteam")
    tt_names = {}
    for v in trapteam["variants"]:
        if not v["name"].startswith("load="):
            tt_names.setdefault((v["type"], v["year"]), []).append(v)
    tables = [load("superchargers"), load("imaginators")]

    for (kind, old), new in CORRECTIONS.items():
        for v in toys[kind].get("variants", []):
            if v.get("id") == "0x%04X" % old:
                v["id"] = "0x%04X" % new

    added, left_out = [], set()
    for tb in tables:
        for t in tb["toys"]:
            toy = toys.get(t["type"])
            if toy is None:
                if t["type"] in NEW_TOYS and t["name"] and not LEFT_OUT.search(t["name"]):
                    toy = {"type": t["type"], "name": t["name"].replace("’", "'"),
                           "element": t["element"] if t["element"] not in (None, "kaos") else "none",
                           "category": "vehicle" if t["class"] == "CVehicleToyData" else "villain", "game": tb["game"]}
                    insert_toy(catalogue, toy)
                    toys[toy["type"]] = toy
                    added.append("toy %d %s" % (toy["type"], toy["name"]))
                else:
                    left_out.add("%d %s" % (t["type"], t["name"]))
                continue
            for v in t["variants"]:
                vid = int(v["id"], 16)
                have = [int(x["id"], 16) for x in toy.get("variants", []) if "id" in x]
                if v["year"] > 15:  # any year: the low 12 bits; Trap Team's table gives the year
                    if any(h & 0xFFF == vid & 0xFFF for h in have):
                        continue
                    if not tt_row(tt_names, t["type"], 0x3000 | vid & 0xFFF):
                        left_out.add("%d %s variant %s (any year)" % (t["type"], toy["name"], v["id"]))
                        continue
                    vid = 0x3000 | vid & 0xFFF
                if vid in have or vid | 0x100 in have:
                    continue
                name, kind = variant_name(toy, vid, v, tt_names)
                toy.setdefault("variants", []).append({"id": "0x%04X" % vid, "name": name, "kind": kind})
                added.append("variant %d %s 0x%04X" % (toy["type"], name, vid))

    # Pictures: each catalogue toy and variant the game's portraits map lacks gets the tables' collection picture (lower
    # case, as the map names the badges; not the face-off or full-screen ones). tools/regress ArkeyanArtTest checks that
    # each is on the disc.
    for tb in tables:
        game = next(g for g in catalogue["games"] if g["id"] == tb["game"])
        for t in tb["toys"]:
            toy = toys.get(t["type"])
            if toy is None:
                continue
            ids = {x["id"] for x in toy.get("variants", []) if "id" in x}
            for key, picture in [("%d" % t["type"], t["picture"])] + \
                    [("%d:%s" % (t["type"], v["id"]), v["picture"]) for v in t["variants"]
                     if v["id"] in ids and v["picture"] != t["picture"] and not (v["picture"] or "").endswith("_Normal")]:
                if key not in game["portraits"] and (picture or "").startswith("Collection_") and \
                        picture.lower() not in NO_BADGE:
                    game["portraits"][key] = picture.lower()
                    added.append("portrait %s %s %s" % (tb["game"], key, picture.lower()))

    out = render(text, catalogue)
    after = count(catalogue)
    for line in added:
        print("added:", line)
    for line in sorted(left_out):
        print("left out:", line)
    for game in before:
        print("%s: %d toys, %d variants -> %d toys, %d variants" % (game, *before[game], *after[game]))
    if check:
        sys.exit(0 if out == text else 1)
    open(catalogue_path, "w").write(out)


def variant_name(toy, vid, v, tt_names):
    known = NAMES.get((toy["type"], vid))
    if known:
        return known[0] or "%s %s" % ("Instant", toy["name"]), known[1]
    row = tt_row(tt_names, toy["type"], vid)
    if row is None:
        raise SystemExit("no name for %d %s 0x%04X (%s, %s): add it to NAMES" % (toy["type"], toy["name"], vid, v["text"],
                                                                              v["picture"]))
    kind = KINDS.get(row["name"]) or KINDS.get(row["kind"]) or ("alt_deco" if row["kind"].lower() == "alt" else "other")
    return "%s %s" % (row["name"], toy["name"]), kind


def tt_row(tt_names, kind, vid):
    """Trap Team's row for a variant: its year's rows for the toy, the one whose deco is the variant's, else one with no
    deco (the toy's alternate deco)."""
    rows = tt_names.get((kind, 2011 + (vid >> 12)), [])
    return next((r for r in rows if r["deco"] == vid & 0xFF), None) or next((r for r in rows if r["deco"] is None), None)


def insert_toy(catalogue, toy):
    """After the last toy of the same game and category, else of the same game."""
    toys = catalogue["toys"]
    same = [i for i, t in enumerate(toys) if t["game"] == toy["game"] and t["category"] == toy["category"]]
    if not same:
        same = [i for i, t in enumerate(toys) if t["game"] == toy["game"] and t["category"] == "vehicle"]
    at = max(same) + 1
    while at < len(toys) and toys[at]["game"] == toy["game"] and toys[at]["category"] == toy["category"] and \
            toys[at]["type"] < toy["type"]:
        at += 1
    toys.insert(at, toy)


def count(catalogue):
    out = {g["id"]: [0, 0] for g in catalogue["games"]}
    for t in catalogue["toys"]:
        out[t["game"]][0] += 1
        out[t["game"]][1] += len(t.get("variants", []))
    return out


def render(text, catalogue):
    """One game or toy per line, as the file is (tools/trapteam_catalogue.py)."""
    head = text.split('  "games": [\n', 1)[0]
    return "".join([head, '  "games": [\n',
                    ",\n".join("    " + json.dumps(g, ensure_ascii=False) for g in catalogue["games"]) + "\n",
                    '  ],\n  "toys": [\n',
                    ",\n".join("    " + json.dumps(t, ensure_ascii=False) for t in catalogue["toys"]) + "\n",
                    "  ]\n}\n"])


if __name__ == "__main__":
    main()
