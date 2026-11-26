#!/bin/bash
# The mouse in Swap Force's own menus (docs/runtime/xbox360/input.md, "Juice menus"), through the Toy Box's script
# (SKY_TOYBOX_SCRIPT: move=, click=, canvas pixels) with the mouse switch off: from the title to the tutorial with
# clicks only (the title, STORY MODE, the second save slot after hovering it, MEDIUM), then START: in the pause menu
# a hover and a click on OPTIONS, B back and START out of it. The steps are far apart: the title takes 20 s or more on
# a busy machine, and a click outside the items does nothing on a page.
# Checks: the level, the accepts and focus moves, the pages read, the pointer the menu's while a page is on screen
# and not in play; shots.
#
#   tools/regress/swapforce_menu_mouse.sh <disc image>        (Swap Force)
#   EXE=<game executable>   default build-swapforce/ArkeyanRuntime/Skylanders Swap Force
#
# Everything is under private/runs/swapforce_menu_mouse/. Automatically wraps the whole run in ../gamelock.sh.
set -u
set -o pipefail
export LC_ALL=C # the log may hold a cut multibyte character
root=$(cd "$(dirname "$0")/../.." && pwd)
# Lock the entire run, including setup, watchdog and validation. The marker is private to the wrapper invocation.
if [ "${SWAPFORCE_MENU_MOUSE_LOCKED:-}" != 1 ]; then
    exec "$root/../gamelock.sh" env SWAPFORCE_MENU_MOUSE_LOCKED=1 bash "$root/tools/regress/swapforce_menu_mouse.sh" "$@"
fi
. "$root/tools/regress/pid_timer.sh"
iso=${1:-}
[ -f "$iso" ] || { echo "FAIL: usage: $0 <Swap Force disc image>" >&2; exit 2; }
exe=${EXE:-"$root/build-swapforce/ArkeyanRuntime/Skylanders Swap Force"}
[ -x "$exe" ] || { echo "FAIL: no game executable under build-swapforce/" >&2; exit 2; }
r=$root/private/runs/swapforce_menu_mouse
rm -rf "${r:?}"; mkdir -p "$r/collection"
fail=0
bad() { echo "FAIL: $*"; fail=1; }

# The title takes a click anywhere: at the bottom of the picture, where no page has an item. STORY MODE is the main
# menu's first item, the save slots are 910x156 boxes from 193/185, MEDIUM the difficulty's second item.
front="click=640/690:20,click=640/690:30,shot=main:38,click=150/359:40,shot=slots:46,move=647/398:48"
front="$front,click=647/398:50,shot=difficulty:56,click=150/313:58,shot=play:95"
pause="move=150/280:104,move=150/384:106,shot=paused:107,click=150/384:108,shot=options:114,shot=resumed:128"
echo "== swapforce: the mouse in the game's own menus"
runner=
cancel_run() {
    if [ -n "$runner" ]; then kill -TERM "$runner" 2>/dev/null; wait "$runner" 2>/dev/null; fi
    echo "FAIL: interrupted menu mouse regression"
    exit 1
}
trap cancel_run INT TERM HUP
( cd "$r" && run_timed 135 env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
    SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" \
    SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" \
    SKY_REPORT_DIR="$r/report" SKY_LAUNCHER_LOG_DIR="$r/logs" SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_SKIP_MOVIES=1 \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:9 SKY_TOYBOX_SHOTS="$r/m" SKY_TOYBOX_SCRIPT="$front,$pause" \
    SKY_PRESS="start:100,b:118,start:122" nice "$exe" "$iso" > "$r/m.out" 2>&1 ) &
runner=$!
wait "$runner"
run_status=$?
runner=
[ "$run_status" -eq 0 ] || bad "game exited early or watchdog failed (status $run_status)"

log=$r/game.log
# The lines from the first one holding the text $1 to the next one holding $2.
between() { awk -v from="$1" -v to="$2" '!on && index($0, from) { on=1; print; next } on { print } on && index($0, to) { exit }' "$log"; }
grep -aq '^\[level\] tutorial\\tutorial_a\\tutorial_a starts' "$log" || bad "the clicks did not reach the tutorial"
front_accepts=$(between '[level] ui' 'tutorial_a starts' | grep -ac '^\[menu\] accept$')
[ "$front_accepts" -ge 4 ] || bad "the clicks were Accept $front_accepts times before the level (4 or more expected)"
between 'game menu move 647/398' 'game menu click 647/398' | grep -aq '^\[menu\] focus: ' ||
    bad "hovering the second save slot did not move the focus"
between 'game menu click 150/313' 'tutorial_a starts' | grep -aq '^\[menu\] focus: ' ||
    bad "the click on MEDIUM did not move the focus first"
last_before_pause=$(awk '/^\[toybox\] script: shot=play/ { exit } /^\[mouse\] game menu pointer (on|off)/ { last=$0 } END { print last }' "$log")
[ "$last_before_pause" = "[mouse] game menu pointer off" ] || bad "the pointer was the menu's in play ($last_before_pause)"
pause_lines=$(between '[toybox] script: shot=play' '[toybox] script: shot=resumed')
echo "$pause_lines" | grep -aq '^\[mouse\] game menu pointer on' || bad "the pause menu did not take the pointer"
echo "$pause_lines" | grep -aq '^\[menu\] page [0-9A-F]*: 6 items$' || bad "the pause menu was not read"
between 'game menu move 150/384' 'game menu click 150/384' | grep -aq '^\[menu\] focus: ' ||
    bad "hovering OPTIONS did not move the focus"
between 'game menu click 150/384' 'script: shot=options' | grep -aq '^\[menu\] accept$' ||
    bad "the click on OPTIONS was not Accept"
last=$(grep -aE '^\[mouse\] game menu pointer (on|off)' "$log" | tail -1)
[ "$last" = "[mouse] game menu pointer off" ] || bad "the pointer stayed the menu's after leaving the pause menu"
grep -aE '^\[menu\]|^\[level\]|game menu pointer' "$log" | grep -av '^\[menu\]   item'
for shot in main slots difficulty play paused options resumed; do
    [ -s "$r/m_$shot.png" ] || bad "missing screenshot: $shot"
done
if [ $fail -eq 0 ]; then echo "PASS: swapforce: clicks from the title to the tutorial; a click in the pause menu (under $r)"; else echo "FAIL (logs in $r)"; fi
exit $fail
