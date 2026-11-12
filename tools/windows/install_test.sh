#!/bin/bash
# The Windows kit's own installer, then its installed game on Wine/lavapipe in the existing test image.
# Usage: tools/windows/install_test.sh <ssa|giants|swapforce|trapteam> <iso> <out below .scratch/>
# WINDOWS_KIT defaults to dist/skylanders-kit-windows-x64; WINDOWS_TOOLCHAIN must name an already extracted
# Windows llvm-mingw toolchain. No image, compiler or dependency is downloaded. All writes stay below <out>.
# INSTALL_SECONDS=3600, RUN_SECONDS=180 (minimum 120), WINDOWS_TEST_IMAGE=ssarecomp-d3d12-test.
set -euo pipefail
[ "$#" -eq 3 ] || { echo "usage: $0 <game> <iso> <out below .scratch/>" >&2; exit 2; }
case $1 in ssa|giants|swapforce|trapteam) game=$1 ;; *) echo "error: unknown game" >&2; exit 2 ;; esac
root=$(cd "$(dirname "$0")/../.." && pwd)
check=$root/tools/windows/install_check.py
# Resolve before creating anything: no symlink can send output outside this clone's scratch directory.
out=$(python3 -I "$check" prepare "$root" "$3")
iso=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
[ -f "$iso" ] || { echo "error: disc image is missing" >&2; exit 2; }
kit=$(cd "${WINDOWS_KIT:-$root/dist/skylanders-kit-windows-x64}" && pwd)
[ -f "$kit/Arkeyan-Builder.exe" ] || { echo "error: kit has no Arkeyan-Builder.exe" >&2; exit 2; }
[ -n "${WINDOWS_TOOLCHAIN:-}" ] || { echo "error: set WINDOWS_TOOLCHAIN to an extracted Windows llvm-mingw toolchain" >&2; exit 2; }
toolchain=$(cd "$WINDOWS_TOOLCHAIN" && pwd)
[ -f "$toolchain/bin/x86_64-w64-mingw32-clang++.exe" ] || { echo "error: Windows compiler is missing" >&2; exit 2; }
image=${WINDOWS_TEST_IMAGE:-ssarecomp-d3d12-test}
install_seconds=${INSTALL_SECONDS:-3600}
run_seconds=${RUN_SECONDS:-180}
case $install_seconds:$run_seconds in *[!0-9:]*|:*|*:) echo "error: durations must be positive integers" >&2; exit 2 ;; esac
[ "$install_seconds" -gt 0 ] && [ "$run_seconds" -ge 120 ] || { echo "error: install limit must be positive; run at least 120 seconds" >&2; exit 2; }
runtime=$(python3 -I "$check" runtime "$kit" "$game")
docker image inspect "$image" > /dev/null || { echo "error: the existing Windows test image is required" >&2; exit 2; }
mkdir -p "$out"
# Keep each attempt's log. One Rosetta recovery is allowed across the entire test, including the movie child.
resumed=0
rosetta() { grep -qi 'invalid gdt selector' "$@" 2>/dev/null; }
recover() {
    [ "$resumed" -eq 0 ] || { echo "FAIL: Rosetta crashed again after the one resume" >&2; exit 1; }
    resumed=1
    echo "@warn Rosetta invalid gdt selector; resuming $1 once (keeping install work and saves)" | tee -a "$out/recovery.txt"
}
install_once() {
    docker run --rm --pull=never --network none --platform linux/amd64 \
        -v "$kit:/kit:ro" -v "$toolchain:/toolchain:ro" -v "$iso:/disc.iso:ro" -v "$out:/work" -w /work \
        -e WINEPREFIX=/work/wine -e WINEDEBUG=-all -e XDG_CACHE_HOME=/work/cache -e SKYLANDERS_DATA='Z:\work\data' \
        -e LIMIT="$install_seconds" "$image" sh -c '
            Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & export DISPLAY=:99; sleep 1
            timeout -k 10 "$LIMIT" /usr/lib/wine/wine64 /kit/Arkeyan-Builder.exe \
                --iso "Z:\disc.iso" --out "Z:\work\game" --kit "Z:\kit" --toolchain "Z:\toolchain" --jobs 2'
}
attempt=1
while :; do
    status=0
    install_once > "$out/install-$attempt.log" 2>&1 || status=$?
    if [ "$status" -eq 0 ] && python3 -I "$check" install "$kit" "$game" "$out" "$out/install-$attempt.log"; then break; fi
    if rosetta "$out/install-$attempt.log" "$out/game/arkeyan-work/movies/game.log"; then
        recover install
        attempt=$((attempt + 1))
    else
        echo "FAIL: Windows install incomplete (status $status); see install-$attempt.log" >&2; exit 1
    fi
done
# Use the installed runtime unchanged. Its presented-image capture is available on the offscreen Vulkan backend.
press=$(python3 -I -c 'print(",".join([f"start:{s}" for s in range(1, 6)] + [f"a:{s}" for s in range(8, 121, 4)]))')
play_once() {
    docker run --rm --pull=never --network none --platform linux/amd64 \
        -v "$iso:/disc.iso:ro" -v "$out:/work" -w /work/game \
        -e WINEPREFIX=/work/wine -e WINEDEBUG=-all -e XDG_CACHE_HOME=/work/cache \
        -e RUNTIME="$runtime" -e LIMIT="$run_seconds" -e ATTEMPT="$attempt" \
        -e SKY_GPU_BACKEND=vulkan -e SDL_VIDEO_DRIVER=offscreen -e SDL_AUDIO_DRIVER=dummy \
        -e SKYLANDERS_DATA='Z:\work\data' -e SKY_SETTINGS='Z:\work\settings.ini' \
        -e SKY_LAUNCHER_INI='Z:\work\launcher.ini' -e SKY_SAVE_DIR='Z:\work\saves' \
        -e SKY_COLLECTION='Z:\work\collection' -e SKY_FIGURES='Z:\work\figures' \
        -e SKY_PORTAL=emulated -e SKY_PORTAL_PLACE=new:14 -e SKY_SKIP_MOVIES=0 -e SKY_PRESS="$press" \
        -e SKY_GPU_SNAPSHOT="Z:\\work\\run-$attempt\\shot" -e SKY_GPU_SNAPSHOT_EVERY=1000000 \
        -e SKY_GPU_SNAPSHOT_SECONDS="$((run_seconds - 60)),$((run_seconds - 30)),$((run_seconds - 5))" \
        -e SKY_LOG="Z:\\work\\run-$attempt\\runtime.log" "$image" sh -c '
            Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & export DISPLAY=:99; sleep 1
            timeout -k 10 "$LIMIT" /usr/lib/wine/wine64 "/work/game/$RUNTIME.exe" "Z:\disc.iso" --vsync 0 --fps-limit 30
            status=$?; echo "$status" > "/work/run-$ATTEMPT/status"'
}
attempt=1
mkdir -p "$out/saves" "$out/collection" "$out/figures"
while :; do
    mkdir -p "$out/run-$attempt"
    status=0
    play_once > "$out/run-$attempt/stdout.log" 2>&1 || status=$?
    if rosetta "$out/run-$attempt/stdout.log" "$out/run-$attempt/runtime.log"; then
        recover game
        attempt=$((attempt + 1))
    elif [ "$status" -ne 0 ]; then
        echo "FAIL: container run failed (status $status)" >&2; exit 1
    else
        python3 -I "$check" play "$out" "$run_seconds" "$attempt"
        break
    fi
done
echo "ALL PASS: Windows install and Vulkan picture; Rosetta resumes: $resumed"
