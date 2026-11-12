#!/bin/sh
# One headless fast-recipe run (docs/getting-started/testing.md) into a level, with per-frame times (SKY_FRAME_TIMES).
# usage: tools/perf/level_run.sh <game executable> <out dir> [scale] [level] [extra game arguments...]
# Writes <out dir>/frames.txt and <out dir>/log, then prints the loads (tools/perf/load_times.py, with the time from the
# level's mission.str to the level) and tools/perf/frames.py's summary. No window, no sound, a new
# save directory per run, and no settings but the arguments' (SKY_SETTINGS: the player's launcher.ini is not read); the
# data folder, log and collection are in <out dir> too (the player's folders are never touched).
# Env: ISO (default: the usual disc image), SECONDS_TO_RUN (default 90).
set -e
exe=$1; out=$2; scale=${3:-1}; level=${4:-level_027}
shift 2; [ $# -gt 0 ] && shift; [ $# -gt 0 ] && shift
root=$(cd "$(dirname "$0")/../.." && pwd)
ISO=${ISO:?set ISO to your disc image}
PRESS="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60"
rm -rf "${out:?}"; mkdir -p "$out/save" "$out/figures" "$out/data"
cp "$root/private/figures/new_14.sky" "$out/figures/"
SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKYLANDERS_DATA="$out/data" SKY_LOG="$out/game.log" \
SKY_SAVE_DIR="$out/save" SKY_SETTINGS="$out/settings.ini" SKY_LAUNCHER_INI="$out/settings.ini" SKY_PERF_LOG=1 SKY_SKIP_MOVIES=1 SKY_PRESS="$PRESS" SKY_LEVEL=$level SKY_RENDER_STATS=1 \
SKY_FRAME_TIMES="$out/frames.txt" SKY_PORTAL=emulated SKY_COLLECTION="$out/figures" SKY_FIGURES="$out/figures" SKY_PORTAL_PLACE=new_14.sky \
nice "$exe" --scale "$scale" --vsync 0 --fps-limit 30 --anisotropy 4 "$@" "$ISO" > "$out/log" 2>&1 & pid=$!
(sleep "${SECONDS_TO_RUN:-90}"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) & wait $pid || true
python3 -I "$root/tools/perf/load_times.py" "$out/frames.txt" "$out/game.log" --file "$level\\mission.str" || true
python3 "$root/tools/perf/frames.py" "$out/frames.txt"
