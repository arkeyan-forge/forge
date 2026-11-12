#!/bin/bash
# Imaginators to the hub (2016/hub_2016), twice: a new game and a continue (games/imaginators/docs/status.md).
#   1. new game: the movies, Start at the title, Y on the software licence, A through the save and the difficulty, then
#      SKY_LEVEL=hub_2016 takes the first zone the game asks for (the first level, 2016/level_508) to the hub; Eruptor on
#      the emulated portal;
#   2. continue: the first run's saves copied, Start at the title, A through the menus: the zone the save chose, with no
#      SKY_LEVEL.
# Each run: the hub's (or the save's) zone starts, no fault, the fps floor in the last seconds, the saves written, frame shots
# that are not black, the sound not silent.
#
#   IMAGINATORS_ISO=<Imaginators .iso> tools/regress/imaginators_hub.sh [<run name>]     (about 9 min; build-imaginators/)
#
# The licence comes at a time that depends on the load, so the presses are written while the game runs (SKY_PRESS_FILE): Start
# every 5 s until the licence's archive opens, then Y, then A every 4 s. Every run is isolated (docs/getting-started/testing.md)
# under private/runs/<name>-new and <name>-continue, through ../gamelock.sh when it exists (one game at a time on a shared
# Mac); it kills only its own pids. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-imaginators_hub}
iso=${IMAGINATORS_ISO:?set IMAGINATORS_ISO to the Imaginators disc image}
exe="${LIVE_EXE:-$root/build-imaginators/ArkeyanRuntime/Skylanders Imaginators.app/Contents/MacOS/Skylanders Imaginators}"
floor=${REGRESS_FPS_FLOOR:-25}
for r in "$name-new" "$name-continue"; do
    [ -e "$root/private/runs/$r" ] && { echo "private/runs/$r exists: give another run name"; exit 2; }
done
NEW_SECONDS=${IMG_NEW_SECONDS:-300}
CONTINUE_SECONDS=${IMG_CONTINUE_SECONDS:-200}

drive() { # <game log> <press file>: Start until the licence, Y, then A
    local log=$1 presses=$2 i
    until [ -e "$log" ]; do sleep 1; done
    sleep 20
    for i in $(seq 1 30); do
        grep -a -q 'softwarelicense.pak' "$log" && break
        echo start >> "$presses"
        sleep 5
    done
    if grep -a -q 'softwarelicense.pak' "$log"; then
        sleep 8
        echo y >> "$presses"
        sleep 3
        echo y >> "$presses"
    fi
    for i in $(seq 1 80); do
        sleep 4
        echo a >> "$presses"
    done
}

play() { # <run dir> <seconds> [VAR=value ...]
    local R=$1 secs=$2; shift 2
    mkdir -p "$R/collection" "$R/save" "$R/data" "$R/shots"
    : > "$R/presses.txt"
    drive "$R/log" "$R/presses.txt" & local driver=$!
    local run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
      SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_PORTAL=emulated \
      SKY_PORTAL_PLACE=new:9 SKY_PRESS_FILE='$R/presses.txt' SKY_TRACE_FILES=1 SKY_FRAME_TIMES='$R/frames.txt' \
      SKY_AUDIO_DUMP='$R/audio.f32' SKY_GPU_SNAPSHOT='$R/shots/s' SKY_GPU_SNAPSHOT_EVERY=900 $* \
      nice '$exe' --fps-limit 30 '$iso' > '$R/log' 2>&1 & pid=\$!
      (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
    kill "$driver" 2> /dev/null
    wait "$driver" 2> /dev/null
}

fails=0
ok() { echo "ok:   $1"; }
bad() { echo "FAIL: $1"; fails=$((fails + 1)); }
check_run() { # <run dir> <seconds> <what> <zone>
    # game.log (SKY_LOG) has everything the console log has, and the fault reports, which only it has.
    local R=$1 secs=$2 what=$3 zone=$4 log=$1/game.log
    [ -f "$log" ] || log=$R/log
    grep -q "^\[level\] $zone starts" "$log" && ok "$what: the zone $zone starts" || bad "$what: no '[level] $zone starts'"
    local result
    # analyze.py: one line, exit 0 when the check passes (faults and stubs against private/test-refs/imaginators).
    if result=$(cd "$root" && python3 tools/regress/analyze.py scan "$log" "$root/private/test-refs/imaginators" 2>&1)
    then ok "$what: $result"; else bad "$what: $result"; fi
    if result=$(cd "$root" && python3 tools/regress/analyze.py fps "$R/frames.txt" "$secs" "$floor" 20 --end 2>&1)
    then ok "$what: $result"; else bad "$what: $result"; fi
    if result=$(cd "$root" && python3 tools/regress/analyze.py audio "$R/audio.f32" 100 5 2>&1)
    then ok "$what: sound: $result"; else bad "$what: sound: $result"; fi
    ls "$R"/save/*/IMAGINATORS_0* > /dev/null 2>&1 && ok "$what: the story save IMAGINATORS_0 written" ||
        bad "$what: no IMAGINATORS_0 save"
    # The last shots: drawn (a black or one-colour frame is a failure), at least 30 % of the pixels lit.
    result=$(python3 -I - "$R/shots" <<'EOF'
import glob, os, sys
shots = sorted(glob.glob(os.path.join(sys.argv[1], "s_*.ppm")), key=os.path.getmtime)[-3:]
if not shots:
    print("no frame saved"); sys.exit(1)
worst = 100.0
for path in shots:
    data = open(path, "rb").read().split(b"\n", 3)[3]
    sample = data[:: max(len(data) // 30000, 1)]
    worst = min(worst, 100.0 * sum(1 for b in sample if b > 24) / len(sample))
print(f"{len(shots)} last frames, the darkest {worst:.0f} % lit")
sys.exit(0 if worst >= 30 else 1)
EOF
    ) && ok "$what: $result" || bad "$what: $result"
}

R1=$root/private/runs/$name-new
play "$R1" "$NEW_SECONDS" SKY_LEVEL=hub_2016
check_run "$R1" "$NEW_SECONDS" "new game" "2016/hub_2016/hub_2016"

R2=$root/private/runs/$name-continue
mkdir -p "$R2/save"
cp -R "$R1/save/." "$R2/save/"
play "$R2" "$CONTINUE_SECONDS"
# The zone a continue enters is the save's; the log names it.
zone=$(grep -a -o '^\[level\] 2016/[a-z0-9_]*/[a-z0-9_]* starts' "$R2/game.log" 2>/dev/null | tail -1 | cut -d' ' -f2)
check_run "$R2" "$CONTINUE_SECONDS" "continue" "${zone:-2016/hub_2016/hub_2016}"
grep -q 'SKY_LEVEL' "$R2/game.log" && bad "continue: SKY_LEVEL was set" || ok "continue: no SKY_LEVEL (the save chose the zone: ${zone:-none})"

[ "$fails" = 0 ] && echo "Imaginators to the hub: PASS" || echo "Imaginators to the hub: $fails FAILED (logs in private/runs/$name-*)"
exit $((fails != 0))
