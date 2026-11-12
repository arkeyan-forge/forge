#!/bin/sh
# Derives everything the runtime build needs from the player's disc, on the build machine (macOS or Linux):
#   private/default.xex, private/image.bin   the game executable and its loaded image (tools/xexdump)
#   private/ssa/ppc/                                     the game code translated by XenonRecomp (games/ssa/config/SSA.toml)
#   private/ssa/ppc/shader_cache.cpp                     the game's shaders translated by XenosRecomp (tools/shaders/build_cache.sh)
# None of it may be committed or distributed: it is derived from the disc.
#
#   tools/prepare_game.sh <game disc .iso> [--shaders] [--shader-dump <dir>]
#
# private/ssa/ppc/ files are only rewritten when their content changes, so a rerun does not recompile the game code. The shader
# cache is built when it is missing or has no DXIL (built before D3D12 rendered the game), or again with --shaders.
# <dir> holds shaders the game builds at run time (the movie player's), saved by a run with SKY_DUMP_SHADERS=<dir>;
# without it Bink movies are not drawn by the GPU path. A cache built on macOS holds Metal AIR, SPIR-V and DXIL (all
# platforms); one built on Linux has no AIR (no macOS build).
set -eu

usage() { echo "usage: $0 <game disc .iso> [--shaders] [--shader-dump <dir>]" >&2; exit 1; }
[ $# -ge 1 ] || usage
iso=$1
shift
shaders=0
dump=
while [ $# -gt 0 ]; do
    case "$1" in
    --shaders) shaders=1 ;;
    --shader-dump) [ $# -ge 2 ] || usage; dump=$2; shift ;;
    *) usage ;;
    esac
    shift
done
[ -f "$iso" ] || { echo "error: no disc image at $iso" >&2; exit 1; }

root=$(cd "$(dirname "$0")/.." && pwd)
tools=$root/private/build-host-tools
jobs=${JOBS:-4}
cd "$root"

if [ ! -f thirdparty/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc.exe ] || [ ! -f thirdparty/SDL/CMakeLists.txt ]; then
    git submodule update --init --recursive
fi

# Host tools (the root project applies patches/*.patch to the submodules at configure time). No FFmpeg needed here.
mkdir -p private
cmake -S . -B "$tools" -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_FFMPEG=OFF -DARKEYAN_TOOLS_ONLY=ON > private/host-tools.log 2>&1 &&
    ninja -C "$tools" -j"$jobs" XenonRecomp xexdump >> private/host-tools.log 2>&1 ||
    { tail -30 private/host-tools.log; exit 1; }

python3 -I tools/shaders/xdvdfs.py "$iso" private /default.xex > /dev/null
"$tools/tools/xexdump/xexdump" private/default.xex private > private/xexdump.txt

# XenonRecomp reads its paths relative to the TOML (config/../private/default.xex, config/../ppc): run it on a copy laid
# out the same way, then move over only what changed.
work=$root/private/ppc-gen
rm -rf "$work"
mkdir -p "$work/config" "$work/private" "$work/ppc" private/ssa/ppc
cp games/ssa/config/* "$work/config/"
ln -s "$root/private/default.xex" "$work/private/default.xex"
"$tools/thirdparty/XenonRecomp/XenonRecomp/XenonRecomp" "$work/config/SSA.toml" \
    thirdparty/XenonRecomp/XenonUtils/ppc_context.h > private/xenonrecomp.log
changed=0
for file in "$work"/ppc/*; do
    name=$(basename "$file")
    if ! cmp -s "$file" "private/ssa/ppc/$name"; then
        cp "$file" "private/ssa/ppc/$name"
        changed=$((changed + 1))
    fi
done
rm -rf "$work"
echo "private/ssa/ppc/: $changed file(s) updated"

if [ "$shaders" = 1 ] || [ ! -f private/ssa/ppc/shader_cache.cpp ] || ! grep -q g_compressedDxilCache private/ssa/ppc/shader_cache.cpp; then
    if [ -n "$dump" ]; then
        tools/shaders/build_cache.sh "$iso" "$dump"
    else
        tools/shaders/build_cache.sh "$iso"
    fi
fi
