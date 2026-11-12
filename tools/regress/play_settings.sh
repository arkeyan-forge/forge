#!/bin/bash
# Proves that the video settings a player turns on in the launcher reach the game: a settings file that holds only the
# game's own copy ([<game>.video], what its settings page writes; no [video], which only PLAY fills in) with
# skip_intro_movies=1 and fps_limit=60. The game must log both in its [settings] line, skip every start-up logo
# ("[movie] skipped <name>") and run with the 60 FPS present ("[video] present interval ... (fps limit 60)").
#
#   tools/regress/play_settings.sh <ssa|giants|swapforce|trapteam> <disc image> [seconds, default 60]
#   EXE=<game executable>      default build/ArkeyanRuntime/Skylanders Spyro's Adventure (build-giants/...: Skylanders Giants, build-swapforce/...: Skylanders Swap Force, build-trapteam/...: Skylanders Trap Team)
#   LAUNCHER=<Arkeyan Launcher> also runs the game through it (--game <id> --play, SKY_LAUNCHER_RUNTIME=EXE), with the
#                               copy in the launcher.ini the launcher itself rewrites (PLAY puts the copy into [video])
#
# Everything is under private/regress/play-settings-<game>/. Exit code 0: PASS (the reason is the last line).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}; seconds=${3:-60}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] || { echo "usage: $0 <ssa|giants|swapforce|trapteam> <disc image> [seconds]" >&2; exit 2; }
case $game in
    ssa) dir=build; name="Skylanders Spyro's Adventure"; logos=4 ;;
    giants) dir=build-giants; name="Skylanders Giants"; logos=2 ;;
    swapforce) dir=build-swapforce; name="Skylanders Swap Force"; logos=2 ;;
    *) dir=build-trapteam; name="Skylanders Trap Team"; logos=2 ;;
esac
exe="${EXE:-$root/$dir/ArkeyanRuntime/$name}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
scratch=$root/private/regress/play-settings-$game
rm -rf "${scratch:?}"; mkdir -p "$scratch"
fail=0

# run <name> <command...>: starts it isolated in $scratch/<name>, waits until the logos were skipped or the time is up,
# stops its own children by PID.
run() {
    label=$1; r=$scratch/$label; shift; mkdir -p "$r/home"
    printf '[%s.video]\nskip_intro_movies=1\nfps_limit=60\n\n[%s]\niso=%s\n' "$game" "$game" "$iso" > "$r/launcher.ini"
    ( cd "$r" && env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" \
        SKYLANDERS_DATA="$r/data" SKY_LAUNCHER_DATA="$r/ldata" SKY_LOG="$r/game.log" HOME="$r/home" CFFIXED_USER_HOME="$r/home" \
        SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_LAUNCHER_DESKTOP="$r/desktop" SKY_PORTAL=none \
        nice "$@" > "$r/run.log" 2>&1 ) &
    pid=$!
    for _ in $(seq "$seconds"); do
        kill -0 "$pid" 2>/dev/null || break
        [ "$(grep -c '^\[movie\] skipped' "$r/game.log" 2>/dev/null || true)" -ge "$logos" ] 2>/dev/null && sleep 6 && break
        sleep 1
    done
    # The game is the launcher's child, or the subshell's: stop what hangs under our subshell, by PID.
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do
        for grandchild in $(pgrep -P "$child" 2>/dev/null); do kill "$grandchild" 2>/dev/null; done
        kill "$child" 2>/dev/null
    done
    sleep 3
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do kill -9 "$child" 2>/dev/null; done
    wait "$pid" 2>/dev/null
    log=$r/game.log
    [ -s "$log" ] || { echo "FAIL: $label: no game log at $log"; fail=1; return; }
    grep -m1 '^\[settings\]' "$log" | grep -q 'fps limit 60, intro movies skipped' \
        || { echo "FAIL: $label: the [settings] line lacks 'fps limit 60, intro movies skipped': $(grep -m1 '^\[settings\]' "$log")"; fail=1; }
    got=$(grep -c '^\[movie\] skipped' "$log")
    [ "$got" -ge "$logos" ] || { echo "FAIL: $label: $got of $logos start-up logos skipped"; fail=1; }
    grep -q '^\[video\] present interval .* (fps limit 60)' "$log" || { echo "FAIL: $label: no present at fps limit 60"; fail=1; }
    echo "$label: $(grep -m1 '^\[settings\]' "$log" | cut -c1-90); $got logos skipped"
}

echo "== $game: the game's own [$game.video] only, the runtime started directly"
run direct "$exe" "$iso"
if [ -n "${LAUNCHER:-}" ]; then
    echo "== $game: through the launcher (--game $game --play)"
    SKY_LAUNCHER_RUNTIME=$exe run launcher "$LAUNCHER" --game "$game" --play
    awk '/^\[video\]/ { in_video = 1; next } /^\[/ { in_video = 0 } in_video' "$scratch/launcher/launcher.ini" | grep -q '^skip_intro_movies=1' || { echo "FAIL: launcher: PLAY did not leave the copy in [video]"; fail=1; }
fi
if [ $fail -eq 0 ]; then echo "PASS: $game: the launcher's video settings reach the game (under $scratch)"; else echo "FAIL (logs in $scratch)"; fi
exit $fail
