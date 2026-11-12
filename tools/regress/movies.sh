#!/bin/bash
# The start-up logo movies are not black (docs/runtime/gpu/render_bugs.md, "SuperChargers and Imaginators: story movies black").
# The game runs isolated for 30 s with its movies played; the presented image is saved every 30 frames. Fails unless a
# `.bk2` is opened, no shader is reported missing ("no translated ..."), and some frame after the first movie opened has
# a mean luminance above the floor (a black movie is 0.0, a logo fading in is above 10 at its brightest).
#
#   tools/regress/movies.sh <superchargers|imaginators> <disc image> [seconds, default 30]
#   EXE=<game executable>   default build-<game>/ArkeyanRuntime/<name>.app/...; a kit-installed game is the real test: the
#                           install collects the movie shaders, a dev build has them in its cache already
#   MOVIE_FLOOR=<n>         mean luminance (0-255), default 10
#
# Everything is under private/regress/movies-<game>/. Exit code 0: PASS (the reason is the last line).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; seconds=${3:-30}; floor=${MOVIE_FLOOR:-10}
case $game in superchargers) name="Skylanders SuperChargers" ;; imaginators) name="Skylanders Imaginators" ;; *) name= ;; esac
[ -n "$name" ] && [ -f "$iso" ] || { echo "usage: $0 <superchargers|imaginators> <disc image> [seconds]" >&2; exit 2; }
exe="${EXE:-$root/build-$game/ArkeyanRuntime/$name.app/Contents/MacOS/$name}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
r=$root/private/regress/movies-$game
rm -rf "${r:?}"; mkdir -p "$r/shots" "$r/home"

# One game at a time on a shared Mac: ../gamelock.sh when it exists. The timer starts inside the lock.
lock=; [ -x "$root/../gamelock.sh" ] && lock="$root/../gamelock.sh"
( cd "$r" && $lock bash -c '. "$0/tools/regress/pid_timer.sh"; run_timed "$@"' "$root" "$seconds" env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    SKY_SETTINGS="$r/none.ini" SKY_LAUNCHER_INI="$r/none.ini" SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" \
    SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" HOME="$r/home" CFFIXED_USER_HOME="$r/home" SKY_PORTAL=none \
    SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_GPU_SNAPSHOT="$r/shots/s" SKY_GPU_SNAPSHOT_EVERY=30 \
    nice "$exe" "$iso" > "$r/run.log" 2>&1 )
[ -s "$r/game.log" ] || { echo "FAIL: no game log"; exit 1; }

movies=$(grep -c '\.bk2 ->' "$r/game.log")
[ "$movies" -gt 0 ] || { echo "FAIL: no movie opened in ${seconds} s"; exit 1; }
missing=$(grep -c 'no translated' "$r/game.log")
[ "$missing" -eq 0 ] || { echo "FAIL: $missing shaders missing: $(grep -m1 'no translated' "$r/game.log" | cut -c1-150)"; exit 1; }
# The brightest frame's mean luminance (every 17th pixel of the PPM, Rec. 601 weights).
best=$(python3 -I - "$r/shots" <<'EOF'
import glob, sys
best = 0.0
for path in glob.glob(sys.argv[1] + "/s_*.ppm"):
    data = open(path, "rb").read()
    head = data.split(b"\n", 3)
    pixels = head[3]
    total = count = 0
    for i in range(0, len(pixels) - 2, 51):
        total += (pixels[i] * 299 + pixels[i + 1] * 587 + pixels[i + 2] * 114) // 1000
        count += 1
    if count:
        best = max(best, total / count)
print("%.1f" % best)
EOF
)
awk -v b="$best" -v f="$floor" 'BEGIN { exit !(b > f) }' || { echo "FAIL: the brightest frame's mean luminance is $best (floor $floor): the movies are black"; exit 1; }
echo "PASS: $movies movies opened, no shader missing, the brightest frame's mean luminance $best (floor $floor)"
