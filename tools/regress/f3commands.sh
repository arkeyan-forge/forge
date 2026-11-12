#!/bin/bash
# The F3 menu's Commands page (docs/runtime/host/gamemenu.md, "Commands"), through the Toy Box's own script (SKY_TOYBOX_SCRIPT):
# F3 opens the menu on Commands (its first row, under "Game"), Enter opens the page; Down x7 reaches
# Spyro's Adventure's "Special Power / Back" (B, key X), Enter waits for a key, K is taken (K was "Same", the right stick down:
# the two rows swap); Right, Enter waits for a pad button, the pad's A is taken (A does B's job, B takes A's); Escape, F3.
# A key held by the script ("hold=K") before and after shows what the game gets from it.
# Checks: the log lines, [ssa.input] key_b=K, key_right_down=X, pad_a=b, pad_b=a, seeded=1, [input] key_b=K, the bindings
# taken by the running game, and K giving the game B (0x2000) after the change and the right stick before it.
#
#   tools/regress/f3commands.sh <disc image>        (Spyro's Adventure; SKY_LANGUAGE=fr for the French page)
#   EXE=<game executable>   default build/ArkeyanRuntime/Skylanders Spyro's Adventure
#
# Everything is under private/runs/commands/ssa/ (scratch settings, collection, saves, log, home, shots).
# Run it whole inside ../gamelock.sh (one game at a time on the machine).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
iso=${1:-}
[ -f "$iso" ] || { echo "usage: $0 <Spyro's Adventure disc image>" >&2; exit 2; }
exe=${EXE:-"$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"}
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
r=$root/private/runs/commands/ssa
rm -rf "${r:?}"; mkdir -p "$r/home"
fail=0
bad() { echo "FAIL: $*"; fail=1; }

# run <label> <seconds> <extra env...>: one isolated run of the game, stopped by its PID when the time is up.
run() {
    label=$1; seconds=$2; shift 2
    ( cd "$r" && env HOME="$r/home" CFFIXED_USER_HOME="$r/home" SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
        SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" \
        SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" \
        SKY_REPORT_DIR="$r/report" SKY_LAUNCHER_LOG_DIR="$r/logs" SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_SKIP_MOVIES=1 \
        SKY_PORTAL=emulated SKY_TOYBOX_SHOTS="$r/$label" "$@" nice "$exe" "$iso" > "$r/$label.out" 2>&1 3>&- ) &
    pid=$!
    for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do kill "$child" 2>/dev/null; done
    sleep 3
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do kill -9 "$child" 2>/dev/null; done
    wait "$pid" 2>/dev/null
}

# F3 opens the panel on Resume; Down x3: Settings, Enter: its first row (Commands), Enter: the Controls page; Down x7: B's row.
downs=Down:32.8,Down:33,Down:33.2,Down:33.4,Down:33.6,Down:33.8,Down:34
script="hold=K*0.6:26,F3:30,shot=menu:31,Down:31.3,Down:31.5,Down:31.7,Return:32,Return:32.4,$downs,shot=page:34.4,Return:35,shot=waitkey:35.5"
script="$script,K:36,shot=key:37,Right:38,Return:38.5,shot=waitpad:39,pad=a:40,shot=pad:41,Escape:42,F3:43,hold=K*0.6:46,hold=X*0.6:48"
echo "== ssa: the Commands page"
run c 55 SKY_TOYBOX_SCRIPT="$script"
log=$r/game.log
ini=$r/launcher.ini
section() { awk '/^\[/ { in_it = ($0 == "['"$1"']") } in_it' "$ini"; }
grep -q '^\[gamemenu\] commands opened' "$log" || bad "the Commands page did not open"
grep -q '^\[gamemenu\] commands: waiting for a key for b' "$log" || bad "Enter did not wait for a key on B's row"
grep -q '^\[gamemenu\] key_b = K (swapped: key_right_down = X)' "$log" || bad "K was not taken for B (with the swap)"
grep -q '^\[gamemenu\] commands: waiting for a pad button for b' "$log" || bad "Enter did not wait for a pad button on B's row"
grep -q '^\[gamemenu\] pad_a = b (swapped: pad_b = a)' "$log" || bad "the pad's A was not given B's job (with the swap)"
grep -q '^\[input\] bindings in use now: .*key_b=K' "$log" || bad "the running game did not take the new key"
grep -q '^\[input\] bindings in use now: .*pad_a=b, pad_b=a' "$log" || bad "the running game did not take the new pad buttons"
grep -q '^\[toybox\] closed' "$log" || bad "F3 did not close the menu"
for line in seeded=1 key_b=K key_right_down=X pad_a=b pad_b=a; do
    section ssa.input | grep -qx "$line" || bad "[ssa.input] $line is not in launcher.ini"
done
section input | grep -qx key_b=K || bad "[input] key_b=K is not in launcher.ini"
held=$(grep '^\[toybox\] script: K held: ' "$log")
echo "$held" | head -1 | grep -q 'right stick 0/-32767' || bad "before: K did not give the game the right stick down"
echo "$held" | tail -1 | grep -q 'buttons 0x2000' || bad "after: K does not give the game B"
grep '^\[toybox\] script: X held: ' "$log" | grep -q 'right stick 0/-32767' || bad "after: X does not give the game the right stick down"
grep -E '^\[(gamemenu|input)\]|held: ' "$log"
ls "$r"/c_*.png 2>/dev/null
if [ $fail -eq 0 ]; then echo "PASS: ssa: the Commands page writes the key and the pad button, swaps, and the running game uses them (under $r)"; else echo "FAIL (logs in $r)"; fi
exit $fail
