#!/usr/bin/env python3
"""CPU time per frame, per thread, between two `ps -M -p <pid>` snapshots of the game (tools/perf/giants_run.sh).

  thread_cpu.py <ps at start> <ps at end> <frames.txt> <from s> <to s>

Prints the process's CPU time per presented frame of the window and its busiest threads' (the render thread is the
first one in a Giants level). Unlike the frame's work time (a wall time), it does not grow when other processes take
the cores, though it does when the thread runs on an efficiency core (Apple silicon) or `sample` interrupts it. Threads
are matched by their order (`ps -M` lists them in creation order, without their ids): a thread that ends inside the
window shifts the list and its successors' figures are wrong (flagged when one exceeds the window's length); the
process's total stays close (less what the ended thread used in the window)."""
import re
import sys


def thread_times(path):
    times = []
    for line in open(path).read().splitlines()[1:]:
        fields = re.findall(r"\b(\d+):(\d+\.\d+)\b", line)[:2] # STIME, UTIME
        if len(fields) == 2:
            times.append(sum(int(minutes) * 60 + float(seconds) for minutes, seconds in fields))
    return times


start, end = thread_times(sys.argv[1]), thread_times(sys.argv[2])
from_ms, to_ms = float(sys.argv[4]) * 1000, float(sys.argv[5]) * 1000
frames = sum(1 for line in open(sys.argv[3]) if len(line.split()) == 3 and from_ms <= float(line.split()[0]) <= to_ms)
if frames == 0 or not start or not end:
    sys.exit("thread_cpu: no frame or no thread in the window")
deltas = sorted((end[i] - start[i] for i in range(min(len(start), len(end)))), reverse=True)
shifted = deltas[0] > (to_ms - from_ms) / 1000 * 1.05
print(f"cpu per frame ({sys.argv[4]}..{sys.argv[5]} s, {frames} frames): process {sum(deltas) * 1000 / frames:.1f} ms, "
      "busiest threads " + " ".join(f"{d * 1000 / frames:.1f}" for d in deltas[:5]) + " ms"
      + (" (threads changed in the window: per-thread figures unreliable)" if shifted else ""))
