#!/usr/bin/env python3
"""Compares the `[dump] floats` lines of two game logs of the same frame (SKY_DUMP_FRAME, SKY_DUMP_FLOATS=1 and
SKY_DUMP_DRAWS=1; docs/porting/debugging.md), typically the Mac's against a tester's PC: the targets in order, each with
the pixel shader of the draw that wrote it, and the first ones whose channel means differ by more than a threshold.

    python3 -I tools/regress/compare_dump_floats.py <reference log> <other log> [threshold, default 0.03]

Prints the targets that differ (at most 12) and says whether the other log has a NaN or an infinity (`BAD`). The two frames
need the same draws (the same scene and settings, such as the resolution scale); a different count is reported. Class [Host].
"""
import re
import sys

FLOATS = re.compile(r"\[dump\] floats file (\d+)(\w) fmt (\d+) (\d+x\d+)( BAD)?(.*)")
CHANNEL = re.compile(r" ([rgba]) nan (\d+) inf (\d+)/(\d+) neg (\d+) den (\d+) \[([^,]+), ([^\]]+)\] mean ([-\d.e+]+)")


def targets(path):
    shader, found = "-", []
    with open(path, "rb") as log:
        for raw in log:
            line = raw.decode("utf-8", "replace")
            if line.startswith("[dump] prim"):
                match = re.search(r" ps ([0-9a-f]+)", line)
                shader = match.group(1)[:8] if match else "-"
            match = FLOATS.search(line)
            if match:
                means = {c[0]: float(c[8]) for c in CHANNEL.findall(match.group(6))}
                found.append((match.group(2), match.group(3), match.group(4), shader, means, bool(match.group(5))))
    return found


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    threshold = float(sys.argv[3]) if len(sys.argv) > 3 else 0.03
    ours, theirs = targets(sys.argv[1]), targets(sys.argv[2])
    print(f"{len(ours)} targets in the reference, {len(theirs)} in the other log")
    if any(not t[4] for t in ours + theirs):
        print("a log has no means: its build's float scan is older than this tool")
        return 1
    bad = [i for i, t in enumerate(theirs) if t[5]]
    if bad:
        print(f"the other log has NaN or Inf in {len(bad)} targets, the first: #{bad[0]} (after ps {theirs[bad[0]][3]})")
    shown = 0
    for i, (a, b) in enumerate(zip(ours, theirs)):
        if a[:3] != b[:3]:
            print(f"#{i}: not the same target ({a[:3]} against {b[:3]}): the frames' draws differ from here")
            break
        worst = max((abs(a[4][c] - b[4].get(c, 0.0)), c) for c in a[4])
        if worst[0] > threshold:
            print(f"#{i} {a[0]} after ps {a[3]} {a[2]} fmt {a[1]}: mean {worst[1]} {a[4][worst[1]]:.4f} against "
                  f"{b[4].get(worst[1], 0.0):.4f}")
            shown += 1
            if shown == 12:
                break
    if shown == 0:
        print(f"no target's mean differs by more than {threshold}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
