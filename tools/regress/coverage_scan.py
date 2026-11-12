#!/usr/bin/env python3
"""One coverage row's verdict, lowest frame rate and detail, from a run's log and frame times. Used by
tools/regress/coverage.sh; it is not a user-facing command.

  coverage_scan.py <log> <frame times> <level|figure|credits> <ok|not_reached|hang|crash> [<prefix detail>]

The verdict is crash when the log has a fault or a [guard] line, hang or not_reached when the caller says so (the
frames stopped; the level, figure or credits were not reached) or the run has no frame rate (under 2 s of frames or a gap of at least 1 s), ok otherwise.
Detail lists the [guard] lines, faults, script errors and missing-file lines of that log (first few of each), after the
prefix. The lowest frame rate is the smallest number of frames in any one-second window of the run: the frames from
the first to the last that draws at least 60 % of the run's busiest draw counts for a level or figure, every frame for
credits.
"""
import os
import re
import sys

log, frames, kind, hint = (sys.argv[1:5] + ["", "", "", ""])[:4]
prefix = sys.argv[5] if len(sys.argv) > 5 else ""
text = open(log, errors="replace").read() if log and os.path.exists(log) else ""

GUARD = re.compile(r"\[guard\]")
FAULT = re.compile(r"Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed")
ERROR = re.compile(r"\b(error|failed|failure)\b", re.I)
MISSING = re.compile(r"(missing|no such file|cannot open|cannot read|not found)", re.I)
IGNORE = ("SDL disk i/o audio driver", "Writing to file")

def lines(pattern):
    out = []
    for line in text.splitlines():
        if pattern.search(line) and not any(word.lower() in line.lower() for word in IGNORE):
            line = " ".join(line.split())
            if line not in out:
                out.append(line[:300])
    return out

guards = lines(GUARD)
faults = lines(FAULT)
errors = [line for line in lines(ERROR) if not any(line in g for g in guards + faults)]
missing = [line for line in lines(MISSING) if not any(line in g for g in guards + faults + errors)]

verdict = "ok"
if faults or guards or hint == "crash":
    verdict = "crash"
elif hint in ("not_reached", "hang"):
    verdict = hint

def rate(path):
    rows = []
    if path and os.path.exists(path):
        for line in open(path, errors="replace"):
            fields = line.split()
            if len(fields) == 3:
                try:
                    rows.append((float(fields[0]), int(float(fields[2]))))
                except ValueError:
                    pass
    if not rows:
        return 0.0
    rows.sort()
    if kind != "credits":
        draws = sorted(row[1] for row in rows)
        floor = 0.6 * draws[int(0.9 * (len(draws) - 1))]  # menus and loading draw far less than the level does
        busy = [i for i, row in enumerate(rows) if row[1] >= floor]
        if busy:
            rows = rows[busy[0]:busy[-1] + 1]  # from the level's first busy frame to its last, hitches included
    times = [row[0] for row in rows]
    end = times[-1]
    if end - times[0] < 2000:
        return 0.0
    if any(after - before >= 1000 for before, after in zip(times, times[1:])):
        return 0.0  # A full second without a frame, even when no window starts there.
    lowest = None
    j = 0
    for i, start in enumerate(times):
        if start + 1000 > end:
            break
        j = max(i, j)
        while j < len(times) and times[j] < start + 1000:
            j += 1
        count = j - i
        lowest = count if lowest is None else min(lowest, count)
    return float(lowest or 0)

lowest = rate(frames)
if verdict == "ok" and lowest <= 0:
    verdict = "not_reached"
    prefix = (prefix + " " if prefix else "") + "no frame rate: under 2 s of frames or a gap of at least 1 s"
detail = []
if prefix:
    detail.append(prefix.strip())
for label, found in (("guards", guards), ("faults", faults), ("errors", errors), ("missing", missing)):
    if found:
        detail.append(f"{label}: " + " | ".join(found[:4]) + (f" (+{len(found) - 4} more)" if len(found) > 4 else ""))
text = "; ".join(detail).replace("\t", " ").replace("\n", " ")
print(f"{verdict}\t{lowest:.1f}\t{text[:1200]}")
