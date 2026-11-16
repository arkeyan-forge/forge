#!/usr/bin/env bash
# Builds the three player kits on a Mac from public sources (KIT_LAYOUT_RECORD=1: no disc, no private/<game>/), checks
# each one, and with --attach adds them to the launcher's draft release (docs/release/launcher/updates_release.md).
#
#   tools/ci/kits_local.sh [--attach vX.Y.Z] [--serial] [macos] [linux] [windows]     (default: the three)
#
# macOS natively (Xcode's Metal compiler, Homebrew's cmake ninja rsync nasm xz molten-vk ccache), Windows
# cross-compiled with llvm-mingw, Linux in its Docker image (tools/make_kit.sh). --attach needs `gh` logged in with write access to
# KITS_LAUNCHER_REPO (default mflr0/Arkeyan-Launcher) and every kit; the draft must already hold the launcher's files
# and SHA256SUMS (tools/release_local.sh in the launcher). The platforms build at the same time, one log each in
# .scratch/kits/<platform>.log, and the first failure stops the others; --serial builds them one after the other, on
# the terminal. JOBS (default 4) sets the parallelism of each platform.
set -eu

root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root"
tag=""
if [ "${1:-}" = --attach ]; then tag=${2:?--attach needs a tag}; shift 2; fi
serial=0
platforms=""
for arg; do
    case $arg in
        --serial) serial=1 ;;
        macos | linux | windows) platforms="$platforms $arg" ;;
        *) echo "usage: tools/ci/kits_local.sh [--attach vX.Y.Z] [--serial] [macos] [linux] [windows]" >&2; exit 2 ;;
    esac
done
platforms=${platforms:-macos linux windows}
export LAUNCHER_TAG=$tag KITS_LAUNCHER_REPO=${KITS_LAUNCHER_REPO:-mflr0/Arkeyan-Launcher}
export JOBS=${JOBS:-4} KIT_LAYOUT_RECORD=1
# Compiles from earlier kit builds are reused (tools/make_kit.sh, KIT_CCACHE); KIT_CCACHE=0 builds cold.
export KIT_CCACHE=${KIT_CCACHE:-1}
# The kits made on a Mac so far run on macOS 26 and later (LSMinimumSystemVersion 26.0).
export MACOSX_DEPLOYMENT_TARGET=26.0
[ -z "$tag" ] || python3 -I tools/ci/kits.py destination
[ "$(uname -s)" = Darwin ] && [ "$(uname -m)" = arm64 ] || { echo "error: needs an Apple Silicon Mac" >&2; exit 1; }
xcrun --find metal >/dev/null
for package in cmake ninja rsync nasm xz molten-vk ccache; do
    brew list --versions "$package" >/dev/null || { echo "error: brew install $package" >&2; exit 1; }
done
LLVM_MINGW_ROOT=$(sh tools/ci/fetch_llvm_mingw.sh)
export LLVM_MINGW_ROOT

# The patches, the public headers and the FFmpeg source, which the platforms share, once (tools/make_kit.sh).
nice tools/make_kit.sh prepare
export KIT_PREPARED=1
build() { # <platform>
    case $1 in macos) target=macos-arm64 ;; linux) target=linux-x64 ;; *) target=windows-x64 ;; esac
    nice tools/make_kit.sh "$1" && sh tools/ci/check_kit.sh "$target"
}
if [ $serial = 1 ]; then
    for platform in $platforms; do build "$platform"; done
else
    logs=$root/.scratch/kits
    mkdir -p "$logs"
    # Each platform in its own process group (bash: even without a terminal): a failure, or Ctrl-C (which background
    # jobs do not get), stops all of its processes, and only those.
    set -m
    pids=""
    stop() { for pid in $pids; do kill -TERM -- "-$pid" 2> /dev/null || :; done; wait 2> /dev/null; }
    trap 'stop; exit 130' INT TERM
    for platform in $platforms; do
        rm -f "$logs/$platform.status"
        (
            set +e
            start=$(date +%s)
            build "$platform" > "$logs/$platform.log" 2>&1
            echo "$? $(($(date +%s) - start))" > "$logs/$platform.status.part"
            mv "$logs/$platform.status.part" "$logs/$platform.status"
        ) &
        pids="$pids $!"
        echo "$platform: building, log in $logs/$platform.log"
    done
    left=$platforms
    failed=""
    while [ -n "$left" ] && [ -z "$failed" ]; do
        sleep 10
        running=""
        for platform in $left; do
            if [ ! -f "$logs/$platform.status" ]; then running="$running $platform"; continue; fi
            read -r result took < "$logs/$platform.status"
            if [ "$result" = 0 ]; then echo "$platform: PASS (${took}s)"; else echo "$platform: FAIL (${took}s)"; failed=$platform; fi
        done
        left=$running
    done
    if [ -n "$failed" ]; then
        stop
        tail -30 "$logs/$failed.log" | sed 's/^/    /' >&2
        echo "error: the $failed kit failed (log: $logs/$failed.log); stopped:${left:- nothing}" >&2
        exit 1
    fi
    wait
    trap - INT TERM
fi

if [ -n "$tag" ]; then
    candidates=.scratch/kits-$tag
    rm -rf "${candidates:?}"
    mkdir -p "$candidates"
    cp dist/skylanders-kit-windows-x64.zip dist/skylanders-kit-macos-arm64.zip dist/skylanders-kit-linux-x64.zip \
        "$candidates"/
    python3 -I tools/ci/kits.py publish "$candidates"
    echo "kits attached to the draft $tag of $KITS_LAUNCHER_REPO"
fi
