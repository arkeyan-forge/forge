#!/bin/bash
# Proves that the game isolates itself with only the SKY_* variables, and with only the older SSA_* names (macOS):
# two short runs of a built game under sandbox-exec (writes to the real data folders denied), one given the isolation
# variables as SKY_*, the other as SSA_* (docs/getting-started/testing.md). Each must log to its scratch SKY_LOG/SSA_LOG
# (the log's first line), name no real folder in any log, and leave the real folders unchanged (listed with find -newer).
#
#   tools/regress/env_names.sh <ssa|giants|swapforce> <disc image> [seconds per run, default 30]
#   EXE=<game executable>  default build/ArkeyanRuntime/Skylanders Spyro's Adventure (build-giants/..., build-swapforce/...: Skylanders Giants, Skylanders Swap Force)
#
# Everything is under private/regress/env-names-<game>/. Exit code 0: PASS.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; seconds=${3:-30}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] || { echo "usage: $0 <ssa|giants|swapforce> <disc image> [seconds]" >&2; exit 2; }
[ "$(uname)" = Darwin ] || { echo "SKIP: macOS only" >&2; exit 0; }
case $game in ssa) dir=build; name="Skylanders Spyro's Adventure" ;; giants) dir=build-giants; name="Skylanders Giants" ;; *) dir=build-swapforce; name="Skylanders Swap Force" ;; esac
exe="${EXE:-$root/$dir/ArkeyanRuntime/$name}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
scratch=$root/private/regress/env-names-$game
rm -rf "${scratch:?}"; mkdir -p "$scratch"
home=$(dscl . -read "/Users/$(id -un)" NFSHomeDirectory | awk '{print $2}')
support=$home/Library/Application\ Support
fail=0
bad() { echo "FAIL: $*"; fail=1; }
profile=$scratch/deny.sb
{
    echo '(version 1)(allow default)'
    for n in Skylanders SSARecomp Skylauncher; do echo "(deny file-write* (subpath \"$support/$n\"))"; done
} > "$profile"
touch "$scratch/start"; sleep 1

for prefix in SKY SSA; do
    r=$scratch/$prefix; mkdir -p "$r"
    echo "== only ${prefix}_* variables, ${seconds} s"
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKYLANDERS_DATA="$r/data" \
        ${prefix}_SETTINGS="$r/none.ini" ${prefix}_LAUNCHER_INI="$r/none.ini" ${prefix}_COLLECTION="$r/collection" \
        ${prefix}_SAVE_DIR="$r/save" ${prefix}_LOG="$r/game.log" ${prefix}_SKIP_MOVIES=1 ${prefix}_PORTAL=none \
        nice sandbox-exec -f "$profile" "$exe" "$iso" > "$r/run.log" 2>&1 &
    pid=$!
    for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
    kill "$pid" 2>/dev/null; sleep 2; kill -9 "$pid" 2>/dev/null # our own PID only
    wait "$pid" 2>/dev/null
    [ -s "$r/game.log" ] || bad "$prefix: no log at $r/game.log"
    head -1 "$r/game.log" | grep -qF "[log] $r/game.log" || bad "$prefix: the log's first line is not the scratch path: $(head -1 "$r/game.log")"
    grep -q 'portal\] mode none' "$r/game.log" "$r/run.log" 2>/dev/null || bad "$prefix: ${prefix}_PORTAL=none was not read"
    leaks=$(grep -lF "$support/" "$r/run.log" "$r/game.log" 2>/dev/null)
    [ -z "$leaks" ] || bad "$prefix: a log names the real data folder: $leaks"
done
for n in Skylanders SSARecomp Skylauncher; do
    [ -d "$support/$n" ] || continue
    changed=$(find "$support/$n" -newer "$scratch/start" 2>/dev/null | head -5)
    [ -z "$changed" ] || bad "written in the real $n folder: $(echo "$changed" | tr '\n' ' ')"
done
if [ $fail -eq 0 ]; then echo "PASS: $game ran isolated with SKY_* alone and with SSA_* alone (under $scratch)"; else echo "FAIL (logs in $scratch)"; fi
exit $fail
