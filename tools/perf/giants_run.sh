#!/bin/sh
# One headless, isolated Giants run into a level, with per-frame times (SKY_FRAME_TIMES) and the [perf] lines
# ([Giants]; docs/runtime/gpu/renderer_threads.md). A new game, SKY_LEVEL=<level> (the tutorial's files are read from the
# level's), an SSA figure on the emulated portal, then the regression harness's presses (tools/regress/giants_press.py:
# the menus, then walking, attacking and the pause menu in the last 20 s).
#
#   tools/perf/giants_run.sh <game executable> <out dir> [scale, default 2] [level, default level_213_tutotial] [args...]
#
# Level `title`: no presses and no SKY_LEVEL, the game stays on its title (MOVIES=1: after the intro movies).
# Writes <out dir>/frames.txt, <out dir>/game.log and <out dir>/load.txt (the load average at the start and the end),
# then prints the loads (tools/perf/load_times.py, with the time from the level's .arc to the level), frames.py's
# summary over the play window (from 50 s to 10 s before the pause menu) and the CPU
# time per frame of the process and its busiest threads over the same window (tools/perf/thread_cpu.py, from `ps -M`
# at the window's ends: <out dir>/ps_from.txt, ps_to.txt). Work times are wall times: other processes' load inflates
# them (Giants' render thread: 8-9 ms at load 3-5, 16-20 ms at load 20-50, 2026-10-27); compare same-time pairs.
# Env: GIANTS_ISO (the disc, read only), SECONDS_TO_RUN (default 100), FPS_LIMIT (default 30), SAMPLE=1 (also runs
# macOS `sample` on the game for 10 s from the window's end: <out dir>/sample.txt; SAMPLE_AT=<s> and SAMPLE_SECONDS=<s>
# move it, e.g. onto a level's first seconds; a sample inside the window adds 2-5 ms to each frame's work), FIGURE (SKY_PORTAL_PLACE: default new_14.sky, or new:<toy type>[:<variant hex>]), MOVIES=1 (the Bink movies play: no SKY_SKIP_MOVIES;
# SKY_BINK_LOG=1 logs their pace), WINDOW="<from s> <to s>" (the summary's window).
set -e
exe=$1; out=$2; scale=${3:-2}; level=${4:-level_213_tutotial}
shift 2; [ $# -gt 0 ] && shift; [ $# -gt 0 ] && shift
root=$(cd "$(dirname "$0")/../.." && pwd)
iso=${GIANTS_ISO:?set GIANTS_ISO to the Giants disc image}
seconds=${SECONDS_TO_RUN:-100}
press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds")
level_env="SKY_LEVEL=$level"
[ "$level" = title ] && { press=""; level_env=""; }
skip_env="SKY_SKIP_MOVIES=1"
[ "${MOVIES:-0}" = 1 ] && skip_env="SKY_BINK_LOG=1"
window=${WINDOW:-"50 $((seconds - 30))"}
from=${window% *}; to=${window#* }
rm -rf "${out:?}"; mkdir -p "$out/save" "$out/figures" "$out/data"
cp "$root/private/figures/new_14.sky" "$out/figures/"
uptime | sed 's/.*load averages*: /start /' > "$out/load.txt"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKYLANDERS_DATA="$out/data" SKY_SETTINGS="$out/none.ini" SKY_LAUNCHER_INI="$out/none.ini" SKY_COLLECTION="$out/figures" \
    SKY_FIGURES="$out/figures" SKY_SAVE_DIR="$out/save" SKY_LOG="$out/game.log" $skip_env SKY_PRESS="$press" \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE="${FIGURE:-new_14.sky}" SKY_FRAME_TIMES="$out/frames.txt" ${level_env:+"$level_env"} SKY_PERF_LOG=1 \
    nice "$exe" --scale "$scale" --vsync 0 --fps-limit "${FPS_LIMIT:-30}" "$@" "$iso" > "$out/stdout.log" 2>&1 & pid=$!
(sleep "$from"; ps -M -p $pid > "$out/ps_from.txt"; sleep $((to - from)); ps -M -p $pid > "$out/ps_to.txt") 2>/dev/null &
if [ "${SAMPLE:-0}" = 1 ]; then (sleep "${SAMPLE_AT:-$to}"; sample $pid "${SAMPLE_SECONDS:-10}" -file "$out/sample.txt" > /dev/null 2>&1) & fi
(sleep "$seconds"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) & wait $pid || true
uptime | sed 's/.*load averages*: /end /' >> "$out/load.txt"
[ "$level" = title ] || python3 -I "$root/tools/perf/load_times.py" "$out/frames.txt" "$out/game.log" --file "$level.arc" || true
python3 -I "$root/tools/perf/frames.py" "$out/frames.txt" --from "$from" --to "$to"
python3 -I "$root/tools/perf/thread_cpu.py" "$out/ps_from.txt" "$out/ps_to.txt" "$out/frames.txt" "$from" "$to" || true
tr '\n' ' ' < "$out/load.txt"; echo
