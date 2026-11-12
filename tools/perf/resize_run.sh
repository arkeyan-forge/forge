#!/bin/sh
# The live-resize check (macOS): one isolated game run in a real window (not offscreen), a mouse drag of the window's
# corner after <wait> seconds (tools/perf/live_resize.swift), then the frame gaps during the drag and captures of the
# game window only (never the screen). docs/runtime/gpu/README.md, "Window and resize".
#
#   tools/perf/resize_run.sh <game executable> <iso> <out dir> [wait s, default 40] [drag s, default 4] [env...]
#
# Extra arguments are environment settings for the game (e.g. SKY_LEVEL=level_027); SCALE (default 2) and FPS_LIMIT
# (default 60: heavier than the default 30), vsync on. Isolated as every run (scratch settings, collection with one
# figure on the emulated portal, saves, data, log; disk audio; no update check). Writes <out dir>/frames.txt, game.log,
# stdout.log, resize.txt (the tool's output) and before.png, drag_NN.png, after.png; prints the gaps during the drag.
# SAMPLE=1: macOS `sample` of the game for the 3 s after the drag (<out dir>/sample.txt; where a stall waits).
# The mouse moves for <drag> seconds: nobody should be using it meanwhile.
set -e
exe=$1; iso=$2; out=$3; wait_s=${4:-40}; drag_s=${5:-4}
shift 3; [ $# -gt 0 ] && shift; [ $# -gt 0 ] && shift
root=$(cd "$(dirname "$0")/../.." && pwd)
rm -rf "${out:?}"; mkdir -p "$out/save" "$out/figures" "$out/data"
cp "$root/private/figures/new_14.sky" "$out/figures/" 2>/dev/null || true # a figure on the portal, as tools/test_all.sh
tool=$out/live_resize
swiftc -O "$root/tools/perf/live_resize.swift" -o "$tool"
env SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKYLANDERS_DATA="$out/data" SKY_SETTINGS="$out/none.ini" \
    SKY_LAUNCHER_INI="$out/none.ini" SKY_COLLECTION="$out/figures" SKY_FIGURES="$out/figures" SKY_SAVE_DIR="$out/save" \
    SKY_LOG="$out/game.log" SKY_SKIP_MOVIES=1 SKY_FRAME_TIMES="$out/frames.txt" SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_PORTAL=emulated \
    SKY_PORTAL_PLACE=new_14.sky "$@" \
    nice "$exe" --scale "${SCALE:-2}" --vsync 1 --fps-limit "${FPS_LIMIT:-60}" "$iso" > "$out/stdout.log" 2>&1 & pid=$!
sleep "$wait_s"
"$tool" $pid "$out" "$drag_s" "$out/frames.txt" > "$out/resize.txt" 2>&1 || true
if [ "${SAMPLE:-0}" = 1 ]; then sample $pid 3 -file "$out/sample.txt" > /dev/null 2>&1 || true; else sleep 3; fi
kill $pid 2>/dev/null || true; sleep 2; kill -9 $pid 2>/dev/null || true
cat "$out/resize.txt"
# The gaps between the game's presents: 3 s before the drag, during it, 2 s after it.
python3 -I - "$out/frames.txt" "$out/resize.txt" "$out" <<'EOF'
import glob, re, struct, sys
# The window's width in each capture (PNG header): did the drag really resize it?
width = lambda path: struct.unpack(">I", open(path, "rb").read()[16:20])[0]
drags = [width(p) for p in sorted(glob.glob(sys.argv[3] + "/drag_*.png"))]
if drags:
    print(f"capture widths: before {width(sys.argv[3] + '/before.png')}, during the drag {min(drags)}..{max(drags)} pixels")
frames = [float(l.split()[0]) for l in open(sys.argv[1]) if len(l.split()) == 3]
m = re.search(r"drag (\d+)\.\.(\d+) ms", open(sys.argv[2]).read())
if not m:
    sys.exit("no drag")
start, end = float(m.group(1)), float(m.group(2))
def gaps(a, b):
    g = [frames[i] - frames[i - 1] for i in range(1, len(frames)) if a <= frames[i] <= b]
    return g
for name, a, b in (("before", start - 3000, start), ("drag", start, end + 100), ("after", end + 100, end + 2100)):
    g = sorted(gaps(a, b))
    if not g:
        print(f"{name}: no frame"); continue
    print(f"{name}: {len(g)} frames in {(b - a) / 1000:.1f} s, gap median {g[len(g) // 2]:.1f} p95 {g[int(len(g) * .95)]:.1f} max {g[-1]:.1f} ms")
EOF
