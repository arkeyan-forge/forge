#!/bin/sh
# Start test of the Windows build under Wine on the Mac (no Windows PC needed), with the fast menu recipe of
# docs/getting-started/testing.md. Wine is not installed by the project: WINE points at a wine binary (default
# private/wine/wswine.bundle/bin/wine), WINEPREFIX at its prefix (default private/wine/prefix).
#
#   tools/windows/wine_smoke_test.sh <game disc .iso> <output dir> [seconds (default 90)] [--gpu]
#
# Default: SKY_HEADLESS=1 (no GPU, no window); the log has every file the game opens (SKY_TRACE_FILES). --gpu: SDL's
# offscreen driver with Wine's macOS display driver loaded (needed for Vulkan surfaces; no window is opened), so the
# Vulkan backend runs on the Mac's GPU through winevulkan and MoltenVK, and one frame's resolves are dumped at
# SKY_DUMP_FRAME seconds (default 12). MoltenVK is stricter than Windows drivers (docs/getting-started/platforms/windows.md). EXE runs another
# build (default build-win64/ArkeyanRuntime/Skylanders Spyro's Adventure.exe; a player kit's: docs/tools/arkeyan-builder.md).
set -eu

[ $# -ge 2 ] || { echo "usage: $0 <game disc .iso> <output dir> [seconds] [--gpu]" >&2; exit 1; }
root=$(cd "$(dirname "$0")/../.." && pwd)
iso=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
out=$2
seconds=${3:-90}
gpu=0
[ "${4:-}" = --gpu ] && gpu=1
wine=${WINE:-$root/private/wine/wswine.bundle/bin/wine}
export WINEPREFIX=${WINEPREFIX:-$root/private/wine/prefix}
rm -rf "$out"
mkdir -p "$out/save"
out=$(cd "$out" && pwd)

windows() { printf 'Z:%s' "$1" | tr / '\\'; } # Wine maps the Mac's / to drive Z:

export WINEDEBUG=-all SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=NUL SKY_SKIP_MOVIES=1 SKY_TRACE_FILES=1
export SKY_SAVE_DIR="$(windows "$out/save")" SKY_PORTAL=emulated SKY_FIGURES="$(windows "$root/private/figures")"
export SKY_PORTAL_PLACE=${SKY_PORTAL_PLACE:-new_14.sky}
export SKY_PRESS="${SKY_PRESS-start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60}"
if [ "$gpu" = 1 ]; then
    export SDL_VIDEO_DRIVER=offscreen SKY_RENDER_STATS=1 SKY_DUMP_FRAME="${SKY_DUMP_FRAME:-12}"
    export SKY_DUMP_PREFIX="$(windows "$out/dump")"
else
    export SKY_HEADLESS=1 WINEDLLOVERRIDES=winemac.drv=d
fi

"$wine" "${EXE:-"$root/build-win64/ArkeyanRuntime/Skylanders Spyro's Adventure.exe"}" "$(windows "$iso")" > "$out/log" 2>&1 &
pid=$!
(sleep "$seconds"; kill "$pid" 2> /dev/null; sleep 3; kill -9 "$pid" 2> /dev/null) &
wait "$pid" || true
"$(dirname "$wine")/wineserver" -k 2> /dev/null || true
echo "files opened: $(grep -c '^\[file\] open' "$out/log" || true), level_027: $(grep -c 'open .levels.level_027.mission' "$out/log" || true)"
grep -E '^\[gpu\]|Fault' "$out/log" || true
