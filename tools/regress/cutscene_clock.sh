#!/bin/bash
# The game clock at a frame rate, by an in-engine cutscene's wall-clock length (no Bink movie, no press inside it): one run, offscreen,
# vsync off. The shots' boundaries come from the frames' draw counts (SKY_FRAME_TIMES, third column), which a shot change moves and
# the frame rate does not; each boundary is checked on a dumped frame (SKY_GPU_SNAPSHOT) by eye the first time.
#
#   tools/regress/cutscene_clock.sh <giants|swapforce|trapteam> <disc image> <fps_limit> <run name> [seconds, default 110]
#   Results in private/regress/cutscene-<run name>/; the last line is the table row: shot A seconds, shot B seconds, fps reached.
#   giants:    the tutorial's Eon narration over the Ark (A), the figure's summon (B)  [Giants]
#   trapteam:  the tutorial's intro cutscene (313_intro_igc_stream): from the stream's open to the first save the game makes after it (its
#              end, a game event); the draw counts do not separate its shots, so no shot B  [TrapTeam]
#   swapforce: the tutorial's ship flying over the lava, after the opening movie (A), the flash that cuts to the player (B)  [SwapForce]
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; fps=${3:-}; name=${4:-}; seconds=${5:-110}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] && [ -n "$fps" ] && [ -n "$name" ] || { sed -n '2,9p' "$0"; exit 2; }
case $game in
    giants) dir=build-giants; bounds="40 500 150"; extra="SKY_LEVEL=level_213_tutotial SKY_PORTAL_PLACE=new:101"
            press=$(awk 'BEGIN { for (t = 4; t <= 26; t += 3.5) printf "%sa:%.1f", (t > 4 ? "," : ""), t }') ;;
    swapforce) dir=build-swapforce; bounds="23 300 150"; extra="SKY_PORTAL_PLACE=new:9"
            press=$(awk 'BEGIN { for (t = 14; t <= 22; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }') ;;
    trapteam) dir=build-trapteam; bounds="0 0 0"; extra="SKY_LEVEL=level_313_tutorial SKY_PORTAL_PLACE=new:450"
            press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }') ;;
    *) echo "no cutscene defined for $game" >&2; exit 2 ;;
esac
exe="$root/$dir/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
r=$root/private/regress/cutscene-$name
rm -rf "${r:?}"; mkdir -p "$r/save" "$r/figures" "$r/snap"
# Trap Team's boundaries are log events: no pictures (each one stalls a frame, which a step clamp may turn into lost game time).
snap="SKY_GPU_SNAPSHOT=$r/snap/s SKY_GPU_SNAPSHOT_FROM=400 SKY_GPU_SNAPSHOT_EVERY=60"
[ "$game" = trapteam ] && snap=""
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKYLANDERS_DATA="$r/data" \
    SKY_SETTINGS="$r/none.ini" SKY_LAUNCHER_INI="$r/none.ini" SKY_COLLECTION="$r/figures" SKY_FIGURES="$r/figures" \
    SKY_SAVE_DIR="$r/save" SKY_LOG="$r/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated SKY_NO_FPS_CAP=1 $extra \
    SKY_FRAME_TIMES="$r/frames.txt" $snap \
    nice "$exe" --vsync 0 --fps-limit "$fps" "$iso" > "$r/log" 2>&1 &
pid=$!
for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
python3 -I - "$r/frames.txt" "$game" "$fps" "$name" "$(uptime | sed 's/.*averages*: //')" $bounds "$r/game.log" <<'PY'
import statistics as st, sys
path, game, fps, name, load, after, high, low, glog = sys.argv[1:]
after, high, low = float(after), float(high), float(low)
f = [tuple(map(float, l.split())) for l in open(path) if len(l.split()) == 3]
t = [x[0] / 1000 for x in f]; d = [x[2] for x in f]
if game == "trapteam":
    import re
    ev = re.findall(r"\[file\] open (\S+) -> \S+ at ([0-9.]+) s", open(glog, errors="replace").read())
    a = next((float(x[1]) for x in ev if x[0].endswith("313_intro_igc_stream.arc")), None)
    b = next((float(x[1]) for x in ev if a is not None and float(x[1]) > a and x[0].startswith("save:\\Save_Slot")), None)
    if a is None or b is None:
        print(f"{name}: {game} fps_limit={fps}: cutscene events not found ({a}, {b}); load {load}")
        sys.exit(1)
    n = [x for x in t if a <= x <= b]
    print(f"{name}: {game} fps_limit={fps}: cutscene {b - a:.2f} s ({a:.2f}-{b:.2f}), fps reached {len(n) / (b - a):.1f}; load {load}")
    sys.exit(0)
# A shot's boundary is the first frame of a run of 8 frames above (or below) the draw thresholds: per frame, so the length of a shot has
# the resolution of a frame (a half-second window of medians had one of 0.5 s, which made Giants' summon look 0.6 s shorter at 120).
HOLD = 8
def first(start, test):
    for i in range(len(f) - HOLD):
        if t[i] > start and all(test(d[j]) for j in range(i, i + HOLD)):
            return i
i0 = first(after, lambda m: m >= high)                       # the first shot: the scene
i1 = first(t[i0], lambda m: m < low) if i0 is not None else None   # the second: few draws
i2 = first(t[i1], lambda m: m >= high) if i1 is not None else None  # gameplay
if None in (i0, i1, i2):
    print(f"{name}: {game} fps_limit={fps}: boundaries not found ({i0}, {i1}, {i2}); load {load}")
    sys.exit(1)
print(f"{name}: {game} fps_limit={fps}: shot A {t[i1] - t[i0]:.2f} s ({t[i0]:.2f}-{t[i1]:.2f}), shot B {t[i2] - t[i1]:.2f} s ({t[i1]:.2f}-{t[i2]:.2f}), "
      f"fps reached {(i2 - i0) / (t[i2] - t[i0]):.1f}; load {load}")
PY
