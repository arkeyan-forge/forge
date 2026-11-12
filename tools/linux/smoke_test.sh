#!/bin/sh
# Headless test of the Linux build in its container: Mesa's software Vulkan (lavapipe), SDL's offscreen video driver,
# no sound device, the fast menu recipe of docs/getting-started/testing.md (new save, emulated portal), presented frames saved as PPM.
#
#   tools/linux/smoke_test.sh <game disc .iso> <output dir> [seconds (default 90)] [--arch arm64|amd64]
#
# Output dir: log, gpu_<n>.ppm (every SKY_GPU_SNAPSHOT_EVERY presented frames, default 60), dump_*.ppm (the resolves
# of one frame at SKY_DUMP_FRAME seconds, default 30; SKY_GPU_SNAPSHOT applies to an offscreen output only, and SDL's
# offscreen driver gives plume a Vulkan swapchain), save/ (fresh). Needs private/figures/new_14.sky for the portal.
# GAME=<id> runs build-linux-<arch>-<id> (tools/build_linux.sh); SKY_PORTAL_PLACE and SKY_LEVEL pass through.
set -eu

[ $# -ge 2 ] || { echo "usage: $0 <game disc .iso> <output dir> [seconds] [--arch arm64|amd64]" >&2; exit 1; }
iso=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
out=$2
seconds=${3:-90}
arch=
[ "${4:-}" = --arch ] && arch=${5:?}
if [ -z "$arch" ]; then
    case "$(uname -m)" in arm64 | aarch64) arch=arm64 ;; *) arch=amd64 ;; esac
fi
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${GAME:-ssa}
build=build-linux-$arch
[ "$game" = ssa ] || build=$build-$game
rm -rf "$out"
mkdir -p "$out/save"
out=$(cd "$out" && pwd)

press=${SKY_PRESS-"start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60"} # SKY_PRESS= (empty): no presses
docker run --rm --platform "linux/$arch" --user "$(id -u):$(id -g)" -v "$root:/src" -v "$iso:/game.iso:ro" -v "$out:/out" \
    -e SDL_VIDEO_DRIVER=offscreen -e SDL_AUDIO_DRIVER=disk -e SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    -e SKY_SAVE_DIR=/out/save -e SKY_SKIP_MOVIES=1 -e SKY_PRESS="$press" \
    -e SKY_PORTAL=emulated -e SKY_FIGURES=/src/private/figures -e SKY_PORTAL_PLACE="${SKY_PORTAL_PLACE:-new_14.sky}" -e SKY_LEVEL="${SKY_LEVEL:-}" \
    -e SKY_GPU_SNAPSHOT=/out/gpu -e SKY_GPU_SNAPSHOT_EVERY="${SKY_GPU_SNAPSHOT_EVERY:-60}" \
    -e SKY_DUMP_FRAME="${SKY_DUMP_FRAME:-30}" -e SKY_DUMP_PREFIX=/out/dump -e SKY_RENDER_STATS=1 \
    -e GAME_EXE="$build/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")" -e RUN_SECONDS="$seconds" \
    "ssarecomp-linux-$arch" sh -c 'timeout -k 5 "$RUN_SECONDS" "$GAME_EXE" /game.iso > /out/log 2>&1; echo exit=$? >> /out/log'
tail -3 "$out/log"
