#!/bin/bash
# The Toy Box's Xbox-style focus through the real game (docs/runtime/host/toybox_ux.md, "Navigation"): rows (seats, the
# search and Filters bar, the grid), Up/Down keeping the column, each row remembering its place, LB/RB, Tab, no wrap, and
# the pointer moving the focus only while it moves. Trap Team, ten scratch figures (two grid rows), keys and pointer
# through SKY_TOYBOX_SCRIPT; every scripted key logs "[toybox] script: focus: ... home <row> <place> seat <n>".
#
#   TRAPTEAM_ISO=<Trap Team .iso> tools/regress/toybox_nav.sh [<run name>]     (about 75 s; build-trapteam/)
#
# Isolated like every run (docs/getting-started/testing.md), through ../gamelock.sh when it exists; kills only its own pid.
# Everything is under private/runs/<name>; the pictures are <name>/shots_*.png. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-toybox_nav}
iso=${TRAPTEAM_ISO:?set TRAPTEAM_ISO to the Trap Team disc image}
exe="${LIVE_EXE:-$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team}"
make=${FIGURE_TOOL:-$root/build-trapteam/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/runs/$name
[ -d "$R" ] && { echo "$R exists: give another run name"; exit 2; }
mkdir -p "$R/collection" "$R/save" "$R/data" "$R/home"
for toy in 450 452 519 482 0 4 8 14 16 19; do
    "$make" --make "$R/collection/new_$toy.sky" "$toy" > /dev/null || { echo "cannot make the dump of $toy"; exit 2; }
done

# Ten cards and the + card, six columns (the 0.9.0 layout beside the rail): grid row 0 holds cards 0-5, row 1 cards 6-10
# (columns 0-4). Card c of row 0 is centred at x = 344 + 164.7 c, y = 307 on the 1280x720 canvas; Player 1's seat at 340/124,
# Filters at 1178/210.
s="F6:35,Right:36,Right:36.2,Right:36.4,shot=01_grid_card3:36.8"
s="$s,Up:37,shot=02_bar_search:37.4,Right:37.6,Up:38,shot=03_seats_remembered:38.4,Down:38.6,shot=04_bar_filters:39"
s="$s,Down:39.2,shot=05_grid_card3_again:39.6,Down:39.8,Up:40,Right:40.2,Right:40.4,Right:40.6,Down:40.8"
s="$s,shot=06_short_row:41.2,Up:41.4,shot=07_column_kept:41.8,Right:42,Left:42.2,Left:42.4,Left:42.6,Left:42.8"
s="$s,Left:43,Left:43.2,Left:43.4,PageUp:43.6,PageDown:43.8,shot=08_rb_player2:44.2,Up:44.4,Up:44.6,Left:44.8"
s="$s,Left:45,Right:45.2,Right:45.4,Right:45.6,Right:45.8,Right:46,Right:46.2,shot=09_seats_end:46.6,Down:46.8,Up:47"
s="$s,PageUp:47.2,PageUp:47.4,PageUp:47.6,PageUp:47.8,Tab:48,shift+Tab:48.2,Tab:48.4,Tab:48.6,shot=10_tab_grid:49"
s="$s,move=1003/307:49.4,shot=11_hover_card4:49.8,Left:50,move=1004/308:50.2,move=340/124:50.4,shot=12_still_pointer:50.8"
s="$s,move=1170/216:51,Escape:51.5"
run="env HOME='$R/home' CFFIXED_USER_HOME='$R/home' SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
  SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' \
  SKY_COLLECTION='$R/collection' SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated \
  SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_TOYBOX_SCRIPT='$s' SKY_TOYBOX_SHOTS='$R/shots' \
  nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$R/log' 2>&1 3>&- & pid=\$!
  ( exec 3>&-; sleep ${TOYBOX_NAV_SECONDS:-60}; kill \$pid; sleep 3; kill -9 \$pid ) > /dev/null 2>&1 & timer=\$!
  wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi

log=$R/game.log; [ -f "$log" ] || log=$R/log
fails=0
# focus <n>: the focus line logged after the n-th scripted key or pointer item (1-based, in script order).
focus() { grep '^\[toybox\] script: focus:' "$log" | sed -n "$1p" | sed -E 's/.*home ([a-z]+ -?[0-9]+ seat [0-9]+).*/\1/'; }
hover() { grep '^\[toybox\] script: focus:' "$log" | sed -n "$1p" | sed -E 's/.*hover (.*)$/\1/'; }
expect() { # <step> <wanted "row place seat n"> <what>
    local got; got=$(focus "$1")
    if [ "$got" = "$2" ]; then echo "ok:   $3"; else echo "FAIL: $3 (wanted '$2', saw '$got')"; fails=$((fails + 1)); fi
}
expect 1 "grid 0 seat 0" "opens on the grid, first card"
expect 4 "grid 3 seat 0" "Right x3: card 3"
expect 5 "bar 0 seat 0" "Up from the first row: the bar, the search field nearest above"
expect 7 "seats 0 seat 0" "Up from the bar: the seats row at its place (Player 1)"
expect 27 "bar 1 seat 1" "Up from the grid: the bar at its place (Filters)"
expect 28 "seats 1 seat 1" "Up: the seats row at its place (Player 2)"
expect 8 "bar 1 seat 0" "Down: the bar remembers Filters"
expect 9 "grid 3 seat 0" "Down: the grid remembers card 3"
expect 10 "grid 9 seat 0" "Down from column 3: the card under it"
expect 15 "grid 10 seat 0" "Down from column 5 to the short row: its nearest card (column 4)"
expect 16 "grid 5 seat 0" "Up again: column 5 kept"
expect 24 "grid 0 seat 0" "Left at column 0 stays: no wrap onto another row"
expect 25 "grid 0 seat 0" "LB at Player 1: no wrap to the last seat"
expect 26 "grid 0 seat 1" "RB: Player 2, still in the grid"
expect 29 "seats 0 seat 0" "Left on the seats: Player 1"
expect 30 "seats 0 seat 0" "Left at Player 1 stays: no wrap to Manage"
expect 35 "seats 5 seat 4" "Right past the last seat: Add a figure"
expect 36 "seats 5 seat 4" "Right on Add a figure stays"
expect 37 "seats 6 seat 4" "Down from Add a figure: Manage, under it"
expect 38 "seats 5 seat 4" "Up: Add a figure"
expect 39 "seats 3 seat 3" "LB on the seats row: the seat before"
expect 42 "seats 0 seat 0" "LB x4: Player 1"
expect 43 "bar 1 seat 0" "Tab: the next row, the bar (Filters, remembered)"
expect 44 "seats 0 seat 0" "Shift+Tab: back to the seats"
expect 46 "grid 5 seat 0" "Tab Tab: the grid, rebuilt for Player 1, on the card nearest under Filters"
expect 47 "grid 4 seat 0" "the pointer moving onto a card focuses it"
expect 48 "grid 3 seat 0" "a key moves the focus away"
expect 49 "grid 3 seat 0" "the pointer moving inside the same card does not take it back"
expect 50 "grid 3 seat 0" "the pointer on a seat does not change the focus or the seat"
expect 51 "bar 1 seat 0" "the pointer moving onto Filters focuses it"
[ "$(hover 47)" = "card=4" ] && echo "ok:   hover reports card 4" || { echo "FAIL: hover $(hover 47)"; fails=$((fails + 1)); }
shots=$(ls "$R"/shots_*.png 2>/dev/null | wc -l | tr -d ' ')
if [ "$shots" -ge 12 ]; then echo "ok:   $shots pictures"; else echo "FAIL: $shots pictures (wanted 12)"; fails=$((fails + 1)); fi
[ "$fails" = 0 ] && echo "Toy Box navigation check: PASS" || echo "Toy Box navigation check: $fails FAILED (logs in $R)"
exit $((fails != 0))
