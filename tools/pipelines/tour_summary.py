#!/usr/bin/env python3
"""One tab-separated line for a tour run (tools/pipelines/tour_giants.sh, tour_swapforce.sh):
level, opened (the level's .arc opened, 1/0; or the optional 5th argument, a log line such as Swap Force's zone .pak
open), draw-time builds in the level, fault lines, last frame (s), median fps in the level's frames (>= 300 draws), max
draws, verdict (ok | no level | fault | hang | black), and with an optional 6th argument, a PPM snapshot of the end of
the run, its mean brightness (0-255; "black" instead of "ok" below 6)."""
import os
import re
import statistics
import sys

level, log, frames, seconds = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])  # log: the run's SKY_LOG
text = open(log, errors="replace").read()
marker = sys.argv[5] if len(sys.argv) > 5 else None  # Swap Force: "open \archives\<map>.pak -> 0x00000000"
opened = (marker or f"\\level\\{level}.arc -> 0x00000000") in text
at = text.find(marker) if marker else text.find(f"loading (\\level\\{level}.arc)")
builds = len(re.findall(r"built at draw time", text[at:])) if at >= 0 else 0
faults = len(re.findall(r"Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed", text))
rows = ([tuple(float(x) for x in l.split()) for l in open(frames) if len(l.split()) == 3]
        if frames and os.path.exists(frames) else [])  # none: the run ended before its first frame
last = rows[-1][0] / 1000 if rows else 0
lvl = [r for r in rows if r[2] >= 300]
fps = 0.0
if len(lvl) > 2:
    gaps = [(b[0] - a[0]) for a, b in zip(lvl, lvl[1:]) if 0 < b[0] - a[0] < 500]
    fps = 1000 / statistics.median(gaps) if gaps else 0.0
verdict = "ok"
if faults:
    verdict = "fault"
elif last < seconds - 12:
    verdict = "hang" if rows else "no frames"
elif not opened:
    verdict = "no level"
mean = []
if len(sys.argv) > 6:
    try:
        data = open(sys.argv[6], "rb").read()
        header = data.split(b"\n", 3)  # P6, width height, 255, pixels (the runtime writes no comments)
        pixels = header[3]
        mean = [f"{sum(pixels[::97]) / len(pixels[::97]):.0f}"]
        if verdict == "ok" and float(mean[0]) < 6:
            verdict = "black"
    except (OSError, IndexError, ZeroDivisionError):
        mean = ["-"]
print("\t".join([level, str(int(opened)), str(builds), str(faults), f"{last:.0f}", f"{fps:.0f}",
                 str(int(max((r[2] for r in rows), default=0))), verdict] + mean))
