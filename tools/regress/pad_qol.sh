#!/bin/bash
# Proves the pad remapping and the rumble strength of a game's settings (docs/runtime/xbox360/input.md, "Remapping and rumble").
# A virtual Xbox 360 pad (SKY_GLYPHS_SCRIPT) holds RB, LT and D-pad up in turn and the title is asked to rumble; the game's
# own [<game>.input] gives RB the job of B, LT the job of A, D-pad up the job of RT, and strong rumble to Player 1.
# SKY_INPUT_LOG=1 logs what the title reads. A second run with no settings must read the buttons as pressed (RB 0x0200,
# LT 0x0000 with trigger 255, D-pad up 0x0001) and the rumble as asked.
#
#   tools/regress/pad_qol.sh <ssa|giants|swapforce> <disc image> [seconds, default 45]
#   EXE=<game executable>      default build/ArkeyanRuntime/Skylanders Spyro's Adventure (build-giants/...: Skylanders Giants, build-swapforce/...: Skylanders Swap Force)
#
# Everything is under private/regress/pad-qol-<game>/. Exit code 0: PASS (the reason is the last line).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; seconds=${3:-45}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] || { echo "usage: $0 <ssa|giants|swapforce> <disc image> [seconds]" >&2; exit 2; }
case $game in ssa) dir=build; name="Skylanders Spyro's Adventure" ;; giants) dir=build-giants; name="Skylanders Giants" ;; *) dir=build-swapforce; name="Skylanders Swap Force" ;; esac
exe="${EXE:-$root/$dir/ArkeyanRuntime/$name}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
scratch=$root/private/regress/pad-qol-$game
rm -rf "${scratch:?}"; mkdir -p "$scratch"
fail=0

# run <label> <ini text>: one isolated run; stops when the last step of the script has been read or the time is up.
run() {
    label=$1; r=$scratch/$label; mkdir -p "$r/home"
    printf '%s\n' "$2" > "$r/launcher.ini"
    ( cd "$r" && env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" \
        SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" HOME="$r/home" CFFIXED_USER_HOME="$r/home" SKY_PORTAL=none SKY_SKIP_MOVIES=1 \
        SKY_INPUT_LOG=1 SKY_GLYPHS_SCRIPT="xbox360:8,btn:rb:14,btnup:rb:16,btn:lt:18,btnup:lt:20,btn:dpad_up:22,btnup:dpad_up:24,vibrate:26,vibrate0:30" \
        nice "$exe" --vsync 0 "$iso" > "$r/run.log" 2>&1 ) &
    pid=$!; disown "$pid" 2>/dev/null # no "Terminated" line when it is stopped by PID below
    for _ in $(seq "$seconds"); do
        kill -0 "$pid" 2>/dev/null || break
        grep -q 'virtual pad rumbles: 0/0' "$r/game.log" 2>/dev/null && sleep 2 && break
        sleep 1
    done
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do
        for grandchild in $(pgrep -P "$child" 2>/dev/null); do kill "$grandchild" 2>/dev/null; done
        kill "$child" 2>/dev/null
    done
    sleep 3
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do kill -9 "$child" 2>/dev/null; done
    { wait "$pid"; } 2>/dev/null
    [ -s "$r/game.log" ] || { echo "FAIL: $label: no game log"; fail=1; return 1; }
}
# expect <label> <text>: the log of the run has the text
expect() { grep -qF -- "$2" "$scratch/$1/game.log" || { echo "FAIL: $1: missing \"$2\""; fail=1; }; }
refuse() { ! grep -qF -- "$2" "$scratch/$1/game.log" || { echo "FAIL: $1: should not have \"$2\""; fail=1; }; }

echo "== $game: remapped (rb->b, lt->a, dpad_up->rt, rumble_p1 strong)"
if run mapped "[$game.input]
pad_rb=b
pad_lt=a
pad_dpad_up=rt
rumble_p1=strong"; then
    expect mapped 'the title reads buttons 0x2000, triggers 0/0'    # RB read as B
    expect mapped 'the title reads buttons 0x1000, triggers 0/0'    # LT read as A, and the trigger itself gone
    expect mapped 'the title reads buttons 0x0000, triggers 0/255'  # D-pad up read as RT
    refuse mapped 'the title reads buttons 0x0200'                  # RB itself never reaches the title
    refuse mapped 'the title reads buttons 0x0001'
    expect mapped 'the game asked 40000/20000, strong: 64000/32000'
    expect mapped 'virtual pad rumbles: 64000/32000'
    expect mapped 'virtual pad rumbles: 0/0'
    grep -m1 '^\[settings\]' "$scratch/mapped/game.log" | grep -q 'pad rb->b,lt->a,dpad_up->rt, rumble strong/normal' \
        || { echo "FAIL: mapped: the [settings] line does not say the pad map"; fail=1; }
fi
echo "== $game: no settings (everything as the pad is)"
if run plain "[$game.input]
rumble_p2=low"; then
    expect plain 'the title reads buttons 0x0200, triggers 0/0'
    expect plain 'the title reads buttons 0x0000, triggers 255/0'
    expect plain 'the title reads buttons 0x0001, triggers 0/0'
    expect plain 'the game asked 40000/20000, normal: 40000/20000'
    expect plain 'virtual pad rumbles: 40000/20000'
fi
if [ $fail -eq 0 ]; then echo "PASS: $game: pad remapping and rumble strength reach the title (under $scratch)"; else echo "FAIL (logs in $scratch)"; fi
exit $fail
