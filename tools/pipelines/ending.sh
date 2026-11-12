#!/usr/bin/env bash
# The end credits of a game, played in isolation ([Lux] [SSA], [Alchemy] [Giants], [Alchemy] [SwapForce]).
#
#   tools/pipelines/ending.sh <ssa|giants|swapforce> <disc .iso> <work dir>
#
# SSA: tools/pipelines/ssa_ending.sh (the start-up movies are swapped for the ending and the credits roll with
# SKY_MOVIE_SWAP; ~11 min, ENDING_SECONDS).
# Giants: the disc has no credits movie (movies\ holds the logos and the story movies); its credits are the
# Credits_Giants level. The run loads that level with SKY_LEVEL and plays it for ENDING_SECONDS (default 180), then asks
# for the level's .arc in the log and no fault.
# Swap Force: the disc has no credits movie by name either (its .bk2 files are story, zone and Soul Gem movies;
# SW_GrandFinale is the finale, not a credits roll), so this stops with exit 2 and says so: no movie is guessed.
# A reason is written to <work dir>/ending.txt; the game log and frame times stay next to it.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; work=${3:-}
[ -n "$game" ] && [ -f "$iso" ] && [ -n "$work" ] || { sed -n '2,13p' "$0"; exit 2; }
mkdir -p "$work"; work=$(cd "$work" && pwd)
marker=$work/.ending-work
if [ ! -f "$marker" ] || [ -L "$marker" ]; then
    contents=$(ls -A "$work")
    [ -z "$contents" ] || { echo "refusing non-empty work directory without an ending.sh marker" >&2; exit 2; }
fi
: > "$marker"
seconds=${ENDING_SECONDS:-180}

case $game in
    ssa)
        # ssa_ending.sh makes the work folder itself and keeps its log there.
        status=0
        "$root/tools/pipelines/ssa_ending.sh" "$iso" "$work" || status=$?
        : > "$marker"  # The helper recreates the directory, including on a failed run.
        [ "$status" -eq 0 ] || exit "$status"
        log=$work/game.log; frames=$work/frames.txt
        reason="SSA: the ending and the credits roll played to their end"
        fault=0
        grep -aq "\[movie\] legal ended after" "$log" 2>/dev/null && grep -aq "\[movie\] atvilogo ended after" "$log" 2>/dev/null || fault=1
        if grep -aEq "Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed" "$log" 2>/dev/null; then
            fault=1
        fi
        if [ $fault -ne 0 ]; then
            verdict=FAIL
            reason="SSA: the ending or the credits did not end (see game.log)"
        else
            verdict=PASS
        fi ;;
    giants)
        exe="${TOUR_EXE:-$root/build-giants/ArkeyanRuntime/Skylanders Giants}"
        [ -x "$exe" ] || { echo "build Giants first ($exe)"; exit 2; }
        rm -rf "${work:?}"; mkdir -p "$work/save" "$work/figures" "$work/shot"
        : > "$marker"
        [ -f "$root/private/figures/new_14.sky" ] && cp "$root/private/figures/new_14.sky" "$work/figures/"
        press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds")
        env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
            SKYLANDERS_DATA="$work/data" SKY_SETTINGS="$work/none.ini" SKY_LAUNCHER_INI="$work/none.ini" \
            SKY_COLLECTION="$work/figures" SKY_FIGURES="$work/figures" SKY_SAVE_DIR="$work/save" \
            SKY_LOG="$work/game.log" SKY_FRAME_TIMES="$work/frames.txt" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" \
            SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky SKY_LEVEL=credits_giants \
            nice "$exe" --vsync 0 --fps-limit 60 "$iso" > "$work/log" 2>&1 & pid=$!
        elapsed=0
        while kill -0 $pid 2>/dev/null && [ $elapsed -lt "$seconds" ]; do sleep 1; elapsed=$((elapsed + 1)); done
        if kill -0 $pid 2>/dev/null; then kill $pid; sleep 3; kill -9 $pid 2>/dev/null || true; fi
        wait $pid 2>/dev/null || true
        log=$work/game.log; frames=$work/frames.txt
        verdict=PASS
        reason="Giants: the Credits_Giants level played (no credits movie on the disc)"
        if ! grep -aqF 'open \level\credits_giants.arc -> 0x00000000' "$log" 2>/dev/null; then
            verdict=FAIL; reason="Giants: the Credits_Giants level did not open"
        fi
        if grep -aEq "Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed" "$log" 2>/dev/null; then
            verdict=FAIL; reason="Giants: a fault in the Credits_Giants level"
        fi ;;
    swapforce)
        verdict=SKIP
        reason="Swap Force: no credits movie on the disc (nothing was guessed)" ;;
    *) echo "unknown game: $game (ssa, giants or swapforce)"; exit 2 ;;
esac

printf '%s\n' "$reason" > "$work/ending.txt"
if [ "$verdict" = PASS ]; then
    last=$(python3 -I - "$frames" <<'PY' 2>/dev/null || true
import sys
rows = [tuple(float(x) for x in l.split()) for l in open(sys.argv[1]) if len(l.split()) == 3]
print(f"{rows[-1][0] / 1000:.0f}" if rows else "0")
PY
)
    echo "ending: PASS (${last:-0} s of frames) — $reason"
    exit 0
elif [ "$verdict" = SKIP ]; then
    echo "ending: SKIP — $reason"
    exit 2
else
    echo "ending: FAIL — $reason"
    exit 1
fi
