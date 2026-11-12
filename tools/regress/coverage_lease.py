#!/usr/bin/env python3
"""Several coverage runs in a row inside one game slot (tools/regress/coverage_all.py runs it under ../gamelock.sh; not a
user-facing command): a slot taken once serves runs for up to <lease seconds>, so a busy lock's queue is waited for once
per lease, not once per entry.

  coverage_lease.py <jobs.json> <lease seconds>

jobs.json: a list of {"id", "args"} (coverage_run.py's arguments). Runs them in order, one game at a time, and stops
starting new ones once the lease is spent (the first always runs). Prints one JSON line per job run: id, seconds,
results (coverage_run.py's JSON lines). A TERM stops the run in progress (its game too) and ends.
"""
import json
import os
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
current = None


def stop(signum, frame):
    if current is not None and current.poll() is None:
        current.send_signal(signal.SIGTERM)
        try:
            current.wait(15)
        except subprocess.TimeoutExpired:
            current.kill()
    sys.exit(143)


def main():
    global current
    jobs = json.load(open(sys.argv[1]))
    lease = float(sys.argv[2])
    signal.signal(signal.SIGTERM, stop)
    started = time.time()
    for n, job in enumerate(jobs):
        if n > 0 and time.time() - started > lease:
            break
        t = time.time()
        current = subprocess.Popen([sys.executable, "-I", os.path.join(HERE, "coverage_run.py")] + job["args"],
                                   stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        out, _ = current.communicate()
        results = []
        for line in out.splitlines():
            try:
                results.append(json.loads(line))
            except ValueError:
                pass
        print(json.dumps({"id": job["id"], "seconds": round(time.time() - t), "results": results}), flush=True)


if __name__ == "__main__":
    main()
