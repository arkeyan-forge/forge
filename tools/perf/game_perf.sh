#!/bin/bash
# One headless, isolated run of any game into a level, for the frame-time split (docs/runtime/gpu/renderer_threads.md,
# "Where the frame goes"): per-frame times (SKY_FRAME_TIMES), the [perf] lines, the CPU time per frame of each thread
# (`ps -M`), the automatic 60 -> 30 changes, and optionally a macOS `sample` of the level.
#
#   tools/perf/game_perf.sh <ssa|giants|swapforce|trapteam|superchargers|imaginators> <disc image> <level> <fps_limit> <out dir> [game args...]
#
# Level: SKY_LEVEL's name (SSA level_027 is its own; another SSA level runs without a Skylander). Defaults: scale auto,
# vsync off (offscreen has no display), fps_auto_fallback on. The window is 35 to 5 s before the kill: the level must
# be running by then (SECONDS_TO_RUN).
# Env: SECONDS_TO_RUN (default 100), SAMPLE=1 (`sample` for the last 4 s, after the window: it slows the frames it
# covers; <out>/sample.txt, tools/perf/sample_threads.py), FALLBACK=0 (fps_auto_fallback=0: what 60 costs, held).
# GAME_EXE=<executable> (another build, e.g. a baseline). SAVE_SEED=<dir> (its files copied into the run's save folder first: a second start of a player, e.g. the
# auto_scale.txt an earlier run wrote). PACK=<dir> (a texture pack, e.g. Arkeyan-Builder's HD textures: linked as the run's
# <game data>/textures-hd, docs/runtime/gpu/texture_packs.md). Wrap the run in ../gamelock.sh on a shared Mac.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; level=${3:-}; fps=${4:-}; out=${5:-}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] && [ -n "$level" ] && [ -n "$fps" ] && [ -n "$out" ] ||
    { sed -n '2,12p' "$0"; exit 2; }
shift 5
seconds=${SECONDS_TO_RUN:-100}
a_every_2s=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }')
press_file=
case $game in
    ssa) dir=build; figure=new_14.sky
         press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60" ;;
    giants) dir=build-giants; figure=new_14.sky; press=$(python3 -I "$root/tools/regress/giants_press.py" "$seconds") ;;
    # A zone written as a file name (barrens_brn2_brn2, as the out dirs are) is the zone barrens/brn2/brn2.
    swapforce) dir=build-swapforce; figure=new:9; press=$a_every_2s
         case $level in */*) ;; *_*_*) level=$(echo "$level" | tr _ /) ;; esac ;;
    trapteam) dir=build-trapteam; figure=new:450; press=$a_every_2s ;;
    # With SAVE_SEED (a save in a zone: the Academy, tools/regress/superchargers_lobby.sh's saves) the A presses continue
    # into that zone. Without, a new game (the lobby test's presses: the intro, the tunnel ride) enters SKY_LEVEL's zone
    # (sky1, nat2...; intro1 is the game's own) at ~330-400 s at 60: SECONDS_TO_RUN=470.
    superchargers) dir=build-superchargers; figure=new:3400,new:3227
         press=$(awk 'BEGIN { for (t = 12; t <= 60; t += 4) printf "%sa:%d", (t > 12 ? "," : ""), t }')
         [ -z "${SAVE_SEED:-}" ] && press=$(awk 'BEGIN { s = ""; for (t = 12; t <= 44; t += 4) s = s sprintf("a:%d,", t)
             s = s "y:46,y:48,y:50,y:54,"; for (t = 58; t <= 150; t += 5) s = s sprintf("a:%d,", t)
             s = s "rt:205:60,rt:266:60,rt:327:45,"; for (t = 375; t <= 440; t += 5) s = s sprintf("a:%d,", t)
             print substr(s, 1, length(s) - 1) }') ;;
    # A new game reaches its first level (2016/level_508, ~60 s) by itself; the licence comes at a time that depends on
    # the load, so the presses are written while it runs (drive below).
    imaginators) dir=build-imaginators; figure=new:9; press=; press_file=$out/presses.txt ;;
    *) exit 2 ;;
esac
exe=${GAME_EXE:-"$root/$dir/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")"}
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
rm -rf "${out:?}"; mkdir -p "$out/save" "$out/figures" "$out/data"
cp "$root/private/figures/new_14.sky" "$out/figures/" 2>/dev/null
[ -n "${SAVE_SEED:-}" ] && cp -R "$SAVE_SEED"/. "$out/save/"
if [ -n "${PACK:-}" ]; then
    case $game in ssa) folder=SSA ;; swapforce) folder=SwapForce ;; trapteam) folder=TrapTeam ;;
        superchargers) folder=SuperChargers ;; *) folder=$(echo "${game:0:1}" | tr a-z A-Z)${game:1} ;; esac
    mkdir -p "$out/data/$folder"; ln -s "$(cd "$PACK" && pwd)" "$out/data/$folder/textures-hd"
fi
uptime | sed 's/.*load averages*: /start /' > "$out/load.txt"
[ "${FALLBACK:-1}" = 0 ] && printf '[video]\nfps_auto_fallback=0\n' > "$out/none.ini"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKYLANDERS_DATA="$out/data" SKY_SETTINGS="$out/none.ini" SKY_LAUNCHER_INI="$out/none.ini" SKY_COLLECTION="$out/figures" \
    SKY_FIGURES="$out/figures" SKY_SAVE_DIR="$out/save" SKY_LOG="$out/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" \
    ${press_file:+SKY_PRESS_FILE="$press_file"} \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE="$figure" SKY_FRAME_TIMES="$out/frames.txt" SKY_LEVEL="$level" SKY_PERF_LOG=1 \
    nice "$exe" --vsync 0 --fps-limit "$fps" --anisotropy 4 "$@" "$iso" > "$out/log" 2>&1 &
pid=$!
if [ -n "$press_file" ]; then # Start until the licence's archive opens, Y twice, then A every 4 s
    (sleep 20; for _ in $(seq 30); do grep -a -q softwarelicense.pak "$out/game.log" && break; echo start >> "$press_file"
        sleep 5; done; sleep 8; echo y >> "$press_file"; sleep 3; echo y >> "$press_file"
        while kill -0 "$pid" 2>/dev/null; do sleep 4; echo a >> "$press_file"; done) &
fi
from=$((seconds - 35)); to=$((seconds - 5))
(sleep "$from"; ps -M -p $pid > "$out/ps_from.txt"; sleep $((to - from)); ps -M -p $pid > "$out/ps_to.txt") 2>/dev/null &
[ "${SAMPLE:-0}" = 1 ] && (sleep "$to"; sample $pid 4 -file "$out/sample.txt" > /dev/null 2>&1) &
for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
uptime | sed 's/.*load averages*: /end /' >> "$out/load.txt"
echo "== $game $level fps_limit=$fps"
python3 -I "$root/tools/perf/frames.py" "$out/frames.txt" --from "$from" --to "$to" | sed -n 1,3p
python3 -I "$root/tools/perf/thread_cpu.py" "$out/ps_from.txt" "$out/ps_to.txt" "$out/frames.txt" "$from" "$to" 2>/dev/null | head -3
grep '\[perf\]' "$out/log" | tail -3
grep -c 'drop to 30 FPS until' "$out/log" | sed 's/^/fallbacks to 30: /'
tr '\n' ' ' < "$out/load.txt"; echo
