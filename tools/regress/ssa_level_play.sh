#!/bin/sh
# One figure (and optionally a magic item beside it) played in an SSA level, to prove what a level does to a figure
# (docs/runtime/host/figures.md, "XP and gold in a level"). Same isolation as ssa_figures.sh; one game, `nice`.
#
#   tools/regress/ssa_level_play.sh <SSA disc .iso> <work dir> [--level NAME] [--seconds N] [--walk <stick>] [--stand] [--item <toy type>] [--xp N] [--gold N]
#
# The character is a Gill Grunt (type 14) made at --xp (default 0) and --gold (default 0); the presses are the level tour's
# (walk in four directions, attack, jump) from 62 s on. After the run `ArkeyanFigureTest --roundtrip` prints XP and gold before
# and after. With --item the magic item is placed on the portal too; frames are kept every 25 s as shot_N.png.
# TEST_AUDIO_FILE=<path> keeps the disk audio (f32 stereo) instead of dropping it. Results in <work dir>/{log,game.log,round_trip.txt,shot_*.png}.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
[ $# -ge 2 ] || { sed -n '2,11p' "$0"; exit 2; }
iso=$1; work=$2; shift 2
level=level_027; seconds=170; stand=0; walk=""; item=""; xp=0; gold=0
while [ $# -gt 0 ]; do
    case $1 in
        --level) shift; level=$1 ;;
        --seconds) shift; seconds=$1 ;;
        --walk) shift; walk=$1 ;; # hold a stick direction (lup, ldown, lleft, lright) from 62 s, attacking on the way
        --stand) stand=1 ;; # attack in place (X every 0.5 s, Y every 3 s) instead of walking
        --item) shift; item=$1 ;;
        --xp) shift; xp=$1 ;;
        --gold) shift; gold=$1 ;;
        *) echo "unknown $1"; exit 2 ;;
    esac
    shift
done
exe="$root/build/ArkeyanRuntime/Skylanders Spyro's Adventure"
figtest=$(find "$root/build" -name ArkeyanFigureTest -type f -perm -u+x | head -1)
[ -x "$exe" ] && [ -x "$figtest" ] || { echo "build ArkeyanRuntime and ArkeyanFigureTest first"; exit 2; }
mkdir -p "$work"; R=$(cd "$work" && pwd); rm -rf "${R:?}"; mkdir -p "$R/collection" "$R/save" "$R/snap"
"$figtest" --make "$R/collection/gill.sky" 14 xp="$xp" gold="$gold" > "$R/make.txt" 2>&1
place="$R/collection/gill.sky"
if [ -n "$item" ]; then "$figtest" --make "$R/collection/item.sky" "$item" >> "$R/make.txt" 2>&1; place="$place,$R/collection/item.sky"; fi
cp "$R/collection/gill.sky" "$R/before_gill.sky"
press=$(WALK=$walk STAND=$stand python3 -I - <<'PY'
import os
p = ["start:1", "start:2", "start:3", "start:4", "start:5"]
p += [f"a:{t}" for t in (6, 7.5, 9, 10.5, 12, 13.5, 15, 16.5, 18, 19.5, 21, 22.5, 24, 25.5, 27)]
p += [f"b:{t}" for t in range(30, 62, 2)]
if os.environ["WALK"]:
    hold = float(os.environ.get("WALK_SECONDS", "200"))
    p += [f"{os.environ['WALK']}:62:{hold:g}"] + [f"x:{62 + 0.7 * k:g}" for k in range(300)] + [f"a:{62 + 4 * k:g}" for k in range(40)]
    print(",".join(p)); raise SystemExit
if os.environ["STAND"] == "1":
    p += [f"x:{62 + 0.5 * k:g}" for k in range(240)] + [f"y:{62 + 3 * k:g}" for k in range(40)]
    print(",".join(p)); raise SystemExit
dirs = ["lup", "lleft", "ldown", "lright"]
t, i = 62.0, 0
while t < 200:
    p += [f"a:{t:g}", f"{dirs[i % 4]}:{t + 0.3:g}:2.5", f"x:{t + 0.6:g}", f"x:{t + 1.2:g}", f"y:{t + 1.8:g}", f"b:{t + 2.4:g}", f"y:{t + 3.0:g}:1"]
    i += 1; t += 4
print(",".join(p))
PY
)
env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE="${TEST_AUDIO_FILE:-/dev/null}" \
    SKYLANDERS_DATA="$R/data" SKY_SETTINGS="$R/none.ini" SKY_LAUNCHER_INI="$R/none.ini" \
    SKY_COLLECTION="$R/collection" SKY_FIGURES="$R/collection" SKY_SAVE_DIR="$R/save" SKY_LOG="$R/game.log" \
    SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated SKY_PORTAL_PLACE="$place" SKY_PORTAL_LOG=1 \
    SKY_LEVEL="$level" SKY_GPU_SNAPSHOT="$R/snap/s" SKY_GPU_SNAPSHOT_FROM=1500 SKY_GPU_SNAPSHOT_EVERY=750 \
    nice "$exe" --vsync 0 --fps-limit 30 "$iso" > "$R/log" 2>&1 &
pid=$!
(sleep "$seconds"; kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null) &
wait "$pid" 2>/dev/null
"$figtest" --roundtrip "$R/before_gill.sky" "$R/collection/gill.sky" > "$R/round_trip.txt" 2>&1
cat "$R/round_trip.txt"
if command -v sips > /dev/null 2>&1; then
    n=0; for f in "$R"/snap/s_*.ppm; do [ -f "$f" ] && sips -s format png -Z 480 "$f" --out "$R/shot_$n.png" > /dev/null 2>&1; n=$((n+1)); done
fi
