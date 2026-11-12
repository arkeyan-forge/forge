#!/bin/bash
# The F3 menu and its problem report (docs/runtime/host/gamemenu.md), through the Toy Box's own script (SKY_TOYBOX_SCRIPT):
#   run A: SKY_CRASH_TEST=fault makes the game fault on a guest thread after 400 frames; its log must carry the
#          "[crash] where:" and "[crash] guest function: sub_XXXXXXXX" lines;
#   run B: the next start (A's log becomes the previous log): F3 opens the menu, the volume goes down, the FPS counter goes on,
#          "Report a problem" writes the file, F3 closes it. Checks: opened and closed, the settings written to the game's own
#          sections, the counter shown, the report there with its parts, the previous run's crash in it, and no personal path
#          or name in it. Then the debug screen: Shift+F3 to Full, F3+R (ten seconds of the performance log), F3+C (the
#          clipboard), F3+G (the graph), the Record row; its log lines, debug_screen=2 and the two CSV files are checked.
#
#   (a game with a known FXAA pass also moves its Edge smoothing row: written at once to the game's [video] section)
#   run C (SuperChargers and Imaginators, whose A and B are not run; PAD=1 adds it to any game): the side panel and every rail
#          page walked with pad presses only (SKY_TOYBOX_SCRIPT "pad=": SKY_PRESS reaches the game's input after the menu's
#          filter, so it cannot open the menu): View + Start opens the panel; the Toy Box, Achievements and Settings rows are
#          opened and left with B; RB and LB step Settings, Controls and back to Achievements; Performance steps Compact, Full, Off;
#          "Report a problem" writes the file; B closes.
#   tools/regress/f3menu.sh <ssa|giants|swapforce|superchargers|imaginators> <disc image>
#   EXE=<game executable>   default build/ArkeyanRuntime/Skylanders Spyro's Adventure (build-giants/...: Skylanders Giants, build-swapforce/...: Skylanders Swap Force,
#                           build-superchargers/...: Skylanders SuperChargers, build-imaginators/...: Skylanders Imaginators)
#
# Everything is under private/regress/f3menu-<game>/ (scratch settings, collection, saves, log, home, report folder).
set -u
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${1:-}; iso=${2:-}
[ -f "$root/games/$game/game.txt" ] && [ -f "$iso" ] || { echo "usage: $0 <ssa|giants|swapforce|superchargers|imaginators> <disc image>" >&2; exit 2; }
case $game in
    ssa) dir=build; name="Skylanders Spyro's Adventure" ;;
    giants) dir=build-giants; name="Skylanders Giants" ;;
    swapforce) dir=build-swapforce; name="Skylanders Swap Force" ;;
    superchargers) dir=build-superchargers; name="Skylanders SuperChargers" ;;
    *) dir=build-imaginators; name="Skylanders Imaginators" ;;
esac
padonly=0
case $game in superchargers|imaginators) padonly=1 ;; esac
exe="${EXE:-$root/$dir/ArkeyanRuntime/$name}"
[ -x "$exe" ] || { echo "no game executable: $exe" >&2; exit 2; }
r=$root/private/regress/f3menu-$game
rm -rf "${r:?}"; mkdir -p "$r/home" "$r/report"
fail=0
bad() { echo "FAIL: $*"; fail=1; }

# run <label> <seconds> <extra env...>: one isolated run of the game, stopped by its PID when the time is up.
run() {
    label=$1; seconds=$2; shift 2
    ( cd "$r" && env HOME="$r/home" CFFIXED_USER_HOME="$r/home" SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk \
        SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null SKY_SETTINGS="$r/launcher.ini" SKY_LAUNCHER_INI="$r/launcher.ini" \
        SKY_COLLECTION="$r/collection" SKY_SAVE_DIR="$r/save" SKYLANDERS_DATA="$r/data" SKY_LOG="$r/game.log" \
        SKY_REPORT_DIR="$r/report" SKY_LAUNCHER_LOG_DIR="$r/logs" SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated \
        SKY_TOYBOX_SHOTS="$r/$label" "$@" nice "$exe" "$iso" > "$r/$label.out" 2>&1 ) &
    pid=$!
    for _ in $(seq "$seconds"); do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do kill "$child" 2>/dev/null; done
    sleep 3
    for child in $(pgrep -P "$pid" 2>/dev/null) "$pid"; do kill -9 "$child" 2>/dev/null; done
    wait "$pid" 2>/dev/null
}

if [ $padonly -eq 0 ]; then
# A limit above 60 is in the game's section: the menu (opened, moved through, a setting changed) must not lower it.
printf '\n[%s.video]\nfps_limit=144\n' "$game" >> "$r/launcher.ini"
echo "== $game: run A, a fault on a guest thread"
run a 150 SKY_CRASH_TEST=fault
grep -q '^\[crash-test\]' "$r/game.log" || bad "A: the crash test did not run (no [crash-test] line in $r/game.log)"
grep -q '^Fault (' "$r/game.log" || bad "A: no Fault line"
grep -q '^\[crash\] where: guest thread [0-9]*, lr 0x[0-9A-F]*, ' "$r/game.log" || bad "A: no '[crash] where:' line with the guest lr"
grep -q '^\[crash\] guest function: sub_[0-9A-F]\{8\}' "$r/game.log" || bad "A: no '[crash] guest function: sub_XXXXXXXX' line"
cp "$r/game.log" "$r/a.log"

echo "== $game: run B, the menu"
# The side panel opens on Resume: Up is Volume (Left x2: 80); Down x4 is Settings, Enter opens the page on its first row
# (Commands); the rows by section (Game, Video, Audio, Debug) end with FPS counter, Debug screen, Record a performance log,
# so Up x3 (round the top) is the FPS counter, Enter: on. A game with a known FXAA pass also moves Edge smoothing (Video's
# row above VSync, Volume, Speakers and the counter): Up x4 more, Right (applied and written at once). Escape: back to the panel on
# Settings; Down x2: Report a problem, Enter.
edge=0
tr '\n' ' ' < "$root/games/$game/addresses.h" | grep -q 'HostKernelPixelShader = *0x' && edge=1
script="F3:30,shot=open:31,Up:31.3,Left:31.6,Left:31.9,Down:32.2,Down:32.4,Down:32.6,Down:32.8,Return:33.1"
script="$script,Up:33.5,Up:33.7,Up:33.9,Return:34.2,shot=settings:34.6"
[ $edge -eq 1 ] && script="$script,Up:34.8,Up:35,Up:35.2,Up:35.4,Right:35.6"
script="$script,Escape:35.8,Down:36.1,Down:36.3,Return:36.6,shot=report:38,F3:39,shot=closed:41"
# Then the debug screen: Shift+F3 twice (Full), F3+R records ten seconds of the performance log, F3+C copies the screen,
# F3+G hides and shows the graphs; after the ten seconds, Settings' last row (Record a performance log: Down x3, Enter, Up
# round the top) records until it is turned off.
script="$script,shift+F3:42,shift+F3:42.5,down=F3:43,R:43.2,up=F3:43.4,shot=full:46,down=F3:47,C:47.2,up=F3:47.4"
script="$script,down=F3:49,G:49.2,up=F3:49.4,down=F3:50,G:50.2,up=F3:50.4"
script="$script,F3:55,Down:55.2,Down:55.4,Down:55.6,Return:55.9,Up:56.2,Return:56.5,F3:57"
script="$script,F3:61,Down:61.2,Down:61.4,Down:61.6,Return:61.9,Up:62.2,Return:62.5,F3:63"
run b 68 SKY_TOYBOX_SCRIPT="$script"
log=$r/game.log
grep -q '^\[gamemenu\] opened' "$log" || bad "B: F3 did not open the menu"
grep -q '^\[toybox\] closed' "$log" || bad "B: F3 did not close the menu"
grep -q '^\[gamemenu\] volume = 80' "$log" || bad "B: the volume did not change"
grep -q '^\[gamemenu\] fps counter shows [0-9]* FPS' "$log" || bad "B: the FPS counter did not appear"
grep -q '^\[gamemenu\] debug_screen = 2' "$log" || bad "B: Shift+F3 did not reach the Full debug screen"
grep -q '^\[debugscreen\] full: [0-9.]* ms a frame' "$log" || bad "B: the Full debug screen did not log its cost"
grep -q '^\[gamemenu\] debug screen copied: yes' "$log" || bad "B: F3+C did not copy the debug screen"
grep -q '^\[gamemenu\] debug screen graphs off' "$log" || bad "B: F3+G did not hide the graphs"
grep -q '^\[gamemenu\] perf_log = off ('"$game"'-perf-.*\.csv finished)' "$log" || bad "B: F3+R did not record ten seconds"
grep -q '^\[gamemenu\] perf_log = off$' "$log" || bad "B: the menu's Record row did not stop the performance log"
[ "$(ls "$r"/logs/"$game"-perf-*.csv 2>/dev/null | wc -l)" -ge 2 ] || bad "B: not two performance logs in $r/logs"
for csv in "$r"/logs/"$game"-perf-*.csv; do
    [ "$(head -1 "$csv")" = "time_s,frame_ms,renderer_ms,gpu_ms,latency_ms,draws,resolves,uploads,upload_mb,write_faults,pipelines_built,file_reads,hitch" ] || bad "B: $csv has no header"
    [ "$(wc -l < "$csv")" -ge 40 ] || bad "B: $csv has under 40 frames"
done
awk '/^\[/ { in_video = ($0 == "['"$game"'.video]") } in_video' "$r/launcher.ini" | grep -q '^debug_screen=2$' || bad "B: [$game.video] debug_screen=2 is not in launcher.ini"
awk '/^\[/ { in_audio = ($0 == "['"$game"'.audio]") } in_audio' "$r/launcher.ini" | grep -q '^volume=80$' || bad "B: [$game.audio] volume=80 is not in launcher.ini"
awk '/^\[/ { in_video = ($0 == "['"$game"'.video]") } in_video' "$r/launcher.ini" | grep -q '^show_fps=1$' || bad "B: [$game.video] show_fps=1 is not in launcher.ini"
awk '/^\[/ { in_video = ($0 == "['"$game"'.video]") } in_video' "$r/launcher.ini" | grep -q '^fps_limit=144$' || bad "B: [$game.video] fps_limit=144 was lowered by the menu"
if [ $edge -eq 1 ]; then
    grep -q '^\[gamemenu\] edge_smoothing = ' "$log" || bad "B: the Edge smoothing row did not change"
    awk '/^\[/ { in_video = ($0 == "['"$game"'.video]") } in_video' "$r/launcher.ini" | grep -q '^edge_smoothing=\(game\|light\|off\)$' || bad "B: [$game.video] edge_smoothing is not in launcher.ini"
else
    grep -q 'edge_smoothing' "$log" && bad "B: a game with no FXAA pass shows the Edge smoothing row"
fi
report=$(ls "$r"/report/*-report-*.txt 2>/dev/null | head -1)
if [ -z "$report" ]; then bad "B: no report file in $r/report"; else
    for part in "Game problem report" "Game: " "[version] " "[system] " "[gpu] " "===== last crash (the previous run) =====" "===== launcher.ini =====" \
                "===== Arkeyan.log =====" "===== Arkeyan.prev.log =====" "===== skylauncher-game.log ====="; do
        grep -qF -- "$part" "$report" || bad "B: the report lacks '$part'"
    done
    grep -q '^Fault (' "$report" && grep -q '^\[crash\] guest function: sub_[0-9A-F]\{8\}' "$report" || bad "B: the report lacks the previous run's crash location"
    user=$(id -un); machine=$(hostname); machine_short=${machine%%.*}
    if grep -F -i -e "/Users/$user" -e "/home/$user" -e "$HOME" "$report" > "$r/leak.txt"; then bad "B: a home path is in the report (see $r/leak.txt)"; fi
    if grep -w -i -F -e "$user" -e "$machine_short" "$report" > "$r/leak2.txt"; then bad "B: the account or computer name is in the report (see $r/leak2.txt)"; fi
    grep -q '~' "$report" || bad "B: no '~' in the report (the home folder is replaced by it)"
    echo "report: $(basename "$report"), $(wc -c < "$report") bytes"
fi
fi

if [ $padonly -eq 1 ] || [ "${PAD:-}" = 1 ]; then
    echo "== $game: run C, the panel and the rail pages by pad"
    # SKY_TOYBOX_SCRIPT "pad=<button>[*<hold seconds>]:<second>": View (back) held with Start pressed just after it opens the panel, on Resume. Down is
    # Toy Box, A opens it, B comes back to the panel (each page left with B logs "opened" again); Achievements, Settings the
    # same. From Settings' page RB goes to Controls, LB back to Settings, LB to Achievements (where LB / RB are the tabs' keys:
    # they step no page); B leaves to the panel, on Achievements: Down x2 is Performance.
    # Right Right (Compact, Full), Left x2 (Off); Down is Report a problem, A; B closes.
    t=${PAD_START:-100} # the game polls the pad once it is at its menus: late on a cold start (no shader cache)
    press=""
    add() { press="$press${press:+,}pad=$1${3:+*$3}:$(echo "$t + $2" | bc)"; }
    add back 0 0.6; add start 0.1 # Start comes 0.1 s into View's hold: before the wheel's 0.25 s
    add down 3; add a 4; add b 7
    add down 9; add a 10; add b 13
    add down 15; add a 16; add rb 19; add lb 21; add lb 23; add b 26
    add down 28; add down 29; add right 31; add right 33; add left 35; add left 37
    add down 39; add a 40; add b 44
    rm -f "$r/game.log"
    run c $((t + 60)) SKY_TOYBOX_SCRIPT="$press"
    log=$r/game.log
    n() { grep -c "$1" "$log"; }
    [ "$(n '^\[gamemenu\] opened')" -ge 4 ] || bad "C: the panel did not open and come back from each page (opened x$(n '^\[gamemenu\] opened'), want 4 or more)"
    [ "$(n '^\[gamemenu\] achievements opened')" -ge 2 ] || bad "C: Achievements did not open from the panel and from the rail"
    [ "$(n '^\[gamemenu\] settings opened')" -ge 2 ] || bad "C: Settings did not open from the panel and from LB on the Controls page"
    [ "$(n '^\[gamemenu\] commands opened')" -ge 1 ] || bad "C: RB on the Settings page did not reach the Controls page"
    grep -q '^\[gamemenu\] debug_screen = 1' "$log" && grep -q '^\[gamemenu\] debug_screen = 2' "$log" && grep -q '^\[gamemenu\] debug_screen = 0' "$log" || bad "C: Performance did not step Compact, Full, Off"
    grep -q '^\[report\] written: ' "$log" || bad "C: Report a problem wrote no file"
    grep -q '^\[toybox\] closed' "$log" || bad "C: B did not close the panel"
fi

if [ $fail -eq 0 ] && [ $padonly -eq 1 ]; then echo "PASS: $game: the panel and every rail page walked by pad, the report written, B closing (under $r)"
elif [ $fail -eq 0 ]; then echo "PASS: $game: the F3 menu opens and closes, the counter shows, the settings are written, the report is complete and clean, the crash is located (under $r)"; else echo "FAIL (logs in $r)"; fi
exit $fail
