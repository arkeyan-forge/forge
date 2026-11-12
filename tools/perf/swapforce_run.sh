#!/bin/sh
# One headless, isolated Swap Force run into a zone, with per-frame times (SKY_FRAME_TIMES) and the [perf] lines
# ([SwapForce]; docs/runtime/gpu/renderer_threads.md): the same as tools/perf/giants_run.sh for Swap Force. A new game,
# SKY_LEVEL=<zone> (engines/alchemy/zone.cpp), Eruptor on the emulated portal, then the zone tour's presses
# (tools/pipelines/tour_swapforce.sh: A every 2 s from 14 to 22 s, then walking, attacking, jumping every 4 s).
#
#   tools/perf/swapforce_run.sh <game executable> <out dir> [scale, default 2] [zone] [args...]
#
# The zone defaults to barrens/brn2/brn2 (the first Barrens level).
#
# Writes <out dir>/frames.txt, game.log, load.txt (the load average at the start and the end), ps_from.txt and
# ps_to.txt, then prints tools/perf/load_times.py (the loads), tools/perf/frames.py's summary over the window and the
# CPU time per frame of the process and its busiest threads (tools/perf/thread_cpu.py). Work times are wall times:
# compare same-time pairs.
# Env: SWAPFORCE_ISO (the disc, read only), SECONDS_TO_RUN (default 75), FPS_LIMIT (default 30), WINDOW="<from s> <to s>"
# (default 45 to 10 s before the end), SAMPLE=1 (macOS `sample` for SAMPLE_SECONDS, default 10, from SAMPLE_AT, default
# the window's end), FIGURE (SKY_PORTAL_PLACE, default new:9; new:2004,new:1004 is the swapper Blast Zone).
set -e
exe=$1; out=$2; scale=${3:-2}; zone=${4:-barrens/brn2/brn2}
shift 2; [ $# -gt 0 ] && shift; [ $# -gt 0 ] && shift
root=$(cd "$(dirname "$0")/../.." && pwd)
iso=${SWAPFORCE_ISO:?set SWAPFORCE_ISO to the Swap Force disc image}
seconds=${SECONDS_TO_RUN:-75}
press=$(awk -v end="$seconds" -v menus=22 'BEGIN {
    for (t = 14; t <= menus && t < end; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t
    split("lup lleft ldown lright", d, " ")
    for (t = menus + 2; t < end - 2; t += 4) printf ",%s:%g:2.5,x:%g,x:%g,y:%g,a:%g", d[int(t / 4) % 4 + 1], t + 0.3, t + 0.6, t + 1.2, t + 1.8, t + 3 }')
window=${WINDOW:-"45 $((seconds - 10))"}
from=${window% *}; to=${window#* }
rm -rf "${out:?}"; mkdir -p "$out/save" "$out/collection" "$out/data"
uptime | sed 's/.*load averages*: /start /' > "$out/load.txt"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKYLANDERS_DATA="$out/data" SKY_SETTINGS="$out/none.ini" SKY_LAUNCHER_INI="$out/none.ini" SKY_COLLECTION="$out/collection" \
    SKY_SAVE_DIR="$out/save" SKY_LOG="$out/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated \
    SKY_PORTAL_PLACE="${FIGURE:-new:9}" SKY_FRAME_TIMES="$out/frames.txt" SKY_LEVEL="$zone" SKY_PERF_LOG=1 \
    nice "$exe" --scale "$scale" --vsync 0 --fps-limit "${FPS_LIMIT:-30}" "$@" "$iso" > "$out/stdout.log" 2>&1 & pid=$!
(sleep "$from"; ps -M -p $pid > "$out/ps_from.txt"; sleep $((to - from)); ps -M -p $pid > "$out/ps_to.txt") 2>/dev/null &
if [ "${SAMPLE:-0}" = 1 ]; then (sleep "${SAMPLE_AT:-$to}"; sample $pid "${SAMPLE_SECONDS:-10}" -file "$out/sample.txt" > /dev/null 2>&1) & fi
(sleep "$seconds"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) & wait $pid || true
uptime | sed 's/.*load averages*: /end /' >> "$out/load.txt"
python3 -I "$root/tools/perf/load_times.py" "$out/frames.txt" "$out/game.log" --file "\\${zone##*/}.pak" || true
python3 -I "$root/tools/perf/frames.py" "$out/frames.txt" --from "$from" --to "$to"
python3 -I "$root/tools/perf/thread_cpu.py" "$out/ps_from.txt" "$out/ps_to.txt" "$out/frames.txt" "$from" "$to" || true
tr '\n' ' ' < "$out/load.txt"; echo
