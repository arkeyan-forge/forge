#!/bin/bash
# Imaginite chests added from the launcher (games/imaginators/chests.cpp, launcher #152), twice:
#   1. a new game to the Academy (SKY_LEVEL=hub_2016, as imaginators_hub.sh) with chests_add.txt already asking for
#      chests of four kinds for slot 0: the game adds them when the Academy is entered, chests.txt shows them and the
#      request is gone;
#   2. a continue from the first run's saves: chests.txt shows the same pile from the save, with no request.
# Every run is isolated (docs/getting-started/testing.md) under private/runs/<name>-new and <name>-continue, through
# ../gamelock.sh when it exists; it kills only its own pids. Exit 0: all checks pass.
#
#   IMAGINATORS_ISO=<Imaginators .iso> tools/regress/imaginators_chests.sh [<run name>]     (about 8 min; build-imaginators/)
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-imaginators_chests}
iso=${IMAGINATORS_ISO:?set IMAGINATORS_ISO to the Imaginators disc image}
exe="${LIVE_EXE:-$root/build-imaginators/ArkeyanRuntime/Skylanders Imaginators.app/Contents/MacOS/Skylanders Imaginators}"
for r in "$name-new" "$name-continue"; do
    [ -e "$root/private/runs/$r" ] && { echo "private/runs/$r exists: give another run name"; exit 2; }
done
NEW_SECONDS=${IMG_NEW_SECONDS:-240}
CONTINUE_SECONDS=${IMG_CONTINUE_SECONDS:-220}

drive() { # <game log> <press file>: Start until the licence, Y, then A (imaginators_hub.sh)
    local log=$1 presses=$2 i
    until [ -e "$log" ]; do sleep 1; done
    sleep 20
    for i in $(seq 1 30); do
        grep -a -q 'softwarelicense.pak' "$log" && break
        echo start >> "$presses"
        sleep 5
    done
    if grep -a -q 'softwarelicense.pak' "$log"; then
        sleep 8
        echo y >> "$presses"
        sleep 3
        echo y >> "$presses"
    fi
    for i in $(seq 1 80); do
        sleep 4
        echo a >> "$presses"
    done
}

play() { # <run dir> <seconds> [VAR=value ...]
    local R=$1 secs=$2; shift 2
    mkdir -p "$R/collection" "$R/save" "$R/data"
    : > "$R/presses.txt"
    drive "$R/game.log" "$R/presses.txt" & local driver=$!
    local run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
      SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_PORTAL=emulated \
      SKY_PORTAL_PLACE=new:9 SKY_PRESS_FILE='$R/presses.txt' $* \
      nice '$exe' --fps-limit 30 '$iso' > '$R/log' 2>&1 & pid=\$!
      (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
    kill "$driver" 2> /dev/null
    wait "$driver" 2> /dev/null
}

fails=0
ok() { echo "ok:   $1"; }
bad() { echo "FAIL: $1"; fails=$((fails + 1)); }
has() { grep -a -q -x "$2" "$1" 2> /dev/null; } # <file> <whole line>
pile_checks() { # <run dir> <what>: chests.txt holds slot 0 and the four chests asked for
    local status=$1/save/chests.txt what=$2 line
    [ -f "$status" ] || { bad "$what: no chests.txt"; return; }
    for line in slot=0 pile=4 pile.imaginite=2 pile.gold=1 pile.toy_bronze=1 pile.platinum=0 limit=99; do
        has "$status" "$line" && ok "$what: chests.txt has $line" || bad "$what: chests.txt has no $line ($(tr '\n' ' ' < "$status"))"
    done
    local faults
    faults=$(grep -a -c '\[guard\]\|unhandled exception\|Indirect call to unrecompiled' "$1/game.log")
    [ "$faults" = 0 ] && ok "$what: no fault" || bad "$what: $faults fault lines"
}

R1=$root/private/runs/$name-new
mkdir -p "$R1/save"
printf 'slot=0\nimaginite=2\ngold=1\ntoy_bronze=1\n' > "$R1/save/chests_add.txt"
play "$R1" "$NEW_SECONDS" SKY_LEVEL=hub_2016
grep -a -q '^\[level\] 2016/hub_2016/hub_2016 starts' "$R1/game.log" && ok "new game: the Academy starts" ||
    bad "new game: no '[level] 2016/hub_2016/hub_2016 starts'"
grep -a -q '^\[chests\] added 4 (imaginite=2 gold=1 toy_bronze=1) to slot 0' "$R1/game.log" &&
    ok "new game: the four chests added to slot 0" || bad "new game: no '[chests] added 4' line"
[ -e "$R1/save/chests_add.txt" ] && bad "new game: the request is still there" || ok "new game: the request was claimed"
pile_checks "$R1" "new game"

R2=$root/private/runs/$name-continue
mkdir -p "$R2/save"
cp -R "$R1/save/." "$R2/save/"
rm -f "$R2/save/chests.txt"
play "$R2" "$CONTINUE_SECONDS"
grep -a -q '^\[chests\] added' "$R2/game.log" && bad "continue: chests added again" || ok "continue: nothing added"
pile_checks "$R2" "continue (from the save)"

[ "$fails" = 0 ] && echo "Imaginators chests: PASS" || echo "Imaginators chests: $fails FAILED (logs in private/runs/$name-*)"
exit $((fails != 0))
