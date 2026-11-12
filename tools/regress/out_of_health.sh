#!/bin/bash
# The Toy Box's "out of health" (docs/runtime/host/toybox_ux.md, "Out of health"): when the game says a player's Skylander died
# (or asks for the seat again while the figure it took is still on it), the Toy Box opens on that seat and the figure shows a
# red cross, on its card and on the seat, until the next level; it can still be picked, and only once the figure has played
# 20 s in the level (host/toybox/in_play.h: the game also asks when a level starts). The run puts an Eruptor on Player 1,
# starts a level at 30 s ("level", the same call the game's own level files make, game::LevelStarted), makes the prompt up
# at 32 s as the game's death prompt would (SKY_TOYBOX_SCRIPT "prompt=1d", the Toy Box's own test aid: the adapter's
# reading is replaced for 3 s): too early, no cross; then again at 58 s: the cross; looks at the Toy Box (shot "out"),
# starts a level at 66 s, and looks again (shot "back"). Checks: the log says what it should, the first shot has the red
# of the cross and the second has none. The real games' own level files and prompts were not driven to
# a death (docs/runtime/host/toybox_ux.md says what is [V] and what is [?]).
#
#   tools/regress/out_of_health.sh <ssa|giants|swapforce> <disc image>       (about 75 s)
#   EXE=<game executable>   default build/ArkeyanRuntime/Skylanders Spyro's Adventure (build-giants/..: Skylanders Giants, build-swapforce/..: Skylanders Swap Force)
#
# Isolated like every run; everything is under private/regress/out-of-health-<game>/. Exit code 0: PASS (the reason is the last line).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] || { echo "usage: $0 <ssa|giants|swapforce> <disc image>" >&2; exit 2; }
case $game in ssa) dir=build; name="Skylanders Spyro's Adventure" ;; giants) dir=build-giants; name="Skylanders Giants" ;; *) dir=build-swapforce; name="Skylanders Swap Force" ;; esac
exe="${EXE:-$root/$dir/ArkeyanRuntime/$name}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
make=${FIGURE_TOOL:-$root/build/ArkeyanRuntime/ArkeyanFigureTest}
R=$root/private/regress/out-of-health-$game
rm -rf "${R:?}"; mkdir -p "$R/collection" "$R/save" "$R/data" "$R/home"
"$make" --make "$R/collection/new_9.sky" 9 > /dev/null || { echo "cannot make the dump of Eruptor"; exit 2; }
script="level:30,prompt=1d:32,prompt=1d:58,shot=out:60,Escape:61,level:66,F6:68,shot=back:70"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null HOME="$R/home" CFFIXED_USER_HOME="$R/home" \
  SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" SKY_COLLECTION="$R/collection" \
  SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_9.sky \
  SKY_TOYBOX_SCRIPT="$script" SKY_TOYBOX_SHOTS="$R/shots" \
  nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$R/log" 2>&1 &
pid=$!
for _ in $(seq 120); do
    kill -0 "$pid" 2>/dev/null || break
    [ -f "$R/shots_back.png" ] && { sleep 2; break; }
    sleep 1
done
kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null; { wait "$pid"; } 2>/dev/null

fail=0
expect() { grep -qF -- "$1" "$R/game.log" || { echo "FAIL: missing \"$1\""; fail=1; }; }
expect 'player 1'"'"'s figure has not played in this level yet: not out of health'
expect 'player 1'"'"'s figure is out of health (the game says its Skylander died)'
expect '[toybox] prompt for player 1'
expect '[level] script level 1 starts'
expect '[level] script level 2 starts'
# The cross is the Toy Box's danger red (0xF85149): many such pixels in the first shot, none in the second.
count() { python3 -I - "$1" <<'PY'
import struct, sys, zlib
data = open(sys.argv[1], "rb").read()
pos, idat, width, height, depth, kind = 8, b"", 0, 0, 8, 2
while pos < len(data):
    size, tag = struct.unpack(">I4s", data[pos:pos + 8])
    body = data[pos + 8:pos + 8 + size]
    if tag == b"IHDR":
        width, height, depth, kind = struct.unpack(">IIBB", body[:10])
    elif tag == b"IDAT":
        idat += body
    pos += 12 + size
channels = {2: 3, 6: 4}[kind]
assert depth == 8
raw, stride, rows, previous, count = zlib.decompress(idat), width * channels, [], bytearray(width * channels), 0
for y in range(height):
    line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
    f = raw[y * (stride + 1)]
    for i in range(stride):
        a = line[i - channels] if i >= channels else 0
        b = previous[i]
        c = previous[i - channels] if i >= channels else 0
        if f == 1: line[i] = (line[i] + a) & 255
        elif f == 2: line[i] = (line[i] + b) & 255
        elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
        elif f == 4:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    for x in range(width):
        r, g, bl = line[x * channels:x * channels + 3]
        count += abs(r - 0xF8) < 6 and abs(g - 0x51) < 12 and abs(bl - 0x49) < 12
    previous = line
print(count)
PY
}
if [ -f "$R/shots_out.png" ] && [ -f "$R/shots_back.png" ]; then
    out=$(count "$R/shots_out.png"); back=$(count "$R/shots_back.png")
    echo "red pixels: with the cross $out, after the level $back"
    [ "$out" -gt 200 ] && [ "$back" -lt 20 ] || { echo "FAIL: the cross is not drawn before the level and gone after it"; fail=1; }
else
    echo "FAIL: the Toy Box shots are missing"; fail=1
fi
if [ $fail -eq 0 ]; then echo "PASS: $game: the figure shows a red cross after its death prompt and loses it at the next level (shots under $R)"; else echo "FAIL (logs in $R)"; fi
exit $fail
