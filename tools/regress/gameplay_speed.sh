#!/bin/bash
# Trap Team's gameplay speed at a frame rate: the first level (the tutorial) after its intro cutscene, the left stick held to the right
# for 2 s from the same spot, then one jump (A). One run, offscreen, vsync off. The pictures are taken at fixed wall-clock seconds
# (SKY_GPU_SNAPSHOT_SECONDS), so the same moments at any rate: tools/regress/gameplay_speed.py compares two runs' pictures (the walk)
# and measures the jump's airtime from the pictures.
#
#   [SNAP_STEP=0.1] [ONLY_AT=101.2] [EXTRA="SKY_NO_FPS_CAP=1"] tools/regress/gameplay_speed.sh <disc image> <fps_limit> <run name> [seconds, default 112]
#   Results in private/regress/gameplay-<run name>/ ; the last line: the jump's airtime.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
iso=${1:-}; fps=${2:-}; name=${3:-}; seconds=${4:-112}
[ -f "$iso" ] && [ -n "$fps" ] && [ -n "$name" ] || { sed -n '2,9p' "$0"; exit 2; }
exe="$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
r=$root/private/regress/gameplay-$name
rm -rf "${r:?}"; mkdir -p "$r/save" "$r/figures" "$r/snap"
walk=80.5; jump=100.5
press=$(awk -v w="$walk" -v j="$jump" 'BEGIN { for (t = 14; t <= 50; t += 2) printf "a:%d,", t; printf "lright:%.1f:2,a:%.1f:0.15", w, j }')
step=${SNAP_STEP:-0.1}   # each picture stalls a frame or two: a wider step disturbs the jump less
snaps=$(awk -v w="$walk" -v j="$jump" -v s="$step" 'BEGIN { printf "%.1f", w - 1; for (d = 0.5; d <= 6; d += 0.5) printf ",%.1f", w + d;
    printf ",%.1f", j - 0.3; for (d = 0; d <= 2.35; d += s) printf ",%.2f", j + d }')
[ -n "${ONLY_AT:-}" ] && snaps=$ONLY_AT   # one picture in the run: nothing before it is stalled
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKYLANDERS_DATA="$r/data" \
    SKY_SETTINGS="$r/none.ini" SKY_LAUNCHER_INI="$r/none.ini" SKY_COLLECTION="$r/figures" SKY_FIGURES="$r/figures" \
    SKY_SAVE_DIR="$r/save" SKY_LOG="$r/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated \
    SKY_PORTAL_PLACE=new:450 SKY_LEVEL=level_313_tutorial SKY_LAUNCHER_NO_UPDATE_CHECK=1 \
    ${EXTRA:-} SKY_GPU_SNAPSHOT="$r/snap/s" SKY_GPU_SNAPSHOT_EVERY=1000000 SKY_GPU_SNAPSHOT_SECONDS="$snaps" \
    nice "$exe" --vsync 0 --fps-limit "$fps" "$iso" > "$r/log" 2>&1 &
pid=$!
for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
kill "$pid" 2>/dev/null || true; sleep 3; kill -9 "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true
python3 "$root/tools/regress/gameplay_speed.py" jump "$r/snap" "$fps" "$name" "$jump"
