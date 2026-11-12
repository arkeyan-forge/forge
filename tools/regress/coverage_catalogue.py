#!/usr/bin/env python3
"""Everything one game offers, from its disc and the toy catalogue: the coverage catalogue (docs/status/COVERAGE.md).

  coverage_catalogue.py <ssa|giants|swapforce|trapteam|superchargers|imaginators> <disc> <out.tsv>

Columns: area, kind, name, route, arg, cap, reason. kind is level, hub, challenge, arena, mode, credits or figure; route
is how tools/regress/coverage_run.py runs it (level, hero, cali, battle, credits, figures), or none: not testable
automatically, `reason` says why (never counted as covered). `cap`: the entry runs (for its faults and frame rate), but
cannot count as covered even when it plays well, for that reason (a challenge loaded alone, without the game's own
challenge state). Levels and zones come from the disc's own files (levels\\, level\\*.arc, the zone list of
zoneinfos.pak); figures from resources/collection/catalogue.json (the game's accept list, by toy type). Prints the count
per area. The output is disc-derived: write it below private/.
"""
import json
import os
import re
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.dont_write_bytecode = True
import toy_tables  # noqa: E402  (the disc reader, the IGA reader)

GAMES = ("ssa", "giants", "swapforce", "trapteam", "superchargers", "imaginators")
NOT_SCRIPTED_HUB = "the hub's menu that starts it is not scripted"


def ssa(disc, rows):
    levels = sorted({m.group(1) for path in disc.files
                     if (m := re.fullmatch(r"/levels/([a-z0-9_]+)/mission\.str", path))} - {"frontend", "dnftest", "mytest"})
    tour = open(os.path.join(ROOT, "tools", "pipelines", "tour_ssa_story.sh")).read()
    chapters = re.findall(r"(\d+b?):(level_[0-9a-z]+)", tour)
    packs = {name for number, name in chapters if number.isdigit() and int(number) > 22}
    story = {name for _, name in chapters} - packs
    for name in levels:
        if name in story:
            rows.append(("story", "level", name, "hero", name, "", ""))
        elif name in packs:
            rows.append(("adventure packs", "level", name, "hero", name, "", ""))
        elif name == "level_hub":
            rows.append(("hub", "hub", name, "hero", name, "", ""))
        elif name.startswith("challenge_level_"):
            rows.append(("heroic challenges", "challenge", name, "hero", name,
                         "loaded alone (SKY_NEXT_LEVEL): no Skylander; Bodi's menu in the Ruins needs a story save "
                         "(the Ruins open after chapter 1, Bodi is rescued later; SLOT.DAT's progress records unknown)",
                         ""))
        elif name.startswith("pvp_level_"):
            # Battle Mode's Arena Rumble in PICK ARENA's first arena, its files this arena's (SKY_LEVEL_FROM): a new
            # save offers four arenas, the story unlocks the others.
            rows.append(("battle arenas", "arena", name, "battle", f"/0/{name}", "", ""))
        else:
            rows.append(("other levels", "level", name, "hero", name, "", ""))
    for mode, keys in (("Battle Mode: Arena Rumble", ""), ("Battle Mode: SkyGoals", "right"),
                       ("Battle Mode: SkyGem Master", "right down")):
        rows.append(("modes", "mode", mode, "battle", f"{keys}/0", "", ""))
    rows.append(("credits", "credits", "ending and credits", "credits", "movies:legal:sl_sc30,atvilogo:credit", "", ""))


def giants(disc, rows):
    arcs = sorted({m.group(1) for path in disc.files if (m := re.fullmatch(r"/level/([a-z0-9_]+)\.arc", path))})
    challenges = []
    for name in arcs:
        if name in ("title", "challengevo") or name.startswith("challengemesh"):
            continue
        if name.startswith("level_hub"):
            rows.append(("hub", "hub", name, "level", name, "", ""))
        elif name.startswith("challenge_level_"):
            challenges.append(name)
        elif name.startswith("arena_challenge_"):
            rows.append(("arena challenges", "challenge", name, "none", "", "",
                         "Brock's arena select on Flynn's ship (ui_arenaselect) is not scripted (the deck's cabin "
                         "doors defeat timed walks); started alone, the round has no Arena_Mesh (no physics world)"))
        elif name.startswith("pvp_level_"):
            rows.append(("battle arenas", "arena", name, "battle", f"/0/{name}", "", ""))
        elif name.startswith("credits"):
            rows.append(("credits", "credits", name, "credits", name, "", ""))
        elif re.match(r"level_(003|008|036|046)_", name):
            rows.append(("adventure packs", "level", name, "level", name, "", ""))
        else:
            rows.append(("story", "level", name, "level", name, "", ""))
    # Cali's select offers them all with an unlocked save (giants_unlock.py), in its own order (entry 14 is
    # challenge_level_028): an entry is named by its place there, its run's detail names the level that opened.
    for k in range(len(challenges)):
        rows.append(("heroic challenges", "challenge", f"Heroic Challenge {k + 1:02d}", "cali", str(k), "", ""))
    for mode, keys in (("Battle Mode: Arena Rumble", ""), ("Battle Mode: SkyGoals", "right"),
                       ("Battle Mode: SkyGem Master", "down"), ("Battle Mode: Ring Out", "right down")):
        rows.append(("modes", "mode", mode, "battle", f"{keys}/0", "", ""))


def zone_list(disc, entry_suffix):
    """The zones of /archives/zoneinfos.pak whose map has an archive: Swap Force's mua (_zoneName), the later games'
    zoneinfo entries (the path in the entry's name)."""
    blob = disc.read("/archives/zoneinfos.pak")
    paks = {m.group(1) for path in disc.files if (m := re.fullmatch(r"/archives/([^/]+)\.pak", path))}
    zones = set()
    if blob[:4] == b"\x1aAGI" and struct.unpack_from(">I", blob, 4)[0] == 11:  # IGA v11, big-endian
        count = struct.unpack_from(">I", blob, 0x0C)[0]
        table = struct.unpack_from(">Q", blob, 0x28)[0]
        for i in range(count):
            at = table + struct.unpack_from(">I", blob, table + 4 * i)[0]
            name = blob[at:blob.index(b"\0", at)].decode("latin-1")
            m = re.search(r"maps/(.+)/([^/]+)_zoneinfo", name, re.I)
            if m:
                zones.add(f"{m.group(1)}/{m.group(2)}".lower())
    else:
        for name, entry in toy_tables.iga_entries(blob).items():
            if name.lower().endswith(entry_suffix):
                found = re.search(rb'name="_zoneName" value="([^"]+)"', toy_tables.iga_read(blob, entry))
                if found:
                    zones.add(found.group(1).decode().lower())
    return sorted(z for z in zones if z.split("/")[-1] in paks)


def swapforce(disc, rows):
    for zone in zone_list(disc, "_zoneinfo.mua"):
        area = zone.split("/")[0]
        if area in ("tutorial", "barrens", "wildwoods", "frostguard", "verdant", "outlands", "finale"):
            rows.append(("story", "level", zone, "level", zone, "", ""))
        elif area in ("town", "townarchetypes"):
            rows.append(("hub", "hub", zone, "level", zone, "", ""))
        elif area == "adventurepacks":
            rows.append(("adventure packs", "level", zone, "level", zone, "", ""))
        elif area == "heroicchallenges":
            rows.append(("bonus missions", "challenge", zone, "level", zone,
                         "started alone (SKY_LEVEL): the bonus mission's own entry and state are not traced", ""))
        elif area == "turbozones":
            rows.append(("swap zones", "level", zone, "level", zone, "", ""))
        elif area == "lockpick_minigame":
            rows.append(("lockpick puzzles", "level", zone, "level", zone, "", ""))
        elif zone.startswith("multiplayer/survival/"):
            rows.append(("arenas", "arena", zone, "level", zone, "", ""))
        elif zone.startswith(("multiplayer/pvp/", "multiplayer/ringout/")):
            # Arena Modes with two pads, the mode's first arena renamed to this one (SKY_LEVEL_FROM).
            downs = 3 if zone.startswith("multiplayer/pvp/") else 4
            rows.append(("arenas", "arena", zone, "sfarena", f"{downs}/{zone}", "", ""))
        else:
            rows.append(("other zones", "level", zone, "level", zone, "", ""))
    rows.append(("credits", "credits", "credits", "none", "", "",
                 "no credits roll found on the disc (SW_GrandFinale is the finale movie); no route"))
    rows.append(("modes", "mode", "Team Survival / Rival Survival", "none", "", "",
                 "two-player survival modes: every survival arena is locked on a new save; the arenas run as Solo "
                 "Survival (their own rows)"))


def trapteam(disc, rows):
    out = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "regress", "trapteam.py"), "levels",
                          disc.path], capture_output=True, text=True, check=True).stdout.split()
    arcs = {m.group(1) for path in disc.files if (m := re.fullmatch(r"/level/([a-z0-9_]+)\.arc", path))}
    packs = ("level_308", "level_320", "level_325", "level_327")
    for name in sorted(set(out) | {a for a in arcs if re.match(r"arena_challenge_.*_00[1-9]$", a)}):
        if name.startswith("level_hub"):
            rows.append(("hub", "hub", name, "level", name, "", ""))
        elif name.startswith("horde_"):
            rows.append(("kaos doom challenges", "challenge", name, "level", name,
                         "loaded alone: the Doom Challenge mode is set by its menu (UI_HordeModeSelect), not scripted",
                         ""))
        elif name.startswith("challenge_level_"):
            rows.append(("heroic challenges", "challenge", name, "level", name,
                         "loaded alone: the challenge mode and mesh are set by a menu not found", ""))
        elif re.match(r"arena_challenge_.*_000$", name):
            rows.append(("arena challenges", "arena", name, "level", name,
                         "loaded alone, no Arena_Mesh: a player starts it from Brock in the Academy (the arena bought "
                         "in the store; UI_ArenaSelect loads the mesh first), not scripted", ""))
        elif name.startswith("arena_challenge_"):
            rows.append(("arena challenges", "arena", name, "none", "", "",
                         "a later round of the same arena: the Academy's arena select is not scripted"))
        elif name.startswith("credits"):
            rows.append(("credits", "credits", name, "credits", name, "", ""))
        elif name.startswith(packs):
            rows.append(("adventure packs", "level", name, "level", name, "", ""))
        else:
            rows.append(("story", "level", name, "level", name, "", ""))
    rows.append(("modes", "mode", "Skystones Smash", "none", "", "", "a card game in the Academy: not scripted"))
    rows.append(("modes", "mode", "two players", "none", "", "", "arena versus: menus not scripted"))


# SuperChargers' Single Race (coverage_run.py scrace): the trophy of the track's pack, a vehicle of its handling type and
# the stick moves from the centre of the race map to the track, found in runs (the zone that started) [V].
SC_RACES = {
    "actionpacks/apa1/apa1": "3500/3220/lup:0.25",                 # Cloud Factory
    "actionpacks/apa4/apa4": "3500/3220/lright:0.5",               # Calamity Canyon
    "actionpacks/aps1/aps1": "3502/3222/lleft:0.6",                # Frozen Fossil Festival
    "actionpacks/aps4/aps4": "3502/3222/lleft:1.05,ldown:0.15",    # Mystical Vault
    "actionpacks/apl1/apl1": "3501/3227/ldown:0.2,lleft:0.25",     # Chompy Garden
    "actionpacks/apl3/apl3": "3501/3227/ldown:0.3,lright:0.2",     # Dragon Spine
    # a pack's own tracks, behind clouds until its trophy comes while the map is open ("pack,")
    "actionpacks/apl2/apl2": "3501/3227/pack,ldown:0.65",          # Temple of Arkus
    "actionpacks/apl4/apl4": "3501/3227/pack,ldown:0.55,lleft:0.6",
    "actionpacks/apa2/apa2": "3500/3220/pack,lup:0.55",
    "actionpacks/apa3/apa3": "3500/3220/pack,lright:0.6,lup:0.45",
    "actionpacks/aps2/aps2": "3502/3222/pack,lleft:0.7,lup:0.55",
    "actionpacks/aps3/aps3": "3502/3222/pack,lleft:1.2,lup:0.3",
}


def later(game, disc, rows):
    for zone in zone_list(disc, "_zoneinfo.mua"):
        parts = zone.split("/")
        if zone.startswith(("ui/", "actionpacks/ap_matchmaking/", "actionpacks/ap_smoketest/")):
            if "matchmaking" in zone:
                rows.append(("online", "mode", zone, "none", "", "", "the online lobby: online play only"))
            continue
        if parts[0] in ("whiterooms", "unittests", "vehicletests", "golden", "characterkidtest", "soul_gems") or \
                "lockpick_allpuzzles" in zone:
            continue  # development and test maps (no archive on the retail disc, or the developers' rooms)
        if zone.startswith("actionpacks/") and game == "superchargers" and zone in SC_RACES:
            rows.append(("race tracks", "arena", zone, "scrace", f"{SC_RACES[zone]}/{zone}", "", ""))
        elif zone.startswith("actionpacks/"):
            rows.append(("race tracks", "arena", zone, "level", zone,
                         "loaded alone (SKY_LEVEL): a race starts from the racing menus (mode, cup), not scripted", ""))
        elif zone.startswith("2015/ezones/"):
            rows.append(("elemental zones", "level", zone, "level", zone,
                         "loaded alone (SKY_LEVEL): a player drives a vehicle of the zone's element through the "
                         "Academy's Elemental Gate (Grizzo sets its element), the Academy's package, which the zone "
                         "needs, still loaded; that drive is not scripted", ""))
        elif parts[-1] in ("academy", "hub_2016"):
            rows.append(("hub", "hub", parts[-1], "level", parts[-1], "", ""))
        elif parts[-1] == "overland_map":
            rows.append(("hub", "hub", parts[-1], "level", parts[-1], "", ""))
        elif re.match(r"ea_\d+$", parts[-1]):
            rows.append(("sensei realms", "level", parts[-1], "level", parts[-1], "", ""))
        elif parts[-1] in ("level_507", "level_512"):
            rows.append(("adventure packs", "level", parts[-1], "level", parts[-1], "", ""))
        else:
            rows.append(("story", "level", parts[-1] if len(parts) <= 3 else zone, "level",
                         parts[-1] if len(parts) <= 3 else zone, "", ""))
    rows.append(("modes", "mode", "racing modes", "none", "", "",
                 "Cup, Mirror Cup, Boss Pursuit, Time Trial: not scripted (SuperChargers' Single Race is, in its race "
                 "tracks)" if game == "superchargers" else
                 "Single Race, Cup, Mirror Cup, Boss Pursuit, Time Trial: the racing menus are not scripted"))
    rows.append(("credits", "credits", "credits", "none", "", "",
                 "credits.bk2 plays after the final battle won (story); SKY_MOVIE_SWAP swaps \\movies\\*.bik only"))


def figures(game, rows):
    catalogue = json.load(open(os.path.join(ROOT, "resources", "collection", "catalogue.json")))
    g = next(x for x in catalogue["games"] if x["id"] == game)
    toys = {t["type"]: t for t in catalogue["toys"]}
    accepted = [toys[t] for t in g["accepts"]] if "accepts" in g else [
        t for t in catalogue["toys"] if t.get("game") in g.get("accepts_games", [])]
    accepted_types = {t["type"] for t in accepted}
    paired = set()
    for t in accepted:  # a swapper: a top (2000-2015) and its bottom (1000-1015), one card on the portal
        if t["category"] == "swap_top" and t["type"] - 1000 in accepted_types and \
                toys[t["type"] - 1000]["category"] == "swap_bottom":
            name = re.sub(r"\s*\(top\)\s*$", "", t["name"])
            rows.append(("figures: swappers", "figure", name, "figures",
                         f"half0={t['type']};half1={t['type'] - 1000}", "", ""))
            paired.update((t["type"], t["type"] - 1000))
    seats = {c for seat in g.get("seats", []) for c in seat.get("accepts", [])}
    # Alone on the portal: what a player's seat takes (SSA's and Giants' sidekicks go beside a Skylander, as items do).
    alone = {c for seat in g.get("seats", []) if seat.get("player") for c in seat.get("accepts", [])}
    if game in ("ssa", "giants"):
        alone -= {"mini"}
    for t in accepted:
        if t["type"] in paired:
            continue
        category = t["category"]
        area = f"figures: {category.replace('_', ' ')}"
        if category == "trap":
            shape = next((v["id"][2:] for v in t.get("variants", []) if v.get("kind") == "shape"), "")
            rows.append((area, "figure", t["name"], "figures", f"hero=14;trap={t['type']}:{shape}", "", ""))
        elif category == "vehicle" and "vehicle" in seats:
            rows.append((area, "figure", t["name"], "figures", f"hero=3400;toy={t['type']}", "", ""))
        elif category in alone:
            rows.append((area, "figure", t["name"], "figures", f"toy={t['type']}", "", ""))
        else:
            rows.append((area, "figure", t["name"], "figures", f"hero=14;toy={t['type']}", "", ""))
    if game == "trapteam":
        traps = {t["element"]: t for t in accepted if t["category"] == "trap"}
        for villain in g.get("villains", []):
            trap = traps.get(villain.get("element"))
            if trap is None:
                rows.append(("figures: villains", "figure", villain["name"], "none", "", "",
                             "no trap of its element in the catalogue"))
                continue
            shape = next((v["id"][2:] for v in trap.get("variants", []) if v.get("kind") == "shape"), "")
            rows.append(("figures: villains", "figure", villain["name"], "figures",
                         f"hero=14;trap={trap['type']}:{shape}:{villain['id']}", "", ""))


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in GAMES:
        print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
        sys.exit(2)
    game, iso, out = sys.argv[1:]
    disc = toy_tables.Disc(iso)
    disc.path = iso
    rows = []
    {"ssa": ssa, "giants": giants, "swapforce": swapforce, "trapteam": trapteam}.get(
        game, lambda d, r: later(game, d, r))(disc, rows)
    figures(game, rows)
    with open(out, "w") as f:
        f.write(f"# game {game}\narea\tkind\tname\troute\targ\tcap\treason\n")
        for row in rows:
            f.write("\t".join(str(x).replace("\t", " ") for x in row) + "\n")
    counts = {}
    for row in rows:
        counts[row[0]] = counts.get(row[0], 0) + 1
    testable = sum(1 for row in rows if row[3] != "none")
    print(f"{game}: {len(rows)} entries ({testable} run, {len(rows) - testable} not testable): " +
          ", ".join(f"{area} {n}" for area, n in counts.items()))


if __name__ == "__main__":
    main()
