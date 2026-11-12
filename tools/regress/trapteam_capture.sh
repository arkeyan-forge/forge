#!/bin/bash
# Trap Team: a villain captured in play, then kept across a restart ([TrapTeam]; games/trapteam/docs/boot.md, "The first capture").
# Gusto and an empty Life Trap on the emulated portal; the recorded route (trapteam_capture.route, presses timed from
# the moment the tutorial level opens) plays level_313_tutorial to Sheep Creep, beats it, answers the capture and plays
# the villain (LT). A second launch on the same collection and save puts the same figures back (portal.txt) and the
# game loads the held villain again.
#
#   TRAPTEAM_ISO=<Trap Team .iso> tools/regress/trapteam_capture.sh [<run name>]     (about 25 min; build-trapteam/)
# The replay can drift (a missed jump leaves the hero against a wall): a FAIL with toybox_end.png away from Sheep Creep
# says nothing about the capture; run it again. It is a tool, not a gate (1 pass of 4: 2026-11-02 and 03).
#
# The route was recorded live at 30 fps; it is played on the game's frame clock, so a busy machine that drops frames
# slows the route with the game. A run that still drifts (a missed jump, a longer fight) fails the capture checks:
# look at private/runs/<name>/capture/shots and toybox_end.png. Each launch goes through ../gamelock.sh when it
# exists, isolated like every run (docs/getting-started/testing.md): scratch collection, save, data, settings;
# offscreen video, disk audio; kills only its own pid. Everything is under private/runs/<name>. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)

# --drive <dir> <route> <pid>: the presses of one launch (run in the foreground of the locked shell). They go through the
# Toy Box script on the game's frame clock (SKY_TOYBOX_CLOCK=frames): A every 3 s until the tutorial level opens, then
# the whole route at once, each press timed from that moment ("+<s> pad=..."; LT and RT are the keys 1 and 3), then a
# picture that marks the end.
if [ "${1:-}" = "--drive" ]; then
    R=$2 route=$3 pid=$4
    n=0
    while kill -0 "$pid" 2> /dev/null && ! grep -aqF '450_typhoon.arc' "$R/game.log" 2> /dev/null; do
        [ $((n % 15)) = 0 ] && echo "pad=a" >> "$R/toybox.txt"
        n=$((n + 1))
        sleep 0.2
    done
    awk '!/^#/ && NF > 1 {
        for (i = 2; i <= NF; i++) {
            n = split($i, p, ":"); hold = n > 1 ? p[2] : 0.15
            if (p[1] == "lt") printf "+%s hold=1*%s\n", $1, hold
            else if (p[1] == "rt") printf "+%s hold=3*%s\n", $1, hold
            else printf "+%s pad=%s*%s\n", $1, p[1], hold
        }
        end = $1 } END { printf "+%s shot=end\n", end + 15 }' "$route" >> "$R/toybox.txt"
    while kill -0 "$pid" 2> /dev/null && ! grep -aqF 'script: shot=end' "$R/game.log" 2> /dev/null; do sleep 2; done
    sleep 2
    kill "$pid" 2> /dev/null
    exit 0
fi

name=${1:-trapteam_capture}
iso=${TRAPTEAM_ISO:?set TRAPTEAM_ISO to the Trap Team disc image}
exe="${LIVE_EXE:-$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team}"
figure=${FIGURE_TOOL:-$root/build-trapteam/ArkeyanRuntime/ArkeyanFigureTest}
self=$root/tools/regress/trapteam_capture.sh
R=$root/private/runs/$name
[ -d "$R" ] && { echo "$R exists: give another run name"; exit 2; }
mkdir -p "$R/collection" "$R/save" "$R/data"
trap_dump=$R/collection/new_217_3003.sky

# launch <phase> <seconds> <route> [<place>]: one game, its presses in the foreground, a timer.
launch() {
    local P=$R/$1
    mkdir -p "$P/shots"
    : > "$P/toybox.txt"
    local place=""
    [ -n "${4:-}" ] && place="SKY_PORTAL_PLACE='$4'"
    local run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
      SKY_SAVE_DIR='$R/save' SKY_LOG='$P/game.log' SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_LOG=1 $place \
      SKY_TOYBOX_SCRIPT_FILE='$P/toybox.txt' SKY_TOYBOX_CLOCK=frames SKY_TOYBOX_SHOTS='$P/toybox' \
      SKY_GPU_SNAPSHOT='$P/shots/s' SKY_GPU_SNAPSHOT_EVERY=300 \
      nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$P/log' 2>&1 & pid=\$!; echo \$pid > '$P/pid'
      (exec 3>&-; sleep $2; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      '$self' --drive '$P' '$3' \$pid
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
}

# The capture, then a restart whose route only waits for the level and plays the trap's villain.
launch capture 1800 "$root/tools/regress/trapteam_capture.route" "new:450,new:217:3003"
cp "$trap_dump" "$R/trap_captured.sky" 2> /dev/null
printf '# the restart: the held villain, played (the end picture shows it)\n20 lt:0.5\n' > "$R/restart.route"
launch restart 240 "$R/restart.route"

fails=0
check() { # <what> <log> <fixed text>
    if grep -aqF -- "$3" "$2" 2> /dev/null; then echo "ok:   $1"; else echo "FAIL: $1 (no '$3' in $2)"; fails=$((fails + 1)); fi
}
trap_line() { "$figure" "$1" 2> /dev/null | grep -aF "$1: "; }
check "the speaker was answered" "$R/capture/game.log" "<- M 4D"
captured=$(trap_line "$R/trap_captured.sky")
case $captured in
    *"valid, count 1, selected 20,"*) echo "ok:   the game wrote Sheep Creep (20) into the trap" ;;
    *) echo "FAIL: the trap after the capture: ${captured:-no dump}"; fails=$((fails + 1)) ;;
esac
check "the game loaded the captured villain" "$R/capture/game.log" '\character\1020_sheep.arc'
check "the restart put the trap back" "$R/restart/game.log" "figure(s) back on the portal"
check "the restart loaded the held villain" "$R/restart/game.log" '\character\1020_sheep.arc'
case $(trap_line "$trap_dump") in
    *"selected 20,"*) echo "ok:   the trap still holds Sheep Creep after the restart" ;;
    *) echo "FAIL: the trap after the restart: $(trap_line "$trap_dump")"; fails=$((fails + 1)) ;;
esac
[ "$fails" = 0 ] && echo "Trap Team capture check: PASS" || echo "Trap Team capture check: $fails FAILED (logs in $R)"
[ "$fails" = 0 ]
