#!/usr/bin/env bash
# The Giants level tour ([Alchemy] [Giants]; docs/runtime/gpu/pipeline_cache.md): every level of the disc in a headless, silent
# run that starts a new game and plays from there with SKY_LEVEL (the tutorial's files are read from the level's), a figure
# on the emulated portal, walking, attacking and opening the pause menu. One summary line per level, then every run's
# pipelines.bin merged with the current baseline.
# Challenge levels are started alone, without their base mesh and challenge session: some fault or stop, as expected
# (games/giants/docs/history.md, "Challenge levels in the tour").
#
#   tools/pipelines/tour_giants.sh <Giants .iso> <work dir> [<parallel runs, default 1>] [<level name>...]
#
# Needs build-giants/ (or TOUR_EXE, a copy of Skylanders Giants, so that a rebuild does not disturb the tour). TOUR_FIGURE: what the
# portal holds (SKY_PORTAL_PLACE: default new_14.sky, or new:<toy type>[:<variant hex>]); another figure's attacks and
# effects add render states the first tour did not see.
# Each run is ~80 s with its own save folder <work dir>/runs/<level>, log and frame times next to
# it. Writes <work dir>/summary.tsv (level, opened, draws-time builds, faults, last frame s, median fps, max draws) and
# <work dir>/merged.bin; then: zstd -19 --rm <work dir>/merged.bin -o games/giants/pipelines/baseline.zst
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
iso="$1" work="$2" parallel="${3:-1}"; shift $(( $# < 3 ? $# : 3 ))
exe="${TOUR_EXE:-$root/build-giants/ArkeyanRuntime/Skylanders Giants}"
seconds=${TOUR_SECONDS:-80}
mkdir -p "$work/runs"

# The presses (tools/regress/giants_press.py): the menus, then walking, attacking and the pause menu.
press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds")

# Every level (the .arc of level\), without the title, the voice-over and mesh packs, and the arena challenges' copies
# (one arc, three different .bld: _000 of each).
if [ $# -gt 0 ]; then levels="$*"; else
levels=$(python3 -I "$root/tools/shaders/xdvdfs.py" "$iso" | grep -io '/level/[a-z0-9_]*\.arc' | cut -d/ -f3 | sed 's/\.arc$//I' \
    | tr 'A-Z' 'a-z' | grep -v -e '^title$' -e '^challengevo$' -e '^challengemesh' -e '^arena_challenge_.*_00[1-9]$')
fi

run() # <level>
{
    local dir="$work/runs/$1"
    rm -rf "$dir"; mkdir -p "$dir/figures" "$dir/save"; cp "$root/private/figures/new_14.sky" "$dir/figures/"
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA="$dir/data" SKY_SETTINGS="$dir/none.ini" SKY_LAUNCHER_INI="$dir/none.ini" SKY_COLLECTION="$dir/figures" \
        SKY_FIGURES="$dir/figures" SKY_SAVE_DIR="$dir/save" SKY_LOG="$dir/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" \
        SKY_PORTAL=emulated SKY_PORTAL_PLACE="${TOUR_FIGURE:-new_14.sky}" SKY_FRAME_TIMES="$dir/frames.txt" SKY_LEVEL="$1" \
        SKY_GPU_SNAPSHOT="$dir/shot" SKY_GPU_SNAPSHOT_FROM=3600 SKY_GPU_SNAPSHOT_EVERY=1200 \
        nice "$exe" --vsync 0 --fps-limit 60 "$iso" > "$dir.log" 2>&1 & local pid=$!
    (sleep "$seconds"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) & wait $pid || true
    python3 -I "$root/tools/pipelines/tour_summary.py" "$1" "$dir/game.log" "$dir/frames.txt" "$seconds" >> "$work/summary.tsv"
    tail -1 "$work/summary.tsv"
}

i=0
for level in $levels; do
    run "$level" &
    i=$((i + 1))
    if (( i % parallel == 0 )); then wait; fi
done
wait

lists=()
[[ -f "$root/games/giants/pipelines/baseline.zst" ]] && lists+=("$root/games/giants/pipelines/baseline.zst")
for level in $levels; do [[ -f "$work/runs/$level/save/pipelines.bin" ]] && lists+=("$work/runs/$level/save/pipelines.bin"); done
python3 -I "$root/tools/pipelines/merge_lists.py" "$work/merged.bin" "${lists[@]}"
