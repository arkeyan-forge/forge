#!/usr/bin/env python3
"""One tab-separated line for a run of the SSA story tour (tools/pipelines/tour_ssa_story.sh):
chapter, level, opened (the level's mission.str opened, SKY_TRACE_FILES), fault lines, [guard] lines, seconds in the
level (from the first frame of 300 draws or more, the level's frames, to the last frame), last frame (s), the mean
brightness of the last snapshot (0-255), hero (1: the game wrote the figure back, its first-use write, which it makes
when the Skylander enters play; only with TOUR_MODE=hero, else "-"), verdict: ok | no level | fault | hang (frames stopped well before the end) |
short (less than 60 s in the level) | black (last snapshot below 6)."""
import os
import re
import sys

chapter, level, log, frames, seconds, shots = sys.argv[1:7]
figure = sys.argv[7:9]  # the figure placed and its copy in the run, with TOUR_MODE=hero
seconds = float(seconds)
text = open(log, errors="replace").read() if os.path.exists(log) else ""
opened = f"{level}\\mission.str -> 0x00000000" in text
faults = len(re.findall(r"Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed", text))
guards = text.count("[guard]")
rows = ([tuple(float(x) for x in l.split()) for l in open(frames) if len(l.split()) == 3]
        if os.path.exists(frames) else [])
last = rows[-1][0] / 1000 if rows else 0
first_level = next((r[0] / 1000 for r in rows if r[2] >= 300), None)
in_level = last - first_level if first_level is not None else 0
mean = "-"
ppms = sorted((f for f in os.listdir(shots) if f.endswith(".ppm")),
              key=lambda f: int(re.sub(r"\D", "", f) or 0)) if os.path.isdir(shots) else []
if ppms:
    try:
        pixels = open(os.path.join(shots, ppms[-1]), "rb").read().split(b"\n", 3)[3]
        mean = f"{sum(pixels[::97]) / len(pixels[::97]):.0f}"
    except (OSError, IndexError, ZeroDivisionError):
        pass
hero = "-"
if len(figure) == 2 and os.path.exists(figure[1]):
    hero = str(int(open(figure[0], "rb").read() != open(figure[1], "rb").read()))
verdict = "ok"
if faults:
    verdict = "fault"
elif not opened:
    verdict = "no level"
elif last < seconds - 20:
    verdict = "hang"
elif in_level < 60:
    verdict = "short"
elif mean != "-" and float(mean) < 6:
    verdict = "black"
elif hero == "0":
    verdict = "no hero"
print("\t".join([chapter, level, str(int(opened)), str(faults), str(guards), f"{in_level:.0f}", f"{last:.0f}", mean,
                 hero, verdict]))
