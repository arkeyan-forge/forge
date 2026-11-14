#!/bin/sh
# Check the finished directory, then independently extract and verify the release ZIP without running kit programs.
set -eu

target=${1:?usage: check_kit.sh windows-x64|macos-arm64|linux-x64}
case "$target" in windows-x64 | macos-arm64 | linux-x64) ;; *) exit 2 ;; esac
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
kit=skylanders-kit-$target
tools/kit_scan.sh "dist/$kit"
python3 -I tools/ci/kits.py inventory "dist/$kit.zip"
mkdir -p .scratch/ci
temporary=$(mktemp -d .scratch/ci/extract.XXXXXXXX)
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
unzip -q "dist/$kit.zip" -d "$temporary"
(cd "$temporary/$kit" && shasum -a 256 -c --quiet --strict files.sha256)
tools/kit_scan.sh "$temporary/$kit"
case "$target" in
    macos-arm64)
        for program in Arkeyan-Builder bin/XenonRecomp bin/XenosRecomp; do
            test -x "$temporary/$kit/$program"
            codesign --verify --strict "$temporary/$kit/$program"
            if otool -l "$temporary/$kit/$program" | \
                awk '/cmd LC_RPATH/ { r = 1; next } r && $1 == "path" { print $2; r = 0 }' | grep -v '^@'; then
                echo "error: absolute rpath in kit program" >&2
                exit 1
            fi
        done
        ;;
    linux-x64)
        for program in Arkeyan-Builder bin/XenonRecomp bin/XenosRecomp; do
            test -x "$temporary/$kit/$program"
        done
        ;;
esac
