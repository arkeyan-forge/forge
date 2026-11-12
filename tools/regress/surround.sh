#!/bin/bash
# Proves the 5.1 output (docs/runtime/xbox360/audio_xma_xaudio.md, "5.1"): the game runs isolated with SKY_SPEAKERS=5.1
# into SDL's disk driver at 6 channels, through the title into a level with music, voices and effects. The log must show
# the layout and the speaker config the title was told (0x00010001), the capture must hold 6 channels and
# tools/regress/surround_rms.py --check must find signal on all six, the centre, LFE and back channels distinct from the
# front ones, and the LFE band-limited (the runtime's 120 Hz low-pass). A second, short run with the default (auto) on the
# disk driver's 2-channel device must fall back to stereo (0x00000001, 2 channels).
#
#   tools/regress/surround.sh <ssa|giants|swapforce|trapteam|superchargers|imaginators> <disc image> [seconds]
#   EXE=<game executable>   default build-<game>/ArkeyanRuntime/<runtime> (build/ for ssa)
#   SURROUND_PRESS=<SKY_PRESS script> SURROUND_ENV="VAR=value ..."   another route
#
# SSA's Wwise mix leaves the centre and the LFE empty (the doc's table): for ssa only the front and back channels are
# required. Wrap a manual run in ../gamelock.sh. Everything is under private/regress/surround-<game>/ (the capture is
# deleted after the check). Exit code 0: PASS (the reason is the last line).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] || { echo "usage: $0 <ssa|giants|swapforce|trapteam|superchargers|imaginators> <disc image> [seconds]" >&2; exit 2; }
dir=build-$game; [ "$game" = ssa ] && dir=build
exe="${EXE:-$root/$dir/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
# The routes of the harness's own runs (tools/test_all.sh, tools/regress/superchargers_lobby.sh).
case $game in
    ssa) seconds=90; press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60"
        extra="SKY_LEVEL=level_027 SKY_PORTAL_PLACE=new_14.sky" ;;
    giants|trapteam) seconds=110; press=$(python3 -I "$root/tools/regress/giants_press.py" 110); extra="SKY_PORTAL_PLACE=new_14.sky" ;;
    swapforce) seconds=100; press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }')
        extra="SKY_PORTAL_PLACE=new:9" ;;
    superchargers) seconds=150; press=$(awk 'BEGIN { s = ""; for (t = 12; t <= 44; t += 4) s = s sprintf("a:%d,", t)
        s = s "y:46,y:48,y:50,y:54,"; for (t = 58; t <= 150; t += 5) s = s sprintf("a:%d,", t); print substr(s, 1, length(s) - 1) }')
        extra="SKY_LEVEL=academy SKY_PORTAL_PLACE=new:3400,new:3227" ;;
    imaginators) seconds=200; extra="SKY_PORTAL_PLACE=new:9"
        # Start until the licence (it comes at ~25 s), Y, then A: the opening level (level_508) by ~120 s.
        press=$(awk 'BEGIN { s = ""; for (t = 20; t <= 75; t += 5) s = s sprintf("start:%d,", t); s = s "y:80,y:83,"
            for (t = 87; t <= 200; t += 4) s = s sprintf("a:%d,", t); print substr(s, 1, length(s) - 1) }') ;;
    *) [ -n "${SURROUND_PRESS:-}" ] || echo "no route for $game: set SURROUND_PRESS and SURROUND_ENV" >&2
        seconds=120; press=""; extra="" ;;
esac
seconds=${3:-$seconds}; press=${SURROUND_PRESS:-$press}; extra=${SURROUND_ENV:-$extra}
scratch=$root/private/regress/surround-$game
rm -rf "${scratch:?}"; mkdir -p "$scratch"
. "$root/tools/regress/pid_timer.sh"
fail=0
bad() { echo "FAIL: $1"; fail=1; }

# run <name> <seconds> <VAR=value...>: the game isolated in $scratch/<name>, its capture in audio.f32, stopped by its PID.
run() {
    label=$1; secs=$2; shift 2; r=$scratch/$label
    mkdir -p "$r/collection" "$r/save" "$r/data" "$r/home"
    cp "$root/private/figures/new_14.sky" "$r/collection/" 2>/dev/null
    # shellcheck disable=SC2086
    ( cd "$r" && run_timed "$secs" env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_CHANNELS=6 \
        SDL_AUDIO_DISK_OUTPUT_FILE="$r/audio.f32" SKY_SETTINGS="$r/none.ini" SKY_LAUNCHER_INI="$r/none.ini" \
        SKY_COLLECTION="$r/collection" SKY_FIGURES="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" \
        SKY_LOG="$r/game.log" HOME="$r/home" CFFIXED_USER_HOME="$r/home" SKY_LAUNCHER_NO_UPDATE_CHECK=1 \
        SKY_PORTAL=emulated SKY_SKIP_MOVIES=1 SKY_PRESS="$press" $extra "$@" nice "$exe" "$iso" > "$r/out.log" 2>&1 )
    [ $? -eq 0 ] || bad "$label: the game exited before the timer ($(tail -1 "$r/out.log" | cut -c1-100))"
}

echo "== $game: 5.1 ($seconds s)"
run surround "$seconds" SKY_SPEAKERS=5.1
r=$scratch/surround
grep -aq '^\[audio\] speakers 5.1: 5.1 ' "$r/game.log" || bad "5.1: no '[audio] speakers 5.1: 5.1' line"
grep -aq '^\[audio\] speaker config 0x00010001 (5.1)' "$r/game.log" || bad "5.1: the title was not told Dolby Digital 5.1"
grep -aq 'channels=6 freq=48000' "$r/out.log" || bad "5.1: the disk device was not opened at 6 channels"
need=""; [ "$game" = ssa ] && need="--front-back"
result=$(python3 -I "$root/tools/regress/surround_rms.py" "$r/audio.f32" 6 --check $need 2>&1); status=$?
printf '%s\n' "$result"
[ $status -eq 0 ] || bad "5.1: $(printf '%s\n' "$result" | tail -1)"
summary=$(printf '%s\n' "$result" | awk '/ rms /{ printf "%s%s %s", sep, $1, $3; sep=", " }')
rm -f "$r/audio.f32"

echo "== $game: auto on a 2-channel device (40 s)"
run auto 40
r=$scratch/auto
grep -aq '^\[audio\] speakers auto: stereo (device: 2 channels)' "$r/game.log" || bad "auto: no '[audio] speakers auto: stereo (device: 2 channels)' line"
grep -aq '^\[audio\] speaker config 0x00000001 (stereo)' "$r/game.log" || bad "auto: the title was not told stereo"
grep -aq 'channels=2 freq=48000' "$r/out.log" || bad "auto: the disk device was not opened at 2 channels"
rm -f "$r/audio.f32"

what="signal on every channel"; [ "$game" = ssa ] && what="signal on the front and back pairs (the centre and the LFE empty: the game's mix)"
if [ $fail -eq 0 ]; then echo "PASS: $game: 5.1 with $what ($summary dBFS), auto falls back to stereo"; else echo "FAIL (logs in $scratch)"; fi
exit $fail
