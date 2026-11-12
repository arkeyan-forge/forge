#!/bin/bash
# SuperChargers: the in-game Toy Box seats a SuperCharger on Player 1 and a vehicle on Vehicle 1, the game takes both in the
# intro (its "place a LAND VEHICLE" moment), and a trophy goes on the item seat ([SuperChargers];
# docs/runtime/host/toybox_ux.md, games/superchargers/docs/toybox.md). Same shape as trapteam_toybox.sh: a scratch collection
# of new dumps, the game on an empty portal, the Toy Box driven by SKY_TOYBOX_SCRIPT, a picture of each screen, the log checked.
#
#   SUPERCHARGERS_ISO=<SuperChargers .iso> tools/regress/superchargers_toybox.sh [<run name>]   (about 4 min; build-superchargers/)
#
# The run goes through ../gamelock.sh when it exists (one game at a time on a shared Mac), isolated like every run
# (docs/getting-started/testing.md): scratch collection, save, data, settings; offscreen video, disk audio; kills only its
# own pid. Everything is under private/runs/<name>. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-superchargers_toybox}
iso=${SUPERCHARGERS_ISO:?set SUPERCHARGERS_ISO to the SuperChargers disc image}
exe="${LIVE_EXE:-$root/build-superchargers/ArkeyanRuntime/Skylanders SuperChargers.app/Contents/MacOS/Skylanders SuperChargers}"
make=${FIGURE_TOOL:-$root/build-superchargers/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/runs/$name
[ -d "$R" ] && { echo "$R exists: give another run name"; exit 2; }
mkdir -p "$R/collection" "$R/save" "$R/data"

# Fiesta, her vehicle Crypt Crusher, and the Sky Trophy: blank tags, as the Toy Box's Add a figure makes them.
for toy in 3400 3227 3500; do
    "$make" --make "$R/collection/new_$toy.sky" "$toy" > /dev/null || { echo "cannot make the dump of $toy"; exit 2; }
done

# The Toy Box opens on Player 1's list: Return seats Fiesta; Page Down goes to Vehicle 1 (Return: Crypt Crusher), then to the
# item seat (Return: the trophy); F2 shows a sheet. The menus take an A every 4 s up to the intro (games/superchargers/docs/boot.md).
script=${SC_TOYBOX_SCRIPT:-"F6:30,shot=01_seats:32,Return:33,shot=02_p1_seated:35,PageDown:36,shot=03_vehicle_choose:38,Return:39,shot=04_vehicle_seated:41,F2:42,shot=05_sheet_vehicle:44,Escape:45,PageDown:46,PageDown:47,PageDown:48,shot=06_item_choose:50,Return:51,shot=07_item_seated:53,Escape:54"}
press=$(awk 'BEGIN { for (t = 60; t <= 236; t += 4) printf "%sa:%d", (t > 60 ? "," : ""), t }')
secs=${SC_SECONDS:-240}
run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
  SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
  SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_PORTAL=emulated SKY_PORTAL_LOG=1 \
  SKY_PRESS='$press' SKY_TOYBOX_SCRIPT='$script' SKY_TOYBOX_SHOTS='$R/shots' \
  nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$R/log' 2>&1 & pid=\$!; echo \$pid > '$R/pid'
  (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
  wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi

log=$R/game.log; [ -f "$log" ] || log=$R/log
fails=0
check() { # <what> <fixed text> [<count>]
    local n; n=$(grep -cF -- "$2" "$log" "$R/log" | awk -F: '{s += $NF} END {print s}')
    if [ "$n" -ge "${3:-1}" ]; then echo "ok:   $1"; else echo "FAIL: $1 (wanted '$2' x${3:-1}, saw $n)"; fails=$((fails + 1)); fi
}
check "the game's badges were read from the disc" "portraits in"
check "3 figures" "[toybox] 3 cards (a swapper is one)"
check "a SuperCharger goes on Player 1" "P1: Fiesta on slot"
check "a vehicle goes on Vehicle 1" "V1: Crypt Crusher on slot"
check "a trophy goes on the item seat" "Magic item: Sky Trophy on slot"
check "the game took the trophy" "Magic item: the game took Sky Trophy"
check "the game took the SuperCharger" "P1: the game took Fiesta"
check "the game took the vehicle" "V1: the game took Crypt Crusher"
shots=$(ls "$R"/shots_*.png 2>/dev/null | wc -l)
if [ "$shots" -ge 7 ]; then echo "ok:   $shots pictures"; else echo "FAIL: $shots pictures (wanted 7)"; fails=$((fails + 1)); fi
[ "$fails" = 0 ] && echo "SuperChargers Toy Box check: PASS" || echo "SuperChargers Toy Box check: $fails FAILED (logs in $R)"
exit $((fails != 0))
