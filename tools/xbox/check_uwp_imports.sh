#!/usr/bin/env bash
# Checks that every function a Windows executable imports is part of the UWP API surface: the windowsapp and
# ucrtapp umbrella import libraries of llvm-mingw, plus optional extra .def files for app-local DLLs.
# An import outside that set may be missing on Xbox and make the app fail at launch.
#
#   tools/xbox/check_uwp_imports.sh <exe-or-dll> [extra.def ...]
#
# Needs LLVM_MINGW_ROOT (default: private/xbox/toolchain/llvm-mingw from tools/xbox/setup_tools.sh).
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="${LLVM_MINGW_ROOT:-$here/../../private/xbox/toolchain/llvm-mingw}"
nm="$root/bin/llvm-nm"
readobj="$root/bin/llvm-readobj"
lib="$root/x86_64-w64-mingw32/lib"

[[ $# -ge 1 ]] || { echo "usage: $0 <exe-or-dll> [extra.def ...]" >&2; exit 2; }
binary="$1"
shift
[[ -x "$nm" && -x "$readobj" ]] || { echo "error: llvm-mingw not found at $root" >&2; exit 2; }

allowed="$(mktemp)"
imports="$(mktemp)"
trap 'rm -f "$allowed" "$imports"' EXIT

# "dll symbol" pairs, DLL names lower-cased. Archive members of import libraries are named after the DLL.
for archive in "$lib/libwindowsapp.a" "$lib/libucrtapp.a"; do
    "$nm" -A "$archive" 2>/dev/null |
        awk -F': ' '$NF ~ / T / { split($1, a, ":"); dll = tolower(a[2]); n = split($NF, f, " "); print dll, f[n] }'
done >"$allowed"
for def in "$@"; do
    awk 'toupper($1) == "LIBRARY" { dll = tolower($2) } dll != "" && $1 !~ /^(;|LIBRARY|EXPORTS)/ && NF { print dll, $1 }' "$def"
done >>"$allowed"

"$readobj" --coff-imports "$binary" |
    awk '/^ *Name: / { dll = tolower($2) } /^ *Symbol: / { print dll, $2 }' >"$imports"

bad="$(awk 'FILENAME == ARGV[1] { ok[$0] = 1; next } !($0 in ok)' "$allowed" "$imports")"
total="$(wc -l <"$imports" | tr -d ' ')"
dlls="$(awk '{ print $1 }' "$imports" | sort -u | tr '\n' ' ')"
echo "imports: $total functions from: $dlls"
if [[ -n "$bad" ]]; then
    echo "NOT in the UWP API surface:"
    echo "$bad" | sed 's/^/  /'
    exit 1
fi
echo "OK: every import is in windowsapp/ucrtapp${*:+ or the extra .def files}"
