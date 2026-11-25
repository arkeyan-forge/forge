#!/bin/bash
# The mouse in Spyro's Adventure's own menus (docs/runtime/xbox360/input.md, "Mouse"), through the Toy Box's script
# (SKY_TOYBOX_SCRIPT: move=, click=, canvas pixels) with the mouse switch off: from the title to level_027 with clicks
# only (the title, the autosave notice, Skyland Adventures, Start a New Game, the second slot), B through the level's
# opening scene, then START: in the pause menu the pointer onto Collections and Options (each takes the focus), a
# click on Objectives (the focus, then Accept), B back and START out of it.
# Checks: the level, the accepts and focus moves, the pointer the menu's while a menu is on screen and not in play;
# shots.
#
#   tools/regress/ssa_menu_mouse.sh <disc image>        (Spyro's Adventure)
#   EXE=<game executable>   default build/ArkeyanRuntime/Skylanders Spyro's Adventure
#
# Everything is under private/runs/ssa_menu_mouse/. Automatically wraps the whole run in ../gamelock.sh.
set -u
set -o pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
# Lock the entire run, including setup, watchdog and validation. The marker is private to the wrapper invocation.
if [ "${SSA_MENU_MOUSE_LOCKED:-}" != 1 ]; then
    exec "$root/../gamelock.sh" env SSA_MENU_MOUSE_LOCKED=1 bash "$root/tools/regress/ssa_menu_mouse.sh" "$@"
fi
. "$root/tools/regress/pid_timer.sh"
iso=${1:-}
[ -f "$iso" ] || { echo "FAIL: usage: $0 <Spyro's Adventure disc image>" >&2; exit 2; }
exe=${EXE:-"$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"}
make=${FIGURE_TOOL:-$root/build/ArkeyanRuntime/ArkeyanFigureTest}
[ -x "$exe" ] && [ -x "$make" ] || { echo "FAIL: no game executable or ArkeyanFigureTest under build/" >&2; exit 2; }
r=$root/private/runs/ssa_menu_mouse
rm -rf "${r:?}"; mkdir -p "$r/collection"
"$make" --make "$r/collection/new_14.sky" 14 > /dev/null || { echo "FAIL: cannot make the figure" >&2; exit 2; }
fail=0
bad() { echo "FAIL: $*"; fail=1; }

# Notices take a click anywhere: at the bottom of the picture, where no menu has an item.
notices() { awk -v from="$1" -v to="$2" 'BEGIN { for (t = from; t <= to; t += 2) printf ",click=640/690:%d", t }'; }
front="click=640/600:3$(notices 6 12),move=640/290:13.5,click=640/290:14,move=640/303:17.5,click=640/303:18"
front="$front,move=640/320:21.5,click=640/320:22$(notices 24 34),shot=play:88"
pause="move=916/323:93,move=920/325:93.3,shot=collections:94.5,move=434/530:95.5,move=430/532:95.8,shot=options:97"
pause="$pause,click=643/323:99,shot=objectives:102,shot=resumed:110"
press=$(awk 'BEGIN { for (t = 40; t <= 70; t += 2) printf "b:%d,", t }')start:90,b:104,start:107
echo "== ssa: the mouse in the game's own menus"
runner=
cancel_run() {
    if [ -n "$runner" ]; then kill -TERM "$runner" 2>/dev/null; wait "$runner" 2>/dev/null; fi
    echo "FAIL: interrupted menu mouse regression"
    exit 1
}
trap cancel_run INT TERM HUP
( cd "$r" && run_timed 112 env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
    SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" \
    SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" \
    SKY_REPORT_DIR="$r/report" SKY_LAUNCHER_LOG_DIR="$r/logs" SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_SKIP_MOVIES=1 \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky SKY_TOYBOX_SHOTS="$r/m" SKY_TOYBOX_SCRIPT="$front,$pause" \
    SKY_PRESS="$press" nice "$exe" "$iso" > "$r/m.out" 2>&1 ) &
runner=$!
wait "$runner"
run_status=$?
runner=
[ "$run_status" -eq 0 ] || bad "game exited early or watchdog failed (status $run_status)"

log=$r/game.log
# The lines from the first one holding the text $1 to the next one holding $2.
between() { awk -v from="$1" -v to="$2" '!on && index($0, from) { on=1; print; next } on { print } on && index($0, to) { exit }' "$log"; }
grep -q '^\[level\] level_027 starts' "$log" || bad "the clicks did not reach level_027"
front_accepts=$(between '[level] frontend starts' '[level] level_027 starts' | grep -c '^\[menu\] accept$')
[ "$front_accepts" -ge 5 ] || bad "the clicks were Accept $front_accepts times before the level (5 or more expected)"
between 'click=640/290' 'level_027' | grep -q '^\[menu\] focus ' || bad "hovering the second slot did not take the focus"
last_before_pause=$(awk '/^\[toybox\] script: shot=play/ { exit } /^\[mouse\] game menu pointer (on|off)/ { last=$0 } END { print last }' "$log")
[ "$last_before_pause" = "[mouse] game menu pointer off" ] || bad "the pointer was the menu's in play ($last_before_pause)"
pause_lines=$(between '[toybox] script: shot=play' '[toybox] script: shot=resumed')
echo "$pause_lines" | grep -q '^\[mouse\] game menu pointer on' || bad "the pause menu did not take the pointer"
hovers=$(between 'script: move=916/323' 'script: shot=options' | grep -c '^\[menu\] focus ')
[ "$hovers" -ge 2 ] || bad "the pointer onto Collections and Options moved the focus $hovers times (2 expected)"
between 'script: click=643/323' 'script: shot=objectives' | grep -q '^\[menu\] focus ' ||
    bad "the click on Objectives did not move the focus first"
between 'script: click=643/323' 'script: shot=objectives' | grep -q '^\[menu\] accept$' ||
    bad "the click on Objectives was not Accept"
last=$(grep -E '^\[mouse\] game menu pointer (on|off)' "$log" | tail -1)
[ "$last" = "[mouse] game menu pointer off" ] || bad "the pointer stayed the menu's after leaving the pause menu"
grep -E '^\[menu\]|^\[level\]|game menu pointer' "$log"
for shot in play collections options objectives resumed; do
    [ -s "$r/m_$shot.png" ] || bad "missing screenshot: $shot"
done
if [ $fail -eq 0 ]; then echo "PASS: ssa: clicks from the title to level_027; hover and a click in the pause menu (under $r)"; else echo "FAIL (logs in $r)"; fi
exit $fail
