#!/bin/bash
# Trap Team: the in-game Toy Box with the game's own pictures, seats (a trap seat among them), Add a figure by category and
# the sheet of each kind ([TrapTeam]; docs/runtime/host/toybox_ux.md, games/trapteam/docs/toybox.md). Same shape as
# swapper_toybox.sh: a scratch collection of new dumps (a Trap Master, a core, a mini, an Eon's Elite, two empty traps), the
# game on an empty portal, the Toy Box driven by SKY_TOYBOX_SCRIPT, a picture of each screen, the log checked.
#
#   TRAPTEAM_ISO=<Trap Team .iso> tools/regress/trapteam_toybox.sh [<run name>]     (about 100 s; build-trapteam/)
#
# The run goes through ../gamelock.sh when it exists (one game at a time on a shared Mac), isolated like every run
# (docs/getting-started/testing.md): scratch collection, save, data, settings; offscreen video, disk audio; kills only its
# own pid. Everything is under private/runs/<name>; the pictures are <name>/shots/*.ppm. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-trapteam_toybox}
iso=${TRAPTEAM_ISO:?set TRAPTEAM_ISO to the Trap Team disc image}
exe="${LIVE_EXE:-$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team}"
make=${FIGURE_TOOL:-$root/build-trapteam/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/runs/$name
[ -d "$R" ] && { echo "$R exists: give another run name"; exit 2; }
mkdir -p "$R/collection" "$R/save" "$R/data"

# Gusto (a Trap Master), Fling Kong (a core), Trigger Snappy (a mini), Eon's Elite Eruptor and two Water Traps holding a
# villain (ArkeyanFigureTest makes traps only as this pair).
for toy in 450 452 519 9:3810; do
    "$make" --make "$R/collection/new_${toy/:/_}.sky" "$toy" > /dev/null || { echo "cannot make the dump of $toy"; exit 2; }
done
traps=.scratch/trappair-$name
rm -rf "${root:?}/$traps"
(cd "$root" && "$make" --make-trap-pair "$traps") > /dev/null || { echo "cannot make the traps"; exit 2; }
mv "$root/$traps"/* "$R/collection/"

script=${TRAPTEAM_SCRIPT:-"F6:35,shot=01_seats:37,Return:38,shot=02_p1_seated:40,PageDown:41,PageDown:42,shot=03_trap_choose:44,Return:45,shot=04_trap_seated:47,F2:48,shot=05_sheet_trap:50,Escape:51,PageUp:52,PageUp:53,shot=06_p1_list:56,Left:57,Left:58,Left:59,Left:60,Left:61,Right:62,F2:63,shot=07_sheet_trapmaster:65,Escape:66,Right:67,F2:68,shot=08_sheet_elite:70,Escape:71,Right:72,F2:73,shot=09_sheet_mini:75,Escape:76,Right:77,Return:78,shot=10_add:80,PageDown:81,PageDown:82,PageDown:83,PageDown:84,PageDown:85,PageDown:86,PageDown:87,PageDown:88,PageDown:89,PageDown:90,shot=11_add_minis:92,PageDown:93,shot=12_add_traps:95,Return:96,shot=13_add_trap_chosen:98"}
secs=${TRAPTEAM_SECONDS:-125}
run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
  SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
  SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_LOG=1 \
  SKY_TOYBOX_SCRIPT='$script' SKY_TOYBOX_SHOTS='$R/shots' \
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
check "the game's portraits were read from the disc" "portraits in"
check "6 figures, no swapper" "[toybox] 6 cards (a swapper is one)"
check "a core goes on Player 1" "P1: Fling Kong on slot 0"
check "the game took it" "P1: the game took Fling Kong"
check "a trap goes on the Trap seat (its own slot)" "Trap: Wet Walter on slot"
check "the game took the trap" "Trap: the game took Wet Walter"
shots=$(ls "$R"/shots_*.png 2>/dev/null | wc -l)
if [ "$shots" -ge 13 ]; then echo "ok:   $shots pictures"; else echo "FAIL: $shots pictures (wanted 13)"; fails=$((fails + 1)); fi
[ "$fails" = 0 ] && echo "Trap Team Toy Box check: PASS" || echo "Trap Team Toy Box check: $fails FAILED (logs in $R)"
exit $((fails != 0))
