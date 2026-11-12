#!/bin/sh
# Cross-compiles "Skylanders Spyro's Adventure.exe" for Windows 10/11 x64 on macOS with llvm-mingw, from a checkout and the player's disc.
#
#   tools/build_windows.sh <game disc .iso> [--shader-dump <dir>]
#
# Steps: tools/prepare_game.sh (ppc/, shader cache), llvm-mingw (LLVM_MINGW_ROOT, else
# private/toolchains/llvm-mingw-<version>-ucrt-macos-universal, downloaded when missing), the minimal LGPL FFmpeg
# (private/ffmpeg/windows-x64), then build-win64. JOBS (default 4) sets the parallel jobs; the first build compiles the
# translated game code for x64 (10 to 30 min). GAME=<id> (default ssa) builds another game into build-win64-<id>, from its
# private/<id>/ppc/ (with the shader cache) prepared on this Mac (docs/getting-started/testing.md, "A fresh clone"): the
# disc is then not read.
#
# Output: build-win64/dist/ArkeyanRuntime/ ("Skylanders Spyro's Adventure.exe", fonts/, THIRD_PARTY_NOTICES.md): copy the folder to the PC and
# run "Skylanders Spyro's Adventure.exe" (docs/getting-started/platforms/windows.md). The executable contains code and shaders translated from your disc: it is for
# your own use, never share it. The disc image is not in it.
set -eu

LLVM_MINGW_VERSION=20260922
LLVM_MINGW_ASSET=llvm-mingw-$LLVM_MINGW_VERSION-ucrt-macos-universal
LLVM_MINGW_SHA256=52e5f5a7b131021d0c39a37a38fa380a1da7885cd04bd61afd0cd4ecfb8bc1f3

usage() { echo "usage: $0 <game disc .iso> [--shader-dump <dir>]" >&2; exit 1; }
[ $# -ge 1 ] || usage
iso=$1
shift
prepare_args=
while [ $# -gt 0 ]; do
    case "$1" in
    --shader-dump) [ $# -ge 2 ] || usage; prepare_args="--shader-dump $2"; shift ;;
    *) usage ;;
    esac
    shift
done

root=$(cd "$(dirname "$0")/.." && pwd)
jobs=${JOBS:-4}
game=${GAME:-ssa}
build=$root/build-win64
cd "$root"
runtime=$(sed -n 's/^runtime=//p' "games/$game/game.txt")

if [ "$game" = ssa ]; then
    # shellcheck disable=SC2086 # prepare_args is two words or none
    tools/prepare_game.sh "$iso" $prepare_args
else
    build=$build-$game
    [ -f "private/$game/ppc/shader_cache.cpp" ] || { echo "error: no private/$game/ppc/shader_cache.cpp: prepare $game first" >&2; exit 1; }
fi

if [ -z "${LLVM_MINGW_ROOT:-}" ]; then
    LLVM_MINGW_ROOT=$root/private/toolchains/$LLVM_MINGW_ASSET
    if [ ! -x "$LLVM_MINGW_ROOT/bin/x86_64-w64-mingw32-clang" ]; then
        [ "$(uname -s)" = Darwin ] || { echo "error: set LLVM_MINGW_ROOT to an llvm-mingw ucrt release" >&2; exit 1; }
        archive=$root/private/toolchains/download/$LLVM_MINGW_ASSET.tar.xz
        mkdir -p "$(dirname "$archive")"
        if [ ! -f "$archive" ]; then
            curl -fL --retry 3 -o "$archive.part" \
                "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VERSION/$LLVM_MINGW_ASSET.tar.xz"
            mv "$archive.part" "$archive"
        fi
        shasum -a 256 "$archive" | grep -q "^$LLVM_MINGW_SHA256 " ||
            { echo "error: $archive does not match the pinned SHA-256" >&2; exit 1; }
        tar -xJf "$archive" -C "$root/private/toolchains"
    fi
fi
export LLVM_MINGW_ROOT

ffmpeg=$root/private/ffmpeg/windows-x64
[ -f "$ffmpeg/lib/libavcodec.a" ] || JOBS=$jobs tools/ffmpeg/build.sh windows-x64 "$ffmpeg"

cmake -S . -B "$build" -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-w64-mingw32-clang.cmake \
    -DLLVM_MINGW_ROOT="$LLVM_MINGW_ROOT" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG" -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" -DARKEYAN_FFMPEG_ROOT="$ffmpeg" -DARKEYAN_GAME="$game"
ninja -C "$build" -j"$jobs" ArkeyanRuntime

dist=$build/dist/ArkeyanRuntime
rm -rf "$dist"
mkdir -p "$dist"
cp "$build/ArkeyanRuntime/$runtime.exe" "$build/ArkeyanRuntime/THIRD_PARTY_NOTICES.md" "$dist/"
cp -R "$build/ArkeyanRuntime/fonts" "$dist/fonts"
echo "DLLs imported by the game executable:"
"$LLVM_MINGW_ROOT/bin/llvm-objdump" -p "$dist/$runtime.exe" | sed -n 's/^ *DLL Name: /  /p'
echo "Built $dist"
