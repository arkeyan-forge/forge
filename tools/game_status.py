#!/usr/bin/env python3
"""The README's game table, written from each game's own pages: the one source of each game's state is the "At a glance"
section of games/<id>/docs/status.md, and its version is games/<id>/game.txt's (the one the kits carry).

usage: python3 -I tools/game_status.py [--check] [--launcher]
  writes the table between README.md's game-status markers; --check changes nothing and exits 1 if it is not up to date
  (tools/check_docs.py runs it); --launcher prints the platform columns for the launcher's README.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAMES = (  # id, the name a player reads, year, engine
    ("ssa", "Spyro's Adventure (SSA, 2011)", "Lux"),
    ("giants", "Giants (2012)", "Alchemy"),
    ("swapforce", "Swap Force (2013)", "Alchemy"),
    ("trapteam", "Trap Team (2014)", "Alchemy"),
    ("superchargers", "SuperChargers (2015)", "Alchemy"),
    ("imaginators", "Imaginators (2016)", "Alchemy"),
)
BEGIN, END = "<!-- game-status:begin -->", "<!-- game-status:end -->"
SYSTEMS = ("macOS", "Windows", "Linux")


def glance(game):
    """(played on, known limits) of a game's status page."""
    text = open(os.path.join(ROOT, "games", game, "docs", "status.md"), encoding="utf-8").read()
    fields = {}
    for name in ("Played on", "Known limits"):
        m = re.search(r"^- \*\*" + name + r":\*\* (.+?)\.?$", text, re.M)
        if not m:
            sys.exit(f"games/{game}/docs/status.md: no '- **{name}:**' line in its At a glance section")
        fields[name] = m.group(1)
    return fields["Played on"], fields["Known limits"]


def version(game):
    m = re.search(r"^version=(\S+)$", open(os.path.join(ROOT, "games", game, "game.txt"), encoding="utf-8").read(), re.M)
    return m.group(1) if m else "?"


def systems(played):
    """The systems named in "Played on", each once, in a fixed order (the evidence in brackets is left to the page)."""
    found = {s for s in SYSTEMS if re.search(r"(^|; )" + s + r"\b", played)}
    return [s for s in SYSTEMS if s in found]


def table():
    rows = ["| Game | Engine | Version | Played on | Known limits |", "|---|---|---|---|---|"]
    for game, name, engine in GAMES:
        played, limits = glance(game)
        rows.append(f"| [{name}](games/{game}/docs/status.md) | {engine} | {version(game)} | "
                    f"{', '.join(systems(played))} | {limits} |")
    return "\n".join(rows)


def main():
    if "--launcher" in sys.argv:
        for game, name, _ in GAMES:
            got = systems(glance(game)[0])
            print(f"{name}: " + ", ".join(f"{s} {'yes' if s in got else 'no'}" for s in SYSTEMS))
        return 0
    path = os.path.join(ROOT, "README.md")
    text = open(path, encoding="utf-8").read()
    if BEGIN not in text or END not in text:
        print(f"README.md: no {BEGIN} ... {END} markers")
        return 1
    head, rest = text.split(BEGIN, 1)
    _, tail = rest.split(END, 1)
    new = head + BEGIN + "\n" + table() + "\n" + END + tail
    if "--check" in sys.argv:
        if new != text:
            print("README.md: the game table differs from the status pages (run python3 -I tools/game_status.py)")
            return 1
        return 0
    open(path, "w", encoding="utf-8").write(new)
    return 0


if __name__ == "__main__":
    sys.exit(main())
