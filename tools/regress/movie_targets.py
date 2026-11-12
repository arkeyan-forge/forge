#!/usr/bin/env python3
"""Every game of the kit declares how many run-time shaders its movie pass must collect (`movie_shaders` in games/<id>/game.txt).

Without a target the pass ends 8 s after the first shader it finds, and a slower start misses Bink's two movie shaders:
every movie is black in that install (docs/runtime/gpu/render_bugs.md). Run: python3 -I tools/regress/movie_targets.py
"""
import pathlib
import sys

games = pathlib.Path(__file__).resolve().parents[2] / "games"
bad = []
count = 0
for path in sorted(games.glob("*/game.txt")):
    values = {}
    for line in path.read_text().splitlines():
        if "=" in line and not line.startswith("#"):
            key, value = line.split("=", 1)
            values[key] = value
    count += 1
    if not values.get("movie_shaders", "").isdigit() or int(values["movie_shaders"]) == 0:
        bad.append(path.parent.name)
if count == 0 or bad:
    print("FAIL: no movie_shaders target in game.txt of: " + ", ".join(bad) if bad else "FAIL: no game.txt found")
    sys.exit(1)
print("PASS: %d games declare a movie_shaders target" % count)
