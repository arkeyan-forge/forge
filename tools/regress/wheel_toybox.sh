#!/bin/bash
# Giants' Toy Box and the quick swap wheel (docs/runtime/host/toybox_ux.md sections 3 and 5): a toy the game cannot read is not on
# its shelf (look at shots_home.png: two cards, no "Not for this game"); the plain toy is one row of "Add a figure" and only the versions Giants tells apart come after it; a figure the
# player adds to the wheel ("On the wheel" on its page) is offered by the wheel although this game never took it, and holding
# Tab, aiming with 1 and releasing puts it on Player 1. The collection holds Spyro (played), Whirlwind and a Swap Force Scratch
# (3000: Giants does not read it). The run starts on an empty portal, takes the Toy Box with F6, puts Whirlwind on Player 1, adds
# Spyro to the wheel from its page, closes the Toy Box and takes the wheel with Tab. Screenshots go to <run>/shots_*.png.
#
#   GIANTS_ISO=<Giants .iso> tools/regress/wheel_toybox.sh [<run name>]        (about 1.5 minutes; build-giants/)
#
# Isolated like every run (docs/getting-started/testing.md); kills only its own pid. Everything is under private/runs/<name>. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-wheel_toybox}
iso=${GIANTS_ISO:?set GIANTS_ISO to the Giants disc image}
exe="${LIVE_EXE:-$root/build-giants/ArkeyanRuntime/Skylanders Giants}"
make=${FIGURE_TOOL:-$root/build/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R/collection" "$R/save" "$R/data" "$R/home"
"$make" --make "$R/collection/new_16.sky" 16 xp=50000 > /dev/null || { echo "cannot make the dump of Spyro"; exit 2; }
"$make" --make "$R/collection/new_0.sky" 0 > /dev/null || { echo "cannot make the dump of Whirlwind"; exit 2; }
"$make" --make "$R/collection/new_3000.sky" 3000 > /dev/null || { echo "cannot make the dump of Scratch"; exit 2; }

# F6 opens the Toy Box (the cursor is on Spyro, the played figure first), F2 opens its page, Down x2 to "On the wheel", Return adds
# it; Escape back to the shelf; Right then Return puts the second card (Whirlwind) on Player 1; Add a figure (the button at
# 1209/99), Return opens Spyro's versions; Escape x3 out of the Toy Box; then the wheel: Tab held, 1 aims place 1, Tab released.
script="F6:25,shot=home:27,F2:28,Down:29,Down:30,Return:31,shot=page:32,Escape:33"
script+=",Right:35,Return:36,click=1209/99:38,shot=add:39,Return:40,shot=versions:41,Escape:42,Escape:43,Escape:44"
script+=",down=Tab:47,shot=wheel:48,1:48.5,shot=aimed:49,up=Tab:50,shot=swapped:55"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null HOME="$R/home" CFFIXED_USER_HOME="$R/home" \
  SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" SKY_COLLECTION="$R/collection" \
  SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated \
  SKY_PORTAL_LOG=1 SKY_TOYBOX_SCRIPT="$script" SKY_TOYBOX_SHOTS="$R/shots" \
  nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$R/log" 2>&1 &
pid=$!; echo $pid > "$R/pid"
(sleep "${WHEEL_SECONDS:-85}"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) > /dev/null 2>&1 &
wait $pid 2>/dev/null

fail=0
check() { if grep -q -- "$2" "$R/log"; then echo "ok:   $1"; else echo "FAIL: $1 ($2)"; fail=1; fi; }
check "Whirlwind on Player 1" "P1: Whirlwind on slot"
check "Spyro added to the wheel" "Spyro is on the wheel"
check "the wheel opened for Player 1" "wheel for P1"
check "the wheel put Spyro on Player 1" "P1: Whirlwind → Spyro"
grep -q '"wheel"' "$R/collection/collection.json" || { echo "FAIL: collection.json has no wheel"; fail=1; }
[ $fail = 0 ] && echo "wheel_toybox: all checks pass"
exit $fail
