#!/bin/bash
# Proves "Nightmare from the start" in a game's real difficulty menu (engines/alchemy/nightmare.cpp; Swap Force:
# games/swapforce/docs/history.md, "Nightmare difficulty"; Imaginators: games/imaginators/docs/status.md, "Nightmare"): a new
# game reaches the difficulty menu and highlights Nightmare; the game asks the hook (Swap Force when the row is highlighted,
# Imaginators when the menu opens). Run 1, the option off: the game's own answer, locked ("the save says locked, the option
# is off: locked"). Run 2, SKY_NIGHTMARE_FROM_START=1, the same new save: offered ("the option is on: offered"), Nightmare
# accepted with A, the first level starts; the game still read the save as "locked": nothing was written to it. The frame of
# each menu with Nightmare selected is kept (private/regress/nightmare/<game>/off|on/menu.ppm: the padlock, or none).
#
#   SWAPFORCE_ISO=<disc> tools/regress/nightmare_menu.sh [swapforce] [seconds per run, default 150]
#   IMAGINATORS_ISO=<disc> tools/regress/nightmare_menu.sh imaginators [seconds per run, default 150]
#   EXE=<game executable>   default build-<game>/ArkeyanRuntime/Skylanders <Name>
#
# Isolated like every run (scratch settings, saves, data), through ../gamelock.sh when it exists; a press file steers it:
#   swapforce    A three times from the front end (title, Story Mode, the NEW slot), Down to the fourth row;
#   imaginators  Start to the licence, Y, then A (the slot, New Game) until the menu asks, two Right to the fourth portrait.
# Then the frame, and (option on) A. Exit code 0: PASS (the reason is the last line).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)

# --drive <run dir> <game> <option 0|1> <pid> <seconds>: the presses of one run, inside the locked shell.
if [ "${1:-}" = "--drive" ]; then
    r=$2 game=$3 option=$4 pid=$5 seconds=$6
    waitlog() { for _ in $(seq "$2"); do kill -0 "$pid" 2>/dev/null || return 1; grep -aq -- "$1" "$r/game.log" 2>/dev/null && return 0; sleep 1; done; return 1; }
    press() { echo "$1" >> "$r/press.txt"; sleep "$2"; }
    frame() { cp "$(ls -t "$r"/shot_*.ppm | sed -n 2p)" "$r/menu.ppm"; }
    case $game in
    swapforce)
        # The front end is up when it opens collision_bank.pak (~13 s on a quiet machine). A reaches the title, Story Mode
        # and the NEW slot, a few seconds apart; the difficulty menu follows. Down until the game asks the hook (Nightmare
        # highlighted): a press while the menu still slides in is lost.
        if waitlog 'collision_bank.pak' "$seconds"; then
            sleep 5
            for _ in 1 2 3; do press a 5; done
            sleep 2
            for _ in 1 2 3 4 5; do grep -aq '\[nightmare\] the menu asked' "$r/game.log" && break; press down 2; done
            sleep 5; frame
            if [ "$option" = 1 ]; then press a 0; waitlog 'tutorial_a.pak' 90; sleep 3; fi
        fi ;;
    imaginators)
        # The title takes Start; the licence (softwarelicense.pak) is accepted with Y; A on the slot screen's first empty
        # slot opens the difficulty menu, which asks the hook as it opens (Medium highlighted, Nightmare the fourth).
        if waitlog 'collision_bank.pak' "$seconds"; then
            sleep 15
            for _ in $(seq 20); do grep -aq 'softwarelicense.pak' "$r/game.log" && break; press start 5; done
            sleep 8; press y 3; press y 3
            for _ in $(seq 12); do grep -aq '\[nightmare\] the menu asked' "$r/game.log" && break; press a 5; done
            sleep 6
            for _ in 1 2; do press right 3; done
            sleep 4; frame
            if [ "$option" = 1 ]; then press a 0; waitlog 'level_508.pak' 60; sleep 3; fi
        fi ;;
    esac
    kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null
    exit 0
fi

game=swapforce
case ${1:-} in swapforce | imaginators) game=$1; shift ;; esac
seconds=${1:-150}
case $game in
swapforce) iso=${SWAPFORCE_ISO:-} name="Swap Force" level='tutorial' movies=SKY_SKIP_MOVIES=1 ;;
imaginators) iso=${IMAGINATORS_ISO:-} name="Imaginators" level='level_508.pak' movies= ;; # Start skips its logos
esac
upper=$(echo "$game" | tr a-z A-Z)
[ -f "$iso" ] || { echo "usage: ${upper}_ISO=<disc> $0 $game [seconds]" >&2; exit 2; }
exe="${EXE:-$root/build-$game/ArkeyanRuntime/Skylanders $name}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
self=$root/tools/regress/nightmare_menu.sh
scratch=$root/private/regress/nightmare/$game
rm -rf "${scratch:?}"; mkdir -p "$scratch"
fail=0

# run <label> <on|off>
run() {
    label=$1; r=$scratch/$label; mkdir -p "$r/home" "$r/save" "$r/figures"; : > "$r/press.txt"
    option=0; [ "$2" = on ] && option=1
    cp "$root/private/figures/new_14.sky" "$r/figures/" 2>/dev/null
    local cmd="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA='$r/data' SKY_SETTINGS='$r/none.ini' SKY_LAUNCHER_INI='$r/none.ini' SKY_COLLECTION='$r/figures' \
        SKY_FIGURES='$r/figures' SKY_SAVE_DIR='$r/save' SKY_LOG='$r/game.log' HOME='$r/home' CFFIXED_USER_HOME='$r/home' \
        SKY_LAUNCHER_NO_UPDATE_CHECK=1 $movies SKY_PRESS_FILE='$r/press.txt' SKY_PORTAL=emulated \
        SKY_PORTAL_PLACE=new:9 SKY_TRACE_FILES=1 SKY_NIGHTMARE_FROM_START=$option SKY_GPU_SNAPSHOT='$r/shot' \
        SKY_GPU_SNAPSHOT_FROM=0 SKY_GPU_SNAPSHOT_EVERY=60 nice '$exe' --vsync 0 --fps-limit 30 '$iso' > '$r/run.log' 2>&1 & pid=\$!
        (exec 3>&-; sleep $((seconds + 120)); kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
        '$self' --drive '$r' $game $option \$pid $seconds
        wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$cmd"; else sh -c "$cmd"; fi
    [ -s "$r/game.log" ] || { echo "FAIL: $label: no game log"; fail=1; return 1; }
}
expect() { grep -qF -- "$2" "$scratch/$1/game.log" || { echo "FAIL: $1: missing \"$2\""; fail=1; }; }

echo "== $game: option off (the game's own lock)"
run off off && {
    expect off '[nightmare] the menu asked: the save says locked, the option is off: locked'
    [ -s "$scratch/off/menu.ppm" ] || { echo "FAIL: off: no frame of the menu"; fail=1; }
}
echo "== $game: option on (a new game, nothing beaten)"
run on on && {
    expect on '[nightmare] offered from the start'
    expect on '[nightmare] the menu asked: the save says locked, the option is on: offered'
    [ -s "$scratch/on/menu.ppm" ] || { echo "FAIL: on: no frame of the menu"; fail=1; }
    grep -qi -- "$level" "$scratch/on/game.log" || { echo "FAIL: on: Nightmare was accepted but the first level did not start"; fail=1; }
    cmp -s "$scratch/off/menu.ppm" "$scratch/on/menu.ppm" && { echo "FAIL: the menu looks the same with the option on and off"; fail=1; }
}
if [ $fail -eq 0 ]; then echo "PASS: $game: Nightmare is locked by default and offered with the option, in the menu (frames under $scratch)"; else echo "FAIL (logs in $scratch)"; fi
exit $fail
