#!/bin/sh
# The regression harness: one command says whether a change broke either game (docs/getting-started/testing.md).
#
#   tools/test_all.sh [--quick] [--record] [--only ssa|giants|swapforce|trapteam|superchargers|imaginators] [--long]
#
# Each game's section also runs the pad remapping and rumble check (tools/regress/pad_qol.sh) and the Toy Box's out-of-health check
# (tools/regress/out_of_health.sh, ~1 min each), FSR 3 and its frame generation to the first level (tools/regress/fsr_all.sh,
# fsr_framegen.sh, ~2 min each; Trap Team's,
# SuperChargers' and Imaginators' through their routes, ~9 min, Trap Team's with --long, ~16 min); Swap Force's and Imaginators' also the Nightmare menu (tools/regress/nightmare_menu.sh, ~4 min).
# SuperChargers (SUPERCHARGERS_ISO: tools/regress/superchargers_lobby.sh, ~9 min) and Imaginators (IMAGINATORS_ISO: tools/regress/imaginators_hub.sh,
# ~9 min) each build, run the host units and a new game plus a continue to the hub; a SKIP row without the disc.
# Trap Team adds its build and host units when TRAPTEAM_ISO is set; runtime coverage remains an explicit SKIP.
# Needs SSA_ISO and GIANTS_ISO (the discs, read only). Builds SSA (build/) and Giants (build-giants/) and every unit
# test, runs them, then one isolated game run each (SSA to level_027, Giants from the title to Chapter 1) with the
# checks of tools/regress/analyze.py. Swap Force (build-swapforce/, one run from the title to the tutorial level,
# ~2 min, then a swapper through the Toy Box, tools/regress/swapper_toybox.sh, ~2 min, and its search and filters, tools/regress/search_toybox.sh, ~2 min) runs only when SWAPFORCE_ISO is set: without it the section is skipped (a SKIP row, not a failure). Giants has
# three runs (~3.5 min): from the title past Chapter 1's start with an SSA figure, the same save read back, and a level
# of the tour (SKY_LEVEL) with a Giants figure (a LightCore). --quick stops after the builds and the unit tests. --record (on a good build) writes the allow-lists and the reference frames to
# private/test-refs/ instead of comparing. --long adds the slower runs: SSA plays a level-15 Giants figure (docs/runtime/host/figures.md, "Data rules"), one toy of each kind (tools/regress/ssa_figures.sh, 5 runs), and Giants' level tour (tools/pipelines/tour_giants.sh, every level
# of the disc, ~3 h; the results are checked against private/test-refs/giants/tour.txt). Exit code 0: all PASS.
# Never touches the player's files: every run uses scratch folders under .scratch/regress/.
# LAUNCHER=<Arkeyan Launcher executable> adds each game's Toy Box window run (tools/regress/toybox_window.sh).
# Env: REGRESS_FPS_FLOOR (27), REGRESS_PSNR_SSA / REGRESS_PSNR_GIANTS / REGRESS_PSNR_SWAPFORCE (min frame PSNR, dB),
# REGRESS_HIGH_LOAD (load average from which a failed fps check is retried once; default 2 x the logical cores).
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
quick=0; record=""; only=""; long=0
while [ $# -gt 0 ]; do
    case $1 in
        --quick) quick=1 ;;
        --record) record=--record ;;
        --long) long=1 ;;
        --only)
            shift
            case ${1:-} in
                ssa|giants|swapforce|trapteam|superchargers|imaginators) only=$1 ;;
                *) echo "error: --only needs a section: ssa, giants, swapforce, trapteam, superchargers, imaginators" >&2; exit 2 ;;
            esac ;;
        *) echo "usage: tools/test_all.sh [--quick] [--record] [--only ssa|giants|swapforce|trapteam|superchargers|imaginators] [--long]"; exit 2 ;;
    esac
    shift
done
refs=$root/private/test-refs
work=$root/.scratch/regress
rows=$work/rows.txt
fps_floor=${REGRESS_FPS_FLOOR:-27}
cores=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
high_load=${REGRESS_HIGH_LOAD:-$((cores * 2))}
analyze() { python3 "$root/tools/regress/analyze.py" "$@"; }
loadavg() { uptime | sed -E 's/.*load averages?: *//' | awk '{gsub(",", "", $1); print $1}'; }
mkdir -p "$work"; : > "$rows"
start=$SECONDS
echo "load average at start: $(loadavg) ($cores cores; a failed fps check is retried once from $high_load)"

# row <section> <name> <PASS|FAIL> <detail>
row() { printf '%s|%s|%s|%s\n' "$1" "$2" "$3" "$4" >> "$rows"; printf '  %-5s %s: %s\n' "$3" "$2" "$4"; }
# check <section> <name> <command...>: the command prints its reason; its status is the verdict
check() {
    section=$1; name=$2; shift 2
    reason=$("$@" 2>&1); status=$?
    if [ $status -eq 0 ]; then row "$section" "$name" PASS "$reason"; else row "$section" "$name" FAIL "$reason"; fi
    return $status
}

all_tests="ArkeyanBuilderTest ArkeyanBuilderGuardTest ArkeyanEditionTest ArkeyanProgressTest ArkeyanAchievementsKernelTest ArkeyanGuestThreadTest ArkeyanPortalTest ArkeyanPortalAdpcmTest ArkeyanPortalHidReportTest ArkeyanPortalUsbTest ArkeyanPortalSpeakerTest ArkeyanSpeakersTest ArkeyanPortalAsyncTest ArkeyanToyboxLinkTest ArkeyanFigureTest ArkeyanArtTest ArkeyanCollectionTest ArkeyanSettingsTest ArkeyanDatadirTest ArkeyanSavesTest ArkeyanGuestFormatTest ArkeyanVersionResourceTest ArkeyanGameDataTest ArkeyanTexturePackTest ArkeyanRuntimeLauncherTest ArkeyanRecompGpuTest ArkeyanRenderScaleTest ArkeyanUpscaleTest ArkeyanFrameGenTest ArkeyanSwapChainRecoveryTest ArkeyanGpuBackendTest ArkeyanFrameLimiterTest ArkeyanFrameRateFallbackTest ArkeyanYieldBackOffTest ArkeyanPresentModeTest ArkeyanRangeHashTest ArkeyanHeapViewTest ArkeyanWriteWatchTest ArkeyanReadFullyTest ArkeyanTiledDepthTest ArkeyanFloatScanTest ArkeyanAutoScaleTest ArkeyanCommandsTest ArkeyanPortablePackTest ArkeyanVfsTest ArkeyanMouseTest ArkeyanDebugScreenTest ArkeyanToyboxLookTest ArkeyanToyboxInPlayTest ArkeyanToyboxSwapTest ArkeyanToyboxTrapTest ArkeyanXenonRecompTest ArkeyanPpcDecoderTest ArkeyanGlyphModelsTest ArkeyanFileListTest ArkeyanProfilesTest ArkeyanPromptTextTest ArkeyanPortalScriptTest ArkeyanZoneNameTest ArkeyanLevelRedirectTest ArkeyanLiveBlocksTest ArkeyanPortalUsbBackendTest ArkeyanFirstFrameTest ArkeyanVideoDriverTest ArkeyanRingCursorTest ArkeyanResolveWritebackTest ArkeyanResourceLockTest"
# ArkeyanRuntimeLauncherTest checks SSA's disc and game entry only: the Giants and Swap Force builds have no "ssa" game to find.
giants_tests=${all_tests% ArkeyanRuntimeLauncherTest*}${all_tests#*ArkeyanRuntimeLauncherTest}

build_and_test() { # <section> <build dir> <cmake args...>
    section=$1; dir=$2; shift 2
    tests=$all_tests; [ "$section" != ssa ] && tests=$giants_tests
    echo "== $section: build ($dir)"
    t=$SECONDS
    [ -f "$dir/build.ninja" ] || cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@" > "$work/$section-cmake.log" 2>&1
    # shellcheck disable=SC2086
    if nice cmake --build "$dir" -j2 --target all $tests > "$work/$section-build.log" 2>&1; then
        row "$section" "build" PASS "$((SECONDS - t)) s"
    else
        row "$section" "build" FAIL "see $work/$section-build.log: $(grep -m1 -E 'error:|FAILED' "$work/$section-build.log" | cut -c1-120)"
        return 1
    fi
    echo "== $section: unit tests"
    for name in $tests; do
        bin=$(find "$dir" -name "$name" -type f -perm -u+x 2>/dev/null | head -1)
        if [ -z "$bin" ]; then row "$section" "unit $name" FAIL "not built"; continue; fi
        scratch=$work/$section-unit-$name; rm -rf "${scratch:?}"; mkdir -p "$scratch"
        set --
        if [ "$name" = ArkeyanRuntimeLauncherTest ]; then
            if [ -z "${SSA_ISO:-}" ]; then row "$section" "unit $name" FAIL "set SSA_ISO"; continue; fi
            set -- "$SSA_ISO" "$scratch"
        fi
        [ "$name" = ArkeyanGameDataTest ] || [ "$name" = ArkeyanFileListTest ] || [ "$name" = ArkeyanPortablePackTest ] || [ "$name" = ArkeyanVfsTest ] && set -- "$scratch"
        [ "$name" = ArkeyanBuilderTest ] || [ "$name" = ArkeyanEditionTest ] && set -- "$root" "$scratch"
        [ "$name" = ArkeyanBuilderGuardTest ] && set -- "$scratch/guard"
        # The achievements tests read this section's own disc (title XDBF counts and gamerscore, the kernel calls on its list).
        if [ "$name" = ArkeyanProgressTest ] || [ "$name" = ArkeyanAchievementsKernelTest ]; then
            case $section in
                ssa) disc=${SSA_ISO:-} ;; giants) disc=${GIANTS_ISO:-} ;;
                swapforce) disc=${SWAPFORCE_ISO:-} ;; trapteam) disc=${TRAPTEAM_ISO:-} ;;
                superchargers) disc=${SUPERCHARGERS_ISO:-} ;; imaginators) disc=${IMAGINATORS_ISO:-} ;;
            esac
            if [ -z "$disc" ] || [ ! -f "$disc" ]; then row "$section" "unit $name" FAIL "set the disc image of $section"; continue; fi
            set -- "$disc"
        fi
        env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
            SKYLANDERS_DATA="$scratch/data" SKY_SETTINGS="$scratch/none.ini" SKY_LAUNCHER_INI="$scratch/none.ini" \
            SKY_COLLECTION="$scratch/collection" SKY_SAVE_DIR="$scratch/save" SKY_LOG="$scratch/game.log" \
            nice "$bin" "$@" > "$scratch/out.txt" 2>&1
        status=$?
        summary=$(grep -E '[0-9]+ checks, [0-9]+ failures|^(PASS|FAIL)$' "$scratch/out.txt" | tail -1)
        if [ $status -eq 0 ]; then
            row "$section" "unit $name" PASS "${summary:-exit 0}"
        else
            row "$section" "unit $name" FAIL "exit $status; ${summary:-$(tail -1 "$scratch/out.txt" | cut -c1-100)}"
        fi
    done
    check "$section" "unit host menu input" python3 -I "$root/tools/regress/f3mouse_unit.py" "$dir"
    check "$section" "unit PID timer" bash "$root/tools/regress/pid_timer_test.sh"
    check "$section" "unit movie shader targets" python3 -I "$root/tools/regress/movie_targets.py"
    check "$section" "unit edition report to release" python3 -I "$root/tools/editions/from_report.py" --self-test
}

# run_game <game> <disc> <seconds> <press> <snapshot from frame> <extra env assignment>...
# Starts the game isolated, waits <seconds> (or until it exits), stops it by PID. Sets run_dir, run_status, run_early and
# first_frame (the second of the first present: SKY_FRAME_TIMES counts from it; Swap Force needs 10-17 s to get there).
# run_name (default: the game) names the run's folder and its references; run_save_from is a save folder to start from.
run_game() {
    game=$1; iso=$2; seconds=$3; press=$4; snap_from=$5; shift 5
    run_dir=$work/${run_name:-$game}-run; rm -rf "${run_dir:?}"; mkdir -p "$run_dir/save" "$run_dir/figures" "$run_dir/snap"
    [ -n "${run_save_from:-}" ] && cp -R "$run_save_from/." "$run_dir/save/"
    cp "$root/private/figures/new_14.sky" "$run_dir/figures/" 2>/dev/null
    env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKYLANDERS_DATA="$run_dir/data" SKY_SETTINGS="$run_dir/none.ini" SKY_LAUNCHER_INI="$run_dir/none.ini" \
        SKY_COLLECTION="$run_dir/figures" SKY_FIGURES="$run_dir/figures" SKY_SAVE_DIR="$run_dir/save" SKY_LOG="$run_dir/game.log" \
        SKY_SKIP_MOVIES=1 SKY_PRESS="$press" SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky SKY_TRACE_FILES=1 \
        SKY_RENDER_STATS=1 SKY_FRAME_TIMES="$run_dir/frames.txt" SKY_AUDIO_DUMP="$run_dir/audio.f32" \
        SKY_GPU_SNAPSHOT="$run_dir/snap/s" SKY_GPU_SNAPSHOT_FROM="$snap_from" SKY_GPU_SNAPSHOT_EVERY=60 "$@" \
        nice "$root/$exe_dir/ArkeyanRuntime/$(sed -n 's/^runtime=//p' "$root/games/$game/game.txt")" --vsync 0 --fps-limit 30 --anisotropy 4 "$iso" > "$run_dir/log" 2>&1 &
    pid=$!
    elapsed=0; run_early=1; first_frame=""
    while kill -0 "$pid" 2>/dev/null; do
        [ -z "$first_frame" ] && [ -s "$run_dir/frames.txt" ] && first_frame=$elapsed
        if [ $elapsed -ge "$seconds" ]; then
            run_early=0; kill "$pid" 2>/dev/null; sleep 3; kill -9 "$pid" 2>/dev/null; break
        fi
        sleep 1; elapsed=$((elapsed + 1))
    done
    wait "$pid" 2>/dev/null; run_status=$?
}

# game_checks <game> <seconds> <fps mode: level|end> <fps window s> <psnr> <log text expected>...
game_checks() {
    game=$1; seconds=$2; mode=$3; window=$4; psnr=$5; shift 5
    label=${run_name:+$run_name: }; refdir=$refs/${run_name:-$game}
    if [ $run_early -eq 1 ]; then
        row "$game" "${label}run" FAIL "the game exited by itself after ${elapsed} s (code $run_status): $(tail -2 "$run_dir/log" | tr '\n' ' ' | cut -c1-140)"
    else
        row "$game" "${label}run" PASS "${seconds} s, stopped by the harness"
    fi
    check "$game" "${label}no fault, [guard] or [stub] regression" analyze scan "$run_dir/log" "$refs/$game" $record
    check "$game" "${label}reaches the expected point" analyze opens "$run_dir/log" "$@"
    flag=""; [ "$mode" = end ] && flag=--end
    load_before=$(loadavg)
    rendered=$((seconds - ${first_frame:-0})) # the frame times start at the first present
    if ! check "$game" "${label}fps and hang" analyze fps "$run_dir/frames.txt" "$rendered" "$fps_floor" "$window" $flag; then
        if [ "$(echo "$load_before $high_load" | awk '{print ($1 >= $2)}')" = 1 ]; then
            echo "  note: load average $load_before is high: running once more for the fps check"
            sed -i.bak '$d' "$rows"
            $retry
            rendered=$((seconds - ${first_frame:-0}))
            check "$game" "${label}fps and hang (second run, load $load_before)" analyze fps "$run_dir/frames.txt" "$rendered" "$fps_floor" "$window" $flag
        fi
    fi
    # Steady scenes (no run_name) compare with one reference frame; the moving ones (dialogue, camera; frame_set=1) with a set.
    single=--single; [ -n "${run_name:-}${frame_set:-}" ] && single=""
    check "$game" "${label}frame close to the reference" analyze frame "$run_dir/snap" "$refdir/frame.png" "$psnr" $record $single
    check "$game" "${label}audio does not repeat" analyze audio "$run_dir/audio.f32" 100 1
}


# Giants' extra checks: each prints its reason and exits 0 (PASS) or 1 (FAIL).
giants_save_written() { # <save dir>: the first run's slot file exists and is not empty
    f=$(find "$1" -name 'Save_Slot_1.xsg' -type f 2>/dev/null | head -1)
    [ -n "$f" ] && [ -s "$f" ] && { echo "$(basename "$f"): $(wc -c < "$f") bytes"; return 0; }
    echo "no non-empty Save_Slot_1.xsg under $1"; return 1
}
giants_save_read() { # <log>: the slot file is opened and the new-game movie of the first run is not played again
    grep -aq 'open save:\\Save_Slot_1.xsg -> 0x00000000' "$1" || { echo "the slot file was not opened"; return 1; }
    if grep -aq 'open \\movies\\fmv01.mov' "$1"; then echo "the new-game movie played again: the save was not used"; return 1; fi
    echo "slot read, no new game"
}
giants_figure_placed() { # <log>: the emulated portal holds the LightCore and the game switched its light on
    grep -aq '\[portal\] placed .*(toy type 100)' "$1" || { echo "the LightCore (toy type 100) was not placed"; return 1; }
    grep -aq '\[portal\] light: #[0-9A-F]*[1-9A-F][0-9A-F]* (on)' "$1" || { echo "placed, but the game never lit the portal"; return 1; }
    echo "LightCore placed, portal light on"
}
giants_tour() { # the long tour: every level; faults and levels not reached against the known list
    tools/pipelines/tour_giants.sh "$GIANTS_ISO" "$work/tour" 1 > "$work/tour.log" 2>&1
    analyze tour "$work/tour/summary.tsv" "$refs/giants/tour.txt" $record
}

play_settings() { # <game> <disc>: the game's own video copy turns the logos off and 60 FPS on (the last line is the reason)
    out=$(tools/regress/play_settings.sh "$@" 2>&1); status=$?
    printf '%s\n' "$out" | tail -1
    return $status
}

f3menu() { # <game> <disc>: the F3 menu opens and closes, the counter shows, the settings are written, the problem report is complete and clean, a crash is located
    out=$(tools/regress/f3menu.sh "$@" 2>&1); status=$?
    printf '%s\n' "$out" | tail -1
    return $status
}

surround() { # <game> <disc>: 5.1 reaches a 6-channel device with signal on every channel; auto falls back to stereo
    out=$(tools/regress/surround.sh "$@" 2>&1); status=$?
    printf '%s\n' "$out" | tail -1
    return $status
}
# The launcher's Toy Box window drives the game (tools/regress/toybox_window.sh, ~2-5 min a game): with LAUNCHER=<the Arkeyan
# Launcher executable>; without it a SKIP row.
toybox_window() { # <game>
    if [ -z "${LAUNCHER:-}" ]; then row "$1" "the Toy Box window drives the game" SKIP "set LAUNCHER to the Arkeyan Launcher executable"; return 0; fi
    check "$1" "the Toy Box window drives the game (place, swap, take off; the game reads each)" toybox_window_run "$1"
}
toybox_window_run() {
    out=$(tools/regress/toybox_window.sh "$1" 2>&1); status=$?
    printf '%s\n' "$out" | grep -E '^FAIL' | head -3 | tr '\n' ' '
    printf '%s\n' "$out" | tail -1
    return $status
}

ssa_figures_subset() { # the harness on one toy of each kind; its summary line is the reason
    tools/regress/ssa_figures.sh "$SSA_ISO" "$work/ssa-figures" 14 404 201 300 505 > "$work/ssa-figures.log" 2>&1
    status=$?; tail -1 "$work/ssa-figures.log"
    [ $status -eq 0 ] && grep -q ' 0 FAIL' "$work/ssa-figures.log"
}

# same_extra <before> <after>: SSA wrote the figure in between and its Giants data (extra areas) came back byte for byte.
same_extra() {
    out=$("$figure_test" --same-extra "$1" "$2" 2>&1); status=$?
    printf '%s\n' "$out" | grep -E '^extra areas|FAIL' | tr '\n' ' '
    return $status
}

ssa_press="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60"
# Giants (tools/regress/giants_press.py): A every 4 s through the title and the menus (the tutorial level is playable at ~40 s),
# then walking, attacking and the pause menu.
giants_seconds=100; giants_press=$(python3 -I "$root/tools/regress/giants_press.py" $giants_seconds)
giants_short_press="a:8,a:12,a:16,a:20,a:24,a:28,a:32,a:36,a:40,a:44,a:48"
ssa_seconds=90

# Keys per game (host/settings.cpp: [<game>.input] wins over [input]): Giants alone binds Y to G. The SSA run and the Giants
# coin run read this file: SSA must keep its own Y (V), Giants must draw G.
keys_ini=$work/keys.ini; printf '[giants.input]\nkey_y=G\n' > "$keys_ini"
run_ssa() { exe_dir=build; run_name=""; run_save_from=""
    run_game ssa "${SSA_ISO:-}" $ssa_seconds "$ssa_press" 900 SKY_LEVEL=level_027 SKY_SETTINGS="$keys_ini" SKY_LAUNCHER_INI="$keys_ini"; }
# The snapshot count (SKY_GPU_SNAPSHOT_FROM, in presents) also counts the main thread's presents of the loading screens,
# whose number depends on how fast the loads go: the Giants runs start their snapshots early enough for the last 20 to
# hold the reference scene at any load speed (2026-10-26: after the kernel wake-up fix, docs/runtime/xbox360/threads_sync.md,
# Chapter 1 had ~1900 fewer such presents and frame 3000 came after the pause menu).
# 1. title to Chapter 1's tutorial and play, with an SSA figure (Gill Grunt) on the emulated portal; a virtual PS5 pad
#    plugged at 8 s: the button glyphs must follow it (glyphs=auto, host/glyphs.h)
run_giants() { exe_dir=build-giants; run_name=""; run_save_from=""; run_game giants "${GIANTS_ISO:-}" $giants_seconds "$giants_press" 1800 SKY_GLYPHS_SCRIPT=ps5:8; }
# 2. the save the first run made, read back (the game must offer it and the tutorial must not start again)
run_giants_save() { exe_dir=build-giants; run_name=giants-save; run_save_from=$work/giants-run/save
    run_game giants "${GIANTS_ISO:-}" 50 "$giants_short_press" 600; }
# 4. the glyphs in the 3D world: the tutorial with a Giant (Swarm, toy type 101) on the portal, whose rock to smash shows a gold
#    Y coin; A every 3.5 s through the title and the cutscenes, then the stick held right 1.2 s every 6 s from 60 s walks the
#    figure to the rock. A virtual PS5 pad plugged at 8 s: the coin must be drawn as a PlayStation button; then (the figure
#    stands by the rock) a Switch pad at 100 s, an Xbox One pad at 108 s, a PS3 pad at 116 s and an Xbox 360 pad at 124 s:
#    one family each. A key press at 141 s goes back to the keyboard, then Y every 2 s from 142 s lifts the rock: its "Mash!"
#    prompt's larger coin (y_coin 128) must be drawn as a keycap, with Giants' own Y key (G, keys_ini).
giants_coin_seconds=170
giants_coin_press=$(awk 'BEGIN { for (t = 4; t <= 70; t += 3.5) printf "a:%.1f,", t; for (t = 60; t <= 140; t += 6) printf "lright:%d:1.2,", t; for (t = 142; t <= 164; t += 2) printf "y:%d,", t }' | sed 's/,$//')
run_giants_coin() { exe_dir=build-giants; run_name=giants-coin; run_save_from=""
    run_game giants "${GIANTS_ISO:-}" $giants_coin_seconds "$giants_coin_press" 3000 SKY_LEVEL=level_213_tutotial SKY_PORTAL_PLACE=new:101 \
        SKY_GLYPHS_SCRIPT=ps5:8,switch:100,xboxone:108,ps3:116,xbox360:124,key:141 SKY_SETTINGS="$keys_ini" SKY_LAUNCHER_INI="$keys_ini"; }
# 3. a level of the tour (SKY_LEVEL), with a Giants figure: a never-played LightCore Jet-Vac (toy type 100, variant 0x1206)
run_giants_level() { exe_dir=build-giants; run_name=giants-level; run_save_from=""
    run_game giants "${GIANTS_ISO:-}" 60 "$giants_short_press" 900 SKY_LEVEL=level_203_the_machine SKY_PORTAL_PLACE=new:100:1206; }

# Swap Force (docs/getting-started/testing.md, "Swap Force to the tutorial"): A every 2 s through the title, Story Mode,
# the NEW slot and the difficulty (4 presses are needed; those that land on a loading screen or a menu still sliding in
# are ignored). The title is up at ~15 s on a quiet machine and ~40 s at a load average of 100, so the presses run to
# 50 s; the tutorial level (tutorial_a) plays from ~25 s (quiet) to ~50 s. An SSA figure, Eruptor (toy type 9), is on the
# portal: the game loads it in the level. 100 s: past the Greeble ships' cutscene, ~58 s into the tutorial, where the game
# froze until the XMA kicks of multi-context voices were all delivered (games/swapforce/docs/boot.md, "XMA").
swapforce_seconds=100
swapforce_press=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }')
swapper_toybox() { out=$("$root/tools/regress/swapper_toybox.sh" swapper_toybox 2>&1); status=$?; echo "$out" | tail -1; return $status; }
wheel_toybox() { out=$("$root/tools/regress/wheel_toybox.sh" wheel_toybox 2>&1); status=$?; echo "$out" | tail -1; return $status; }
# Pad remapping and rumble, the Toy Box's out-of-health cross, Swap Force's Nightmare menu: own isolated runs (~1 min each, Nightmare ~4 min).
pad_qol() { out=$("$root/tools/regress/pad_qol.sh" "$1" "$2" 2>&1); status=$?; echo "$out" | tail -1; return $status; }
# FSR 3 on, the game to its first level (tools/regress/fsr_all.sh; its disc variable is already set).
fsr_all() { out=$("$root/tools/regress/fsr_all.sh" "$1" 2>&1); status=$?; echo "$out" | tail -1; return $status; }
# FSR 3 frame generation on, the game to its first level (tools/regress/fsr_framegen.sh).
fsr_framegen() { out=$("$root/tools/regress/fsr_framegen.sh" "$1" 2>&1); status=$?; echo "$out" | tail -1; return $status; }
out_of_health() { out=$("$root/tools/regress/out_of_health.sh" "$1" "$2" 2>&1); status=$?; echo "$out" | tail -1; return $status; }
nightmare_menu() { out=$("$root/tools/regress/nightmare_menu.sh" "$@" 2>&1); status=$?; echo "$out" | tail -1; return $status; }
search_toybox() { out=$("$root/tools/regress/search_toybox.sh" search_toybox_en 2>&1); status=$?; echo "$out" | tail -1; return $status; }
# The tutorial run also saves 40 consecutive frames from 50 s (the ship in the lava canyon) for the shimmer check.
swapforce_burst=$(awk 'BEGIN { for (i = 0; i < 40; i++) printf "%s%.3f", (i ? "," : ""), 50 + i * 0.001 }')
run_swapforce() { exe_dir=build-swapforce; run_name=""; run_save_from=""; frame_set=1
    run_game swapforce "${SWAPFORCE_ISO:-}" $swapforce_seconds "$swapforce_press" 600 SKY_PORTAL_PLACE=new:9 SKY_GLYPHS_SCRIPT=ps5:8 \
        SKY_GPU_SNAPSHOT_SECONDS="$swapforce_burst"; }

if [ -z "$only" ] || [ "$only" = ssa ]; then
    echo "##### SSA"
    build_and_test ssa build
    if [ $quick -eq 0 ]; then
        if [ -z "${SSA_ISO:-}" ] || [ ! -f "$SSA_ISO" ]; then row ssa "run" FAIL "set SSA_ISO to the disc image"
        else
            echo "== ssa: level_027 run ($ssa_seconds s)"
            retry=run_ssa; run_ssa
            game_checks ssa $ssa_seconds level 30 "${REGRESS_PSNR_SSA:-20}" 'level_027\mission.str'
            check ssa "the launcher's video settings reach the game (intro logos skipped, 60 FPS)" play_settings ssa "$SSA_ISO"
            check ssa "the F3 menu and the problem report (crash located, no personal path)" f3menu ssa "$SSA_ISO"
            toybox_window ssa
            check ssa "keys per game: another game's binding is not SSA's" analyze opens "$run_dir/log" \
                '[glyphs] keycaps: a=Space b=X x=C y=V start=Enter back=Bksp'
            echo "== ssa: pad remapping and rumble, out of health"
            check ssa "pad remapping and rumble strength reach the title" pad_qol ssa "$SSA_ISO"
            check ssa "a figure out of health shows a red cross until the next level" out_of_health ssa "$SSA_ISO"
            check ssa "FSR 3 to the first level (no failure, no guard)" fsr_all ssa
            check ssa "FSR 3 frame generation to the first level (twice the frames shown)" fsr_framegen ssa
            if [ $long -eq 1 ]; then
                # A Giants figure (level 15, a Giants hat) played by SSA: it shows level 10 and writes the figure on first
                # use; the extra areas must come back byte for byte, still level 15 for Giants.
                echo "== ssa: a level-15 Giants figure (120 s)"
                long_dir=$work/ssa-long; rm -rf "${long_dir:?}"; mkdir -p "$long_dir"
                figure_test=$(find build -name ArkeyanFigureTest -type f -perm -u+x | head -1)
                "$figure_test" --make "$long_dir/eruptor15.sky" 9 xp=96500 hat=6 gold=1234 > "$long_dir/make.txt" 2>&1
                cp "$long_dir/eruptor15.sky" "$long_dir/before.sky"
                exe_dir=build; run_game ssa "$SSA_ISO" 120 "$ssa_press,a:74,a:77,a:80" 900 SKY_LEVEL=level_027 SKY_PORTAL_PLACE="$long_dir/eruptor15.sky"
                check ssa "a Giants level-15 figure keeps its data" same_extra "$long_dir/before.sky" "$long_dir/eruptor15.sky"
                # One toy of each kind through the figure harness (tools/regress/ssa_figures.sh runs all 54 types and the variants).
                echo "== ssa: figure types (--long): a character, a legendary, a magic item, an adventure pack, a sidekick"
                check ssa "figure types round trip (14 404 201 300 505)" ssa_figures_subset
            fi
        fi
    fi
fi
if [ -z "$only" ] || [ "$only" = giants ]; then
    echo "##### Giants"
    build_and_test giants build-giants -DARKEYAN_GAME=giants
    if [ $quick -eq 0 ]; then
        if [ -z "${GIANTS_ISO:-}" ] || [ ! -f "$GIANTS_ISO" ]; then row giants "run" FAIL "set GIANTS_ISO to the disc image"
        else
            echo "== giants: title to Chapter 1 and play, an SSA figure ($giants_seconds s)"
            retry=run_giants; run_giants
            game_checks giants $giants_seconds end 30 "${REGRESS_PSNR_GIANTS:-20}" 'open \level\level_213_tutotial.arc'
            check giants "the launcher's video settings reach the game (intro logos skipped, 60 FPS)" play_settings giants "$GIANTS_ISO"
            check giants "the F3 menu and the problem report (crash located, no personal path)" f3menu giants "$GIANTS_ISO"
            check giants "5.1 sound: six channels with signal, the LFE band-limited, auto falls back to stereo" surround giants "$GIANTS_ISO"
            toybox_window giants
            check giants "glyphs follow the device (PS5 pad)" analyze opens "$run_dir/log" '[glyphs] playstation' \
                'a_button 64 drawn as playstation' 'font atlas 1024 drawn as playstation'
            check giants "the save is written" giants_save_written "$run_dir/save"
            echo "== giants: the rock's gold Y coins in every pad family, then a keycap ($giants_coin_seconds s)"
            retry=run_giants_coin; run_giants_coin
            check giants "giants-coin: the world prompt follows the device (PS5 pad)" analyze opens "$run_dir/log" \
                '[glyphs] playstation' 'y_coin 64 drawn as playstation'
            check giants "giants-coin: one style per pad family" analyze opens "$run_dir/log" \
                'y_coin 64 drawn as nintendo' 'y_coin 64 drawn as xboxone' 'y_coin 64 drawn as playstation3' '[glyphs] xbox360'
            check giants "giants-coin: the Mash! prompt follows the keyboard" analyze opens "$run_dir/log" \
                '[glyphs] keyboard' 'y_coin 128 drawn as keyboard'
            check giants "giants-coin: Giants' own key binding" analyze opens "$run_dir/log" '[glyphs] keycaps: a=Space b=X x=C y=G'
            echo "== giants: the save read back (50 s)"
            retry=run_giants_save; run_giants_save
            game_checks giants 50 end 20 "${REGRESS_PSNR_GIANTS:-20}" 'Save_Slot_1.xsg'
            check giants "giants-save: the save is read, not rewritten as new" giants_save_read "$run_dir/log"
            echo "== giants: a level of the tour, a Giants figure (60 s)"
            retry=run_giants_level; run_giants_level
            # level_203's opening dialogue and camera move with the run's timing: the frame check uses a set of recorded
            # frames (analyze.py frame), so it keeps the 20 dB of the others.
            game_checks giants 60 end 15 "${REGRESS_PSNR_GIANTS_LEVEL:-${REGRESS_PSNR_GIANTS:-20}}" 'open \level\level_203_the_machine.arc'
            check giants "giants-level: the figure is on the portal" giants_figure_placed "$run_dir/log"
            run_name=""
            check giants "the Toy Box shelf and the player's wheel" wheel_toybox
            echo "== giants: pad remapping and rumble, out of health"
            check giants "pad remapping and rumble strength reach the title" pad_qol giants "$GIANTS_ISO"
            check giants "a figure out of health shows a red cross until the next level" out_of_health giants "$GIANTS_ISO"
            check giants "FSR 3 to the first level (no failure, no guard)" fsr_all giants
            check giants "FSR 3 frame generation to the first level (twice the frames shown)" fsr_framegen giants
            if [ $long -eq 1 ]; then
                echo "== giants: the level tour (--long)"
                check giants "level tour" giants_tour
            fi
        fi
    fi
fi

if [ -z "$only" ] || [ "$only" = swapforce ]; then
    echo "##### Swap Force"
    if [ -z "${SWAPFORCE_ISO:-}" ] || [ ! -f "$SWAPFORCE_ISO" ]; then
        row swapforce "section" SKIP "SWAPFORCE_ISO is not set to the disc image (build-swapforce needs its private/swapforce/ppc too)"
    else
        build_and_test swapforce build-swapforce -DARKEYAN_GAME=swapforce
        if [ $quick -eq 0 ]; then
            echo "== swapforce: title to the tutorial level, an SSA figure ($swapforce_seconds s)"
            retry=run_swapforce; run_swapforce
            # The tutorial's frames move (jumps, the camera, the figure's flash): a set of references, 17 dB (the good
            # build scored 20.3-26.9 dB; the title, a menu, a loading screen, a black frame or another level 8-12 dB).
            game_checks swapforce $swapforce_seconds end 30 "${REGRESS_PSNR_SWAPFORCE:-17}" \
                'open \archives\tutorial_a.pak' 'create save: "SWAPFORCE_0"' 'open \archives\eruptor.pak'
            # The canyon's lava decals need the polygon offset (games/swapforce/docs/shaders.md): without it patches of
            # lava flip from one frame to the next (good build 0.5-0.8 % of the pixels here, without the offset 3.2 %).
            check swapforce "the lava canyon does not shimmer (40 consecutive frames)" analyze shimmer "$run_dir/snap" \
                "$run_dir/frames.txt" 50 1.5
            # A table target scaled with the frame posterizes the lava at scale 2 (good build 1.2, before the fix 6 to 19).
            check swapforce "the lava canyon's gradients are not posterized" analyze banding "$run_dir/snap" 3
            check swapforce "the launcher's video settings reach the game (intro logos skipped, 60 FPS)" play_settings swapforce "$SWAPFORCE_ISO"
            check swapforce "the F3 menu and the problem report (crash located, no personal path)" f3menu swapforce "$SWAPFORCE_ISO"
            toybox_window swapforce
            # A virtual PS5 pad at 8 s: the fonts' button pictures, redrawn in guest memory (host/glyphs.h, MemoryImage).
            check swapforce "glyphs follow the device (PS5 pad, buttons redrawn in memory)" analyze opens "$run_dir/log" \
                '[glyphs] playstation' 'body font buttons 256x83 (guest memory) drawn as playstation' \
                'info font buttons 256x168 (guest memory) drawn as playstation'
            # The title's "PRESS START": START is a picture of the Start button, drawn for the device (engines/alchemy/prompt_text.cpp).
            check swapforce "the title's START is the Start button's picture" analyze opens "$run_dir/log" \
                "START shown as the Start button's picture" "the info font's Start character made"
            frame_set=""
            # A swapper is one card in the Toy Box and both tags go on the portal (~2 min; its own isolated run).
            echo "== swapforce: a swapper through the Toy Box"
            check swapforce "swapper in the Toy Box" swapper_toybox
            # The Toy Box's search field, Filters panel, tags and sort (~2 min; its own isolated run).
            echo "== swapforce: search, filters and sort in the Toy Box"
            check swapforce "search and filters in the Toy Box" search_toybox
            echo "== swapforce: pad remapping and rumble, out of health, Nightmare from the start"
            check swapforce "pad remapping and rumble strength reach the title" pad_qol swapforce "$SWAPFORCE_ISO"
            check swapforce "a figure out of health shows a red cross until the next level" out_of_health swapforce "$SWAPFORCE_ISO"
            check swapforce "FSR 3 to the first level (no failure, no guard)" fsr_all swapforce
            check swapforce "FSR 3 frame generation to the first level (twice the frames shown)" fsr_framegen swapforce
            check swapforce "Nightmare is locked, and offered from the start with the option" nightmare_menu
        fi
    fi
fi

# Trap Team shares the host units, including the disc-driven achievements tests. Its game route still needs validation.
if [ -z "$only" ] || [ "$only" = trapteam ]; then
    echo "##### Trap Team"
    if [ -z "${TRAPTEAM_ISO:-}" ] || [ ! -f "$TRAPTEAM_ISO" ]; then
        row trapteam "section" SKIP "TRAPTEAM_ISO is not set to the disc image (build-trapteam needs its private/trapteam/ppc too)"
    else
        build_and_test trapteam build-trapteam -DARKEYAN_GAME=trapteam
        if [ $quick -eq 0 ]; then
            row trapteam "runtime" SKIP "Trap Team game route and picture references await validation; see the Trap Team tours in docs/getting-started/testing.md"
            toybox_window trapteam
            if [ $long -eq 1 ]; then
                echo "== trapteam: FSR 3 through the chapters (~16 min)"
                check trapteam "FSR 3 through the chapters (no failure, no guard)" fsr_all trapteam
                check trapteam "FSR 3 frame generation through the chapters" fsr_framegen trapteam
            fi
        fi
    fi
fi

# SuperChargers and Imaginators: the host units, then each game's hub test (new game, then continue; they isolate and lock themselves).
for g in superchargers imaginators; do
    if [ -n "$only" ] && [ "$only" != $g ]; then continue; fi
    case $g in
        superchargers) var=SUPERCHARGERS_ISO; disc=${SUPERCHARGERS_ISO:-}; label=SuperChargers; script=superchargers_lobby ;;
        imaginators) var=IMAGINATORS_ISO; disc=${IMAGINATORS_ISO:-}; label=Imaginators; script=imaginators_hub ;;
    esac
    echo "##### $label"
    if [ -z "$disc" ] || [ ! -f "$disc" ]; then
        row $g "section" SKIP "$var is not set to the disc image (build-$g needs its private/$g/ppc too)"
    else
        build_and_test $g build-$g -DARKEYAN_GAME=$g
        if [ $quick -eq 0 ]; then
            echo "== $g: new game and continue to the hub (~9 min)"
            hub_test() { out=$(env "$var=$disc" "$root/tools/regress/$script.sh" "${script}_$$" 2>&1); status=$?; echo "$out" | tail -1; return $status; }
            check $g "to the hub, twice (new game, continue)" hub_test
            echo "== $g: the logo movies are lit (~40 s)"
            check $g "movies are not black" "$root/tools/regress/movies.sh" $g "$disc"
            echo "== $g: FSR 3 to the hub (~9 min)"
            check $g "FSR 3 to the hub (no failure, no guard)" fsr_all $g
            check $g "FSR 3 frame generation to the hub" fsr_framegen $g
            echo "== $g: the F3 panel and its rail pages by pad (~3 min)"
            check $g "the F3 panel and its pages, by pad" f3menu $g "$disc"
            toybox_window $g
            if [ $g = imaginators ]; then
                echo "== imaginators: Nightmare from the start (~4 min)"
                export IMAGINATORS_ISO="$disc"
                check $g "Nightmare is locked, and offered from the start with the option" nightmare_menu imaginators
            fi
        fi
    fi
done

echo
echo "================ regression harness: results ================"
fails=0
for section in ssa giants swapforce trapteam superchargers imaginators; do
    grep "^$section|" "$rows" > /dev/null 2>&1 || continue
    echo "-- $section"
    grep "^$section|" "$rows" | awk -F'|' '{ printf "  %-4s  %-42s %s\n", $3, $2, $4 }'
    fails=$((fails + $(grep -c "^$section|[^|]*|FAIL|" "$rows")))
done
echo "load average at the end: $(loadavg); total time $((SECONDS - start)) s"
skipped=""; [ "$(grep -c '^[^|]*|[^|]*|SKIP|' "$rows")" -gt 0 ] && skipped=" (a section skipped: SKIP above)"
if [ $fails -eq 0 ]; then echo "ALL PASS$skipped"; exit 0; fi
echo "$fails FAIL"; exit 1
