#!/bin/bash
# SuperChargers keeps the player's owner id from one session to the next (Skylauncher issue #28;
# docs/runtime/host/figures.md "Owner"): a new game registers Fiesta and Crypt Crusher (blank tags) with the save's
# owner id, then a second session on that save and those dumps must use the same id, leave both toys as they are (no new
# owner written) and list them in the game's collection (Portal Master > Collection > Skylanders: a picture to look at).
#
#   SUPERCHARGERS_ISO=<SuperChargers .iso> tools/regress/superchargers_owner.sh [<run name>]
#   (about 10 min; build-superchargers/)
#
# Each session goes through ../gamelock.sh when it exists, isolated like every run (docs/getting-started/testing.md):
# scratch collection, save, data, settings; offscreen video, disk audio; kills only its own pid. Everything is under
# private/runs/<name>-new and <name>-continue. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-superchargers_owner}
iso=${SUPERCHARGERS_ISO:?set SUPERCHARGERS_ISO to the SuperChargers disc image}
exe="${LIVE_EXE:-$root/build-superchargers/ArkeyanRuntime/Skylanders SuperChargers.app/Contents/MacOS/Skylanders SuperChargers}"
make=${FIGURE_TOOL:-$root/build-superchargers/ArkeyanRuntime/ArkeyanFigureTest}
R1=$root/private/runs/$name-new
R2=$root/private/runs/$name-continue
for R in "$R1" "$R2"; do
    [ -e "$R" ] && { echo "$R exists: give another run name"; exit 2; }
    mkdir -p "$R/collection" "$R/save" "$R/data"
done
for toy in 3400 3227; do
    "$make" --make "$R1/collection/new_$toy.sky" "$toy" > /dev/null || { echo "cannot make the dump of $toy"; exit 2; }
done

play() { # <run dir> <seconds> <presses> [VAR=value ...]: one session, in the background ($!)
    local R=$1 secs=$2 press=$3; shift 3
    local run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
      SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_PORTAL=emulated SKY_PORTAL_LOG=1 \
      SKY_PORTAL_PLACE=new_3400.sky,new_3227.sky SKY_SKIP_MOVIES=1 SKY_PRESS='$press' $* \
      nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$R/log' 2>&1 & pid=\$!
      (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
}

fails=0
ok() { echo "ok:   $1"; }
bad() { echo "FAIL: $1"; fails=$((fails + 1)); }
owner_of() { # <log> <the save's|new>: the id of the last "[owner] the player's owner id" line of that kind
    grep -a "^\[owner\] the player's owner id: .* ($2)" "$1" | tail -1 | sed 's/.*owner id: \([0-9A-F]*\).*/\1/'
}
tag_owner() { # <dump>: block 6 as the log writes it ("offline XXXXXXXX"), from the dump's bytes 0x60-0x64
    python3 -I -c 'import sys; b = open(sys.argv[1], "rb").read()[0x60:0x65]
print("offline %08X" % int.from_bytes(b[1:5], "little") if b[0] == 1 else "kind %02X" % b[0])' "$1"
}

# 1. A new game to the Academy (superchargers_lobby.sh's presses): the save's owner id is made, both toys get it.
press=$(awk 'BEGIN { s = ""; for (t = 12; t <= 44; t += 4) s = s sprintf("a:%d,", t)
    s = s "y:46,y:48,y:50,y:54,"; for (t = 58; t <= 150; t += 5) s = s sprintf("a:%d,", t)
    s = s "rt:205:60,rt:266:60,rt:327:45,"; for (t = 375; t <= 440; t += 5) s = s sprintf("a:%d,", t)
    print substr(s, 1, length(s) - 1) }')
play "$R1" "${SC_NEW_SECONDS:-470}" "$press" SKY_LEVEL=academy
id=$(owner_of "$R1/log" new)
[ -n "$id" ] && ok "new game: owner id $id made" || bad "new game: no '[owner] the player's owner id: ... (new)'"
for toy in 3400 3227; do
    got=$(tag_owner "$R1/collection/new_$toy.sky")
    [ "$got" = "offline $id" ] && ok "new game: toy $toy registered ($got)" || bad "new game: toy $toy has owner '$got'"
done

# 2. The same save and dumps again: A for up to 100 s (into the zone the save continues in), then Start,
# Portal Master > Collection > Skylanders, a picture.
cp -R "$R1/save/." "$R2/save/"
cp "$R1"/collection/*.sky "$R2/collection/"
: > "$R2/press.txt"
: > "$R2/script.txt"
play "$R2" 200 "" SKY_PRESS_FILE="$R2/press.txt" SKY_TOYBOX_SCRIPT_FILE="$R2/script.txt" SKY_TOYBOX_SHOTS="$R2/shot" &
session=$!
for _ in $(seq 1 360); do [ -f "$R2/log" ] && break; sleep 5; done # waiting for a game slot
sleep 12
for _ in $(seq 1 25); do
    grep -aq '^\[level\] academy/academy starts' "$R2/log" && break
    echo a >> "$R2/press.txt"
    sleep 4
done
sleep 12
for step in start:4 a:4 down:2 down:3 a:5 right:3 a:6; do
    echo "${step%%:*}" >> "$R2/press.txt"
    sleep "${step##*:}"
done
echo "shot=collection" >> "$R2/script.txt"
sleep 3
wait $session
again=$(owner_of "$R2/log" "the save's")
[ -n "$id" ] && [ "$again" = "$id" ] && ok "continue: the save's owner id again ($again)" ||
    bad "continue: owner id '$again', the new game's was '$id'"
grep -aq -- '-> W 57 1. 06' "$R2/log" && bad "continue: the game wrote a new owner on a toy" ||
    ok "continue: no new owner written on the toys"
grep -aq "toy: owner offline $id" "$R2/log" && ok "continue: '[portal] ... owner offline $id' logged" ||
    bad "continue: no '[portal] ... toy: owner offline $id'"
[ -f "$R2/shot_collection.png" ] && ok "the collection screen: $R2/shot_collection.png (Undead 1)" ||
    bad "no picture of the collection screen"
[ "$fails" = 0 ] && echo "SuperChargers owner id: PASS" ||
    echo "SuperChargers owner id: $fails FAILED (logs in private/runs/$name-*)"
exit $((fails != 0))
