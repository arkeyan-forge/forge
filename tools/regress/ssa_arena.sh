#!/bin/bash
# SSA's Battle Mode (two players) played through the game's own menus, unattended: the press file (SKY_PRESS_FILE) steers
# both pads, the second figure comes from the emulated portal. Isolated like every run (docs/getting-started/testing.md).
#
#   SSA_ISO=<disc> tools/regress/ssa_arena.sh <name> [<mode keys>] [<arena index>]     (~4 min; one game at a time)
#
# <mode keys>: pad-2 presses after "Continue" on PICK MODE, '' = Arena Rumble (the default), "right" = SkyGoals,
# "right down" = SkyGem Master. <arena index>: how many "right" presses on PICK ARENA (0 = Cyclops Square; a new save
# has four arenas, a fifth press wraps to the first). Everything is under private/runs/<name>/; shots are `shot_N.ppm`
# (one every 2 s of game time) and the verdict is printed at the end.
#
# The menus belong to the first pad that pressed A at the title: here Player 2 ("2 <button>" lines), so the Battle Mode
# presses are Player 2's; Player 1 only picks its Skylander ("a") and fights. SKY_LEVEL does not do this: it starts a new
# game in a level with no Skylander in it (games/ssa/docs/100.md, "Modes").
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
name=$1 modeKeys=${2:-} arena=${3:-0}
exe="${EXE:-"$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"}"
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R/figures" "$R/save"; : > "$R/press.txt"
cp "$root/private/figures/new_14.sky" "$R/figures/"
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" SKY_COLLECTION="$R/figures" \
    SKY_FIGURES="$R/figures" SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" SKY_SKIP_MOVIES=1 SKY_PRESS_FILE="$R/press.txt" \
    SKY_PORTAL=emulated SKY_PORTAL_PLACE="new_14.sky,new:9" \
    SKY_GPU_SNAPSHOT="$R/shot" SKY_GPU_SNAPSHOT_FROM=0 SKY_GPU_SNAPSHOT_EVERY=60 \
    nice "$exe" --vsync 0 --fps-limit 30 "$SSA_ISO" > "$R/log" 2>&1 &
pid=$!; echo $pid > "$R/pid"
(sleep 280; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null) &
press() { echo "$1" >> "$R/press.txt"; sleep "${2:-2}"; }
until [ -f "$R/shot_4.ppm" ] || ! kill -0 $pid 2>/dev/null; do sleep 2; done
sleep 8; press "2 a" 25                               # the title -> CHOOSE PLAY
press "2 down" 2; press "2 a" 6; press "2 a" 6        # Battle Mode, "requires both players": Continue
for m in $modeKeys; do press "2 $m" 2; done; press "2 a" 7        # PICK MODE
for ((i = 0; i < arena; i++)); do press "2 right" 2; done; press "2 a" 6   # PICK ARENA
press "2 a" 25                                        # the match summary: Start
press "2 a" 4; press "a" 4; press "a" 30              # each player's Skylander, then the level loads
press "b" 1; press "2 b" 15                           # skip the intro camera
for _ in $(seq 1 14); do                              # 30 s of both players walking and attacking
    echo "lright:1" >> "$R/press.txt"; echo "2 lleft:1" >> "$R/press.txt"; sleep 1.2
    echo a >> "$R/press.txt"; echo "2 a" >> "$R/press.txt"; sleep 1.2
done
kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null
placed=$(grep -c '^\[portal\] placed' "$R/game.log")
faults=$(grep -cE 'Fault \(|Indirect call to unrecompiled|\[guard\]' "$R/game.log")
level=$(grep -o 'levels\\pvp_level_[0-9a-z_]*\\mission.str' "$R/game.log" | tail -1)
echo "$name: figures placed $placed, level ${level:-none}, faults $faults, last frame $(ls "$R"/shot_*.ppm | wc -l) shots"
[ "$placed" = 2 ] && [ "$faults" = 0 ] && [ -n "$level" ]
