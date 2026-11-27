#!/bin/bash
# The mouse in Trap Team's own menus (docs/runtime/xbox360/input.md, "Script menus"), through the Toy Box's script
# (SKY_TOYBOX_SCRIPT: move=, click=, canvas pixels) with the mouse switch off: from the title to the tutorial with
# clicks only (the title and the notices anywhere, the first save slot after hovering the second, Medium), then START:
# in the pause carousel a click on its right side turns it, one on its left side turns it back, START out of it.
# Checks: the level, the accepts and focus moves, the pages read, the pointer the menu's while a page is on screen and
# not in play; shots.
#
#   tools/regress/trapteam_menu_mouse.sh <disc image>        (Trap Team)
#   EXE=<game executable>   default build-trapteam/ArkeyanRuntime/Skylanders Trap Team
#
# Everything is under private/runs/trapteam_menu_mouse/. Automatically wraps the whole run in ../gamelock.sh.
set -u
set -o pipefail
export LC_ALL=C # the log may hold a cut multibyte character
root=$(cd "$(dirname "$0")/../.." && pwd)
# Lock the entire run, including setup, watchdog and validation. The marker is private to the wrapper invocation.
if [ "${TRAPTEAM_MENU_MOUSE_LOCKED:-}" != 1 ]; then
    exec "$root/../gamelock.sh" env TRAPTEAM_MENU_MOUSE_LOCKED=1 bash "$root/tools/regress/trapteam_menu_mouse.sh" "$@"
fi
. "$root/tools/regress/pid_timer.sh"
iso=${1:-}
[ -f "$iso" ] || { echo "FAIL: usage: $0 <Trap Team disc image>" >&2; exit 2; }
exe=${EXE:-"$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team"}
[ -x "$exe" ] || { echo "FAIL: no game executable under build-trapteam/" >&2; exit 2; }
r=$root/private/runs/trapteam_menu_mouse
rm -rf "${r:?}"; mkdir -p "$r/collection"
fail=0
bad() { echo "FAIL: $*"; fail=1; }

# The title and the notices take a click anywhere: at the bottom of the picture, where no page has an item. The save
# slots are a 2x2 grid of 392x201 boxes from 242/153 (the first focused), the difficulty three 202x405 portraits from
# 325/158 (Medium, the middle one, focused). The steps are far apart: the title takes 30 s or more on a busy machine.
front="click=640/690:35,click=640/690:45,shot=slots:53,move=842/254:55,click=438/254:58,shot=difficulty:66"
front="$front,click=640/360:68,click=640/690:78,click=640/690:88,shot=play:140"
pause="move=1100/360:150,shot=paused:151,click=1100/360:152,shot=turned:155,click=150/360:157,shot=back:160"
echo "== trapteam: the mouse in the game's own menus"
runner=
cancel_run() {
    if [ -n "$runner" ]; then kill -TERM "$runner" 2>/dev/null; wait "$runner" 2>/dev/null; fi
    echo "FAIL: interrupted menu mouse regression"
    exit 1
}
trap cancel_run INT TERM HUP
( cd "$r" && run_timed 175 env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
    SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" \
    SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" \
    SKY_REPORT_DIR="$r/report" SKY_LAUNCHER_LOG_DIR="$r/logs" SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_SKIP_MOVIES=1 \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:450 SKY_TOYBOX_SHOTS="$r/m" SKY_TOYBOX_SCRIPT="$front,$pause" \
    SKY_PRESS="start:146,start:165" nice "$exe" "$iso" > "$r/m.out" 2>&1 ) &
runner=$!
wait "$runner"
run_status=$?
runner=
[ "$run_status" -eq 0 ] || bad "game exited early or watchdog failed (status $run_status)"

log=$r/game.log
# The lines from the first one holding the text $1 to the next one holding $2.
between() { awk -v from="$1" -v to="$2" '!on && index($0, from) { on=1; print; next } on { print } on && index($0, to) { exit }' "$log"; }
grep -aq '^\[level\] level_313_tutorial starts' "$log" || bad "the clicks did not reach the tutorial"
front_accepts=$(between '[level] title starts' '[level] level_313_tutorial starts' | grep -ac '^\[menu\] accept$')
[ "$front_accepts" -ge 5 ] || bad "the clicks were Accept $front_accepts times before the level (5 or more expected)"
grep -aq '^\[menu\] page [0-9A-F]*: 4 items$' "$log" || bad "the save slots were not read"
between 'game menu move 842/254' 'game menu click 438/254' | grep -aq '^\[menu\] focus: ' ||
    bad "hovering the second save slot did not move the focus"
between 'game menu click 438/254' 'game menu click 640/360' | grep -aq '^\[menu\] focus: ' ||
    bad "the click on the first save slot did not move the focus back first"
last_before_pause=$(awk '/^\[toybox\] script: shot=play/ { exit } /^\[mouse\] game menu pointer (on|off)/ { last=$0 } END { print last }' "$log")
[ "$last_before_pause" = "[mouse] game menu pointer off" ] || bad "the pointer was the menu's in play ($last_before_pause)"
pause_lines=$(between '[toybox] script: shot=play' '[toybox] script: shot=back')
echo "$pause_lines" | grep -aq '^\[mouse\] game menu pointer on' || bad "the pause menu did not take the pointer"
echo "$pause_lines" | grep -aq '^\[menu\] page .*, a carousel turned by its sides$' || bad "the pause carousel was not read"
between 'game menu move 1100/360' 'game menu click 1100/360' | grep -aq '^\[menu\] ' &&
    bad "hovering the pause carousel did something"
between 'game menu click 1100/360' 'script: shot=turned' | grep -aq '^\[menu\] turn: d-pad 0008$' ||
    bad "the click on the carousel's right side did not turn it right"
between 'game menu click 150/360' 'script: shot=back' | grep -aq '^\[menu\] turn: d-pad 0004$' ||
    bad "the click on the carousel's left side did not turn it left"
last=$(grep -aE '^\[mouse\] game menu pointer (on|off)' "$log" | tail -1)
[ "$last" = "[mouse] game menu pointer off" ] || bad "the pointer stayed the menu's after leaving the pause menu"
grep -aE '^\[menu\]|^\[level\]|game menu pointer' "$log" | grep -av '^\[menu\]   item'
for shot in slots difficulty play paused turned back; do
    [ -s "$r/m_$shot.png" ] || bad "missing screenshot: $shot"
done
if [ $fail -eq 0 ]; then echo "PASS: trapteam: clicks from the title to the tutorial; the pause carousel turned by clicks (under $r)"; else echo "FAIL (logs in $r)"; fi
exit $fail
