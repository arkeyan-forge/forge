#!/bin/bash
# Trap Team: a trap's villain chosen in the in-game Toy Box ([TrapTeam]; docs/runtime/host/toybox_ux.md, "A trap's
# villain"). Gusto and a Water Trap holding the Gulper on the emulated portal; in the tutorial LT plays the Gulper and
# gives Gusto back, then the Toy Box opens the trap's page, its Villain row goes to Chill Bill (Right three times), A puts
# it in (the Toy Box lifts the trap and puts it back), and LT plays Chill Bill: his \character\1019_*.arc opens. The
# villain's gauge refills after the Gulper: LT at +60 does nothing yet, +80 plays Chill Bill (6_lt_80.png), +100 Gusto.
#
#   TRAPTEAM_ISO=<Trap Team .iso> tools/regress/trapteam_villain.sh [<run name>] [en|fr]     (about 3 min; build-trapteam/)
#
# The run goes through ../gamelock.sh when it exists (one game at a time on a shared Mac), isolated like every run
# (docs/getting-started/testing.md): scratch collection, save, data, settings; offscreen video, disk audio; kills only its
# own pid. The presses go through the Toy Box script on the game's frame clock, timed from the moment the tutorial level
# opens. Everything is under private/runs/<name>; the pictures are <name>/toybox_*.png. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)

# --drive <dir> <pid>: A every 3 s until the tutorial level opens, then the presses from that moment.
if [ "${1:-}" = "--drive" ]; then
    R=$2 pid=$3
    n=0
    while kill -0 "$pid" 2> /dev/null && ! grep -aqF '450_typhoon.arc' "$R/game.log" 2> /dev/null; do
        [ $((n % 15)) = 0 ] && echo "pad=a" >> "$R/toybox.txt"
        n=$((n + 1))
        sleep 0.2
    done
    cat >> "$R/toybox.txt" << 'EOF'
+20 hold=1*0.5
+25 shot=1_gulper_played
+28 hold=1*0.5
+33 F6
+35 PageDown
+35.5 PageDown
+37 F2
+39 Down
+39.5 Down
+40.5 shot=2_sheet_before
+41.5 Right
+42 Right
+42.5 Right
+44 shot=3_sheet_chosen
+45 Return
+47 shot=4_sheet_after
+48 Escape
+48.5 Escape
+50 Escape
+60 hold=1*0.5
+66 shot=5_lt_60
+80 hold=1*0.5
+86 shot=6_lt_80
+100 hold=1*0.5
+106 shot=7_lt_100
+120 hold=1*0.5
+126 shot=8_lt_120
+130 shot=end
EOF
    while kill -0 "$pid" 2> /dev/null && ! grep -aqF 'script: shot=end' "$R/game.log" 2> /dev/null; do sleep 2; done
    sleep 2
    kill "$pid" 2> /dev/null
    exit 0
fi

name=${1:-trapteam_villain}
language=${2:-en}
iso=${TRAPTEAM_ISO:?set TRAPTEAM_ISO to the Trap Team disc image}
exe="${LIVE_EXE:-$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team}"
make=${FIGURE_TOOL:-$root/build-trapteam/ArkeyanRuntime/ArkeyanFigureTest}
self=$root/tools/regress/trapteam_villain.sh
R=$root/private/runs/$name
[ -d "$R" ] && { echo "$R exists: give another run name"; exit 2; }
mkdir -p "$R/collection" "$R/save" "$R/data"
: > "$R/toybox.txt"

# One trap only (the Trap seat's shelf then has one card): the pair's normal Gulper.
traps=.scratch/trappair-$name
rm -rf "${root:?}/$traps"
(cd "$root" && "$make" --make-trap-pair "$traps") > /dev/null || { echo "cannot make the traps"; exit 2; }
mv "$root/$traps/water-gulper-normal.sky" "$R/collection/"
rm -rf "${root:?}/$traps"
trap_dump=$R/collection/water-gulper-normal.sky

run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
  SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
  SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_LOG=1 \
  SKY_PORTAL_PLACE='new:450,water-gulper-normal.sky' SKY_LANGUAGE='$language' \
  SKY_TOYBOX_SCRIPT_FILE='$R/toybox.txt' SKY_TOYBOX_CLOCK=frames SKY_TOYBOX_SHOTS='$R/toybox' \
  nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$R/log' 2>&1 & pid=\$!; echo \$pid > '$R/pid'
  (exec 3>&-; sleep ${TRAPTEAM_SECONDS:-600}; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
  '$self' --drive '$R' \$pid
  wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi

log=$R/game.log
fails=0
check() { # <what> <fixed text>
    if grep -aqF -- "$2" "$log" 2> /dev/null; then echo "ok:   $1"; else echo "FAIL: $1 (no '$2')"; fails=$((fails + 1)); fi
}
# after <what> <first text> <then text>: the second appears on a later line than the first.
after() {
    local first then
    first=$(grep -anF -- "$2" "$log" | head -1 | cut -d: -f1)
    then=$(grep -anF -- "$3" "$log" | tail -1 | cut -d: -f1)
    if [ -n "$first" ] && [ -n "$then" ] && [ "$then" -gt "$first" ]; then echo "ok:   $1"
    else echo "FAIL: $1 ('$3' after '$2')"; fails=$((fails + 1)); fi
}
check "the tutorial plays" "[level] level_313_tutorial starts"
check "the trap is on the portal" "(toy type 211) in slot 1"
check "LT played the Gulper first" '\character\1009_'
check "the Toy Box put Chill Bill in, the trap lifted and put back" \
    "[toybox] trap water-gulper-normal.sky: villain 19, lifted and put back"
# The portal's own traffic: block 1 of the trap (toy type 211, D3 00) read again after it was put back.
after "the game read the trap again" "[toybox] trap water-gulper-normal.sky: villain 19" "<- Q 51 11 01 D3 00"
after "the game opened Chill Bill" "[toybox] trap water-gulper-normal.sky: villain 19" '\character\1019_'
case $("$make" "$trap_dump" 2> /dev/null | grep -aF "$trap_dump: ") in
    *"selected 19, evolved 0"*) echo "ok:   the trap holds Chill Bill" ;;
    *) echo "FAIL: the trap: $("$make" "$trap_dump" 2> /dev/null | grep -aF "$trap_dump: ")"; fails=$((fails + 1)) ;;
esac
[ "$fails" = 0 ] && echo "Trap Team villain check: PASS" || echo "Trap Team villain check: $fails FAILED (logs in $R)"
[ "$fails" = 0 ]
