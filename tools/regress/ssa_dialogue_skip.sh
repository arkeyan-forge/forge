#!/bin/sh
# SSA's spoken lines skipped with B (engines/lux/dialogue_skip.cpp, docs/engines/lux/dialogue_skip.md): the fast recipe
# into level_027 (its B at 30-34 s skip the level's first cinematic, the game's own skip, and a line of the second),
# then B during Hugo's speech (from ~40 s to ~61 s, in a cinematic the game does not let the player skip), A on the
# visiting-Skylander dialog that follows, and the hero walking. It fails unless Hugo's line is skipped, the level's next
# step (its save) follows within SKIP_WAIT seconds instead of when the speech would end, and the game runs on to the end
# without a crash or a bad call.
#
#   SSA_ISO=<disc> tools/regress/ssa_dialogue_skip.sh <name>
#   BUILD=<build dir> (default build). Wrap it in ../gamelock.sh. Output: private/runs/<name>/ (log, game.log).
set -eu
name=$1
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${BUILD:-$root/build}
R=$root/private/runs/$name
rm -rf "${R:?}"; mkdir -p "$R/collection"
export SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null
export SKY_SETTINGS=$R/none.ini SKY_LAUNCHER_INI=$R/none.ini SKY_COLLECTION=$R/collection SKY_SAVE_DIR=$R/save
export SKYLANDERS_DATA=$R/data SKY_LOG=$R/game.log SKY_LAUNCHER_NO_UPDATE_CHECK=1
export SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:14 SKY_PORTAL_LOG=1
# The fast recipe's menus and cutscene presses (testing.md), then B at 45 s, A on the dialog, a walk.
menus="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27"
export SKY_PRESS="$menus,b:30,b:32,b:34,b:45,a:50,a:52,a:54,lright:57:3,lup:61:3"
exe="$build/ArkeyanRuntime/Skylanders Spyro's Adventure"
nice "$exe" "$SSA_ISO" > "$R/log" 2>&1 &
pid=$!
( sleep "${RUN_SECONDS:-70}"; kill $pid 2>/dev/null; sleep 3; kill -9 $pid 2>/dev/null ) 3>&- &
wait $pid 2>/dev/null || true
python3 -I - "$R/log" "${SKIP_WAIT:-5}" <<'PY'
import re, sys
log = open(sys.argv[1], encoding="latin-1").read().splitlines()
wait = float(sys.argv[2])
skips = [float(m.group(1)) for l in log if (m := re.search(r"\[dialogue\] line skipped at ([0-9.]+) s", l))]
skip = skips[-1] if skips else None
if skip is None or skip < 44:
    sys.exit(f"FAIL: Hugo's line not skipped (skips at {skips})")
saves = [float(m.group(1)) for l in log if (m := re.search(r"open lux:\\SLOT\.DAT -> 0x00000000 at ([0-9.]+) s", l))]
after = [t for t in saves if t > skip]
if not after or after[0] - skip > wait:
    sys.exit(f"FAIL: line skipped at {skip:.1f} s, the level's next save at {after[0] if after else 'never'} (more than {wait} s)")
bad = [l for l in log if l.startswith(("[crash]", "[guard]"))]
if bad:
    sys.exit(f"FAIL: {bad[0]}")
print(f"ok: Hugo's line skipped at {skip:.1f} s, the level saved at {after[0]:.1f} s, no crash")
PY
