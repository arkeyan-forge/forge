#!/usr/bin/env bash
# SSA's ending and credits played to their end in the game's own movie player ([Lux] [SSA]; games/ssa/docs/status.md,
# "Story tour"). A script cannot beat Kaos, so the start-up movies are swapped for the ending ones (SKY_MOVIE_SWAP, a
# test switch: runtime/kernel/files.cpp): the legal screen (the first start-up movie) is read from movies\sl_sc30.bik
# (the ending, 5025 frames, 167 s; level_010b's mission names it) and the Activision logo (the second) from
# movies\credit.bik (the credits roll, 13804 frames, 460 s). The game plays them as it plays its logos, in that order,
# then goes on to the title.
# Passes when the log shows "[movie] legal ended after <n> updates" and the same for atvilogo (engines/lux/movie.cpp
# names the movie the game asked for: these are sl_sc30 and credit), with no fault. 2026-10-28: 5026 and 13799 updates. Shots at a few seconds (SKY_GPU_SNAPSHOT_SECONDS) in <work dir>/shot.
#
#   tools/pipelines/ssa_ending.sh <SSA .iso> <work dir>
#
# Needs build/ (or TOUR_EXE). Isolated, headless, silent; ~11 min (ENDING_SECONDS, default 720).
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
iso="$1" work="$2"
exe="${TOUR_EXE:-"$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"}"
seconds=${ENDING_SECONDS:-720}
rm -rf "${work:?}"; mkdir -p "$work/save" "$work/figures" "$work/shot"

env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKYLANDERS_DATA="$work/data" SKY_SETTINGS="$work/none.ini" SKY_LAUNCHER_INI="$work/none.ini" SKY_COLLECTION="$work/figures" \
    SKY_FIGURES="$work/figures" SKY_SAVE_DIR="$work/save" SKY_LOG="$work/game.log" SKY_FRAME_TIMES="$work/frames.txt" \
    SKY_MOVIE_SWAP="legal:sl_sc30,atvilogo:credit" \
    SKY_GPU_SNAPSHOT="$work/shot/s" SKY_GPU_SNAPSHOT_FROM=36000 SKY_GPU_SNAPSHOT_SECONDS=30,90,150,200,300,450,600 \
    nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$work/log" 2>&1 & pid=$!
elapsed=0
while kill -0 $pid 2>/dev/null && [ $elapsed -lt "$seconds" ]; do
    grep -aq "\[movie\] atvilogo ended" "$work/game.log" 2>/dev/null && { sleep 10; break; }
    sleep 1; elapsed=$((elapsed + 1))
done
if kill -0 $pid 2>/dev/null; then kill $pid; sleep 3; kill -9 $pid 2>/dev/null || true; fi
wait $pid || true

grep -a "\[movie\]" "$work/game.log" | sort | uniq -c | sort -rn | head -12
faults=$(grep -acE "Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed" "$work/game.log" || true)
verdict=PASS
for movie in legal:sl_sc30 atvilogo:credit; do
    grep -aq "\[movie\] ${movie%%:*} ended after" "$work/game.log" || { verdict=FAIL; echo "${movie#*:} did not end"; }
done
[ "$faults" = 0 ] || { verdict=FAIL; echo "$faults fault lines"; }
echo "ending: $verdict (${elapsed} s)"
