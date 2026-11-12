#!/bin/sh
# Runs the Windows build's D3D12 backend on the Mac, for want of a Windows PC: the executable runs under Wine in an
# x86_64 Linux container (Docker; OrbStack emulates x86_64 with Rosetta) with vkd3d-proton as d3d12.dll on Mesa's
# lavapipe, a conformant Vulkan driver on the CPU. Wine's own D3D12 (vkd3d 1.14) cannot compile dxc's DXIL, and
# vkd3d-proton needs Vulkan features MoltenVK lacks, hence the container. The fast recipe of docs/getting-started/testing.md; one
# frame's resolves are dumped at SKY_DUMP_FRAME seconds (default 12). About 2.5 fps in the level.
#
#   tools/windows/d3d12_test.sh <game disc .iso> <output dir> [seconds (default 120)] [d3d12|vulkan]
#
# vulkan runs the same executable on Wine's Vulkan, on the same driver, for comparisons. SKY_PRESS= (empty) stays on
# the title screen. ARGS adds runtime options (ARGS="--scale 2" as on Xbox Series); VALIDATE=1 loads the Vulkan
# validation layer under vkd3d-proton (messages in the log). SKY_TOYBOX_SCRIPT drives the Toy Box (docs/runtime/host/figures.md);
# its "shot" steps save the presented image as <output dir>/toybox_<n>.png. SKY_UPSCALER, SKY_FSR_QUALITY and
# SKY_WINDOW_SIZE pass through (FSR 3); SKY_GPU_SNAPSHOT_EVERY=<n> (and _FROM) saves presented frames as <output dir>/shot_<n>.ppm.
# SAVE_FROM=<earlier output dir> starts from
# its save folder (a second launch: pipelines.bin, the D3D12 pipeline cache). GAME=<id> runs build-win64-<id>
# (tools/build_windows.sh); SKY_PORTAL_PLACE and SKY_LEVEL pass through. The write watch is off (SKY_WRITE_WATCH=0;
# SKY_WRITE_WATCH= turns it on): its write faults under Wine on Rosetta end in "invalid gdt selector" within a minute. Needs build-win64
# (tools/build_windows.sh) and Docker. Downloads vkd3d-proton 3.0.1 (LGPL-2.1, only run here, never shipped) into
# private/vkd3d-proton. Under Rosetta, Wine sometimes dies after a few minutes
# ("rosetta error: invalid gdt selector"): an emulation problem, not the game's.
set -eu

[ $# -ge 2 ] || { echo "usage: $0 <game disc .iso> <output dir> [seconds] [d3d12|vulkan]" >&2; exit 1; }
root=$(cd "$(dirname "$0")/../.." && pwd)
iso=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
out=$2
seconds=${3:-120}
backend=${4:-d3d12}
image=ssarecomp-d3d12-test
vkd3d_version=3.0.1
vkd3d_sha256=3cf2315522af5e43605ef6d3c41dad91387040bf97199934f3f7ab76caaa2f0c

vkd3d=$root/private/vkd3d-proton/vkd3d-proton-$vkd3d_version
if [ ! -f "$vkd3d/x64/d3d12.dll" ]; then
    mkdir -p "$root/private/vkd3d-proton"
    archive=$root/private/vkd3d-proton/vkd3d-proton-$vkd3d_version.tar.zst
    curl -fL --retry 3 -o "$archive" \
        "https://github.com/HansKristian-Work/vkd3d-proton/releases/download/v$vkd3d_version/vkd3d-proton-$vkd3d_version.tar.zst"
    shasum -a 256 "$archive" | grep -q "^$vkd3d_sha256 " || { echo "error: $archive does not match the pinned SHA-256" >&2; exit 1; }
    zstd -dc "$archive" | tar -x -C "$root/private/vkd3d-proton"
fi
docker run --rm --platform linux/amd64 "$image" test -f /usr/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json 2> /dev/null ||
    docker build --platform linux/amd64 -t "$image" -f "$root/tools/windows/d3d12_test.Dockerfile" "$root/tools/windows"

rm -rf "$out"
mkdir -p "$out/save" "$out/exe" "$root/private/wine" # Wine makes its prefix only inside an existing folder
out=$(cd "$out" && pwd)
# SAVE_FROM=<earlier output dir>: start from its saves, pipelines.bin and pipelines.d3d12 (a second launch).
[ -z "${SAVE_FROM:-}" ] || cp -R "$SAVE_FROM/save/." "$out/save/"
game=${GAME:-ssa}
build=$root/build-win64
[ "$game" = ssa ] || build=$build-$game
exe=${EXE:-"$build/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt").exe"} # EXE: another build (a player kit's: docs/tools/arkeyan-builder.md)
cp "$exe" "$vkd3d/x64/d3d12.dll" "$vkd3d/x64/d3d12core.dll" "$out/exe/"
cp -R "$(dirname "$exe")/fonts" "$out/exe/fonts"

windows() { printf 'Z:%s' "$1" | tr / '\\'; } # Wine maps / to drive Z:

docker run --rm --platform linux/amd64 -v "$root:$root" -v "$iso:$iso:ro" -v "$out:$out" -w "$out" \
    -e WINEPREFIX="$root/private/wine/d3d12-test-prefix" -e WINEDEBUG=-all -e VKD3D_DEBUG=err \
    -e WINEDLLOVERRIDES="d3d12,d3d12core=n" -e SKY_GPU_BACKEND="$backend" -e ARGS="${ARGS:-}" \
    -e VKD3D_CONFIG="$([ "${VALIDATE:-0}" = 1 ] && echo vk_debug)" \
    -e VK_INSTANCE_LAYERS="$([ "${VALIDATE:-0}" = 1 ] && echo VK_LAYER_KHRONOS_validation)" -e SDL_VIDEO_DRIVER=offscreen \
    -e SDL_AUDIO_DRIVER=disk -e SDL_AUDIO_DISK_OUTPUT_FILE=NUL -e SKY_SKIP_MOVIES="${SKY_SKIP_MOVIES-1}" -e SKY_RENDER_STATS=1 \
    -e SKY_SAVE_DIR="$(windows "$out/save")" -e SKY_PORTAL=emulated -e SKY_FIGURES="$(windows "$root/private/figures")" \
    -e SKY_PORTAL_PLACE="${SKY_PORTAL_PLACE:-new_14.sky}" -e SKY_LEVEL="${SKY_LEVEL:-}" -e SKY_WRITE_WATCH="${SKY_WRITE_WATCH-0}" -e SKY_DUMP_FRAME="${SKY_DUMP_FRAME:-12}" -e SKY_DUMP_PREFIX="$(windows "$out/dump")" \
    -e SKY_TOYBOX_SCRIPT="${SKY_TOYBOX_SCRIPT:-}" -e SKY_TOYBOX_SHOTS="$(windows "$out/toybox")" \
    -e SKY_UPSCALER="${SKY_UPSCALER:-}" -e SKY_FSR_QUALITY="${SKY_FSR_QUALITY:-}" -e SKY_WINDOW_SIZE="${SKY_WINDOW_SIZE:-}" \
    -e SKY_GPU_SNAPSHOT="$([ -n "${SKY_GPU_SNAPSHOT_EVERY:-}" ] && windows "$out/shot")" \
    -e SKY_GPU_SNAPSHOT_FROM="${SKY_GPU_SNAPSHOT_FROM:-}" -e SKY_GPU_SNAPSHOT_EVERY="${SKY_GPU_SNAPSHOT_EVERY:-}" \
    -e SKY_PRESS="${SKY_PRESS-start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60}" \
    -e EXE="$out/exe/$(basename "$exe")" -e ISO="$(windows "$iso")" -e LOG="$out/log" -e SECONDS_LIMIT="$seconds" \
    "$image" sh -c 'Xvfb :99 -screen 0 1280x800x24 > /dev/null 2>&1 & export DISPLAY=:99; sleep 1;
        timeout -s KILL "$SECONDS_LIMIT" /usr/lib/wine/wine64 "$EXE" "$ISO" $ARGS > "$LOG" 2>&1; true'

echo "level_027: $(grep -c 'level_027' "$out/log" || true), rejected pipelines: $(grep -c 'CreateGraphicsPipelineState failed' "$out/log" || true)"
grep -E '^\[gpu\]|Fault|\[render\] frame' "$out/log" | tail -6 || true
