#!/bin/bash
# Proves that a kit-built game, and the Arkeyan-Builder that builds it, write nothing outside their scratch folders (macOS):
# the player's real data folder ("Skylanders" in Application Support, docs/getting-started/testing.md, "Kit isolation")
# stays as it was. A kit-built game is the one a player runs: Arkeyan-Builder links it and runs it once headless (movie
# shaders), then the installed app is started. Neither run gets SKYLANDERS_DATA or SKY_LOG from the environment of
# Arkeyan-Builder itself (the game's scratch variables are set for the second run only): Arkeyan-Builder must isolate its own run.
#
#   tools/regress/kit_isolation.sh <ssa|giants|swapforce> <disc image> [seconds the installed game runs, default 40]
#   KIT=<kit dir>  the macOS kit to use (default dist/skylanders-kit-macos-arm64, built with tools/make_kit.sh when missing)
#
# Both runs are under sandbox-exec with writes to the real data folders denied: a leak is a failed write ("[log] cannot
# write <real path>") instead of a changed file, and is reported. The real folder is only listed (find -newer a marker
# made at the start): files newer than the start there fail the check. Everything else is under private/regress/.
# Exit code 0: PASS.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; seconds=${3:-40}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] || { echo "usage: $0 <ssa|giants|swapforce> <disc image> [seconds]" >&2; exit 2; }
[ "$(uname)" = Darwin ] || { echo "SKIP: macOS only" >&2; exit 0; }
runtime=$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")
kit=${KIT:-$root/dist/skylanders-kit-macos-arm64}
[ -x "$kit/Arkeyan-Builder" ] || KIT_GAMES=$game "$root/tools/make_kit.sh" macos || exit 1
scratch=$root/private/regress/kit-isolation-$game
rm -rf "${scratch:?}"; mkdir -p "$scratch"
# The passwd home, as the runtime reads it (not $HOME).
home=$(dscl . -read "/Users/$(id -un)" NFSHomeDirectory | awk '{print $2}')
support=$home/Library/Application\ Support
fail=0
bad() { echo "FAIL: $*"; fail=1; }

profile=$scratch/deny.sb
{
    echo '(version 1)(allow default)'
    for name in Skylanders SSARecomp Skylauncher; do echo "(deny file-write* (subpath \"$support/$name\"))"; done
} > "$profile"
touch "$scratch/start"; sleep 1

echo "== Arkeyan-Builder ($game), its own environment untouched"
nice sandbox-exec -f "$profile" "$kit/Arkeyan-Builder" --iso "$iso" --out "$scratch/game" > "$scratch/arkeyan-builder.log" 2>&1
status=$?
[ $status -eq 0 ] || bad "Arkeyan-Builder exited with $status (see $scratch/arkeyan-builder.log)"

echo "== the installed game, ${seconds} s"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKYLANDERS_DATA="$scratch/data" SKY_SETTINGS="$scratch/none.ini" SKY_LAUNCHER_INI="$scratch/none.ini" \
    SKY_COLLECTION="$scratch/collection" SKY_SAVE_DIR="$scratch/save" SKY_LOG="$scratch/game.log" SKY_SKIP_MOVIES=1 \
    SKY_PORTAL=none nice sandbox-exec -f "$profile" "$scratch/game/$runtime.app/Contents/MacOS/$runtime" "$iso" \
    > "$scratch/run.log" 2>&1 &
pid=$!
for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
kill "$pid" 2>/dev/null; sleep 2; kill -9 "$pid" 2>/dev/null # our own PID only
wait "$pid" 2>/dev/null

echo "== checks"
[ -s "$scratch/game.log" ] || bad "SKY_LOG got no log: $scratch/game.log"
head -1 "$scratch/game.log" | grep -qF "[log] $scratch/game.log" || bad "the log's first line is not the scratch path: $(head -1 "$scratch/game.log")"
# No log of either run (nor Arkeyan-Builder's headless run of the game) names a real folder.
leaks=$(grep -rlF "$support/" "$scratch/arkeyan-builder.log" "$scratch/run.log" "$scratch/game.log" "$scratch/game/arkeyan-work/movies" 2>/dev/null)
[ -z "$leaks" ] || bad "a log names the real data folder: $leaks"
# Nothing newer than the start in the real folders (listed only).
for name in Skylanders SSARecomp Skylauncher; do
    [ -d "$support/$name" ] || continue
    changed=$(find "$support/$name" -newer "$scratch/start" 2>/dev/null | head -5)
    [ -z "$changed" ] || bad "written in the real $name folder: $(echo "$changed" | tr '\n' ' ')"
done
# Arkeyan-Builder's headless run wrote its log (SKY_LOG) in its work folder.
[ -s "$scratch/game/arkeyan-work/movies/runtime.log" ] || echo "note: no movies/runtime.log: the headless run was skipped, so only the installed game was checked"
if [ $fail -eq 0 ]; then echo "PASS: $game's kit-built game wrote only under $scratch"; else echo "FAIL (logs in $scratch)"; fi
exit $fail
