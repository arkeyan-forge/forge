#!/bin/bash
# Trap Team: a Skylander swapped through the Toy Box and the wheel while a trap is on the portal ([TrapTeam];
# docs/runtime/host/portal.md). A trap's voice turns the portal's speaker on and off, and the game reads no figure until
# the speaker answers: the swap never landed and the game asked for a Skylander again.
#
#   TRAPTEAM_ISO=<Trap Team .iso> tools/regress/trapteam_swap.sh [<run name>]     (about 3 min; build-trapteam/)
#
# The run goes through ../gamelock.sh when it exists (one game at a time on a shared Mac), isolated like every run
# (docs/getting-started/testing.md): scratch collection, save, data, settings; offscreen video, disk audio; kills only its
# own pid. Everything is under private/runs/<name>. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-trapteam_swap}
iso=${TRAPTEAM_ISO:?set TRAPTEAM_ISO to the Trap Team disc image}
exe="${LIVE_EXE:-$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team}"
make=${FIGURE_TOOL:-$root/build-trapteam/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/runs/$name
[ -d "$R" ] && { echo "$R exists: give another run name"; exit 2; }
mkdir -p "$R/collection" "$R/save" "$R/data"

# Gusto (a Trap Master), Fling Kong (a core), Trigger Snappy (a mini), two Water Traps holding a villain.
for toy in 450 452 519; do
    "$make" --make "$R/collection/new_$toy.sky" "$toy" > /dev/null || { echo "cannot make the dump of $toy"; exit 2; }
done
traps=.scratch/trappair-$name
rm -rf "${root:?}/$traps"
(cd "$root" && "$make" --make-trap-pair "$traps") > /dev/null || { echo "cannot make the traps"; exit 2; }
mv "$root/$traps"/* "$R/collection/"

# The attack key through the intro until the level plays, then a swap in the Toy Box and one on the wheel.
press=$(awk 'BEGIN { for (t = 32; t <= 96; t += 4) printf "%sa:%d", (t > 32 ? "," : ""), t }')
script="F6:30,Return:32,PageDown:34,PageDown:35,Return:37,Escape:40,Escape:41,F6:110,Right:112,Return:113,Escape:115,down=Tab:125,1:126.5,up=Tab:128,shot=swapped:133,shot=later:170"
secs=${TRAPTEAM_SECONDS:-180}
run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
  SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
  SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_LOG=1 \
  SKY_PRESS='$press' SKY_TOYBOX_SCRIPT='$script' SKY_TOYBOX_SHOTS='$R/shots' \
  nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$R/log' 2>&1 & pid=\$!; echo \$pid > '$R/pid'
  (exec 3>&-; sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
  wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi

log=$R/game.log; [ -f "$log" ] || log=$R/log
fails=0
check() { # <what> <fixed text> [<count>]
    local n; n=$(grep -acF -- "$2" "$log")
    if [ "$n" -ge "${3:-1}" ]; then echo "ok:   $1"; else echo "FAIL: $1 (wanted '$2' x${3:-1}, saw $n)"; fails=$((fails + 1)); fi
}
check "the tutorial is play, not a menu (the mouse's capture)" "[level] level_313_tutorial starts"
check "the trap is on the portal" "Trap: the game took Wet Walter"
check "the speaker was answered" "<- M 4D" 2
check "the first swap landed" "P1: the game took Trigger Snappy"
check "the wheel's swap landed" "P1: the game took Fling Kong" 2
if grep -aqF "not read by the game" "$log"; then echo "FAIL: a figure was not read"; fails=$((fails + 1)); else echo "ok:   every figure was read"; fi
if grep -aqE 'ignores command 0x[0-9A-F]+ \(30 bytes\)' "$log"; then echo "FAIL: a sound block was read as a command"; fails=$((fails + 1)); else echo "ok:   no sound block read as a command"; fi
[ "$fails" = 0 ] && echo "Trap Team swap check: PASS" || echo "Trap Team swap check: $fails FAILED (logs in $R)"
[ "$fails" = 0 ]
