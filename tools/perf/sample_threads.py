#!/usr/bin/env python3
"""Summarises a macOS `sample <pid> <seconds> -file <out>` report per thread: how many samples the thread spent
running (not blocked in the kernel) and its hottest functions by self time, plus, with --under, how much of each
thread's time passed through the named functions (inclusive).

  sample_threads.py <sample.txt> [--top N] [--min SAMPLES] [--under name,name...] [--thread SUBSTRING] [--callers NAME]
                    [--depth N]

--callers lists, per thread, the call paths (--depth host or guest functions up, default 3) into the functions named NAME.

"Blocked" leaves: __psynch_cvwait, __semwait_signal, __workq_kernreturn, __psynch_mutexwait, mach_msg2_trap,
__ulock_wait*, kevent*, __select. A thread's busy count is its samples minus those."""
import re
import sys
from collections import Counter

BLOCKED = ("__psynch_cvwait", "__semwait_signal", "__workq_kernreturn", "__psynch_mutexwait", "mach_msg2_trap",
           "__ulock_wait", "__ulock_wait2", "kevent", "kevent_id", "kevent_qos", "__select", "__sigsuspend")

args = sys.argv[1:]
path = args[0]
top = int(args[args.index("--top") + 1]) if "--top" in args else 12
minimum = int(args[args.index("--min") + 1]) if "--min" in args else 50
under = args[args.index("--under") + 1].split(",") if "--under" in args else []
only = args[args.index("--thread") + 1] if "--thread" in args else None
callers_of = args[args.index("--callers") + 1] if "--callers" in args else None
depth_up = int(args[args.index("--depth") + 1]) if "--depth" in args else 3

text = open(path).read()
text = text.split("Total number in stack")[0].split("Sort by top of stack")[0]
line_re = re.compile(r"^([ +!:|]*)(\d+) (.+?)(?:  \(in [^)]*\).*)?$")
threads = re.split(r"\n    (?=\d+ Thread_)", text)[1:]
for block in threads:
    lines = block.splitlines()
    head = lines[0]
    total = int(head.split()[0])
    if only and only not in head:
        continue
    # (depth, count, name) per node; self = count - sum(direct children)
    nodes = []
    for line in lines[1:]:
        m = line_re.match(line)
        if not m:
            continue
        nodes.append((len(m.group(1)), int(m.group(2)), m.group(3).strip()))
    selfs = Counter()
    inclusive = Counter()
    callers = Counter()
    blocked = 0
    stack = []  # (depth, name)
    for i, (depth, count, name) in enumerate(nodes):
        children = 0
        # direct children: the next nodes at the first deeper depth until a node at depth <= ours
        child_depth = None
        for d, c, _ in nodes[i + 1:]:
            if d <= depth:
                break
            if child_depth is None:
                child_depth = d
            if d == child_depth:
                children += c
        own = count - children
        short = re.sub(r"\(.*", "", name.replace("(anonymous namespace)", "anon")).strip() or name
        if own > 0:
            selfs[short] += own
            if short in BLOCKED:
                blocked += own
        while stack and stack[-1][0] >= depth:
            stack.pop()
        stack.append((depth, short))
        for u in under:
            if u in short and not any(u in s for _, s in stack[:-1]):
                inclusive[u] += count
        if callers_of and callers_of in short and not any(callers_of in s for _, s in stack[:-1]):
            callers[" < ".join(s for _, s in reversed(stack[-depth_up - 1:-1]))] += count
    busy = total - blocked
    if busy < minimum:
        continue
    print(f"== {head.strip()}: busy {busy} of {total}")
    shown = [(n, name) for name, n in selfs.most_common() if name not in BLOCKED][:top]
    for n, name in shown:
        print(f"   {n:6d}  {name[:110]}")
    for u in under:
        if inclusive[u]:
            print(f"   under {u}: {inclusive[u]}")
    for path, n in callers.most_common(top):
        print(f"   {n:6d}  {callers_of} < {path[:150]}")
