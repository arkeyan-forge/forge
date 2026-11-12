#!/bin/sh
# Proves our PowerPC decoder (tools/xenonrecomp/ppc_decoder.cpp) decodes as the binutils disassembler XenonRecomp used
# before it: every word of each executable's code sections, or all 2^32 words (--all, about half an hour on 8 cores).
# Local only: the reference is GPL code, taken from the XenonRecomp submodule with the translator's last change to it
# (the mulchw entries off, from the commit before our decoder) into private/, built there, never committed or shipped.
#   tools/xenonrecomp/decoder_compare.sh <default.xex>... | --all
set -eu
[ $# -ge 1 ] || { echo "usage: $0 <default.xex>... | --all" >&2; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
before=6be5136 # the last commit whose XenonRecomp used binutils' disassembler
work=$root/private/decoder-compare
ref=$work/reference
rm -rf "${ref:?}"
mkdir -p "$ref"
git -C thirdparty/XenonRecomp archive HEAD thirdparty/disasm | tar -x -C "$ref"
git show "$before:patches/xenonrecomp.patch" |
    awk '/^diff --git/ { keep = ($0 ~ /thirdparty\/disasm\//) } keep' | patch -s -p1 -d "$ref"
cmake -S . -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_FFMPEG=OFF -DARKEYAN_TOOLS_ONLY=ON \
    -DARKEYAN_DECODER_REFERENCE="$ref/thirdparty/disasm" > "$work/cmake.log" 2>&1 || { tail -20 "$work/cmake.log"; exit 1; }
nice ninja -C "$work/build" -j"${JOBS:-4}" ArkeyanDecoderCompare > "$work/build.log" 2>&1 || { tail -20 "$work/build.log"; exit 1; }
nice "$work/build/tools/xenonrecomp/ArkeyanDecoderCompare" "$@"
