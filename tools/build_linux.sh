#!/bin/sh
# Builds ArkeyanRuntime for Linux (plume's Vulkan backend) in an Ubuntu 24.04 container (tools/linux/Dockerfile), from a
# checkout and the player's disc. Runs on macOS (OrbStack, Docker Desktop) or Linux with Docker.
#
#   tools/build_linux.sh <game disc .iso> [--arch arm64|amd64] [--shader-dump <dir>]
#
# The game data (private/ssa/ppc/, with the shader cache) is prepared on the host by tools/prepare_game.sh; the project is mounted
# into the container at /src, never copied into the image. Output: build-linux-<arch>/ArkeyanRuntime/Skylanders Spyro's Adventure (with its
# fonts/ next to it). It contains code and shaders translated from your disc: keep it to yourself.
# --arch defaults to the host's; the other one runs emulated (much slower). JOBS (default 4) sets the parallel jobs.
# GAME=<id> (default ssa) builds another game into build-linux-<arch>-<id>, from its private/<id>/ppc/ prepared on this Mac
# (docs/getting-started/testing.md, "A fresh clone"): the disc is then not read.
set -eu

usage() { echo "usage: $0 <game disc .iso> [--arch arm64|amd64] [--shader-dump <dir>]" >&2; exit 1; }
[ $# -ge 1 ] || usage
iso=$1
shift
arch=
prepare_args=
while [ $# -gt 0 ]; do
    case "$1" in
    --arch) [ $# -ge 2 ] || usage; arch=$2; shift ;;
    --shader-dump) [ $# -ge 2 ] || usage; prepare_args="--shader-dump $2"; shift ;;
    *) usage ;;
    esac
    shift
done
if [ -z "$arch" ]; then
    case "$(uname -m)" in
    arm64 | aarch64) arch=arm64 ;;
    *) arch=amd64 ;;
    esac
fi

root=$(cd "$(dirname "$0")/.." && pwd)
jobs=${JOBS:-4}
# The kit's Linux baseline (tools/make_kit.sh): without it the compiler's x86-64 is SSE2 and simde emulates SSSE3/SSE4.1.
march=
[ "$arch" = amd64 ] && march=-march=x86-64-v2
image=ssarecomp-linux-$arch
build=build-linux-$arch
game=${GAME:-ssa}

if [ "$game" = ssa ]; then
    # shellcheck disable=SC2086 # prepare_args is two words or none
    "$root/tools/prepare_game.sh" "$iso" $prepare_args
else
    build=$build-$game
    [ -f "$root/private/$game/ppc/shader_cache.cpp" ] || { echo "error: no private/$game/ppc/shader_cache.cpp: prepare $game first" >&2; exit 1; }
fi

docker build --platform "linux/$arch" -t "$image" "$root/tools/linux"
docker run --rm --platform "linux/$arch" --user "$(id -u):$(id -g)" -v "$root:/src" -e JOBS="$jobs" -e MARCH="$march" -e GAME="$game" "$image" sh -euc '
    ffmpeg=private/ffmpeg/linux-$(uname -m)
    [ -f "$ffmpeg/lib/libavcodec.a" ] || tools/ffmpeg/build.sh host "$ffmpeg"
    cmake -S . -B '"$build"' -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARKEYAN_FFMPEG_ROOT=/src/$ffmpeg -DARKEYAN_GAME="$GAME" \
        -DCMAKE_C_FLAGS="$MARCH" -DCMAKE_CXX_FLAGS="$MARCH"
    ninja -C '"$build"' -j"$JOBS" ArkeyanRuntime
'
echo "Built $root/$build/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")"
