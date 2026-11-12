#!/bin/bash
# A real portal that numbers its toys anew on each reset (issue #20, Giants' "Please put a Skylander" loop with a Wii Swap
# Force portal), per game: SKY_PORTAL=fake-real (host/portal_mixed.cpp: status reports from the start, each reset renumbers
# the toys and reports them arriving again, the id SKY_PORTAL_FAKE_REAL_ID, by default 02 0A 05 08 as in the player's log,
# 02 1B for Trap Team),
# behind AsyncPortal and the mixed portal as a HID portal is. Giants plays the player's run: Camo on from the start, Tree
# Rex put beside it at 45 s, Camo lifted at 50 s; the others one character of their own on from the start.
# Checks per game: the game read the real toy; it reset the portal at most 3 times (not in bursts: answers wait behind
# no status report, host/portal_async.h); a toy it read never came back as a new one after a reset. Giants also: the
# portal script reaches "In Game" after Tree Rex came, and no "Please put a Skylander" after it.
#
#   tools/regress/portal_renumber.sh [<game> ...]   (default the six; the discs from SSA_ISO, GIANTS_ISO, ...)
#
# ~2 to 4 min a game. Isolated (docs/getting-started/testing.md), under private/runs/portal_renumber/<game>, through
# ../gamelock.sh when it exists; kills only its own processes. Exit 0: every check passes (a game without disc: SKIP).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
games=("$@"); [ ${#games[@]} -eq 0 ] && games=(ssa giants swapforce trapteam superchargers imaginators)
work=$root/private/runs/portal_renumber
make=$root/build-giants/ArkeyanRuntime/ArkeyanFigureTest; [ -x "$make" ] || make=$root/build/ArkeyanRuntime/ArkeyanFigureTest
wanted_id=${SKY_PORTAL_FAKE_REAL_ID:-}
fails=0
ok() { echo "ok:   $1"; }
bad() { echo "FAIL: $1"; fails=$((fails + 1)); }
a_every() { awk -v from="$1" -v to="$2" -v step="$3" 'BEGIN { for (t = from; t <= to; t += step) printf "%sa:%g", (t > from ? "," : ""), t }'; }

for game in "${games[@]}"; do
    extra=(); toys=""
    case $game in
        ssa) iso=${SSA_ISO:-}; build=build; secs=90; toys="spyro:16"; extra=(SKY_LEVEL=level_027)
             press="start:1,start:2,start:3,start:4,start:5,$(a_every 6 27 1.5),b:30,b:34,b:38,b:42,b:46,b:50" ;;
        giants) iso=${GIANTS_ISO:-}; build=build-giants; secs=150; toys="camo:24@0-50 treerex:112@45"
             press=$(python3 -I "$root/tools/regress/giants_press.py" "$secs") ;;
        swapforce) iso=${SWAPFORCE_ISO:-}; build=build-swapforce; secs=100; toys="eruptor:9"; press=$(a_every 14 50 2) ;;
        trapteam) iso=${TRAPTEAM_ISO:-}; build=build-trapteam; secs=180; toys="eruptor:9"
             press=$(python3 -I "$root/tools/regress/giants_press.py" "$secs") ;;
        superchargers) iso=${SUPERCHARGERS_ISO:-}; build=build-superchargers; secs=150; toys="eruptor:9"
             press="$(a_every 12 44 4),y:46,y:48,y:50,y:54,$(a_every 58 145 5)" ;;
        imaginators) iso=${IMAGINATORS_ISO:-}; build=build-imaginators; secs=150; toys="eruptor:9"
             press="$(awk 'BEGIN { for (t = 5; t <= 40; t += 5) printf "start:%d,", t }')y:48,y:51,$(a_every 55 145 4)" ;;
        *) echo "unknown game: $game"; exit 2 ;;
    esac
    # Trap Team stays on its title with a 02 0A 05 08 portal (it reads the toy with a Traptanium one, 02 1B).
    id=${wanted_id:-02 0A 05 08}; [ "$game" = trapteam ] && [ -z "$wanted_id" ] && id="02 1B"
    exe="$root/$build/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")"
    if [ -z "$iso" ] || [ ! -f "$iso" ] || [ ! -x "$exe" ]; then echo "SKIP: $game (no disc or no build)"; continue; fi
    r=$work/$game; rm -rf "${r:?}"; mkdir -p "$r/collection" "$r/save" "$r/data" "$r/real"
    spec=""
    for toy in $toys; do
        name=${toy%%:*}; rest=${toy#*:}; type=${rest%%@*}; when=""; [ "$rest" != "$type" ] && when=@${rest#*@}
        "$make" --make "$r/real/$name.sky" "$type" > /dev/null || { bad "$game: cannot make $name"; continue 2; }
        spec="$spec${spec:+,}$r/real/$name.sky$when"
    done
    export PORTAL_RENUMBER_EXE="$exe" PORTAL_RENUMBER_ISO="$iso" # through the environment: "Spyro's" has an apostrophe
    inner="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA='$r/data' SKY_SETTINGS='$r/none.ini' SKY_LAUNCHER_INI='$r/none.ini' SKY_COLLECTION='$r/collection' \
        SKY_SAVE_DIR='$r/save' SKY_LOG='$r/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_SKIP_MOVIES=1 SKY_PRESS='$press' \
        SKY_PORTAL=fake-real SKY_PORTAL_FAKE_REAL='$spec' SKY_PORTAL_FAKE_REAL_ID='$id' SKY_PORTAL_LOG=1 ${extra[*]:-} \
        nice \"\$PORTAL_RENUMBER_EXE\" --vsync 0 --fps-limit 30 \"\$PORTAL_RENUMBER_ISO\" > '$r/log' 2>&1 & pid=\$!
        (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
        wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$inner"; else sh -c "$inner"; fi
    log=$r/game.log; [ -f "$log" ] || log=$r/log
    grep -q "the game read the real portal's" "$log" && ok "$game: the game read the real toy" \
        || bad "$game: no '[toybox] ... the game read the real portal's ...'"
    resets=$(grep -c '^\[portal\] -> R' "$log" 2>/dev/null); resets=${resets:-99}
    [ "$resets" -le 3 ] && ok "$game: $resets reset(s) of the portal" || bad "$game: $resets resets of the portal (bursts)"
    # A toy the game read, then seen arriving as a new one: the slot line without "again" after a reset.
    if grep -q "did not come back after the reset" "$log"; then bad "$game: a held toy did not come back"
    else ok "$game: no toy lost by a reset"; fi
    if [ "$game" = giants ]; then
        after=$(sed -n '/treerex.sky put on/,$p' "$log")
        printf '%s\n' "$after" | grep -q 'state "Portal Control - In Game' && ok "giants: in game after Tree Rex came" \
            || bad "giants: the portal script never reached In Game after Tree Rex"
        prompts=$(printf '%s\n' "$after" | grep -c '203_PortalError_TagTypeNotOnPortal')
        [ "$prompts" = 0 ] && ok "giants: no 'Please put a Skylander' after Tree Rex" \
            || bad "giants: 'Please put a Skylander' $prompts time(s) after Tree Rex"
    fi
done
echo "portal_renumber: $fails failure(s)"
[ $fails -eq 0 ]
