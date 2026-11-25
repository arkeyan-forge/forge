#!/bin/bash
# The mouse in Giants' own menus (docs/runtime/xbox360/input.md, "Mouse"), through the Toy Box's script
# (SKY_TOYBOX_SCRIPT: move=, click=, canvas pixels) with the mouse switch off: from the title to Chapter 1's tutorial
# with clicks only (the title screens, the autosave notice, Story, the second slot after hovering it, Medium), then
# START: in the pause menu a click on Settings (the focus, then Accept), B back and START out of it.
# Checks: the level, the accepts and focus moves, the pointer the menu's while a menu is on screen and not in play;
# shots.
#
#   tools/regress/giants_menu_mouse.sh <disc image>        (Giants)
#   EXE=<game executable>   default build-giants/ArkeyanRuntime/Skylanders Giants
#
# Everything is under private/runs/giants_menu_mouse/. Automatically wraps the whole run in ../gamelock.sh.
set -u
set -o pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
# Lock the entire run, including setup, watchdog and validation. The marker is private to the wrapper invocation.
if [ "${GIANTS_MENU_MOUSE_LOCKED:-}" != 1 ]; then
    exec "$root/../gamelock.sh" env GIANTS_MENU_MOUSE_LOCKED=1 bash "$root/tools/regress/giants_menu_mouse.sh" "$@"
fi
. "$root/tools/regress/pid_timer.sh"
iso=${1:-}
[ -f "$iso" ] || { echo "FAIL: usage: $0 <Giants disc image>" >&2; exit 2; }
exe=${EXE:-"$root/build-giants/ArkeyanRuntime/Skylanders Giants"}
[ -x "$exe" ] || { echo "FAIL: no game executable under build-giants/" >&2; exit 2; }
r=$root/private/runs/giants_menu_mouse
rm -rf "${r:?}"; mkdir -p "$r/collection"
fail=0
bad() { echo "FAIL: $*"; fail=1; }

# The title screens and the notices take a click anywhere: at the bottom of the picture, where no menu has an item.
front="click=640/690:8,click=640/690:12,click=640/690:15,move=440/330:18.5,click=440/330:19.5"
front="$front,move=640/300:22,shot=slot:23,click=640/300:24,move=748/290:27,click=748/290:28,shot=play:85"
pause="move=640/360:92,move=890/360:93,shot=paused:94,click=890/360:95,shot=settings:98,shot=resumed:108"
echo "== giants: the mouse in the game's own menus"
runner=
cancel_run() {
    if [ -n "$runner" ]; then kill -TERM "$runner" 2>/dev/null; wait "$runner" 2>/dev/null; fi
    echo "FAIL: interrupted menu mouse regression"
    exit 1
}
trap cancel_run INT TERM HUP
( cd "$r" && run_timed 110 env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
    SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" \
    SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" \
    SKY_REPORT_DIR="$r/report" SKY_LAUNCHER_LOG_DIR="$r/logs" SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_SKIP_MOVIES=1 \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:14 SKY_TOYBOX_SHOTS="$r/m" SKY_TOYBOX_SCRIPT="$front,$pause" \
    SKY_PRESS="start:90,b:100,start:104" nice "$exe" "$iso" > "$r/m.out" 2>&1 ) &
runner=$!
wait "$runner"
run_status=$?
runner=
[ "$run_status" -eq 0 ] || bad "game exited early or watchdog failed (status $run_status)"

log=$r/game.log
# The lines from the first one holding the text $1 to the next one holding $2.
between() { awk -v from="$1" -v to="$2" '!on && index($0, from) { on=1; print; next } on { print } on && index($0, to) { exit }' "$log"; }
grep -aq '^\[level\] level_213_tutotial starts' "$log" || bad "the clicks did not reach the tutorial"
front_accepts=$(between '[level] title starts' '[level] level_213_tutotial starts' | grep -ac '^\[menu\] accept$')
[ "$front_accepts" -ge 6 ] || bad "the clicks were Accept $front_accepts times before the level (6 or more expected)"
between 'game menu move 640/300' 'game menu click 640/300' | grep -aq '^\[menu\] focus: ' ||
    bad "hovering the second slot did not move the focus"
last_before_pause=$(awk '/^\[toybox\] script: shot=play/ { exit } /^\[mouse\] game menu pointer (on|off)/ { last=$0 } END { print last }' "$log")
[ "$last_before_pause" = "[mouse] game menu pointer off" ] || bad "the pointer was the menu's in play ($last_before_pause)"
pause_lines=$(between '[toybox] script: shot=play' '[toybox] script: shot=resumed')
echo "$pause_lines" | grep -aq '^\[mouse\] game menu pointer on' || bad "the pause menu did not take the pointer"
echo "$pause_lines" | grep -aq '^\[menu\] page .*, a carousel$' || bad "the pause menu was not read"
between 'game menu move 640/360' 'game menu click 890/360' | grep -aq '^\[menu\] focus: ' &&
    bad "hovering the pause carousel moved the focus"
between 'game menu click 890/360' 'script: shot=settings' | grep -aq '^\[menu\] focus: ' ||
    bad "the click on Settings did not move the focus first"
between 'game menu click 890/360' 'script: shot=settings' | grep -aq '^\[menu\] accept$' ||
    bad "the click on Settings was not Accept"
last=$(grep -aE '^\[mouse\] game menu pointer (on|off)' "$log" | tail -1)
[ "$last" = "[mouse] game menu pointer off" ] || bad "the pointer stayed the menu's after leaving the pause menu"
grep -aE '^\[menu\]|^\[level\]|game menu pointer' "$log"
for shot in slot play paused settings resumed; do
    [ -s "$r/m_$shot.png" ] || bad "missing screenshot: $shot"
done
if [ $fail -eq 0 ]; then echo "PASS: giants: clicks from the title to the tutorial; a click in the pause menu (under $r)"; else echo "FAIL (logs in $r)"; fi
exit $fail
