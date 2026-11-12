#!/usr/bin/env bash
# Trap Team's disc-driven level tour: Giants-style .arc discovery, sequential runs and Swap Force-style summaries.
#   tools/pipelines/tour_trapteam.sh <disc .iso> <work dir> [<level> ...]
#   tools/pipelines/tour_trapteam.sh --list <disc .iso>
# Also accepts --list after <work dir>; listing only reads disc directories, with no build or extraction needed.
# Work must be below private/. TOUR_EXE overrides Skylanders Trap Team (including its macOS app path).
# TOUR_SECONDS (default 130), TOUR_FIGURE (default new:14; SKY_PORTAL_PLACE's list, absolute dump paths too), TOUR_PRESS
# (more SKY_PRESS presses, e.g. lt:60 to play a trap's villain). Every run has isolated settings, collection, saves,
# data, log and frame times; one game at a time, `nice`. summary.tsv uses tour_summary.py's existing columns;
# runs/ retains logs, frames and pipeline lists; merged.bin merges only lists actually written by these runs.
# SKY_LEVEL needs Trap Team runtime support; a requested .arc that never opens is reported as not reached.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
inputs=$root/tools/regress/trapteam.py
if [ "${1:-}" = --list ]; then
    [ $# -eq 2 ] || { echo "usage: $0 --list <disc .iso>" >&2; exit 2; }
    exec python3 -I "$inputs" levels "$2"
fi
[ $# -ge 2 ] || { sed -n '2,6p' "$0"; exit 2; }
iso=$1; work=$2; shift 2
if [ "${1:-}" = --list ]; then
    [ $# -eq 1 ] || exit 2
    exec python3 -I "$inputs" levels "$iso"
fi
work=$(python3 -I "$inputs" work "$work")
# Always discover at run time, including when the caller requests a subset.
available=$(python3 -I "$inputs" levels "$iso")
levels=$available
if [ $# -gt 0 ]; then
    levels="$*"
    for level in "$@"; do
        printf '%s\n' "$available" | grep -qxF -- "$level" || { echo "level not on disc: $level" >&2; exit 2; }
    done
fi
exe="${TOUR_EXE:-$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team}"
if [ ! -x "$exe" ] && [ -z "${TOUR_EXE:-}" ]; then
    exe="$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team"
fi
[ -x "$exe" ] || { echo "build Skylanders Trap Team first ($exe)" >&2; exit 2; }
seconds=${TOUR_SECONDS:-130}
[[ "$seconds" =~ ^[0-9]+$ ]] && [ "$seconds" -ge 8 ] || { echo "TOUR_SECONDS must be an integer >= 8" >&2; exit 2; }
# Reuse the established walking/attack/pause recipe; timings remain provisional until a game run validates them.
press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds")${TOUR_PRESS:+,$TOUR_PRESS}
mkdir -p "$work/runs"
: > "$work/summary.tsv"
rm -f "$work/merged.bin"
pid=""
stop() {
    if [ -n "$pid" ]; then
        kill "$pid" 2>/dev/null || true
        sleep 3
        kill -9 "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        pid=""
    fi
}
trap stop EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
lists=()
for level in $levels; do
    dir=$work/runs/$level
    rm -rf "${dir:?}"; mkdir -p "$dir/collection" "$dir/save"
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA="$dir/data" SKY_SETTINGS="$dir/none.ini" SKY_LAUNCHER_INI="$dir/none.ini" \
        SKY_COLLECTION="$dir/collection" SKY_FIGURES="$dir/collection" SKY_SAVE_DIR="$dir/save" \
        SKY_LOG="$dir/game.log" SKY_FRAME_TIMES="$dir/frames.txt" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" \
        SKY_PORTAL=emulated SKY_PORTAL_PLACE="${TOUR_FIGURE:-new:14}" SKY_LEVEL="$level" \
        SKY_GPU_SNAPSHOT="$dir/shot" SKY_GPU_SNAPSHOT_FROM=1000000000 SKY_GPU_SNAPSHOT_SECONDS=$((seconds - 4)) \
        nice "$exe" --vsync 0 --fps-limit 60 "$iso" > "$dir/log" 2>&1 & pid=$!
    elapsed=0
    while kill -0 "$pid" 2>/dev/null && [ "$elapsed" -lt "$seconds" ]; do
        sleep 1; elapsed=$((elapsed + 1))
    done
    stop
    # A missing log is an unsuccessful launch, not a reason to lose all remaining summary rows.
    touch "$dir/game.log"
    python3 -I "$root/tools/pipelines/tour_summary.py" "$level" "$dir/game.log" "$dir/frames.txt" "$seconds" \
        "open \\level\\$level.arc -> 0x00000000" "$dir/shot_t$((seconds - 4)).ppm" | tee -a "$work/summary.tsv"
    [ ! -f "$dir/save/pipelines.bin" ] || lists+=("$dir/save/pipelines.bin")
done
if [ "${#lists[@]}" -gt 0 ]; then
    python3 -I "$root/tools/pipelines/merge_lists.py" "$work/merged.bin" "${lists[@]+"${lists[@]}"}"
else
    echo 'no pipeline lists written; summary.tsv records the unsuccessful runs'
fi
