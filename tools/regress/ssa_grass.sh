#!/bin/sh
# SSA's sun light tiles at a window size and resolution scale: the fast recipe into level_027, the hero walks so the
# camera moves, a frame a second is kept and scored by tools/regress/grid_score.py (blocks of 32 scene pixels lit
# with the wrong shadow class). docs/runtime/gpu/plume_backends.md ("yellow-green blocks on the grass").
#
#   SSA_ISO=<disc> tools/regress/ssa_grass.sh <name> <WxH> <scale> [metal|vulkan]
#   BUILD=<build dir> (default build for metal, build-vk for vulkan: -DARKEYAN_MACOS_VULKAN=ON)
# Wrap it in ../gamelock.sh. Output: private/runs/<name>/ (game.log, frames/*.png, score.txt).
set -eu
name=$1; size=$2; scale=$3; backend=${4:-metal}
root=$(cd "$(dirname "$0")/../.." && pwd)
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R/collection" "$R/frames"
if [ "$backend" = vulkan ]; then
    build=${BUILD:-$root/build-vk}
    brew=${HOMEBREW_PREFIX:-/opt/homebrew}
    export VK_ICD_FILENAMES=$brew/opt/molten-vk/etc/vulkan/icd.d/MoltenVK_icd.json
    export DYLD_LIBRARY_PATH=$brew/opt/vulkan-loader/lib:$brew/opt/molten-vk/lib
else
    build=${BUILD:-$root/build}
fi
export SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null
export SKY_SETTINGS=$R/none.ini SKY_LAUNCHER_INI=$R/none.ini SKY_COLLECTION=$R/collection SKY_SAVE_DIR=$R/save
export SKYLANDERS_DATA=$R/data SKY_LOG=$R/game.log SKY_LAUNCHER_NO_UPDATE_CHECK=1
export SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:14 SKY_WINDOW_SIZE=$size
# The fast recipe (testing.md), A on the visiting-Skylander dialog, then the hero walks a square and back.
walk="a:68,a:71,a:74,a:77,lright:80:3,lup:84:3,lleft:88:3,ldown:92:3,lright:96:2,lup:99:2,lleft:102:2,ldown:105:2"
export SKY_PRESS="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60,$walk"
secs=${SECONDS_LIST:-$(seq -s, 60 1 110)}
export SKY_GPU_SNAPSHOT=$R/frames/f SKY_GPU_SNAPSHOT_EVERY=1000000 SKY_GPU_SNAPSHOT_SECONDS=$secs
exe="$build/ArkeyanRuntime/Skylanders Spyro's Adventure.app/Contents/MacOS/Skylanders Spyro's Adventure"
# no nice in front on Vulkan (a protected binary drops DYLD_LIBRARY_PATH): renice after the start
"$exe" "$SSA_ISO" --scale "$scale" > "$R/log" 2>&1 &
pid=$!
renice -n 10 -p $pid > /dev/null
( sleep "${RUN_SECONDS:-114}"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null ) 3>&- &
wait $pid 2>/dev/null || true
for f in "$R"/frames/*.ppm; do
    [ -e "$f" ] || continue
    python3 -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])" "$f" "${f%.ppm}.png"
    rm -f "$f"
done
python3 "$root/tools/regress/grid_score.py" "$R"/frames/*.png > "$R/score.txt" || true
grep -a "settings\] scale\|resolution scale" "$R/game.log" | head -2 || true
tail -1 "$R/score.txt"
