#!/usr/bin/env bash
# The Swap Force zone tour ([Alchemy] [SwapForce]; docs/runtime/gpu/pipeline_cache.md, games/swapforce/docs/status.md
# "Zone tour"): every zone of the disc (tools/pipelines/swapforce_zones.py: the CZoneInfo names of zoneinfos.pak whose
# map has a .pak) in a headless, silent run that starts a new game in that zone with SKY_LEVEL (engines/alchemy/zone.cpp),
# Eruptor on the emulated portal, A every 2 s through the title and the menus, then walking, attacking and jumping.
# One summary line per zone, then every run's pipelines.bin merged with the current baseline.
# Two story zones open with a movie played inside their intro cutscene (barrens/brn3, wildwoods/ww1): SKY_SKIP_MOVIES
# cuts it to one frame and the cutscene never ends (a black screen), so they run with the movies on, TOUR_MOVIE_SECONDS
# each (default 240), with A every 2 s to 70 s (the logos play: the title comes ~20 s later, and the walk's Y would open
# the main menu's Legal screen; games/swapforce/docs/history.md, "Zone tour").
#
#   tools/pipelines/tour_swapforce.sh <Swap Force .iso> <work dir> [<zone>...]
#
# Needs build-swapforce/ (or TOUR_EXE, a copy of Skylanders Swap Force, so that a rebuild does not disturb the tour). TOUR_FIGURE:
# what the portal holds instead of Eruptor (SKY_PORTAL_PLACE: new:3003 for Scorp, new:2004,new:1004 for the swapper Blast
# Zone, both halves). One run at a time (one game instance), TOUR_SECONDS each (default 40: ~20 s in the zone). Each
# run has its own folder <work dir>/runs/<zone, / as _> (save, log, frame times; deleted after the run unless
# TOUR_KEEP=1; its pipelines.bin and game.log are kept in <work dir>/lists and logs). Writes <work dir>/summary.tsv (zone, opened (its .pak), draw-time
# builds after it, faults, last frame s, median fps, max draws, verdict, mean brightness 0-255 of the frame 4 s before
# the end: "black" below 6, that frame kept as <work dir>/logs/<zone>.ppm) and <work dir>/merged.bin; then:
#   zstd -19 --rm <work dir>/merged.bin -o games/swapforce/pipelines/baseline.zst
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
iso="$1" work="$2"; shift 2
exe="${TOUR_EXE:-$root/build-swapforce/ArkeyanRuntime/Skylanders Swap Force}"
seconds=${TOUR_SECONDS:-40}
movie_zones=" barrens/brn3/brn3 wildwoods/ww1/ww1 "
mkdir -p "$work/runs" "$work/lists"

# The presses: A every 2 s from 14 s (the title, Story Mode, the save slot, the portal prompt; A only jumps in a zone),
# then from 24 s (a zone plays from ~15-20 s on a quiet machine) every 4 s the left stick in one of four directions,
# X twice (attack), Y and A (which also passes any late prompt).
presses() # <seconds> [<last A of the menus, default 22>]
{
    awk -v end="$1" -v menus="${2:-22}" 'BEGIN {
    for (t = 14; t <= menus && t < end; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t
    split("lup lleft ldown lright", d, " ")
    for (t = menus + 2; t < end - 2; t += 4) printf ",%s:%g:2.5,x:%g,x:%g,y:%g,a:%g", d[int(t / 4) % 4 + 1], t + 0.3, t + 0.6, t + 1.2, t + 1.8, t + 3 }'
}
press=$(presses "$seconds")

if [ $# -gt 0 ]; then zones="$*"; else zones=$(python3 -I "$root/tools/pipelines/swapforce_zones.py" "$iso" "$work/disc"); fi

run() # <zone>: one run, its summary line on stdout
{
    local zone=$1 dir="$work/runs/${1//\//_}" pid timer seconds=$seconds skip=1 press=$press
    if [[ "$movie_zones" == *" $zone "* ]]; then
        seconds=${TOUR_MOVIE_SECONDS:-240} skip=0 press=$(presses "$seconds" 70)
    fi
    rm -rf "$dir"; mkdir -p "$dir/collection" "$dir/save"
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA="$dir/data" SKY_SETTINGS="$dir/none.ini" SKY_LAUNCHER_INI="$dir/none.ini" SKY_COLLECTION="$dir/collection" \
        SKY_SAVE_DIR="$dir/save" SKY_LOG="$dir/game.log" SKY_SKIP_MOVIES=$skip SKY_PRESS="$press" \
        SKY_PORTAL=emulated SKY_PORTAL_PLACE="${TOUR_FIGURE:-new:9}" SKY_FRAME_TIMES="$dir/frames.txt" SKY_LEVEL="$zone" \
        SKY_GPU_SNAPSHOT="$dir/shot" SKY_GPU_SNAPSHOT_FROM=1000000000 SKY_GPU_SNAPSHOT_SECONDS=$((seconds - 4)) \
        nice "$exe" --vsync 0 --fps-limit 60 "$iso" > /dev/null 2>&1 & pid=$!
    (sleep "$seconds"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) > /dev/null 2>&1 & timer=$!
    wait $pid || true; { kill $timer && wait $timer; } 2>/dev/null || true
    python3 -I "$root/tools/pipelines/tour_summary.py" "$zone" "$dir/game.log" "$dir/frames.txt" "$seconds" \
        "open \\archives\\${zone##*/}.pak -> 0x00000000" "$dir/shot_t$((seconds - 4)).ppm"
}

# A run that never opens its zone is run once more (TOUR_RETRY=0: never): new games sometimes stop at the save slots
# (games/swapforce/docs/status.md, "Open problems").
mkdir -p "$work/logs"
for zone in $zones; do
    name=${zone//\//_} dir="$work/runs/${zone//\//_}"
    line=$(run "$zone")
    if [[ "$line" == *$'\t'"no level"$'\t'* && "${TOUR_RETRY:-1}" = 1 ]]; then cp "$dir/game.log" "$work/logs/$name.first.log"; line=$(run "$zone"); fi
    echo "$line" | tee -a "$work/summary.tsv"
    [[ -f "$dir/save/pipelines.bin" ]] && cp "$dir/save/pipelines.bin" "$work/lists/$name.bin"
    [[ "$line" == *$'\t'"black"$'\t'* ]] && cp "$dir/shot_t$((seconds - 4)).ppm" "$work/logs/$name.ppm"
    cp "$dir/game.log" "$work/logs/$name.log" 2>/dev/null || true
    [[ "${TOUR_KEEP:-0}" = 1 ]] || rm -rf "${dir:?}"
done

lists=()
[[ -f "$root/games/swapforce/pipelines/baseline.zst" ]] && lists+=("$root/games/swapforce/pipelines/baseline.zst")
for zone in $zones; do [[ -f "$work/lists/${zone//\//_}.bin" ]] && lists+=("$work/lists/${zone//\//_}.bin"); done
python3 -I "$root/tools/pipelines/merge_lists.py" "$work/merged.bin" "${lists[@]}"
