#!/usr/bin/env python3
"""The load times of one run, from its SKY_FRAME_TIMES file and its log (docs/runtime/gpu/renderer_threads.md, "Loading").

A loading screen draws little (Swap Force 5-21 draws, Giants and SSA under 100) while the title, the menus and a level
draw hundreds: a load is a stretch of frames under --draws (default 100) lasting at least --min seconds (default 1),
between two stretches over it. Both files count from start-up (frames: ms at each present; the log's
`[file] open <path> -> <status> at <s> s` lines), so each load is listed with the first file it opened.

  load_times.py <frames.txt> <game.log> [--draws N] [--min S] [--file NAME]

Prints the first frame over --draws ("first full frame": the title or the menu, or the level with SKY_LEVEL), then each
load: its start (the last full frame before it), its end (the first full frame after it), its length and its first
opened file; a load the run ended in is marked "not finished". With --file, also the time from the first open of a path
containing NAME (the level's own archive: level_213_tutotial.arc, brn2.pak, level_027\mission.str) to the end of the
load it falls in: a load screen can start with something else (Giants saves and fades for ~4 s before the level)."""
import re
import sys

args = sys.argv[1:]
frames_path, log_path = args[0], args[1]
draws = int(args[args.index("--draws") + 1]) if "--draws" in args else 100
minimum = float(args[args.index("--min") + 1]) if "--min" in args else 1.0
name = args[args.index("--file") + 1] if "--file" in args else None

frames = [(float(f[0]) / 1000, int(f[2])) for f in (line.split() for line in open(frames_path)) if len(f) == 3]
opens = []
for line in open(log_path, errors="replace"):
    m = re.match(r"\[file\] open (.+) -> 0x00000000 at ([0-9.]+) s", line)
    if m:
        opens.append((float(m.group(2)), m.group(1)))

full = [t for t, d in frames if d >= draws]
if not full:
    sys.exit(f"no frame with {draws} draws or more")
print(f"first full frame (>= {draws} draws) at {full[0]:.2f} s")
loads = 0
for i, (t, d) in enumerate(frames):
    if d >= draws and i + 1 < len(frames) and frames[i + 1][1] < draws:
        end = next((u for u, e in frames[i + 1:] if e >= draws), None)
        if end is not None and end - t < minimum:
            continue
        first = next((path for at, path in opens if t <= at and (end is None or at <= end) and not path.startswith("save:")), "-")
        loads += 1
        if end is None:
            print(f"load {loads}: from {t:.2f} s, not finished at {frames[-1][0]:.2f} s ({frames[-1][0] - t:.2f} s so far); first file {first}")
        else:
            print(f"load {loads}: {t:.2f} -> {end:.2f} s = {end - t:.2f} s; first file {first}")
            opened = next((at for at, path in opens if name is not None and name in path and t <= at <= end), None)
            if opened is not None:
                print(f"  {name} opened at {opened:.2f} s: {end - opened:.2f} s to the load's end")
