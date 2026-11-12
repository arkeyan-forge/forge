#!/bin/bash
# FSR 3 frame generation in every game (docs/runtime/gpu/upscaling.md, "Frame generation"): each game whose disc is set
# (SSA_ISO, GIANTS_ISO, SWAPFORCE_ISO, TRAPTEAM_ISO, SUPERCHARGERS_ISO, IMAGINATORS_ISO) plays its first level with
# SKY_UPSCALER=fsr3 SKY_FSR_FRAME_GENERATION=on at a 2560x1440 window, about 60 s in the level.
# Checks per game: frame generation's contexts are made ("[framegen] FSR 3.1 frame generation"), it never fails ("off
# for this run"), no "[guard]" line, generated frames written (SKY_FSR_FG_SNAPSHOT), and in the busiest 10 s window of
# the log the frames shown are about twice the real ones (1.8 to 2.05 times); a game that keeps it off (Swap Force) must
# say so and never run it. SSA, Giants, Swap Force: one run each
# (~2 min); Trap Team, SuperChargers, Imaginators: their own route scripts with frame generation on (as fsr_all.sh).
#
#   tools/regress/fsr_framegen.sh [<game>...]     (default: every game whose disc is set)
#   EXE_DIR=<dir> overrides build-<game>/ (build/ for SSA).
#   SKY_FSR_QUALITY=native: also checks the scene is at the frame's own scale ("[upscale] fsr3 native" line).
#
# Every run is isolated (docs/getting-started/testing.md) under private/runs/fsr_framegen-<game>[...], through
# ../gamelock.sh when it exists; it kills only its own pids. Exit 0: every check of every game passed.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
games=("$@")
[ ${#games[@]} -eq 0 ] && games=(ssa giants swapforce trapteam superchargers imaginators)
# Frame generation is held back for players (F3 and the launcher say "Coming later"): the preview variable runs it.
export SKY_UPSCALER=fsr3 SKY_FSR_FRAME_GENERATION=on SKY_FSR_FRAME_GENERATION_PREVIEW=1 SKY_WINDOW_SIZE=2560x1440
fails=0
ok() { echo "ok:   $1"; }
bad() { echo "FAIL: $1"; fails=$((fails + 1)); }

# The games whose adapter keeps frame generation off (game::Adapter::frameGeneration): the check is that it says so and
# stays off.
blocked=""

# framegen_checks <game> <log> <frames dir>
framegen_checks() {
    local game=$1 log=$2 frames=$3
    case " $blocked " in *" ${game%% *} "*)
        grep -aq 'not available in this game yet: off' "$log" && ok "$game: frame generation off (not available yet)" ||
            bad "$game: no line saying frame generation is off in this game"
        grep -aq '\[framegen\] FSR 3.1 frame generation' "$log" && bad "$game: frame generation ran" || ok "$game: it never ran"
        grep -aq '\[guard\]' "$log" && bad "$game: a [guard] line" || ok "$game: no [guard]"
        return ;;
    esac
    grep -aq '\[framegen\] FSR 3.1 frame generation' "$log" && ok "$game: frame generation's contexts made" ||
        bad "$game: no frame generation context in $log"
    grep -aq 'frame generation is off for this run' "$log" &&
        bad "$game: frame generation failed: $(grep -a 'off for this run' "$log" | head -1)" ||
        ok "$game: frame generation never failed"
    grep -aq '\[guard\]' "$log" && bad "$game: a [guard] line: $(grep -a '\[guard\]' "$log" | head -1)" || ok "$game: no [guard]"
    if [ "${SKY_FSR_QUALITY:-}" = native ]; then
        # "[upscale] fsr3 native: the scene at scale 2, upscaled to the frame's scale 2 (...)": both scales the same.
        local scales
        scales=$(grep -a '\[upscale\] fsr3 native: the scene at scale' "$log" | head -1 |
            sed -n "s/.*scene at scale \([0-9]*\), upscaled to the frame's scale \([0-9]*\).*/\1 \2/p")
        [ -n "$scales" ] && [ "${scales% *}" = "${scales#* }" ] && ok "$game: native, the scene at the frame's scale (${scales% *})" ||
            bad "$game: native, the scene not at the frame's scale (${scales:-no '[upscale] fsr3 native' line})"
    fi
    local n pattern='*gen_*.png'
    [ "${route_mode:-0}" = 1 ] && pattern='*.p[pn][mg]' # the route's own shots (its runs set no snapshot prefix)
    n=$(find "$frames" -name "$pattern" 2> /dev/null | wc -l | tr -d ' ')
    [ "$n" -gt 0 ] && ok "$game: $n frames written" || bad "$game: no frames in $frames"
    # "[framegen] last 10 s: 29.9 real fps, 59.8 shown; 299 generated; ...": the window with the most generated frames.
    local best
    best=$(grep -a '\[framegen\] last' "$log" | sed -n 's/.*: \([0-9.]*\) real fps, \([0-9.]*\) shown; \([0-9]*\) generated.*/\3 \1 \2/p' |
        sort -n | tail -1)
    # A route's launches are judged together (route below): a "continue" launch may end in the level's opening movie,
    # where frame generation is rightly off.
    if [ "${route_mode:-0}" = 1 ]; then
        [ -n "$best" ] && route_best=$(printf '%s\n%s\n' "$route_best" "$best" | sed '/^$/d' | sort -n | tail -1)
        return
    fi
    rate_check "$game" "$best"
}

# rate_check <game> <"generated real shown">: the window with the most generated frames shows about twice the real rate.
rate_check() {
    local game=$1 best=$2
    if [ -n "$best" ] && echo "$best" | awk '{ exit !($2 > 0 && $3 / $2 >= 1.8 && $3 / $2 <= 2.05) }'; then
        ok "$game: shown rate twice the real one ($(echo "$best" | awk '{ printf "%s real, %s shown", $2, $3 }'))"
    else
        bad "$game: the shown rate is not twice the real one (${best:-no [framegen] window line})"
    fi
}

# play <game> <seconds> <presses> [VAR=value...]: one isolated run of build-<game> to its first level.
play() {
    local game=$1 secs=$2 press=$3
    shift 3
    local R=$root/private/runs/fsr_framegen-$game iso exe
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
      SKY_FSR_FG_SNAPSHOT='$R/shots/fg' SKY_FSR_FG_SNAPSHOT_FROM=600 SKY_FSR_FG_SNAPSHOT_EVERY=900 $* \
      nice \"\$FG_EXE\" --vsync 0 --fps-limit 30 \"\$FG_ISO\" > '$R/log' 2>&1 & pid=\$!
      (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    # The paths go through the environment: SSA's executable and disc names hold an apostrophe.
    export FG_EXE="$exe" FG_ISO="$iso"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
    framegen_checks $game "$R/log" "$R/shots"
}

# live <game> <seconds> <presses> <row>: frame generation off at the start, the F3 menu's Frame generation row (Settings'
# row <row>: Commands is 0) turns it on at 45 s and off at 75 s (SKY_TOYBOX_SCRIPT). Checks: both changes logged and
# written, frames generated after the first, none after the second.
live() {
    local game=$1 secs=$2 press=$3 row=$4
    local R=$root/private/runs/fsr_framegen-$game-live iso=${GIANTS_ISO:-} exe
    local dir=${EXE_DIR:-$root/build-$game}
    exe="$dir/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")"
    rm -rf "${R:?}"
    mkdir -p "$R/figures" "$R/save" "$R/data"
    cp "$root/private/figures/new_14.sky" "$R/figures/" 2> /dev/null
    : > "$R/launcher.ini"
    # The panel opens on Resume; Down x3 is Settings, Return opens its first row; Down x<row>, Return flips the row.
    local menu="F3:%s,Down:%s.2,Down:%s.4,Down:%s.6" script="" t
    for t in 45 75; do
        script="$script$(printf "$menu" $t $t $t $t),Return:$((t + 1))"
        for i in $(seq 1 "$row"); do script="$script,Down:$((t + 1)).$i"; done
        script="$script,Return:$((t + 2)),Escape:$((t + 3)),F3:$((t + 4)),"
    done
    local run="env -u SKY_FSR_FRAME_GENERATION SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/launcher.ini' SKY_LAUNCHER_INI='$R/launcher.ini' SKY_COLLECTION='$R/figures' \
      SKY_FIGURES='$R/figures' SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 \
      SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky SKY_PRESS='$press' \
      SKY_TOYBOX_SCRIPT='${script%,}' nice \"\$FG_EXE\" --vsync 0 --fps-limit 30 \"\$FG_ISO\" > '$R/log' 2>&1 & pid=\$!
      (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    # The paths go through the environment: SSA's executable and disc names hold an apostrophe.
    export FG_EXE="$exe" FG_ISO="$iso"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
    local log=$R/log
    grep -aq '\[gamemenu\] fsr_frame_generation = on' "$log" && ok "$game live: the F3 row turned it on" ||
        bad "$game live: no '[gamemenu] fsr_frame_generation = on'"
    grep -aq '\[gamemenu\] fsr_frame_generation = off' "$log" && ok "$game live: and off" ||
        bad "$game live: no '[gamemenu] fsr_frame_generation = off'"
    grep -aq '^fsr_frame_generation=off' "$R/launcher.ini" && ok "$game live: written to [$game.video]" ||
        bad "$game live: the setting is not in $R/launcher.ini"
    local gen
    gen=$(grep -a '\[framegen\] last' "$log" | sed -n 's/.*; \([0-9]*\) generated.*/\1/p' | sort -n | tail -1)
    [ "${gen:-0}" -ge 250 ] && ok "$game live: $gen frames generated in a 10 s window while on" ||
        bad "$game live: frame generation did not run after the F3 row (${gen:-no} window)"
}

# route <game> <script> <disc variable>: the game's own route with frame generation on; its checks, then ours on every
# launch's log.
route() {
    local game=$1 script=$2 var=$3 name=fsr_framegen-$1-$$
    local out status
    out=$(env "$var=${!var}" SKY_FSR_FG_SNAPSHOT_FROM=600 SKY_FSR_FG_SNAPSHOT_EVERY=900 \
        "$root/tools/regress/$script.sh" "$name" 2>&1)
    status=$?
    [ $status -eq 0 ] && ok "$game: $script passes with frame generation on" ||
        bad "$game: $script: $(echo "$out" | grep FAIL | head -2 | tr '\n' ' ')"
    local found=0 log
    route_mode=1; route_best=
    for log in $(find "$root/private/runs" -maxdepth 3 -path "*$name*" -name game.log 2> /dev/null); do
        found=1
        framegen_checks "$game ($(basename "$(dirname "$log")"))" "$log" "$(dirname "$log")"
    done
    route_mode=0
    rate_check "$game (the route's best window)" "$route_best"
    [ $found -eq 1 ] || bad "$game: no log of $script under private/runs/$name*"
}

ssa_press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27"
ssa_press="$ssa_press,$(awk 'BEGIN { for (t = 30; t <= 60; t += 2) printf "b:%d,", t; print "lup:62:30" }')"
swapforce_press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }')

for game in "${games[@]}"; do
    case $game in
        ssa) [ -n "${SSA_ISO:-}" ] && play ssa 100 "$ssa_press" SKY_LEVEL=level_027 ;;
        giants) [ -n "${GIANTS_ISO:-}" ] && { play giants 100 "$(python3 -I "$root/tools/regress/giants_press.py" 100)"
            live giants 95 "$(python3 -I "$root/tools/regress/giants_press.py" 95)" 7; } ;;
        swapforce) [ -n "${SWAPFORCE_ISO:-}" ] && play swapforce 90 "$swapforce_press" SKY_PORTAL_PLACE=new:9 ;;
        trapteam) [ -n "${TRAPTEAM_ISO:-}" ] && route trapteam trapteam_chapters TRAPTEAM_ISO ;;
        superchargers) [ -n "${SUPERCHARGERS_ISO:-}" ] && route superchargers superchargers_lobby SUPERCHARGERS_ISO ;;
        imaginators) [ -n "${IMAGINATORS_ISO:-}" ] && route imaginators imaginators_hub IMAGINATORS_ISO ;;
        *) echo "unknown game $game"; exit 2 ;;
    esac
done
[ $fails -eq 0 ] && echo "fsr_framegen: all checks pass" || echo "fsr_framegen: $fails failure(s)"
[ $fails -eq 0 ]
