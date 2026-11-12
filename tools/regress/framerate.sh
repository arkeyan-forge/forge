#!/bin/bash
# One game at one frame rate, offscreen with vsync off (the limiter alone paces the frames; offscreen has no display
# refresh): the presented frame rate, the gaps, the draws a frame (particles and scene size), the game clock when the
# title's step address is known (SKY_CLOCK_STEP, SSA only), the falls. Same presses as tools/test_all.sh.
#
#   tools/regress/framerate.sh <ssa|giants|swapforce|trapteam> <disc image> <fps_limit: 30|60|120|144|240|0> [seconds]
#   Results in private/regress/framerate-<game>-<fps>/ ; the last line is the summary table row.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; fps=${3:-}; seconds=${4:-}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] && [ -n "$fps" ] || { sed -n '2,8p' "$0"; exit 2; }
case $game in
    ssa) dir=build; seconds=${seconds:-90}; extra="SKY_LEVEL=level_027 SKY_CLOCK_STEP=82B62CA4"; skip=60
         press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60" ;;
    giants) dir=build-giants; seconds=${seconds:-100}; extra=""; skip=60
         press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds") ;;
    trapteam) dir=build-trapteam; seconds=${seconds:-100}; extra="SKY_LEVEL=level_313_tutorial SKY_PORTAL_PLACE=new:450"; skip=60; lvdraws=600
         press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }') ;;
    *) dir=build-swapforce; seconds=${seconds:-100}; extra="SKY_PORTAL_PLACE=new:9"; skip=60
         press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }') ;;
esac
exe="$root/$dir/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
r=$root/private/regress/framerate-$game-$fps
rm -rf "${r:?}"; mkdir -p "$r/save" "$r/figures"
cp "$root/private/figures/new_14.sky" "$r/figures/" 2>/dev/null
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKYLANDERS_DATA="$r/data" SKY_SETTINGS="$r/none.ini" SKY_LAUNCHER_INI="$r/none.ini" SKY_COLLECTION="$r/figures" \
    SKY_FIGURES="$r/figures" SKY_SAVE_DIR="$r/save" SKY_LOG="$r/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky SKY_FRAME_TIMES="$r/frames.txt" $extra \
    nice "$exe" --vsync 0 --fps-limit "$fps" --anisotropy 4 "$iso" > "$r/log" 2>&1 &
pid=$!
for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
python3 -I - "$r/frames.txt" "$r/log" "$game" "$fps" "$seconds" "${lvdraws:-1000}" <<'PY'
import re, sys, statistics as st
frames_path, log, game, fps, seconds, lvdraws = sys.argv[1:]
f = [tuple(float(x) for x in l.split()) for l in open(frames_path) if len(l.split()) == 3]
text = open(log, errors="replace").read()
def window(name, sel):
    if len(sel) < 30:
        print(f"{game} fps_limit={fps} {name}: only {len(sel)} frames ({len(f)} in all)")
        return
    span = (f[sel[-1]][0] - f[sel[0] - 1][0]) / 1000
    gaps = sorted(f[i][0] - f[i - 1][0] for i in sel)
    draws = st.mean(f[i][2] for i in sel)
    clock = [float(m) for m in re.findall(r"\[clock\].*: ([0-9.]+) s/s", text)]
    clock = f"{st.mean(clock[-4:]):.3f}" if clock else "n/a"
    falls = len(re.findall(r"\[fall\]", text))
    print(f"{game} fps_limit={fps} {name}: {len(sel)/span:.1f} fps, gap p50 {gaps[len(gaps)//2]:.1f} p99 {gaps[int(.99*(len(gaps)-1))]:.1f} ms, "
          f"draws/frame {draws:.0f}, game s/s {clock}, [fall] lines {falls}")

# The level: frames with >= 1000 draws (Trap Team's tutorial: 600), the last 30 s of them; the menus: 20..50 s of the run.
lv = [i for i, x in enumerate(f) if x[2] >= float(lvdraws)]
window("level", [i for i in lv if i > 0 and f[i][0] >= f[lv[-1]][0] - 30000] if lv else [])
window("menus", [i for i in range(1, len(f)) if 20000 <= f[i][0] <= 50000])
PY
