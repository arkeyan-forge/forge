#!/bin/bash
# Unit checks only: dummy processes, no runtime, disc or figure data.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/tools/regress/pid_timer.sh"
scratch=$(mktemp -d "${TMPDIR:-/tmp}/f3mouse-timer-test.XXXXXX") || exit 1
sentinel=
cleanup_test() {
    [ -z "$sentinel" ] || { kill "$sentinel" 2>/dev/null; wait "$sentinel" 2>/dev/null; }
    rm -rf "$scratch"
}
trap cleanup_test EXIT
mkdir "$scratch/bin"
# Observe fd 3 in the timer's own sleep children, and keep the escalation test short.
cat > "$scratch/bin/sleep" <<'PY'
#!/usr/bin/env python3
import os
import pathlib
import sys
import time
try:
    os.fstat(3)
except OSError:
    pass
else:
    pathlib.Path(os.environ['TIMER_TEST_DIR'], 'fd-leaked').touch()
time.sleep(min(float(sys.argv[1]), 0.2))
PY
cat > "$scratch/dummy.py" <<'PY'
import os
import pathlib
import signal
import sys
import time
if sys.argv[1] == 'stubborn':
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
try:
    os.fstat(3)
except OSError:
    pass
else:
    pathlib.Path(os.environ['TIMER_TEST_DIR'], 'fd-leaked').touch()
pathlib.Path(os.environ['TIMER_TEST_DIR'], 'pid').write_text(str(os.getpid()))
time.sleep(30)
PY
chmod +x "$scratch/bin/sleep"
export TIMER_TEST_DIR="$scratch"
export PATH="$scratch/bin:$PATH"
python3 -c 'import time; time.sleep(30)' & sentinel=$!
fail=0
bad() { echo "FAIL: $*"; fail=1; }
for mode in normal stubborn; do
    (run_timed 0.2 python3 "$scratch/dummy.py" "$mode" 3>"$scratch/lock") >"$scratch/$mode.out" 2>&1 || bad "$mode timer run failed"
    dummy_pid=$(cat "$scratch/pid")
    kill -0 "$dummy_pid" 2>/dev/null && bad "$mode dummy survived the watchdog"
    kill -0 "$sentinel" 2>/dev/null || bad "watchdog killed an unrelated process"
done
[ ! -e "$scratch/fd-leaked" ] || bad "game or timer inherited fd 3"
(run_timed 1 true) >"$scratch/early.out" 2>&1 && bad "early successful exit was accepted"
(run_timed 1 false) >"$scratch/error.out" 2>&1 && bad "early error exit was accepted"
# Cancellation must also reap a still-running command and its timer.
(run_timed 30 python3 "$scratch/dummy.py" normal) >"$scratch/cancel.out" 2>&1 & runner=$!
for attempt in $(seq 20); do
    cancel_pid=$(cat "$scratch/pid")
    [ "$cancel_pid" != "$dummy_pid" ] && break
    /bin/sleep 0.05
done
kill -TERM "$runner" 2>/dev/null
wait "$runner" 2>/dev/null
kill -0 "$cancel_pid" 2>/dev/null && bad "dummy survived cancellation"
if [ "$fail" -eq 0 ]; then echo "PASS: PID timer, escalation, fd 3, early exit, cancellation and unrelated process"; fi
exit "$fail"
