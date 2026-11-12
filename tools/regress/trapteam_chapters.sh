#!/bin/bash
# Trap Team 0.1.0 path ([TrapTeam]; games/trapteam/docs/010.md): what a new player meets, launch by launch.
#   new       a fresh save, Gusto and an empty Life Trap placed: the logos, "Press A", the autosave notice, the opening
#             movie fmv1 skipped with Start (A does not skip it), the tutorial (chapter 1) with Gusto in it.
#   continue  the same collection and save, nothing placed: the figures come back from portal.txt, "Select a save slot",
#             Continue Game, the opening movie again (the save is at 0 %), Start, the tutorial again.
#   level_314_tutorial_2, level_302_chompywurm, level_hub_2014_01   (SKY_LEVEL, a new save each): chapter 2, chapter 3
#             and Skylanders Academy open and play a scripted walk with attacks. SKY_LEVEL swaps the tutorial's two
#             files only: the hub then has no Level_Hub_2014_Lighting_*.bld (a grey sky), as an arena has no mesh.
#
#   TRAPTEAM_ISO=<Trap Team .iso> tools/regress/trapteam_chapters.sh [<run name>]     (about 16 min; build-trapteam/)
#
# A chapter's end is not reached: that needs a person's route (games/trapteam/docs/010.md). Each launch goes through
# ../gamelock.sh when it exists, isolated like every run (docs/getting-started/testing.md): scratch collection, save,
# data, settings; offscreen video, disk audio; kills only its own pid. Presses go through the Toy Box script on the
# game's frame clock. Everything is under private/runs/<name>/<phase>; pictures <phase>/tb_*.png. Exit 0: all pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)

# --drive <dir> <phase> <pid>: the presses of one launch, in the foreground of the locked shell.
if [ "${1:-}" = "--drive" ]; then
    P=$2 phase=$3 pid=$4
    # until <fixed text>: A every 3 s until the log has the text (or the game is gone).
    until_log() {
        local n=0
        while kill -0 "$pid" 2> /dev/null && ! grep -aqF -- "$1" "$P/game.log" 2> /dev/null; do
            [ $((n % 15)) = 0 ] && echo "pad=a" >> "$P/toybox.txt"
            n=$((n + 1))
            sleep 0.2
        done
    }
    case $phase in
        new)
            until_log 'fmv1.mov'
            printf '+4 shot=movie\n+6 pad=start\n' >> "$P/toybox.txt"
            sleep 8
            until_log '450_typhoon.arc'
            printf '+40 shot=level\n+42 shot=end\n' >> "$P/toybox.txt" ;;
        continue)
            until_log 'save:\Save_Header.xsg'
            printf '+4 shot=slots\n' >> "$P/toybox.txt"
            sleep 6
            until_log 'fmv1.mov'
            printf '+6 pad=start\n' >> "$P/toybox.txt"
            sleep 8
            until_log '450_typhoon.arc'
            printf '+40 shot=level\n+42 shot=end\n' >> "$P/toybox.txt" ;;
        *)
            until_log '450_typhoon.arc'
            awk 'BEGIN { for (t = 4; t <= 60; t += 3) print "+" t " pad=a"
                for (t = 64; t <= 150; t += 12) { print "+" t " pad=lup*2.5"; print "+" t + 3 " pad=x"
                    print "+" t + 5 " pad=lright*2"; print "+" t + 8 " pad=a"; print "+" t + 9 " pad=b"; print "+" t + 10 " pad=lleft*1.5" }
                for (t = 30; t <= 150; t += 30) print "+" t " shot=t" t
                print "+155 shot=end" }' >> "$P/toybox.txt" ;;
    esac
    while kill -0 "$pid" 2> /dev/null && ! grep -aqF 'script: shot=end' "$P/game.log" 2> /dev/null; do sleep 2; done
    sleep 2
    kill "$pid" 2> /dev/null
    exit 0
fi

name=${1:-trapteam_chapters}
iso=${TRAPTEAM_ISO:?set TRAPTEAM_ISO to the Trap Team disc image}
exe="${LIVE_EXE:-$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team}"
self=$root/tools/regress/trapteam_chapters.sh
R=$root/private/runs/$name
[ -d "$R" ] && { echo "$R exists: give another run name"; exit 2; }

# launch <phase> <state dir> <seconds> [VAR=value...]: one game, its presses in the foreground, a timer.
launch() {
    local phase=$1 P=$R/$1 S=$2 secs=$3
    shift 3
    mkdir -p "$P" "$S/collection" "$S/save" "$S/data"
    : > "$P/toybox.txt"
    local run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$S/data' SKY_SETTINGS='$S/none.ini' SKY_LAUNCHER_INI='$S/none.ini' SKY_COLLECTION='$S/collection' \
      SKY_SAVE_DIR='$S/save' SKY_LOG='$P/game.log' SKY_PORTAL=emulated SKY_PORTAL_LOG=1 $* \
      SKY_TOYBOX_SCRIPT_FILE='$P/toybox.txt' SKY_TOYBOX_CLOCK=frames SKY_TOYBOX_SHOTS='$P/tb' \
      nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$P/log' 2>&1 & pid=\$!; echo \$pid > '$P/pid'
      (exec 3>&-; sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      '$self' --drive '$P' '$phase' \$pid
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
}

fails=0
check() { # <what> <log> <fixed text>
    if grep -aqF -- "$3" "$2" 2> /dev/null; then echo "ok:   $1"; else echo "FAIL: $1 (no '$3' in $2)"; fails=$((fails + 1)); fi
}
nofault() { # <what> <log>
    if [ -f "$2" ] && ! grep -aqF 'Fault (' "$2"; then echo "ok:   $1: no fault"; else echo "FAIL: $1: a fault or no log ($2)"; fails=$((fails + 1)); fi
}

launch new "$R/state" 420 "SKY_PORTAL_PLACE=new:450,new:217:3003"
log=$R/new/game.log
check "new: the logos played" "$log" "[bink] open tfb_logo_640x456"
check "new: the save was made" "$log" 'save:\Save_Slot_1.xsg'
check "new: the opening movie opened" "$log" "[bink] open fmv1"
check "new: the tutorial opened" "$log" "[level] level_313_tutorial starts"
# The movie is 6941 frames (231 s): the level opening under a minute after it proves that Start ended it.
at() { grep -aF -- "$1" "$log" | head -1 | sed -n 's/.* at \([0-9.]*\) s$/\1/p'; }
movie=$(at '\movies\fmv1.mov') level=$(at '\level\level_313_tutorial.arc')
if [ -n "$movie" ] && [ -n "$level" ] && awk -v m="$movie" -v l="$level" 'BEGIN { exit !(l - m < 60) }'; then
    echo "ok:   new: Start ended the movie (fmv1 at $movie s, the tutorial at $level s)"
else
    echo "FAIL: new: the movie was not ended by Start (fmv1 at ${movie:-?} s, the tutorial at ${level:-?} s)"; fails=$((fails + 1))
fi
check "new: Gusto in the level" "$log" '\character\450_typhoon.arc'
nofault "new" "$log"

launch continue "$R/state" 300
log=$R/continue/game.log
check "continue: the figures came back" "$log" "2 figure(s) back on the portal"
check "continue: the save was read" "$log" '(disposition 2)'
check "continue: the opening movie again (a save at 0 %)" "$log" "[bink] open fmv1"
check "continue: Continue Game opened the tutorial" "$log" "[level] level_313_tutorial starts"
check "continue: Gusto in the level" "$log" '\character\450_typhoon.arc'
nofault "continue" "$log"

for level in level_314_tutorial_2 level_302_chompywurm level_hub_2014_01; do
    launch "$level" "$R/state_$level" 330 "SKY_PORTAL_PLACE=new:450,new:217:3003" "SKY_LEVEL=$level" SKY_SKIP_MOVIES=1
    log=$R/$level/game.log
    check "$level: opened" "$log" "[level] $level starts"
    check "$level: Gusto in the level" "$log" '\character\450_typhoon.arc'
    check "$level: played to the end of the walk" "$log" "script: shot=end"
    nofault "$level" "$log"
done
[ "$fails" = 0 ] && echo "Trap Team chapters check: PASS" || echo "Trap Team chapters check: $fails FAILED (logs in $R)"
[ "$fails" = 0 ]
