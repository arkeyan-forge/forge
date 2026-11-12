#!/bin/sh
# The Linux player kit on a player's machine (tools/linux/player.Dockerfile, amd64): Arkeyan-Builder from the disc to @done,
# then, with a number of seconds, the built game offscreen with the fast recipe of tools/linux/smoke_test.sh.
#
#   tools/linux/kit_test.sh <image> <kit dir> <disc .iso> <game dir> [seconds] [figures dir]
#
# The kit, the disc (read only) and the game folder are mounted; the compiler cache stays in <game dir>/home, so a
# rerun reuses it. Game runs are isolated: scratch settings, saves and collection, offscreen video, no sound device.
set -eu

[ $# -ge 4 ] || { echo "usage: $0 <image> <kit dir> <disc .iso> <game dir> [seconds] [figures dir]" >&2; exit 1; }
image=$1
kit=$(cd "$2" && pwd)
iso=$(cd "$(dirname "$3")" && pwd)/$(basename "$3")
mkdir -p "$4"
game=$(cd "$4" && pwd)
seconds=${5:-0}
figures=${6:-}
jobs=${JOBS:-4}

docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -e HOME=/game/home \
    -v "$kit:/kit:ro" -v "$iso:/game.iso:ro" -v "$game:/game" "$image" \
    sh -c "mkdir -p /game/home && nice /kit/Arkeyan-Builder --iso /game.iso --out /game/out --jobs $jobs > /game/arkeyan-builder.log 2>&1; echo exit=\$? >> /game/arkeyan-builder.log"
grep -E '^@(step|error|done)|warning|exit=' "$game/arkeyan-builder.log" | tail -20
grep -q '^@done ' "$game/arkeyan-builder.log" || exit 1
[ "$seconds" -gt 0 ] || exit 0

press=${SKY_PRESS-"start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60"}
rm -rf "${game:?}/run"
mkdir -p "$game/run/save" "$game/run/collection"
set --
[ -n "$figures" ] && set -- -v "$(cd "$figures" && pwd):/figures:ro" -e SKY_PORTAL=emulated -e SKY_FIGURES=/figures \
    -e SKY_PORTAL_PLACE=new_14.sky
# SKY_PORTAL and SKY_PORTAL_PLACE from the caller's environment (a game without new_14.sky: Swap Force, new:9).
[ -z "$figures" ] && [ -n "${SKY_PORTAL_PLACE:-}" ] && set -- -e SKY_PORTAL="${SKY_PORTAL:-emulated}" -e SKY_PORTAL_PLACE="$SKY_PORTAL_PLACE"
docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" -v "$iso:/game.iso:ro" -v "$game:/game" "$@" \
    -e HOME=/game/run -e SKY_SETTINGS=/game/run/none.ini -e SKY_LAUNCHER_INI=/game/run/none-launcher.ini \
    -e SKY_COLLECTION=/game/run/collection -e SKY_SAVE_DIR=/game/run/save \
    -e SDL_VIDEO_DRIVER=offscreen -e SDL_AUDIO_DRIVER=disk -e SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
    -e SKY_SKIP_MOVIES=1 -e SKY_PRESS="$press" -e SKY_DUMP_FRAME="${SKY_DUMP_FRAME:-30}" -e SKY_DUMP_PREFIX=/game/run/dump \
    -e SKY_RENDER_STATS=1 "$image" \
    sh -c "timeout -k 5 $seconds \"\$(grep '^@done ' /game/arkeyan-builder.log | cut -c7-)\" /game.iso > /game/run/log 2>&1; echo exit=\$? >> /game/run/log"
grep -E '\[gpu\]|level_|fault|\[guard\]|exit=' "$game/run/log" | head -20
