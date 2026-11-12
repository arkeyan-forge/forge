#!/bin/bash
# SuperChargers to the Academy (the hub), twice: a new game and a continue (games/superchargers/docs/status.md).
#   1. new game: the menus, the opening movie, the intro and its rift tunnel ride (Fiesta and Crypt Crusher on the emulated
#      portal, RT held), then SKY_LEVEL=academy takes the zone the tunnel leads to: the Academy;
#   2. continue: the first run's saves copied, the title, Continue on the slot: the Academy again, with no SKY_LEVEL.
# Each run: the Academy's archive opens and its zone starts, no fault, the fps floor in the last seconds, the saves written,
# frame shots that are not black, the sound not silent.
#
#   SUPERCHARGERS_ISO=<SuperChargers .iso> tools/regress/superchargers_lobby.sh [<run name>]     (about 9 min;
#   build-superchargers/)
#
# Every run is isolated (docs/getting-started/testing.md) under private/runs/<name>-new and <name>-continue, through
# ../gamelock.sh when it exists (one game at a time on a shared Mac); it kills only its own pid. Exit 0: all checks pass.
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
name=${1:-superchargers_lobby}
iso=${SUPERCHARGERS_ISO:?set SUPERCHARGERS_ISO to the SuperChargers disc image}
exe="${LIVE_EXE:-$root/build-superchargers/ArkeyanRuntime/Skylanders SuperChargers.app/Contents/MacOS/Skylanders SuperChargers}"
floor=${REGRESS_FPS_FLOOR:-25}
for r in "$name-new" "$name-continue"; do
    [ -e "$root/private/runs/$r" ] && { echo "private/runs/$r exists: give another run name"; exit 2; }
done

# A new game: A through the title, the play style and the slot (12-44 s), Y skips the opening movie, A through the
# dialogues, RT held through the tunnel ride (the game waits for it), A again at its end.
NEW_PRESS=${SC_NEW_PRESS:-$(awk 'BEGIN { s = ""; for (t = 12; t <= 44; t += 4) s = s sprintf("a:%d,", t)
    s = s "y:46,y:48,y:50,y:54,"; for (t = 58; t <= 150; t += 5) s = s sprintf("a:%d,", t)
    s = s "rt:205:60,rt:266:60,rt:327:45,"; for (t = 375; t <= 440; t += 5) s = s sprintf("a:%d,", t)
    print substr(s, 1, length(s) - 1) }')}
NEW_SECONDS=${SC_NEW_SECONDS:-470}
# Continue: A on the title, the slot, Continue.
CONTINUE_PRESS=${SC_CONTINUE_PRESS:-$(awk 'BEGIN { s = ""; for (t = 12; t <= 60; t += 4) s = s sprintf("a:%d,", t)
    print substr(s, 1, length(s) - 1) }')}
CONTINUE_SECONDS=${SC_CONTINUE_SECONDS:-150}

play() { # <run dir> <seconds> <presses> [VAR=value ...]
    local R=$1 secs=$2 press=$3; shift 3
    mkdir -p "$R/collection" "$R/save" "$R/data" "$R/shots"
    local run="env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
      SKYLANDERS_DATA='$R/data' SKY_SETTINGS='$R/none.ini' SKY_LAUNCHER_INI='$R/none.ini' SKY_COLLECTION='$R/collection' \
      SKY_SAVE_DIR='$R/save' SKY_LOG='$R/game.log' SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_PORTAL=emulated \
      SKY_PORTAL_PLACE=new:3400,new:3227 SKY_PRESS='$press' SKY_TRACE_FILES=1 SKY_FRAME_TIMES='$R/frames.txt' \
      SKY_AUDIO_DUMP='$R/audio.f32' SKY_GPU_SNAPSHOT='$R/shots/s' SKY_GPU_SNAPSHOT_EVERY=900 $* \
      nice '$exe' --fps-limit 30 '$iso' > '$R/log' 2>&1 & pid=\$!
      (sleep $secs; kill \$pid; sleep 3; kill -9 \$pid) > /dev/null 2>&1 & timer=\$!
      wait \$pid; { kill \$timer && wait \$timer; } 2>/dev/null"
    if [ -x "$root/../gamelock.sh" ]; then "$root/../gamelock.sh" sh -c "$run"; else sh -c "$run"; fi
}

fails=0
ok() { echo "ok:   $1"; }
bad() { echo "FAIL: $1"; fails=$((fails + 1)); }
check_run() { # <run dir> <seconds> <what>
    # game.log (SKY_LOG) has everything the console log has, and the fault reports, which only it has.
    local R=$1 secs=$2 what=$3 log=$1/game.log
    [ -f "$log" ] || log=$R/log
    grep -q '\[file\] open \\archives\\academy.pak' "$log" && ok "$what: the Academy's archive opens" ||
        bad "$what: Academy.pak never opened"
    grep -q '^\[level\] academy/academy starts' "$log" && ok "$what: the Academy's zone starts" ||
        bad "$what: no '[level] academy/academy starts'"
    local result
    # analyze.py: one line, exit 0 when the check passes (faults and stubs against private/test-refs/superchargers).
    if result=$(cd "$root" && python3 tools/regress/analyze.py scan "$log" "$root/private/test-refs/superchargers" 2>&1)
    then ok "$what: $result"; else bad "$what: $result"; fi
    if result=$(cd "$root" && python3 tools/regress/analyze.py fps "$R/frames.txt" "$secs" "$floor" 20 --end 2>&1)
    then ok "$what: $result"; else bad "$what: $result"; fi
    if result=$(cd "$root" && python3 tools/regress/analyze.py audio "$R/audio.f32" 200 5 2>&1)
    then ok "$what: sound: $result"; else bad "$what: sound: $result"; fi
    ls "$R"/save/*/SUPERCHARGERS_0 > /dev/null 2>&1 && ok "$what: the story save SUPERCHARGERS_0 written" ||
        bad "$what: no SUPERCHARGERS_0 save"
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
play "$R1" "$NEW_SECONDS" "$NEW_PRESS" SKY_LEVEL=academy
check_run "$R1" "$NEW_SECONDS" "new game"
# Y ends the opening movie (the adapter's storyMovieSkipButton: the game's own skip never comes).
grep -aq '^\[movie\] skipped fmv01 at frame' "$R1/log" && ok "new game: Y skipped fmv01" || bad "new game: fmv01 not skipped by Y"

R2=$root/private/runs/$name-continue
mkdir -p "$R2/save"
cp -R "$R1/save/." "$R2/save/"
play "$R2" "$CONTINUE_SECONDS" "$CONTINUE_PRESS"
check_run "$R2" "$CONTINUE_SECONDS" "continue"
grep -q 'SKY_LEVEL' "$R2/game.log" && bad "continue: SKY_LEVEL was set" || ok "continue: no SKY_LEVEL (the save chose the zone)"

[ "$fails" = 0 ] && echo "SuperChargers to the Academy: PASS" || echo "SuperChargers to the Academy: $fails FAILED (logs in private/runs/$name-*)"
exit $((fails != 0))
