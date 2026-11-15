#!/bin/sh
# Builds the three player kits on a Mac from public sources (KIT_LAYOUT_RECORD=1: no disc, no private/<game>/), checks
# each one, and with --attach adds them to the launcher's draft release (docs/release/launcher/updates_release.md).
#
#   tools/ci/kits_local.sh [--attach vX.Y.Z] [macos] [linux] [windows]     (default: the three)
#
# macOS natively (Xcode's Metal compiler, Homebrew's cmake ninja rsync nasm xz molten-vk), Windows cross-compiled with
# llvm-mingw, Linux in its Docker image (tools/make_kit.sh). --attach needs `gh` logged in with write access to
# KITS_LAUNCHER_REPO (default mflr0/Arkeyan-Launcher) and every kit; the draft must already hold the launcher's files
# and SHA256SUMS (tools/release_local.sh in the launcher). JOBS (default 4) sets the parallelism.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
tag=""
if [ "${1:-}" = --attach ]; then tag=${2:?--attach needs a tag}; shift 2; fi
platforms=${*:-macos linux windows}
export LAUNCHER_TAG=$tag KITS_LAUNCHER_REPO=${KITS_LAUNCHER_REPO:-mflr0/Arkeyan-Launcher}
export JOBS=${JOBS:-4} KIT_LAYOUT_RECORD=1
# The kits made on a Mac so far run on macOS 26 and later (LSMinimumSystemVersion 26.0).
export MACOSX_DEPLOYMENT_TARGET=26.0
[ -z "$tag" ] || python3 -I tools/ci/kits.py destination
[ "$(uname -s)" = Darwin ] && [ "$(uname -m)" = arm64 ] || { echo "error: needs an Apple Silicon Mac" >&2; exit 1; }
xcrun --find metal >/dev/null
for package in cmake ninja rsync nasm xz molten-vk; do
    brew list --versions "$package" >/dev/null || { echo "error: brew install $package" >&2; exit 1; }
done
LLVM_MINGW_ROOT=$(sh tools/ci/fetch_llvm_mingw.sh)
export LLVM_MINGW_ROOT

for platform in $platforms; do
    nice tools/make_kit.sh "$platform"
    case $platform in
        macos) sh tools/ci/check_kit.sh macos-arm64 ;;
        linux) sh tools/ci/check_kit.sh linux-x64 ;;
        windows) sh tools/ci/check_kit.sh windows-x64 ;;
        *) echo "error: unknown platform $platform" >&2; exit 2 ;;
    esac
done

if [ -n "$tag" ]; then
    candidates=.scratch/kits-$tag
    rm -rf "${candidates:?}"
    mkdir -p "$candidates"
    cp dist/skylanders-kit-windows-x64.zip dist/skylanders-kit-macos-arm64.zip dist/skylanders-kit-linux-x64.zip \
        "$candidates"/
    python3 -I tools/ci/kits.py publish "$candidates"
    echo "kits attached to the draft $tag of $KITS_LAUNCHER_REPO"
fi
