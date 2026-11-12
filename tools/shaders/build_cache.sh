#!/bin/sh
# Translates every Xenos shader of the game into host shaders (Metal AIR on macOS, SPIR-V and signed DXIL everywhere)
# and writes private/<game>/ppc/shader_cache.cpp, which the runtime links. Like the ppc/ next to it, the output is derived from the player's disc and
# is never committed.
#
#   tools/shaders/build_cache.sh <game disc .iso> [<dump dir>]
#   SKY_GAME=giants tools/shaders/build_cache.sh <disc> <dump dir>   another game: reads private/<game>/, writes
#                                                                     private/<game>/ppc/shader_cache.cpp
#   SKY_SHADER_OUT=<dir> ...   writes <dir>/shader_cache.cpp instead, with its input in <dir>/input (the game's
#                              ppc/ and shaders/input are left alone)
#
# Inputs: /shaders.lsa and /effects.lsa from the disc, private/image.bin (the loaded default.xex, written by
# tools/xexdump), which embeds 81 more shaders, and the containers a run with SKY_DUMP_SHADERS=<dump dir> saved (shaders
# the game builds at run time, such as the movie player's). Takes about 1.5 min on an M1 Pro.
set -eu

iso=$1
root=$(cd "$(dirname "$0")/../.." && pwd)
tools=$root/private/build-xenos
game=${SKY_GAME:-${SSA_GAME:-ssa}}
out=$root/private/$game/ppc
if [ "$game" = ssa ]; then data=$root/private; else data=$root/private/$game; fi
input=$data/shaders/input
SKY_SHADER_OUT=${SKY_SHADER_OUT:-${SSA_SHADER_OUT:-}} # the older SSA_* names still work
if [ -n "$SKY_SHADER_OUT" ]; then
    out=$SKY_SHADER_OUT
    input=$out/input
fi

cmake -S "$root/thirdparty/XenosRecomp" -B "$tools" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE=-O2 -DCMAKE_C_FLAGS_RELEASE=-O2 > /dev/null # asserts stay on: they reject bad shaders
ninja -C "$tools" XenosRecomp

rm -rf "${input:?}"
mkdir -p "$input" "$out"
if [ "$game" = ssa ]; then
    python3 -I "$root/tools/shaders/xdvdfs.py" "$iso" "$input" /shaders.lsa /effects.lsa
    cp "$data/image.bin" "$input/default.xex.image"
fi
# Giants and Swap Force (Alchemy) keep their shaders in compressed archives and none in the XEX: the dump dir holds
# what tools/shaders/giants_extract.py or swapforce_extract.py wrote, the run-time dump, or both.
if [ $# -ge 2 ] && [ -d "$2" ]; then
    mkdir -p "$input/runtime"
    cp "$2"/* "$input/runtime/" 2>/dev/null || true
fi

"$tools/XenosRecomp/XenosRecomp" "$input" "$out/shader_cache.cpp" \
    "$root/thirdparty/XenosRecomp/XenosRecomp/shader_common.h" | grep -v '^Recompiling shaders\.\.\.'
