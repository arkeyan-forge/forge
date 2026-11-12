#!/bin/bash
# A Giants run steered from outside, for the paths that need a person's eyes (the real challenge flow, docs/runtime/xbox360/input.md):
# the runtime reads lines appended to $R/press.txt as presses (SKY_PRESS_FILE: "a", "lup:2", "2 start" for the second pad), and
# writes a frame every 2 s as $R/shot_*.ppm to look at. Isolated like every run; one instance at a time.
#
#   FIG="gold_14.sky" PLACE=gold_14.sky tools/regress/live_run.sh <name> <seconds> [ENV=VAL ...]   (GIANTS_ISO set)
# The figure dumps come from private/figures/ (ArkeyanFigureTest --make <dump> <type> gold=65000 makes one). Kill the run with the
# pid in $R/pid. Everything is under private/runs/<name>.
# Another game: LIVE_EXE (default build-giants/ArkeyanRuntime/Skylanders Giants) and LIVE_ISO (default $GIANTS_ISO), e.g. Swap Force:
#   LIVE_EXE="build-swapforce/ArkeyanRuntime/Skylanders Swap Force" LIVE_ISO="$SWAPFORCE_ISO" PLACE=new:9 tools/regress/live_run.sh <name> <seconds>
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=$1 secs=$2; shift 2
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R/figures" "$R/save"; : > "$R/press.txt"
for f in ${FIG:-new_14.sky}; do cp "$root/private/figures/$f" "$R/figures/"; done
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
  SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" SKY_COLLECTION="$R/figures" \
  SKY_FIGURES="$R/figures" SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS_FILE="$R/press.txt" \
  SKY_PORTAL=emulated SKY_PORTAL_PLACE="${PLACE:-new_14.sky}" \
  SKY_GPU_SNAPSHOT="$R/shot" SKY_GPU_SNAPSHOT_FROM=0 SKY_GPU_SNAPSHOT_EVERY=60 "$@" \
  nice "${LIVE_EXE:-$root/build-giants/ArkeyanRuntime/Skylanders Giants}" --vsync 0 --fps-limit 30 "${LIVE_ISO:-$GIANTS_ISO}" > "$R/log" 2>&1 &
pid=$!; echo $pid > "$R/pid"
(sleep "$secs"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) &
wait $pid
