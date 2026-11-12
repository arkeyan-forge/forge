#!/bin/sh
# Validates every DXIL container of private/ssa/ppc/shader_cache.cpp with Microsoft's validator (dxil.dll) under Wine, and checks
# that XenosRecomp signed them as the validator does. Needs llvm-mingw (tools/build_windows.sh fetches it), Wine
# (WINE, default private/wine/wswine.bundle/bin/wine) and the zstd command line tool.
#
#   tools/shaders/check_dxil.sh [<shader_cache.cpp>]
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
cache=${1:-$root/private/ssa/ppc/shader_cache.cpp}
mingw=${LLVM_MINGW_ROOT:-$root/private/toolchains/llvm-mingw-20260922-ucrt-macos-universal}
wine=${WINE:-$root/private/wine/wswine.bundle/bin/wine}
export WINEPREFIX=${WINEPREFIX:-$root/private/wine/prefix} WINEDEBUG=-all
work=$root/private/dxil-check
mkdir -p "$work"

"$mingw/bin/x86_64-w64-mingw32-clang++" -O2 -fms-extensions -I "$root/thirdparty/XenosRecomp/thirdparty/dxc-bin/inc" \
    "$root/tools/shaders/dxil_check.cpp" -o "$work/dxil_check.exe" -static -luuid
python3 -I "$root/tools/shaders/extract_dxil.py" "$cache" "$work/containers.bin"
cp "$root/thirdparty/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxil.dll" "$work/"
windows() { printf 'Z:%s' "$1" | tr / '\\'; }
"$wine" "$work/dxil_check.exe" "$(windows "$work/dxil.dll")" "$(windows "$work/containers.bin")"
