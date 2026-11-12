#!/bin/bash
# The launcher's Toy Box window drives the running game (docs/runtime/host/toybox_link.md): the launcher starts the game
# (--game <id> --play) with "Open with the game" ticked, so it opens its Toy Box window; the window (its test script,
# SKY_LAUNCHER_TEST_TOYBOX) puts a figure on Player 1 and another on Player 2 through the link, swaps them (Player 1's
# figure picked for Player 2, as a drop does), takes one off and, where the game has one, fills its own seat (a swapper's
# two tags, a trap, a vehicle); the game's log must show each change on its portal and the game reading each figure, the
# window's log each answer, and the window must close when the game ends. A picture of the window at each step goes to
# <run>/<game>/shots, beside the game's own picture of the same moment in <run>/<game>/beside. With MIXED=1 it also runs the mixed portal on Giants: a fake real portal (SKY_PORTAL=fake-real) whose
# Spyro arrives at 40 s, after the window put a virtual Whirlwind on Player 1, and the reverse on Spyro's Adventure (a real
# Spyro from the start is Player 1, the window puts a virtual Whirlwind on Player 2).
#
#   LAUNCHER=<Arkeyan Launcher executable> [MIXED=1] tools/regress/toybox_window.sh [<game> ...]   (default: the six games)
#   CHECK_ONLY=1 checks the runs already in private/runs/toybox_window again, starting nothing (LAUNCHER can be anything).
#   the discs: SSA_ISO, GIANTS_ISO, SWAPFORCE_ISO, TRAPTEAM_ISO, SUPERCHARGERS_ISO, IMAGINATORS_ISO (a game without one: SKIP)
#
# About 2 to 5 minutes a game. Isolated like every run (docs/getting-started/testing.md): scratch collection, saves, data,
# settings, launcher data and desktop; offscreen video, disk audio; kills only its own processes, by PID. Each game run
# goes through ../gamelock.sh when it exists. Everything is under private/runs/toybox_window. Exit 0: every check passes.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
launcher=${LAUNCHER:?set LAUNCHER to the Arkeyan Launcher executable (its build/ArkeyanLauncher.app/Contents/MacOS/ArkeyanLauncher)}
[ -x "$launcher" ] || { echo "no launcher at $launcher"; exit 2; }
games=("$@"); [ ${#games[@]} -eq 0 ] && games=(ssa giants swapforce trapteam superchargers imaginators)
work=$root/private/runs/toybox_window
mkdir -p "$work"
fails=0; skipped=0

exe_of() { # <game>: the built runtime
    case $1 in
        ssa) echo "$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure.app/Contents/MacOS/Skylanders Spyro's Adventure" ;;
        giants) echo "$root/build-giants/ArkeyanRuntime/Skylanders Giants.app/Contents/MacOS/Skylanders Giants" ;;
        swapforce) echo "$root/build-swapforce/ArkeyanRuntime/Skylanders Swap Force.app/Contents/MacOS/Skylanders Swap Force" ;;
        trapteam) echo "$root/build-trapteam/ArkeyanRuntime/Skylanders Trap Team.app/Contents/MacOS/Skylanders Trap Team" ;;
        superchargers) echo "$root/build-superchargers/ArkeyanRuntime/Skylanders SuperChargers.app/Contents/MacOS/Skylanders SuperChargers" ;;
        imaginators) echo "$root/build-imaginators/ArkeyanRuntime/Skylanders Imaginators.app/Contents/MacOS/Skylanders Imaginators" ;;
    esac
}

# run <game> <run dir> <seconds> <window script> <env...>: the launcher plays the game; both stop by the timer, by PID.
run() {
    local game=$1 r=$2 secs=$3 script=$4; shift 4
    local exe; exe=$(exe_of "$game")
    printf '[%s]\niso=%s\n' "$game" "$iso" > "$r/launcher.ini"
    printf 'with_game=1\n' > "$r/ldata/toybox-window.ini"
    export TOYBOX_WINDOW_RUNTIME="$exe" # through the environment: "Spyro's Adventure" has an apostrophe
    local inner="cd '$r' && env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKY_SETTINGS='$r/launcher.ini' SKY_LAUNCHER_INI='$r/launcher.ini' SKY_COLLECTION='$r/collection' SKY_SAVE_DIR='$r/save' \
        SKYLANDERS_DATA='$r/data' SKY_LAUNCHER_DATA='$r/ldata' SKY_LOG='$r/game.log' HOME='$r/home' CFFIXED_USER_HOME='$r/home' \
        SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_LAUNCHER_DESKTOP='$r/desktop' SKY_SKIP_MOVIES=1 SKY_LAUNCHER_RUNTIME=\"\$TOYBOX_WINDOW_RUNTIME\" \
        SKY_LAUNCHER_TEST_TOYBOX='$script' SKY_LAUNCHER_TOYBOX_SHOTS='$r/shots' $* \
        nice '$launcher' --game $game --play > '$r/launcher.log' 2>&1 & pid=\$!
      sleep $secs
      for child in \$(pgrep -P \$pid); do for grandchild in \$(pgrep -P \$child); do kill \$grandchild; done; kill \$child; done
      sleep 4; kill \$pid; sleep 2
      for child in \$(pgrep -P \$pid); do kill -9 \$child; done; kill -9 \$pid; wait \$pid"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$inner" > /dev/null 2>&1
    else sh -c "$inner" > /dev/null 2>&1; fi
}

beside() { # <run dir>: each window picture with the game's of the same moment, side by side (<run>/beside/<step>.png)
    python3 - "$1" <<'PY' || true
import os, sys
from PIL import Image
r = sys.argv[1]
os.makedirs(os.path.join(r, "beside"), exist_ok=True)
for name in sorted(os.listdir(os.path.join(r, "shots"))) if os.path.isdir(os.path.join(r, "shots")) else []:
    step = name[:-4]
    game = os.path.join(r, "game_" + step + ".png")
    if not name.endswith(".png") or not os.path.exists(game):
        continue
    window, frame = Image.open(os.path.join(r, "shots", name)).convert("RGB"), Image.open(game).convert("RGB")
    height = max(window.height, 540)
    frame = frame.resize((round(frame.width * height / frame.height), height))
    window = window.resize((round(window.width * height / window.height), height))
    out = Image.new("RGB", (frame.width + 16 + window.width, height), (16, 16, 20))
    out.paste(frame, (0, 0))
    out.paste(window, (frame.width + 16, 0))
    out.save(os.path.join(r, "beside", step + ".png"))
PY
}

drive_imaginators() { # <game log> <press file>: Start until the licence, Y, then A (as tools/regress/imaginators_hub.sh)
    local log=$1 presses=$2 i
    until [ -e "$log" ]; do sleep 1; done
    sleep 20
    for i in $(seq 1 30); do grep -a -q 'softwarelicense.pak' "$log" && break; echo start >> "$presses"; sleep 5; done
    if grep -a -q 'softwarelicense.pak' "$log"; then sleep 8; echo y >> "$presses"; sleep 3; echo y >> "$presses"; fi
    for i in $(seq 1 80); do sleep 4; echo a >> "$presses"; done
}

check() { # <game> <what> <file> <fixed text> [<count>]
    local n; n=$(grep -acF -- "$4" "$3" 2>/dev/null); n=${n:-0}
    if [ "$n" -ge "${5:-1}" ]; then echo "ok:   $1: $2"
    else echo "FAIL: $1: $2 (wanted '$4' x${5:-1} in $(basename "$3"), saw $n)"; fails=$((fails + 1)); fi
}

for game in "${games[@]}"; do
    case $game in
        ssa) var=SSA_ISO ;; giants) var=GIANTS_ISO ;; swapforce) var=SWAPFORCE_ISO ;; trapteam) var=TRAPTEAM_ISO ;;
        superchargers) var=SUPERCHARGERS_ISO ;; imaginators) var=IMAGINATORS_ISO ;;
        *) echo "unknown game: $game"; exit 2 ;;
    esac
    iso=${!var:-}
    if [ -z "$iso" ] || [ ! -f "$iso" ] || [ ! -x "$(exe_of "$game")" ]; then
        echo "SKIP: $game: set $var to its disc and build its runtime"; skipped=$((skipped + 1)); continue
    fi
    r=$work/$game
    [ "${CHECK_ONLY:-0}" = 1 ] || { rm -rf "${r:?}"; mkdir -p "$r/collection" "$r/save" "$r/data" "$r/ldata" "$r/home" "$r/desktop"; }
    make=$root/build-giants/ArkeyanRuntime/ArkeyanFigureTest; [ -x "$make" ] || make=$root/build/ArkeyanRuntime/ArkeyanFigureTest
    mk() { [ "${CHECK_ONLY:-0}" = 1 ] && return 0; "$make" --make "$r/collection/$1.sky" "$2" > /dev/null || { echo "cannot make $1"; exit 2; }; }
    # Per game: the press script that takes the title to where it reads the portal, when to start (s after the link), the
    # run's length, the two players' figures (file name, the name the log gives), the player seats, and the game's own seat.
    p2=1; extra=""; t0=8; secs=110; first=""; k=1 # k: the steps' spacing (Swap Force reads a swap at its next prompt, ~8 s)
    a=spyro; an=Spyro; b=whirlwind; bn=Whirlwind; mk spyro 16; mk whirlwind 0
    case $game in
        ssa) press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27"
             env_extra="SKY_LEVEL=level_027"; t0=35 ;;
        giants) press=$(python3 -I "$root/tools/regress/giants_press.py" 130); env_extra=""; t0=45; secs=130 ;;
        swapforce) press="a:14,a:16,a:18,a:20,a:22,a:24,a:26"; env_extra=""; t0=30; k=2; secs=130
             mk new_2004 2004; mk new_1004 1004; a=new_2004; an="Blast Zone"
             [ "${CHECK_ONLY:-0}" = 1 ] || printf '{\n  "format": 1,\n  "figures": {\n    "new_2004.sky": { "partner": "new_1004.sky", "origin": "created" },\n    "new_1004.sky": { "partner": "new_2004.sky", "origin": "created" }\n  }\n}\n' > "$r/collection/collection.json" ;;
        trapteam) press=$(python3 -I "$root/tools/regress/giants_press.py" 125); env_extra=""; t0=35; secs=140
             mk fling 452; a=fling; an="Fling Kong"
             if [ "${CHECK_ONLY:-0}" = 1 ]; then trap=$(cd "$r/collection" && ls | grep -v '^fling\|^spyro\|^whirlwind\|\.json$\|\.lock$' | head -1)
             else
                 traps=.scratch/toybox-window-traps; rm -rf "${root:?}/$traps"
                 (cd "$root" && "$make" --make-trap-pair "$traps") > /dev/null || { echo "cannot make the traps"; exit 2; }
                 trap=$(ls "$root/$traps" | head -1); mv "$root/$traps"/* "$r/collection/"; rm -rf "${root:?}/$traps"
             fi
             extra="2:$trap:Trap" ;;
        superchargers) # its intro reads the vehicle at ~182 s and the Skylander at ~196 s (games/superchargers/docs/toybox.md):
             # both are put on from the window at the start, the rest once it plays.
             press=$(awk 'BEGIN { for (t = 60; t <= 236; t += 4) printf "%sa:%d", (t > 60 ? "," : ""), t }')
             env_extra=""; t0=215; secs=300; k=2; p2=2; first="1:put 1 crypt.sky|3:put 0 fiesta.sky"
             mk fiesta 3400; mk crypt 3227; a=fiesta; an=Fiesta; extra="" ;;
        imaginators) # Start until the licence, Y, then A (tools/regress/imaginators_hub.sh), to the hub; Player 1 from the start.
             press=""; env_extra="SKY_LEVEL=hub_2016 SKY_PRESS_FILE=$r/presses.txt"; t0=230; secs=320; first="3:put 0 spyro.sky" ;;
    esac
    s() { awk -v t0="$t0" -v d="$1" -v k="$k" 'BEGIN { printf "%g", t0 + d * k }'; }
    script="${first:+$first|}1:shot 00_open|$(s 0):put 0 $a.sky|$(s 6):shot 01_p1|$(s 8):put $p2 $b.sky|$(s 14):shot 02_p2"
    script+="|$(s 16):put $p2 $a.sky|$(s 22):shot 03_swapped|$(s 24):off $p2|$(s 30):shot 04_off"
    if [ -n "$extra" ]; then
        IFS=: read -r eseat efile etag <<< "$extra"
        script+="|$(s 32):put $eseat $efile|$(s 38):shot 05_own_seat"
    fi
    # The game's own picture a moment after each of the window's (its script counts from the game's start).
    gameshots="shot=01_p1:$(s 7.5),shot=02_p2:$(s 15.5),shot=03_swapped:$(s 23.5),shot=04_off:$(s 31.5)"
    [ -n "$extra" ] && gameshots+=",shot=05_own_seat:$(s 39.5)"
    echo "== $game: the launcher plays it, the Toy Box window opens with it (~$secs s)"
    if [ "${CHECK_ONLY:-0}" != 1 ]; then
        driver=""
        if [ $game = imaginators ]; then : > "$r/presses.txt"; drive_imaginators "$r/game.log" "$r/presses.txt" & driver=$!; fi
        run "$game" "$r" "$secs" "$script" ${press:+SKY_PRESS="'$press'"} SKY_TOYBOX_SCRIPT="'$gameshots'" SKY_TOYBOX_SHOTS="'$r/game'" $env_extra
        [ -n "$driver" ] && { kill "$driver" 2> /dev/null; wait "$driver" 2> /dev/null; }
        beside "$r"
    fi
    log=$r/game.log; win=$r/launcher.log
    check $game "the window opened with the game" "$win" "launcher: Toy Box window opened for $game"
    check $game "the game listens" "$log" "[toybox-link] listening on the loopback"
    check $game "the window connected" "$log" "[toybox-link] a window connected (toybox-window)"
    check $game "P1 through the window" "$log" "P1: $an on slot"
    check $game "the game read P1" "$log" "P1: the game took $an"
    check $game "P2 through the window" "$log" "P2: $bn on slot"
    check $game "the game read P2" "$log" "P2: the game took $bn"
    check $game "the players swapped" "$log" "Players swapped"
    if sed -n '/Players swapped/,$p' "$log" | grep -aqF "P1: the game took $bn"; then echo "ok:   $game: the game read the swap (Player 1)"
    else echo "FAIL: $game: the game did not read Player 1 after the swap"; fails=$((fails + 1)); fi
    # Player 2 after the swap: a game with one controller may leave it unread until it needs it (Swap Force, SuperChargers).
    sed -n '/Players swapped/,$p' "$log" | grep -aqF "P2: the game took $an" && echo "ok:   $game: the game read Player 2 after the swap too" \
        || echo "note: $game: Player 2 after the swap was taken off before the game read it"
    check $game "taken off through the window" "$log" "P2: $an taken off"
    check $game "the window got every answer" "$win" ": ok" 4
    if [ -n "$extra" ]; then
        check $game "the game's own seat ($etag) through the window" "$log" "$etag: "
        grep -aE "^\[toybox\] $etag: the game took" "$log" > /dev/null && echo "ok:   $game: the game read the $etag seat" \
            || { echo "FAIL: $game: the game did not read the $etag seat"; fails=$((fails + 1)); }
    fi
    check $game "the window closed with the game" "$win" "Toy Box window closes: the game ended"
    shots=$(ls "$r"/shots/*.png 2>/dev/null | wc -l | tr -d ' ')
    [ "$shots" -ge 5 ] && echo "ok:   $game: $shots pictures of the window" || { echo "FAIL: $game: $shots pictures"; fails=$((fails + 1)); }
done

if [ "${MIXED:-0}" = 1 ] && [ "${CHECK_ONLY:-0}" != 1 ] && [ -n "${GIANTS_ISO:-}" ]; then
    # Player 1 virtual through the window, then a real Spyro arrives on the fake real portal: it is Player 2, read only.
    game=giants; iso=$GIANTS_ISO; r=$work/mixed; rm -rf "${r:?}"; mkdir -p "$r/collection" "$r/save" "$r/data" "$r/ldata" "$r/home" "$r/desktop" "$r/real"
    make=$root/build-giants/ArkeyanRuntime/ArkeyanFigureTest
    "$make" --make "$r/real/spyro.sky" 16 > /dev/null; "$make" --make "$r/collection/whirlwind.sky" 0 > /dev/null
    echo "== mixed portal (Giants): a virtual Player 1 from the window, a real Player 2 (~100 s)"
    run giants "$r" 100 "1:shot 00_open|6:put 0 whirlwind.sky|12:shot 01_p1_virtual|50:shot 02_p2_real|52:put 1 whirlwind.sky|56:shot 03_refused" \
        SKY_PORTAL=fake-real SKY_PORTAL_FAKE_REAL="'$r/real/spyro.sky@40'" SKY_PRESS="'$(python3 -I "$root/tools/regress/giants_press.py" 100)'" \
        SKY_TOYBOX_SCRIPT="'shot=01_p1_virtual:13.5,shot=02_p2_real:51.5,shot=03_refused:57.5'" SKY_TOYBOX_SHOTS="'$r/game'"
    beside "$r"
    log=$r/game.log; win=$r/launcher.log
    check mixed "one portal of both" "$log" "[portal] mixed: the real portal's toys and the Toy Box's virtual seats"
    check mixed "the virtual figure on Player 1" "$log" "P1: Whirlwind on slot"
    check mixed "the game read it" "$log" "P1: the game took Whirlwind"
    check mixed "the real toy arrives" "$log" "fake real portal: "
    check mixed "the real toy has a slot of the game" "$log" "[portal] mixed: the real portal's slot 0 is the game's slot"
    check mixed "the game read the real toy, on Player 2" "$log" "P2: the game read the real portal's Spyro"
    check mixed "the window may not change the real seat" "$win" "seat 1 holds a real figure: not changed"
fi

if [ "${MIXED:-0}" = 1 ] && [ "${CHECK_ONLY:-0}" != 1 ] && [ -n "${SSA_ISO:-}" ] && [ -x "$(exe_of ssa)" ]; then
    # The reverse, on Spyro's Adventure (Lux): a real Spyro on the portal from the start is Player 1; the window puts a virtual
    # Whirlwind on Player 2, and may not change Player 1.
    game=ssa; iso=$SSA_ISO; r=$work/mixed-ssa; rm -rf "${r:?}"; mkdir -p "$r/collection" "$r/save" "$r/data" "$r/ldata" "$r/home" "$r/desktop" "$r/real"
    make=$root/build-giants/ArkeyanRuntime/ArkeyanFigureTest
    "$make" --make "$r/real/spyro.sky" 16 > /dev/null; "$make" --make "$r/collection/whirlwind.sky" 0 > /dev/null
    echo "== mixed portal (Spyro's Adventure): a real Player 1, a virtual Player 2 from the window (~110 s)"
    run ssa "$r" 110 "1:shot 00_open|40:put 1 whirlwind.sky|46:shot 01_p2_virtual|48:put 0 whirlwind.sky|52:shot 02_refused" \
        SKY_PORTAL=fake-real SKY_PORTAL_FAKE_REAL="'$r/real/spyro.sky'" SKY_LEVEL=level_027 \
        SKY_PRESS="'start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27'" \
        SKY_TOYBOX_SCRIPT="'shot=01_p2_virtual:47.5,shot=02_refused:53.5'" SKY_TOYBOX_SHOTS="'$r/game'"
    beside "$r"
    log=$r/game.log; win=$r/launcher.log
    check mixed-ssa "one portal of both" "$log" "[portal] mixed: the real portal's toys and the Toy Box's virtual seats"
    check mixed-ssa "the game read the real toy, on Player 1" "$log" "P1: the game read the real portal's Spyro"
    check mixed-ssa "the virtual figure on Player 2" "$log" "P2: Whirlwind on slot"
    check mixed-ssa "the game read it" "$log" "P2: the game took Whirlwind"
    check mixed-ssa "the window may not change the real seat" "$win" "seat 0 holds a real figure: not changed"
fi

[ $fails -eq 0 ] && echo "toybox_window: all checks pass ($skipped skipped; logs and pictures in $work)" \
    || echo "toybox_window: $fails FAILED (logs in $work)"
exit $((fails != 0))
