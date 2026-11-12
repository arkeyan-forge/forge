#!/usr/bin/env python3
"""Summarises an SKY_FRAME_TIMES file (tools/perf/level_run.sh, tools/perf/giants_run.sh).

Each line: present time (ms since start), the frame's work (ms, without the vblank wait), draws. The level starts at
the first frame with at least 1000 draws. Reports the worst frame gaps in the level's first 5 s (the start stalls,
from the gap that ends with that first frame) and
the frame gaps, work times and frame rate over a fixed window of the level (from 10 s to 40 s after its start).

  frames.py <frames.txt> [--from <s> --to <s>]

With --from and --to the window is those seconds since launch instead (Giants: its levels have fewer draws), and the
ten worst gaps of the window are listed with their time."""
import sys


def percentile(values, p):
    values = sorted(values)
    return values[min(len(values) - 1, int(round(p / 100 * (len(values) - 1))))]


def summary(name, values):
    return (f"{name}: min {min(values):.1f} median {percentile(values, 50):.1f} p95 {percentile(values, 95):.1f} "
            f"p99 {percentile(values, 99):.1f} max {max(values):.1f} ms")


frames = [tuple(float(x) for x in line.split()) for line in open(sys.argv[1]) if len(line.split()) == 3]
if "--from" in sys.argv:
    start_s = float(sys.argv[sys.argv.index("--from") + 1])
    end_s = float(sys.argv[sys.argv.index("--to") + 1])
    window = [i for i in range(1, len(frames)) if start_s * 1000 <= frames[i][0] <= end_s * 1000]
    if len(window) < 30:
        sys.exit(f"window too short ({len(window)} frames)")
else:
    start = next((i for i, f in enumerate(frames) if f[2] >= 1000), None)
    if start is None:
        sys.exit("no level frame (>= 1000 draws)")
    t0 = frames[start][0]
    gaps = [(frames[i][0] - frames[i - 1][0], frames[i][0] - t0) for i in range(start, len(frames))] # the first one too
    first = [g for g in gaps if g[1] <= 5000]
    worst = sorted(first, reverse=True)[:3]
    print(f"level start at {t0 / 1000:.1f} s; first 5 s: worst gaps " + ", ".join(f"{g:.0f} ms (at +{t:.0f} ms)" for g, t in worst))
    window = [i for i in range(start + 1, len(frames)) if 10000 <= frames[i][0] - t0 <= 40000]
    if len(window) < 30:
        sys.exit(f"window too short ({len(window)} frames)")
    start_s, end_s = (t0 + 10000) / 1000, (t0 + 40000) / 1000
seconds = (frames[window[-1]][0] - frames[window[0] - 1][0]) / 1000
print(f"window {start_s:g}..{end_s:g} s: {len(window)} frames, {len(window) / seconds:.1f} fps, "
      f"draws median {percentile([frames[i][2] for i in window], 50):.0f}")
print(summary("  gap ", [frames[i][0] - frames[i - 1][0] for i in window]))
print(summary("  work", [frames[i][1] for i in window]))
if "--from" in sys.argv:
    worst = sorted(window, key=lambda i: frames[i][0] - frames[i - 1][0], reverse=True)[:10]
    print("  worst gaps: " + ", ".join(f"{frames[i][0] - frames[i - 1][0]:.0f} ms at {frames[i][0] / 1000:.1f} s"
                                       for i in sorted(worst)))
