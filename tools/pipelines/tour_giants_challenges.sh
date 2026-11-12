#!/bin/bash
# Giants' Heroic Challenges played the game's way (no SKY_LEVEL on the challenge): hub, Cali, "Select Challenge", the Nth entry, a minute of play.
# The save has the challenges unlocked by tools/regress/giants_unlock.py; every press goes through SKY_PRESS_FILE (never the real keyboard).
#
#   tools/pipelines/tour_giants_challenges.sh <Giants .iso> <work dir> [<entry index>...]      (default 0..44; one game at a time)
#
# Run 0 makes the scratch save (the hub for ~60 s, SKY_LEVEL only here, to reach the hub), unlocks it, and keeps it as <work>/unlocked.
# Then, per entry k: the hub with that save, a walk to Cali, A through her talk until the select UI opens (the log shows
# misc\ui_heroicchallengeselect), k presses of right, A, and TOUR_SECONDS (default 60) of attacks. Writes <work>/challenges.tsv:
# entry, challenge level, base mesh, faults, last frame s, median fps (tools/pipelines/tour_summary.py).
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
iso="$1" work="$2"; shift 2
exe="${TOUR_EXE:-$root/build-giants/ArkeyanRuntime/Skylanders Giants}"
secs=${TOUR_SECONDS:-60}
mkdir -p "$work/runs"
[ $# -gt 0 ] && entries="$*" || entries=$(seq 0 44)

start() # <dir> <save to copy or ''> [ENV=VAL...]  (starts the game, sets $pid)
{
    local dir=$1 save=$2; shift 2
    rm -rf "${dir:?}"; mkdir -p "$dir/figures" "$dir/save"; : > "$dir/press.txt"
    cp "$root/private/figures/new_14.sky" "$dir/figures/"
    [ -n "$save" ] && cp -R "$save"/. "$dir/save/"
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA="$dir/data" SKY_SETTINGS="$dir/none.ini" SKY_LAUNCHER_INI="$dir/none.ini" SKY_COLLECTION="$dir/figures" \
        SKY_FIGURES="$dir/figures" SKY_SAVE_DIR="$dir/save" SKY_LOG="$dir/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS_FILE="$dir/press.txt" \
        SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky SKY_FRAME_TIMES="$dir/frames.txt" \
        SKY_PRESS="a:8,a:12,a:16,a:20,a:24,a:28,a:32,a:36" \
        SKY_GPU_SNAPSHOT="$dir/shot" SKY_GPU_SNAPSHOT_FROM=1800 SKY_GPU_SNAPSHOT_EVERY=600 "$@" \
        nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$dir.log" 2>&1 &
    pid=$!
}
stop() { kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null; wait "$pid" 2>/dev/null; }
press() { echo "$1" >> "$dir/press.txt"; sleep "${2:-2}"; }
seen() { grep -q -F "$1" "$dir/game.log" 2>/dev/null; }   # the log has the string
waitfor() { local n=$2; while ! seen "$1" && (( n-- > 0 )); do sleep 1; done; seen "$1"; }

if [ ! -d "$work/unlocked" ]; then
    dir="$work/runs/make"; start "$dir" "" SKY_LEVEL=level_hub_2_stage_1
    waitfor 'level_hub_2_stage_1.bld' 120; sleep 70; stop
    cp -R "$dir/save" "$work/unlocked"; rm -f "$work/unlocked/pipelines.bin" "$work/unlocked/portal.txt"
    python3 -I "$root/tools/regress/giants_unlock.py" "$work/unlocked"
fi

for k in $entries; do
    dir="$work/runs/entry_$k"; start "$dir" "$work/unlocked" SKY_LEVEL=level_hub_2_stage_1
    waitfor 'level_hub_2_stage_1.bld' 120; sleep 28           # the deck is playable
    press lup:1.5 5; press lright:0.4 3; press lup:2.5 5; press lright:1.6 3; press lup:0.5 3; press a 3
    press lleft:0.4 2; press ldown:0.3 2; press lright:1.2 3; press y 5
    n=0; while ! seen 'ui_heroicchallengeselect.bld' && (( n++ < 15 )); do press a 3; done   # Cali's talk, then the select UI
    sleep 6
    for ((i = 0; i < k; i++)); do press right 1.2; done
    press a 1
    waitfor 'challengemesh' 40; sleep "$secs"
    stop
    python3 -I "$root/tools/pipelines/tour_summary.py" "entry_$k" "$dir/game.log" "$dir/frames.txt" "$secs" >> "$work/challenges.tsv"
    lvl=$(grep -io '\\level\\challenge_level_[0-9]*\.arc' "$dir/game.log" | tail -1 | sed 's/.*level\\//')
    mesh=$(grep -io '\\level\\challengemesh_[a-z0-9]*\.bld' "$dir/game.log" | tail -1 | sed 's/.*level\\//')
    echo "entry $k: ${lvl:-none} ${mesh:-none}" | tee -a "$work/challenges.log"; tail -1 "$work/challenges.tsv"
done
