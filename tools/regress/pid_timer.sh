#!/bin/bash
# Source from a regression and call in a subshell: (run_timed seconds command...). The command must exec the game,
# so $! remains its PID. No descendant search or global kill.
# Returns 0 only when the timer stopped the command; an early exit is a failed run, even if its status was zero.
run_timed() {
    seconds=$1; shift
    pid=; timer=
    timer_state=$(mktemp -d "${TMPDIR:-/tmp}/f3mouse-timer.XXXXXX") || exit 1
    cleanup() {
        if [ -n "$timer" ]; then kill "$timer" 2>/dev/null; wait "$timer" 2>/dev/null; fi
        if [ -n "$pid" ]; then kill -KILL "$pid" 2>/dev/null; wait "$pid" 2>/dev/null; fi
        rm -rf "$timer_state"
        :
    }
    trap cleanup EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM HUP
    "$@" 3>&- &
    pid=$!
    (
        exec 3>&- # the timer must never keep gamelock's descriptor open
        sleeper=
        trap 'if [ -n "$sleeper" ]; then kill "$sleeper" 2>/dev/null; wait "$sleeper" 2>/dev/null; fi; exit' TERM INT HUP
        sleep "$seconds" & sleeper=$!; wait "$sleeper"; sleeper=
        : > "$timer_state/fired"
        kill -TERM "$pid" 2>/dev/null || exit 1
        sleep 3 & sleeper=$!; wait "$sleeper"; sleeper=
        kill -KILL "$pid" 2>/dev/null
        exit 0
    ) &
    timer=$!
    wait "$pid"
    pid=
    [ -f "$timer_state/fired" ] || exit 1
    exit 0
}
