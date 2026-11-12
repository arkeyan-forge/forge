#!/usr/bin/env python3
"""Trap Team tour inputs, read from the disc and the shared toy catalogue.

Commands: levels <disc>; work <directory>; figures <catalogue> [<type or type:variant> ...].
No extraction and no game launch. Disc-derived output belongs under private/.
"""
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools/shaders"))
import xdvdfs


def levels(disc):
    with open(disc, "rb") as stream:
        base = xdvdfs.find_partition(stream)
        stream.seek(base + 32 * xdvdfs.SECTOR + 20)
        sector, size = struct.unpack("<II", stream.read(8))
        entries = []
        xdvdfs.walk(stream, base, sector, size, "", entries)
    found = set()
    for path, size, _ in entries:
        match = re.fullmatch(r"/level/([a-z0-9_]+)\.arc", path.lower())
        if size is None or not match:
            continue
        name = match[1]
        # As in the Giants tour, these are support packs, not standalone levels.
        if name in ("title", "challengevo") or name.startswith("challengemesh"):
            continue
        if name.endswith(("_dialogue", "_globalincludes", "_music_common")):
            continue
        if re.search(r"^arena_challenge_.*_00[1-9]$", name):
            continue
        found.add(name)
    if not found:
        raise ValueError("no standalone /level/*.arc found on the disc")
    return sorted(found)


def work_directory(directory):
    path = Path(directory).resolve()
    scratch = (ROOT / "private").resolve()
    # Logs and figure dumps are derived data: they stay in the git-ignored private/.
    if not scratch.is_relative_to(ROOT) or not path.is_relative_to(scratch) or path == scratch:
        raise ValueError("Trap Team work directory must be below private/")
    return path


def figure_rows(catalogue, wanted=()):
    game = next(g for g in catalogue["games"] if g["id"] == "trapteam")
    accepted = [t for t in catalogue["toys"] if t["type"] in game.get("accepts", [])
                or t.get("game") in game.get("accepts_games", [])]
    bytype = {t["type"]: t for t in accepted}
    paired = set()
    rows = []
    for toy in accepted:
        t = toy["type"]
        if toy["category"] == "swap_top" and bytype.get(t - 1000, {}).get("category") == "swap_bottom":
            bottom = bytype[t - 1000]
            variants = [v for v in toy.get("variants", []) if "id" in v and int(v["id"], 16) >> 12 <= 3
                        and not v.get("same_tag")]
            bottom_tags = {v["id"] for v in bottom.get("variants", []) if "id" in v}
            if not variants:
                rows.append((f"{t}+{t - 1000}", "skip", toy["name"], f"{t}+{t - 1000}"))
            for variant in variants:
                tag = variant["id"][2:]
                key = f"{t}:{tag}+{t - 1000}:{tag}"
                kind = "swap" if variant["id"] in bottom_tags else "skip"
                rows.append((key, kind, re.sub(r"\s*\(top\)$", "", variant["name"]), f"{t}+{t - 1000}"))
            paired.update((t, t - 1000))
    for toy in accepted:
        if toy["type"] in paired:
            continue
        t, category = str(toy["type"]), toy["category"]
        variants = [v for v in toy.get("variants", []) if "id" in v and int(v["id"], 16) >> 12 <= 3
                    and not v.get("same_tag")]
        base = next((v["id"][2:] for v in variants if v["kind"] == "base"), None)
        tags = [] if category == "trap" else [(f"{t}:{base}" if base else t, toy["name"])]
        tags += [(f'{t}:{v["id"][2:]}', v["name"]) for v in variants]
        if not tags:
            tags = [(t, toy["name"])]
        seen = set()
        for key, name in tags:
            if key in seen:
                continue
            seen.add(key)
            kind = "core" if category in ("core", "giant", "trap_master", "mini") else "other"
            if category == "mini" and toy["game"] != "trapteam" and not key.endswith(":3000"):
                kind = "other"  # an older sidekick needs a playable companion
            if category == "trap":
                kind = "skip"  # the generator/roundtrip checker has no trap/villain area support
            rows.append((key, kind, name, t))
    selected = []
    matched = set()
    for row in rows:
        key, _, _, types = row
        aliases = {key, types} | set(types.split("+")) | set(key.split("+"))
        matches = set(wanted) & aliases
        if not wanted or matches:
            selected.append(row)
            matched.update(matches)
    for key in sorted(set(wanted) - matched):
        selected.append((key, "skip", key, key))
    return selected


def main():
    command, *args = sys.argv[1:]
    if command == "levels":
        print("\n".join(levels(args[0])))
    elif command == "work":
        print(work_directory(args[0]))
    elif command == "figures":
        with open(args[0]) as stream:
            for key, kind, name, types in figure_rows(json.load(stream), args[1:]):
                print(key, kind, name.replace(" ", "_"), types, sep="\t")
    else:
        raise ValueError("unknown Trap Team input command")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, IndexError, struct.error) as error:
        sys.exit(str(error))
