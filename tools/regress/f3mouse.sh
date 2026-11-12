#!/bin/bash
# The mouse in the F3 menu, its Commands page and the Toy Box (docs/runtime/host/gamemenu.md, "Commands"; toybox_ux.md), through
# the Toy Box's own script (SKY_TOYBOX_SCRIPT: move=, click=, rclick=, wheel=). Spyro's Adventure, 24 figures in a scratch
# collection. F3 (the side panel); the pointer onto Settings (hover: the focus), a click (the page); nine wheel ticks down (the
# focus to the FPS counter, the list scrolls), the pointer onto VSync then the FPS counter (each takes the focus), a click (the
# counter goes on), a wheel tick up; a click on the rail's Controls; on the page the next page's button, a click on the Mouse
# row (the switch goes on), the first page again, a click on B's mouse cell, a right click (B gets the right button, X loses
# it), Escape, F3; F6 opens the Toy Box, the pointer off the cards, then onto a card (it takes the focus), a click on it, three
# wheel ticks down (the cursor moves a row each), F6 closes it. Coordinates: look.h's layout (docs/runtime/host/gamemenu.md).
# Checks: the log lines, the focus after each scripted action, [ssa.input] mouse=1, mouse_b=Mouse Right, mouse_x=none, the
# running game taking them; shots.
#
#   tools/regress/f3mouse.sh <disc image>        (Spyro's Adventure; SKY_LANGUAGE=fr for French)
#   EXE=<game executable>   default build/ArkeyanRuntime/Skylanders Spyro's Adventure
#
# Everything is under private/runs/commands/mouse/. Automatically wraps the whole run in ../gamelock.sh.
set -u
set -o pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
# Lock the entire run, including setup, watchdog and validation. The marker is private to the wrapper invocation.
if [ "${F3MOUSE_LOCKED:-}" != 1 ]; then
    exec "$root/../gamelock.sh" env F3MOUSE_LOCKED=1 bash "$root/tools/regress/f3mouse.sh" "$@"
fi
. "$root/tools/regress/pid_timer.sh"
iso=${1:-}
[ -f "$iso" ] || { echo "FAIL: usage: $0 <Spyro's Adventure disc image>" >&2; exit 2; }
exe=${EXE:-"$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"}
make=${FIGURE_TOOL:-$root/build/ArkeyanRuntime/ArkeyanFigureTest}
[ -x "$exe" ] && [ -x "$make" ] || { echo "FAIL: no game executable or ArkeyanFigureTest under build/" >&2; exit 2; }
r=$root/private/runs/commands/mouse
rm -rf "${r:?}"; mkdir -p "$r/collection"
for toy in $(seq 0 23); do
    "$make" --make "$r/collection/new_$toy.sky" "$toy" > /dev/null || { echo "FAIL: cannot make the dump of $toy" >&2; exit 2; }
done
fail=0
bad() { echo "FAIL: $*"; fail=1; }

menu="F3:30,shot=menu:31,move=1000/380:31.5,shot=hover:32,click=1000/380:32.5,shot=settings:33"
ticks=$(awk 'BEGIN { for (i = 0; i < 9; i++) printf ",wheel=down:%.1f", 33.2 + i * 0.1 }')
menu="$menu$ticks,move=500/365:34.2,move=500/623:34.5,shot=settings_fps:34.8,click=500/623:35,wheel=up:35.3,click=116/245:35.6"
page="click=1224/52:36,click=500/287:36.3,click=1180/52:36.6,shot=page:36.9,move=1146/455:37.2,click=1146/455:37.5"
page="$page,shot=waitmouse:37.8,rclick=600/600:38.1,move=630/363:38.4,shot=mouse:38.7,Escape:39,F3:39.5"
toybox="F6:41,shot=toybox:43,move=640/30:43.2,move=838/307:43.5,shot=card_hover:44,click=838/307:44.5,shot=card:45,wheel=down:46,wheel=down:46.5"
toybox="$toybox,wheel=down:47,shot=scrolled:48,F6:49"
echo "== ssa: the mouse in the menus"
runner=
cancel_run() {
    if [ -n "$runner" ]; then kill -TERM "$runner" 2>/dev/null; wait "$runner" 2>/dev/null; fi
    echo "FAIL: interrupted mouse regression"
    exit 1
}
trap cancel_run INT TERM HUP
( cd "$r" && run_timed 56 env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
    SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" \
    SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" \
    SKY_REPORT_DIR="$r/report" SKY_LAUNCHER_LOG_DIR="$r/logs" SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_SKIP_MOVIES=1 \
    SKY_PORTAL=emulated SKY_TOYBOX_SHOTS="$r/m" SKY_TOYBOX_SCRIPT="$menu,$page,$toybox" \
    nice "$exe" "$iso" > "$r/m.out" 2>&1 ) &
runner=$!
wait "$runner"
run_status=$?
runner=
[ "$run_status" -eq 0 ] || bad "game exited early or watchdog failed (status $run_status)"

log=$r/game.log
ini=$r/launcher.ini
section() { awk '/^\[/ { in_it = ($0 == "['"$1"']") } in_it' "$ini"; }
focus_after() {
    awk -v action="[toybox] script: $1" '
        $0 == action { found=1; next }
        found && /^\[toybox\] script: focus:/ { print; exit }
        found && /^\[toybox\] script:/ { exit }
    ' "$log"
}
focus_after move=1000/380 | grep -q 'menu row 4,.*hover menu=4$' || bad "the pointer moving onto Settings did not focus it"
grep -q '^\[gamemenu\] settings opened' "$log" || bad "a click on Settings did not open the page"
focus_after wheel=down | grep -q 'settings row 1,' || bad "a wheel tick did not move the Settings focus down"
focus_after move=500/365 | grep -q 'settings row 6,.*hover settings=6$' || bad "the pointer moving onto VSync did not focus it"
focus_after move=500/623 | grep -q 'settings row 9,.*hover settings=9$' || bad "the pointer moving onto the FPS row did not focus it"
grep -q '^\[gamemenu\] show_fps = 1' "$log" || bad "a click on the FPS counter row did not turn it on"
focus_after click=500/623 | grep -q 'settings row 9,' || bad "the click did not keep the FPS counter row"
focus_after wheel=up | grep -q 'settings row 8,' || bad "a wheel tick up did not move the focus up"
focus_after move=1146/455 | grep -q 'hover commands=7/2$' || bad "hover did not hit the Commands mouse cell"
grep -q '^\[gamemenu\] commands opened' "$log" || bad "a click on the rail's Controls did not open the page"
grep -q '^\[gamemenu\] mouse = 1' "$log" || bad "a click on the Mouse row did not turn the mouse on"
grep -q '^\[gamemenu\] commands: waiting for a mouse button for b' "$log" || bad "a click on B's mouse cell did not wait for a button"
grep -q '^\[gamemenu\] mouse_b = Mouse Right (mouse_x = none)' "$log" || bad "the right click was not given to B (taken from X)"
grep -q '^\[input\] bindings in use now: .*mouse=1' "$log" || bad "the running game did not take the switch"
grep -q '^\[input\] bindings in use now: .*mouse_b=Mouse Right' "$log" || bad "the running game did not take the binding"
for line in mouse=1 "mouse_b=Mouse Right" mouse_x=none; do
    section ssa.input | grep -qx "$line" || bad "[ssa.input] $line is not in launcher.ini"
done
focus_after move=838/307 | grep -q 'card 3, home grid 3 seat 0, hover card=3$' || bad "the pointer moving onto the fourth card did not focus it"
clicked=$(focus_after click=838/307 | sed -E 's/.*card ([0-9]+),.*/\1/')
scrolled=$(grep '^\[toybox\] script: focus:' "$log" | tail -1 | sed -E 's/.*card ([0-9]+),.*/\1/')
focus_after click=838/307 | grep -q 'screen 0,' || bad "the Toy Box was not open for the card click"
[ "$clicked" = 3 ] || bad "the click did not select the fourth card (card $clicked)"
[ -n "$scrolled" ] && [ "$scrolled" -gt "$clicked" ] 2>/dev/null || bad "the wheel did not move down the list (card $scrolled)"
grep -E '^\[gamemenu\]|^\[input\] bindings|script: focus' "$log"
for shot in menu hover settings settings_fps page waitmouse mouse toybox card_hover card scrolled; do
    [ -s "$r/m_$shot.png" ] || bad "missing screenshot: $shot"
done
if [ $fail -eq 0 ]; then echo "PASS: ssa: clicks, hover and the wheel in the F3 panel, Settings, the rail, Controls and the Toy Box; the mouse written and in use (under $r)"; else echo "FAIL (logs in $r)"; fi
exit $fail
