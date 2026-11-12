#!/bin/bash
# FSR 3 in every game (docs/runtime/gpu/upscaling.md): each game whose disc is set (SSA_ISO, GIANTS_ISO, SWAPFORCE_ISO,
# TRAPTEAM_ISO, SUPERCHARGERS_ISO, IMAGINATORS_ISO) plays its first level with SKY_UPSCALER=fsr3 at a 2560x1440 window.
# Checks per game: FSR's context is made ("[fsr] FSR 3.1 upscaler"), FSR never fails ("no longer upscaled"), no
# "[guard]" line, frames written. SSA, Giants and Swap Force: one run each (~2 min, presses as in tools/test_all.sh);
# Trap Team, SuperChargers and Imaginators: their own route scripts (trapteam_chapters.sh ~16 min, superchargers_lobby.sh
# and imaginators_hub.sh ~9 min each) with FSR on, whose own checks must pass too.
#
#   tools/regress/fsr_all.sh [<game>...]     (default: every game whose disc is set)
#   SKY_FSR_QUALITY=<preset> passes through; EXE_DIR=<dir> overrides build-<game>/ (build/ for SSA).
#
# Every run is isolated (docs/getting-started/testing.md) under private/runs/fsr_all-<game>[...], through
# ../gamelock.sh when it exists; it kills only its own pids. Exit 0: every check of every game passed.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
games=("$@")
[ ${#games[@]} -eq 0 ] && games=(ssa giants swapforce trapteam superchargers imaginators)
export SKY_UPSCALER=fsr3 SKY_WINDOW_SIZE=2560x1440
fails=0
ok() { echo "ok:   $1"; }
bad() { echo "FAIL: $1"; fails=$((fails + 1)); }

# fsr_checks <game> <log> <frames dir>
fsr_checks() {
    local game=$1 log=$2 frames=$3
    grep -aq '\[fsr\] FSR 3.1 upscaler' "$log" && ok "$game: FSR 3.1's context made" || bad "$game: no FSR context in $log"
    grep -aq 'no longer upscaled' "$log" && bad "$game: FSR failed: $(grep -a 'no longer upscaled' "$log" | head -1)" ||
        ok "$game: FSR never failed"
    grep -aq '\[guard\]' "$log" && bad "$game: a [guard] line: $(grep -a '\[guard\]' "$log" | head -1)" || ok "$game: no [guard]"
    local n
    n=$(find "$frames" -name '*.p[pn][mg]' 2> /dev/null | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && ok "$game: $n frames written" || bad "$game: no frames in $frames"
}

# play <game> <seconds> <presses> [VAR=value...]: one isolated run of build-<game> to its first level.
play() {
    local game=$1 secs=$2 press=$3
    shift 3
    local R=$root/private/runs/fsr_all-$game iso exe
    case $game in
        ssa) iso=${SSA_ISO:-} ;;
        giants) iso=${GIANTS_ISO:-} ;;
        swapforce) iso=${SWAPFORCE_ISO:-} ;;
    esac
    local dir=${EXE_DIR:-$root/build-$game}
    [ $game = ssa ] && dir=${EXE_DIR:-$root/build}
    exe="$dir/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")"
    rm -rf "${R:?}"
    mkdir -p "$R/figures" "$R/save" "$R/data" "$R/shots"
    cp "$root/private/figures/new_14.sky" "$R/figures/" 2> /dev/null
    local run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/figures' \
      SKY_FIGURES='$R/figures' SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 \
      SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky SKY_PRESS='$press' \
      SKY_GPU_SNAPSHOT='$R/shots/s' SKY_GPU_SNAPSHOT_FROM=600 SKY_GPU_SNAPSHOT_EVERY=300 $* \
      nice \"\$0\" --vsync 0 --fps-limit 30 \"\$1\" > '$R/log' 2>&1 & pid=\$!
      (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    # The executable and the disc as arguments: SSA's name has an apostrophe.
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run" "$exe" "$iso"; else sh -c "$run" "$exe" "$iso"; fi
    fsr_checks $game "$R/log" "$R/shots"
}

# route <game> <script> <disc variable>: the game's own route with FSR on; its checks, then FSR's on every launch's log.
route() {
    local game=$1 script=$2 var=$3 name=fsr_all-$1-$$
    local out status
    out=$(env "$var=${!var}" "$root/tools/regress/$script.sh" "$name" 2>&1)
    status=$?
    [ $status -eq 0 ] && ok "$game: $script passes with FSR on" || bad "$game: $script: $(echo "$out" | grep FAIL | head -2 | tr '\n' ' ')"
    local found=0
    for log in $(find "$root/private/runs" -maxdepth 3 -path "*$name*" -name log 2> /dev/null); do
        found=1
        fsr_checks "$game ($(basename "$(dirname "$log")"))" "$log" "$(dirname "$log")"
    done
    [ $found -eq 1 ] || bad "$game: no log of $script under private/runs/$name*"
}

ssa_press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27"
ssa_press="$ssa_press,$(awk 'BEGIN { for (t = 30; t <= 60; t += 2) printf "b:%d,", t; print "lup:62:30" }')"
swapforce_press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }')

for game in "${games[@]}"; do
    case $game in
        ssa) [ -n "${SSA_ISO:-}" ] && play ssa 100 "$ssa_press" SKY_LEVEL=level_027 ;;
        giants) [ -n "${GIANTS_ISO:-}" ] && play giants 100 "$(python3 -I "$root/tools/regress/giants_press.py" 100)" ;;
        swapforce) [ -n "${SWAPFORCE_ISO:-}" ] && play swapforce 90 "$swapforce_press" SKY_PORTAL_PLACE=new:9 ;;
        trapteam) [ -n "${TRAPTEAM_ISO:-}" ] && route trapteam trapteam_chapters TRAPTEAM_ISO ;;
        superchargers) [ -n "${SUPERCHARGERS_ISO:-}" ] && route superchargers superchargers_lobby SUPERCHARGERS_ISO ;;
        imaginators) [ -n "${IMAGINATORS_ISO:-}" ] && route imaginators imaginators_hub IMAGINATORS_ISO ;;
        *) echo "unknown game $game"; exit 2 ;;
    esac
done
[ $fails -eq 0 ] && echo "fsr_all: all checks pass" || echo "fsr_all: $fails failure(s)"
[ $fails -eq 0 ]
