#!/usr/bin/env bash
# The SSA story tour ([Lux] [SSA]; games/ssa/docs/history.md, "Story tour"): every story level of the disc, chapter by
# chapter (the 22 chapters, Lair of Kaos' second part level_010b, the 4 adventure packs), in a headless, silent, isolated
# run that starts a new game and plays there with SKY_LEVEL (level_027's files are read from the level's): the menus,
# B through the level's opening cutscene, then from 62 s walking, attacking, jumping and the pause menu
# (the presses of tools/pipelines/tour.sh). One instance at a time. One summary line per level.
#
#   tools/pipelines/tour_ssa_story.sh <SSA .iso> <work dir> [<level>...]
#
# Needs build/ (or TOUR_EXE, a copy of the SSA executable, so that a rebuild does not disturb the tour). TOUR_SECONDS: each run's
# length (default 140: the level opens at ~35-60 s, then at least 60 s of play). TOUR_FIGURE: what the portal holds
# (SKY_PORTAL_PLACE, default new_14.sky from private/figures). Each run has its own folder <work dir>/runs/<level>
# (save, figures, game.log, frames.txt, shots) and its stdout next to it. Writes <work dir>/summary.tsv (chapter, level,
# opened, faults, guards, seconds in the level, last frame s, end shot brightness, verdict: ok | no level | fault | hang
# | short | black | no hero). ~2 min 30 s a level, ~70 min for the 27.
# TOUR_MODE=hero: each level is started by the game itself (SKY_NEXT_LEVEL, the opening movie's nextLevel), so the
# figure's Skylander and its HUD spawn; without it, SKY_LEVEL swaps level_027's files and no Skylander is created
# outside level_027. The hero column: the game wrote the figure back (its first use: the Skylander entered play). A
# level that is not a chapter (level_hub, challenge_level_<n>) may be named too; its chapter column reads "-".
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
iso="$1" work="$2"; shift 2
exe="${TOUR_EXE:-"$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"}"
seconds=${TOUR_SECONDS:-140}
mkdir -p "$work/runs"

# Chapter order: text\eng\global.lxb, "Level_<n>_Chapter" (CRC-32 keys; tools/toy_tables.py lxb_texts). 22b is
# level_010b, Lair of Kaos' second part (its mission names the ending movie sl_sc30 and the credits).
chapters="1:level_027 2:level_021 3:level_018 4:level_026 5:level_014 6:level_037 7:level_006 8:level_001 9:level_019
10:level_038 11:level_017 12:level_009 13:level_040 14:level_034 15:level_023 16:level_002 17:level_032 18:level_000
19:level_024 20:level_039 21:level_025 22:level_010 22b:level_010b 23:level_003 24:level_008 25:level_036 26:level_046"
if [ $# -gt 0 ]; then
    want=" $* "; list=""
    for c in $chapters; do [[ "$want" == *" ${c#*:} "* ]] && list+="$c "; done
    for l in "$@"; do [[ " $(echo $chapters) " == *":$l "* ]] || list+="-:$l "; done # the hub, challenges, bonus levels
    chapters=$list
fi

# Menus (new game, slot 1), B through the level's cutscene, then play from 62 s: A (dialogs, jump), the left stick in
# four directions, X, Y, B; the pause menu at 100-112 s.
press=$(python3 -I - "$seconds" <<'PY'
import sys
end = float(sys.argv[1])
p = ["start:1", "start:2", "start:3", "start:4", "start:5"]
p += [f"a:{t}" for t in (6, 7.5, 9, 10.5, 12, 13.5, 15, 16.5, 18, 19.5, 21, 22.5, 24, 25.5, 27)]
p += [f"b:{t}" for t in range(30, 62, 2)]
dirs = ["lup", "lleft", "ldown", "lright"]
t, i = 62.0, 0
while t < end:
    if not 98 <= t < 114:
        p += [f"a:{t:g}", f"{dirs[i % 4]}:{t + 0.3:g}:2.5", f"x:{t + 0.6:g}", f"x:{t + 1.2:g}", f"y:{t + 1.8:g}",
              f"b:{t + 2.4:g}", f"y:{t + 3.0:g}:1"]
        i += 1
    t += 4
p += ["start:100", "down:101.5", "a:103", "right:105", "b:107", "down:108", "a:109", "b:111", "start:112.5"]
print(",".join(p))
PY
)

run() # <chapter> <level>
{
    local dir="$work/runs/$2"
    rm -rf "${dir:?}"; mkdir -p "$dir/figures" "$dir/save" "$dir/shot"
    cp "$root/private/figures/new_14.sky" "$dir/figures/"
    # TOUR_MODE=hero: the game itself goes to the level (SKY_NEXT_LEVEL, the opening movie's nextLevel: the hero and
    # the HUD spawn); the default swaps level_027's files (SKY_LEVEL: the level loads and runs, without a Skylander).
    local how="SKY_LEVEL=$2"
    if [ "${TOUR_MODE:-}" = hero ]; then
        local name="" word
        for word in ${2//_/ }; do name+="${name:+_}$(tr a-z A-Z <<< "${word:0:1}")${word:1}"; done
        how="SKY_NEXT_LEVEL=${name/Pvp_/PvP_}"
    fi
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA="$dir/data" SKY_SETTINGS="$dir/none.ini" SKY_LAUNCHER_INI="$dir/none.ini" SKY_COLLECTION="$dir/figures" \
        SKY_FIGURES="$dir/figures" SKY_SAVE_DIR="$dir/save" SKY_LOG="$dir/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS="$press" \
        SKY_PORTAL=emulated SKY_PORTAL_PLACE="${TOUR_FIGURE:-new_14.sky}" SKY_FRAME_TIMES="$dir/frames.txt" "$how" \
        SKY_TRACE_FILES=1 SKY_GPU_SNAPSHOT="$dir/shot/s" SKY_GPU_SNAPSHOT_FROM=1200 SKY_GPU_SNAPSHOT_EVERY=600 \
        nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$dir.log" 2>&1 & local pid=$!
    local elapsed=0
    while kill -0 $pid 2>/dev/null && [ $elapsed -lt "$seconds" ]; do sleep 1; elapsed=$((elapsed + 1)); done
    if kill -0 $pid 2>/dev/null; then kill $pid; sleep 3; kill -9 $pid 2>/dev/null || true; fi
    wait $pid || true
    python3 -I "$root/tools/pipelines/story_summary.py" "$1" "$2" "$dir/game.log" "$dir/frames.txt" "$seconds" "$dir/shot" \
        ${TOUR_MODE:+"$root/private/figures/new_14.sky" "$dir/figures/new_14.sky"} >> "$work/summary.tsv"
    tail -1 "$work/summary.tsv"
}

printf 'chapter\tlevel\topened\tfaults\tguards\tlevel_s\tlast_s\tshot\thero\tverdict\n' > "$work/summary.tsv"
for c in $chapters; do run "${c%%:*}" "${c#*:}"; done
