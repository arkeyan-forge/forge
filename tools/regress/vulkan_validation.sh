#!/bin/sh
# Runs SSA on the Vulkan backend (MoltenVK) with the Khronos validation layer and sorts the layer's messages by kind.
# macOS test configuration, see docs/runtime/gpu/plume_backends.md. Isolated like every run (docs/getting-started/testing.md).
#
#   tools/regress/vulkan_validation.sh <name> <seconds> [<SKY_PRESS value>]
#   SSA_ISO=<disc>   BUILD=<build dir>  (default build-vk, configured with -DARKEYAN_MACOS_VULKAN=ON -DARKEYAN_VULKAN_VALIDATION=ON)
#   SKY_LEVEL=<level> and the other SKY_* switches pass through; SKY_PORTAL_PLACE= (empty) puts no figure on the portal;
#   SYNC=1 adds synchronization validation; SEED_FIGURE=<.sky> copies a figure dump into the scratch collection. Output: private/runs/<name>/ (log, summary.txt).
set -eu
name=$1; secs=$2; press=${3:-}
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${BUILD:-$root/build-vk}
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R"
brew=${HOMEBREW_PREFIX:-/opt/homebrew}
export VK_LAYER_PATH=$brew/opt/vulkan-validationlayers/share/vulkan/explicit_layer.d
export VK_ICD_FILENAMES=$brew/opt/molten-vk/etc/vulkan/icd.d/MoltenVK_icd.json
export DYLD_LIBRARY_PATH=$brew/opt/vulkan-loader/lib:$brew/opt/vulkan-validationlayers/lib:$brew/opt/molten-vk/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}
export VK_LAYER_DUPLICATE_MESSAGE_LIMIT=${VK_LAYER_DUPLICATE_MESSAGE_LIMIT:-1000000} # the layer prints each message 10 times by default
# SYNC=1 also turns on synchronization validation (hazards between the frame's commands; slower)
[ "${SYNC:-0}" = 1 ] && export VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
export SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null
export SKY_SETTINGS=$R/none.ini SKY_LAUNCHER_INI=$R/none.ini SKY_COLLECTION=$R/collection SKY_SAVE_DIR=$R/save
export SKYLANDERS_DATA=$R/data SKY_LOG=$R/game.log SKY_PORTAL=${SKY_PORTAL:-emulated} SKY_PORTAL_PLACE=${SKY_PORTAL_PLACE-new:14}
export SKY_SKIP_MOVIES=${SKY_SKIP_MOVIES:-1}
[ -n "$press" ] && export SKY_PRESS="$press"
[ -z "$SKY_PORTAL_PLACE" ] && unset SKY_PORTAL_PLACE   # SKY_PORTAL_PLACE= (empty): no figure on the portal (the Toy Box prompt)
# SEED_FIGURE=<.sky dump>: copied into the scratch collection (the Toy Box lists it)
if [ -n "${SEED_FIGURE:-}" ]; then mkdir -p "$R/collection"; cp "$SEED_FIGURE" "$R/collection/"; fi
# no nice in front: it is a protected binary and would drop DYLD_LIBRARY_PATH; renice after the start
"$build/ArkeyanRuntime/Skylanders Spyro's Adventure" "$SSA_ISO" > "$R/log" 2>&1 &
pid=$!
renice -n 10 -p $pid > /dev/null
( sleep "$secs"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null ) &
wait $pid 2>/dev/null || true
# Messages start with "Validation Error: [ <id> ]" or "Validation Warning: [ <id> ]"; the summary counts them by kind.
grep -a -A4 -E "^Validation (Error|Warning)|MoltenVK|\[mvk" "$R/log" > "$R/validation.txt" || true
{
  echo "by kind (count, severity, id):"
  grep -a -o -E "^Validation (Error|Warning): \[ [A-Za-z0-9_-]+ \]" "$R/log" | sed 's/Validation //; s/: \[ / /; s/ \]//' | sort | uniq -c | sort -rn
} > "$R/summary.txt"
cat "$R/summary.txt"
