# Testing: isolated runs, the fast recipe, the regression harness, frame times

> **Purpose:** how to run each game without touching the player's files, reach SSA's first level in ~75 s and Swap Force's in 25-60 s, check that a change broke no game (`tools/test_all.sh`), and measure frame times · **Status:** current (2026-10-27) · **Tags:** none (runtime checks); the press timings, level names and references are [SSA], [Giants] or [SwapForce]

Every environment variable, log tag and capture tool: [../porting/debugging.md](../porting/debugging.md).

## Isolation (every game run)

```sh
R=$PWD/private/runs/$NAME; mkdir -p "$R"      # a NEW, EMPTY folder: the menu presses assume no save
SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
SKY_SETTINGS=$R/none.ini SKY_LAUNCHER_INI=$R/none.ini SKY_COLLECTION=$R/collection SKY_SAVE_DIR=$R/save \
SKYLANDERS_DATA=$R/data SKY_LOG=$R/game.log \
nice "./build/ArkeyanRuntime/Skylanders Spyro's Adventure" "$ISO" > "$R/log" 2>&1 & pid=$!     # Giants: "./build-giants/ArkeyanRuntime/Skylanders Giants"
(sleep 90; kill $pid; sleep 3; kill -9 $pid) & wait $pid
```

- `SKY_SETTINGS` / `SKY_LAUNCHER_INI` name a **missing** file: the defaults apply, never the player's `launcher.ini`
  (its `fps_limit=60` once skewed tests; a test run once rewrote it). Check that the `[settings]` line shows defaults.
- `SKY_COLLECTION` is a scratch folder (never the shared `Skylanders/Collection`); `SKY_SAVE_DIR` holds the saves,
  `pipelines.bin` and `portal.txt`; `SKYLANDERS_DATA` moves everything else out of the player's data root; `SKY_LOG`
  keeps the log out of it. Any of them also **skips the data-folder migration**
  ([platforms/data_folders.md](platforms/data_folders.md)).
- `SDL_VIDEO_DRIVER=offscreen` and the disk audio driver: no window, no sound, and `SKY_GPU_SNAPSHOT` can read frames
  back.
- Launcher runs (the runtime's `--launcher` or Arkeyan Launcher): `SKY_LAUNCHER_INI` and `SKY_COLLECTION` scratch paths and
  `SKY_LAUNCHER_NO_UPDATE_CHECK=1`.
- **A kit-built game is not isolated by these variables alone** (2026-10-27): Arkeyan-Builder links the game and runs it once
  headless to collect the movie shaders, and that run inherits Arkeyan-Builder's environment, not the variables above. It once
  wrote `SSARecomp.log` / `.prev.log` (now `Arkeyan.log`) into the player's real data folder (the log's first line named it). Arkeyan-Builder now sets
  `SKYLANDERS_DATA`, `SKY_LAUNCHER_INI` and `SKY_LOG` (scratch files in its work folder) next to the ones it already set,
  and puts the caller's values back after the run. Whoever adds a variable the game reads for a path must add it there too.
  Check: `tools/regress/kit_isolation.sh <ssa|giants|swapforce> <disc image>` ([Xbox], macOS only; ~10 min). It runs a
  kit's Arkeyan-Builder with no scratch variables, then the installed app with them, both under `sandbox-exec` with writes to the
  real `Skylanders`, `SSARecomp` and `Skylauncher` folders denied (a leak shows as "[log] cannot write ..." in a log, never
  as a changed file; macOS reads the home from the passwd entry, so a changed `$HOME` protects nothing). It fails if a log
  names the real folder, if `SKY_LOG` is not the log's first line, or if `find -newer` lists anything in the real folders
  (listed only, never written). `KIT=<dir>` picks the kit (default `dist/skylanders-kit-macos-arm64`, built when missing).
- **The launcher's video settings reach the game** (2026-10-28): `tools/regress/play_settings.sh <ssa|giants|swapforce> <disc>`
  starts the game with a settings file that holds only `[<game>.video]` (`skip_intro_movies=1`, `fps_limit=60`, what the
  game's page writes) and fails unless the `[settings]` line says "fps limit 60, intro movies skipped", every start-up logo
  is logged as `[movie] skipped` (SSA 4, Giants 2, Swap Force 2) and the present runs at fps limit 60. With
  `LAUNCHER=<Arkeyan Launcher>` it also runs the game through `--game <id> --play`. `test_all.sh` runs it for each game
  (~1 min each); it is the only check of the setting, since the harness's runs use `SKY_SKIP_MOVIES`.
- **The F3 menu and the problem report** (2026-10-28): `tools/regress/f3menu.sh <ssa|giants|swapforce> <disc>` faults the game on a
  guest thread (`SKY_CRASH_TEST=fault`) and checks the `[crash]` lines, then starts it again and, through `SKY_TOYBOX_SCRIPT`,
  opens the menu with F3, changes the volume, turns the FPS counter on, writes the report (`SKY_REPORT_DIR`) and closes it. It fails
  unless the settings are in the game's own sections, the report has its parts and the previous run's crash, and no home path,
  account name or host name is in it. `test_all.sh` runs it for each game (~1.5 min each). [gamemenu.md](../runtime/host/gamemenu.md).
- **5.1 sound** (2026-11-07): `tools/regress/surround.sh <game> <disc>` runs the game with `SKY_SPEAKERS=5.1` into the disk
  driver at 6 channels (`SDL_AUDIO_CHANNELS=6`) into a level, then 40 s with the default (auto) on its 2-channel device, and
  fails unless the log shows the layout and the speaker config the title was told, and `tools/regress/surround_rms.py --check`
  finds signal on all six channels, nothing copied, the LFE band-limited. `test_all.sh` runs it for Giants (~3 min).
  [audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md#51-the-speakers-setting).
- **One game at one frame rate** (2026-10-30): `tools/regress/framerate.sh <ssa|giants|swapforce> <disc> <30|60|120|144|240|0> [seconds]`
  runs the test_all recipe with `--vsync 0 --fps-limit N` (offscreen has no display refresh, so the limiter alone paces the
  frames) and prints, for the level and for the menus, the presented rate, the gap p50/p99, the draws a frame, the game's seconds per
  second (`SKY_CLOCK_STEP`, SSA only) and the `[fall]` lines. Numbers taken at a load average above 4 are not comparable.
  Results: [video_vblank.md](../runtime/xbox360/video_vblank.md#above-60-measured).
- **The movies are lit** (2026-11-06): `tools/regress/movies.sh <superchargers|imaginators> <disc image> [seconds]` plays the start-up logos isolated and fails on a missing shader (`[render] no translated ...`), on no `.bk2` opened, or on a brightest frame (every 30th presented frame) with a mean luminance below `MOVIE_FLOOR` (10; a black movie is 0). A dev build has every shader in its cache, so the real check is a kit-installed game (`EXE=<installed game>`); `tools/regress/movie_targets.py` (unit row of every section) fails a game without `movie_shaders` in its `game.txt`. `test_all.sh` runs the first for SuperChargers and Imaginators (~40 s each).
- **The launcher's Toy Box window drives the game** (0.9.4): `LAUNCHER=<Arkeyan Launcher executable> [MIXED=1]
  tools/regress/toybox_window.sh [<game> ...]` (default the six games; a game without its disc or build is a SKIP). Per game the
  launcher plays it (`--game <id> --play`) with "Open with the game" ticked; its window puts a figure on Player 1 and Player 2
  through the link, swaps them, takes one off and fills the game's own seat where it has one; the game's log must show each
  change and the game reading each figure, and the window must close with the game. Pictures: `<run>/<game>/shots` and, beside
  the game's own picture of the same moment, `<run>/<game>/beside`. `MIXED=1` adds the mixed portal (a fake real portal) on
  Giants and Spyro's Adventure. `test_all.sh` runs it per game when `LAUNCHER` is set ([toybox_link.md](../runtime/host/toybox_link.md)).
- **Variable names** (0.6.0): the project's variables are `SKY_*` (`SKY_SETTINGS`, `SKY_LOG`, ...) and the launcher's
  `SKY_LAUNCHER_*`; the older `SSA_*` and `SKYLAUNCHER_*` names still work (`runtime/host/env.h` reads the new name,
  then the old one). Names of one game keep theirs: `SSA_ISO`, `GIANTS_ISO`, `SWAPFORCE_ISO`; `SKYLANDERS_DATA`, `SDL_*`
  and the CMake options (`ARKEYAN_GAME`, ...) did not change. Check: `tools/regress/env_names.sh <ssa|giants|swapforce> <disc>`
  runs the game twice under `sandbox-exec`, once with only `SKY_*` isolation variables and once with only `SSA_*`, and
  fails on a leak (macOS; ~1 min).
- Kill by PID with a timer: never `pkill`/`killall`, never `perl alarm`. One game instance at a time; `nice`, `-j4`.

## Automatic frame-rate fallback

`SKY_FPS_TEST_LATE_MS=40` delays every real present by 40 ms before `WaitForFlip` samples the actual
vblank counter (clamped to 0–100 ms; unset or 0 has no effect). Use only for short isolated runs, with
`fps_limit=60` and `[<game>.video] fps_auto_fallback=1` or `0`. The `[video-test]` line records the delay,
selected limit and switch every 120 presents; on must log `drop to 30 FPS until they fit again` (after 5 s of late
frames out of a load), off must never log it. A file without the key also logs `drop to 30 FPS when needed: on by
default (Apple GPU)` and gets `fps_auto_fallback=1` written.
This is a pacing check, not a performance measurement: the delay intentionally makes the frames late.
The shared decision is covered by `ArkeyanFrameRateFallbackTest` without sleeping or game data.
Wrap the whole run and timer in `../gamelock.sh`, with `3>&-` in any background timer; use the isolation
block above plus `SKY_LAUNCHER_NO_UPDATE_CHECK=1`. Keep all logs/settings under `private/runs/<name>`.

## A fresh clone

What a new clone needs before it builds and runs both games (every item is derived from a disc or downloaded, so none
is in git):
1. The submodules: `git submodule update --init --recursive` (`tools/prepare_game.sh` runs it when they are missing).
2. SSA's game data: `tools/prepare_game.sh "<SSA disc>.iso"` writes `private/default.xex`, `private/image.bin`,
   `private/xexdump.txt` and `private/ssa/ppc/` with its shader cache (~3-5 min; build the cache on the Mac). Or copy
   `private/ssa/` and those files from another clone.
3. Giants' game data: `private/giants/ppc/` (with `shader_cache.cpp`). `prepare_game.sh` does not prepare Giants: copy
   `private/giants/` from another clone, or generate it as in [../porting/day1.md](../porting/day1.md) (XenonRecomp on
   `games/giants/config/Giants.toml`, then `SKY_GAME=giants tools/shaders/build_cache.sh "<Giants disc>.iso" <dump dir>`).
   CMake reads `private/<game>/ppc/` (`ARKEYAN_PPC_DIR`).
4. FFmpeg: a host build builds the minimal LGPL FFmpeg into each new build folder on its first configure (~1 min);
   `-DARKEYAN_FFMPEG_ROOT=<prefix>` reuses one built with `tools/ffmpeg/build.sh`. The Windows and Linux scripts keep
   theirs in `private/ffmpeg/<target>/`.
5. For the harness: `SSA_ISO` and `GIANTS_ISO` (the discs, read only; `SWAPFORCE_ISO` too for the Swap Force section,
   with `private/swapforce/ppc/` and its shader cache: [games/swapforce/docs/boot.md](../../games/swapforce/docs/boot.md)),
   `python3` with numpy and Pillow,
   `private/figures/new_14.sky` (a figure dump the runs place; `SKY_PORTAL_PLACE=new:14` creates one), and the
   references in `private/test-refs/` (`tools/test_all.sh --record` on a good build).

## The fast recipe: SSA to level_027 in ~75 s [SSA]

The isolation block plus:
```sh
PRESS="start:1,start:2,start:3,start:4,start:5,a:6,a:7.5,a:9,a:10.5,a:12,a:13.5,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,b:30,b:32,b:34,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60"
SKY_SKIP_MOVIES=1 SKY_PRESS="$PRESS" SKY_PORTAL=emulated SKY_PORTAL_PLACE=new_14.sky    # new_14.sky in $R/collection
```

What happens (3 runs, shared machine; `SKY_TRACE_FILES=1` shows the file opens):

| Step | Time |
|---|---|
| intro movies (legal, atvilogo, xpeclogo, tfb) end at once, the frontend loads | ~1.3 s |
| START: title -> autosave notice; A: "Skyland Adventures" -> "Start a New Game" -> slot 1 -> save | 17-18.5 s |
| story movie `sl_sc01.bik` opens and ends at once, `levels\level_027\mission.str` opens | 20.8-22.3 s |
| B skips the in-level cutscene ("B Skip"); Hugo's lines play (not skippable, ~40 s) | |
| "This Skylander is visiting from another adventure" dialog (and a save) | 65-73 s |

The run ends on that dialog; A on it ("No") enters play. Kill at 90 s for margin; level frames are drawn from ~22 s.
Why repeated presses: each screen takes the first press it accepts, so a slow screen only shifts the step. A is
harmless during loading and the level cutscene; B means "Back" in the menus, so the B presses start ~8 s after the
level load is expected. `SKY_LEVEL=<level>` starts the new game in another level of the disc (both games).
`SKY_LEVEL` only reads level_027's files from another level's: in SSA that level loads and runs **without a Skylander**
(no hero, no HUD) outside level_027. **`SKY_NEXT_LEVEL=<Name>`** [SSA] makes the game itself go to the level (the
opening movie's `nextLevel`, by its `gamedata.lxb` name: `Level_021`, `Level_010b`, `Level_Hub`, `Challenge_Level_000`),
so the hero and the HUD spawn ([str_levels_loading.md](../engines/lux/str_levels_loading.md#which-level-a-new-game-starts-and-starting-another-one-lux-ssa-v));
`TOUR_MODE=hero tools/pipelines/tour_ssa_story.sh <iso> <work dir> [<level>...]` uses it.

- **Without `SKY_SKIP_MOVIES`** (~77 s): START skips the intros except the 5 s legal screen, B skips the story movie.
  Level load at 36.8 s, dialog at 75-77 s. Less margin: a B earlier than the movie lands in the slot menu.
  `SKY_PRESS="start:2,start:3,start:4,start:5,start:6,start:7,start:8,start:9,start:10,start:11,start:12,start:13,start:14,a:15,a:16.5,a:18,a:19.5,a:21,a:22.5,a:24,a:25.5,a:27,a:28.5,a:30,a:31.5,a:33,a:34.5,b:36,b:38,b:40,b:42,b:44,b:46,b:48,b:50,b:52,b:54,b:56,b:58,b:60,b:62,b:64,b:66"`
- **The old recipe** (reference): `start:40,a:45,a:57,a:62,a:67,a:72,b:150,b:160`: level load at 134 s, dialog ~190 s.
- **The sun's light tiles at a size and scale** [SSA]: `SSA_ISO=<disc> tools/regress/ssa_grass.sh <name> <WxH> <scale>
  [metal|vulkan]` (wrap it in `gamelock.sh`; `vulkan` takes `build-vk`, `-DARKEYAN_MACOS_VULKAN=ON`) runs this recipe, A on the
  dialog, the hero walking a square so the camera moves, keeps a frame a second from 60 to 110 s and scores them with
  `tools/regress/grid_score.py` (the luminance step across the 32-scene-pixel tile edges over the step elsewhere,
  1.0 = no grid, and the tile edges that look like a block's side): `private/runs/<name>/score.txt`
  ([plume_backends.md](../runtime/gpu/plume_backends.md)).
- **Giants**: the harness's presses (`tools/regress/giants_press.py`: A every 4 s through the title and menus, the
  tutorial playable at ~40 s, then walking, attacking, the pause menu).
- **Swap Force**: below.

## Swap Force to the tutorial in 25-60 s [SwapForce]

The isolation block (`"./build-swapforce/ArkeyanRuntime/Skylanders Swap Force"`) plus:
```sh
PRESS=$(awk 'BEGIN { for (t = 14; t <= 50; t += 2) printf "%sa:%d", (t > 14 ? "," : ""), t }')   # a:14,a:16,...,a:50
SKY_SKIP_MOVIES=1 SKY_PRESS="$PRESS" SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:9    # Eruptor, an SSA figure
```

Four presses are needed, all A: the title ("PRESS START"), "Story Mode", the first "NEW" slot, the difficulty ("Easy").
The logos end at once with `SKY_SKIP_MOVIES`, and so does the opening movie `fmv01`. What happens [V] (2026-10-27, ~20
runs; the times move with the machine's load):

| Step | Quiet machine (load 3-5) | Loaded (load 50-100) |
|---|---|---|
| the title is up (`[render]` ~355 draws a frame) | ~15 s | 21-40 s |
| title -> `create save: "SWAPFORCE_OPTIONS"`; Story Mode -> the same again; NEW slot; difficulty -> `create save: "SWAPFORCE_0"` | 20-27 s | 30-41 s |
| `open \archives\tutorial_a.pak`, `fmv01` skipped, the level loads | ~23 s | 37-44 s |
| the level plays (Eruptor on Flynn's ship, ~400 draws a frame) and the game saves (`SWAPFORCE_0` again) | ~25 s | 43-49 s |

Why an A every 2 s and not four exact times: the title comes up 15 to 40 s after the start, and a menu ignores a press
while it slides in (an A 4 s after the title's was dropped once, the next one 2 s later taken). Presses on a loading
screen are ignored; in the level A jumps. On a quiet machine (load ~7) `a:16,a:20,a:22,a:24` is enough: the tutorial
opens at 26.6 s and plays at 29 s. The Greeble ships' cutscene comes ~58 s in (it froze there until 2026-10-27:
[boot.md](../../games/swapforce/docs/boot.md), "XMA"); a 175 s run reaches the next zone's loading screen.
**A portal is needed** [V]: with `SKY_PORTAL=none` the title's A gives `[content] device selector`, two
`SL_LOAD_SLOTS Thread` lines and then the game's own "PORTAL DISCONNECTED" screen (it waits for a portal; headless or
not). `SKY_PORTAL=emulated` with an empty collection is enough. Under a heavy load the title used to stop after one
`SL_LOAD_SLOTS` line: a race in the game's save tasks, fixed 2026-10-27 ([history.md](../../games/swapforce/docs/history.md),
"The stop at the save slots"; the fix logs `[save] the task's thread ended before ...`).

**Another level** (`SKY_LEVEL=<area>/<map>` or a full zone name): a zone of `zoneinfos.pak` (`maps\<area>\<map>\`), e.g.
`SKY_LEVEL=barrens/brn1` (Boulder Brush: `brn1.pak`, its intro movie skipped, Eruptor in the level at ~30 s on a quiet
machine) or `multiplayer/pvp/pvp_sandcastle/pvp_sandcastle`, with the same presses ([debugging.md](../porting/debugging.md), `SKY_LEVEL`).
A survival arena (`multiplayer/survival/...`) gets Solo Survival's game mode. `barrens/brn3` and `wildwoods/ww1` need
`SKY_SKIP_MOVIES=0` and ~200 s: their intro movie plays inside a cutscene that a skipped movie stalls (black screen).

**The zone tour** (`tools/pipelines/tour_swapforce.sh <iso> <work dir> [<zone>...]`, ~50 s a zone, 195 zones ~2.7 h,
one game at a time): every zone of the disc (`tools/pipelines/swapforce_zones.py`) in its own isolated run, Eruptor, A
every 2 s to 22 s then walking and attacking, 40 s (`TOUR_SECONDS`; `brn3` and `ww1` with their movies,
`TOUR_MOVIE_SECONDS`, 240 s); `<work>/summary.tsv` has one line per zone (opened,
draw-time builds, faults, fps, draws, verdict `ok`/`fault`/`hang`/`no level`/`no frames`/`black`, brightness), the
logs are in `<work>/logs/`, the merged pipeline list in `<work>/merged.bin` (`zstd -19 --rm` it into
`games/swapforce/pipelines/baseline.zst`). A zone that never opens is run once more (`TOUR_RETRY=0`: not);
`TOUR_EXE` runs a copy of `Skylanders Swap Force` (rebuild meanwhile); `TOUR_KEEP=1` keeps each run's folder (`frames.txt` for
`tools/perf/frames.py --from 40 --to 85`, with `TOUR_SECONDS=90` and `SKY_PERF_LOG=1`: the performance runs of
[history.md](../../games/swapforce/docs/history.md), "Zone tour"). Results: that page.

Other recipes:
- **FSR 3 in every game**: `tools/regress/fsr_all.sh [<game>...]` (the discs' variables set; ~2 min per game for SSA, Giants
  and Swap Force, the route scripts for Trap Team, SuperChargers and Imaginators, ~9-16 min): `SKY_UPSCALER=fsr3` at a
  2560x1440 window to the first level; FSR's context made, never failed, no `[guard]`, frames written. Without a game:
  `ArkeyanRecompGpuTest` (with the isolation block) checks FSR on the running backend ([upscaling.md](../runtime/gpu/upscaling.md)).
- **Battle Mode, two players** [SSA]: `SSA_ISO=<disc> tools/regress/ssa_arena.sh <name> [<mode keys>] [<arena index>]` (~4 min,
  `build/`, isolated under `private/runs/<name>`): the game's own menus steered by `SKY_PRESS_FILE` (the menus belong to
  Player 2, the pad that pressed A at the title: lines `2 <button>`; Player 1 picks its Skylander and fights), the two figures
  from `SKY_PORTAL_PLACE="new_14.sky,new:9"`. Prints the figures placed, the `pvp_level_NNN` opened and the fault count; the frames
  are `shot_N.ppm`. `SKY_LEVEL` cannot replace it: a pvp or challenge level started that way has no Skylander in it
  ([games/ssa/docs/100.md](../../games/ssa/docs/100.md), "Modes").
- **The portal prompt** [SSA]: no `SKY_PORTAL_PLACE`, `SKY_COLLECTION` holding `new_14.sky`, the presses up to `a:27`
  only (a `b` would close the Toy Box), `SKY_TOYBOX_SCRIPT="Return:92"`, `SKY_PORTAL_LOG=1`, ~120 s: `[toybox] prompt
  for player 1`, `P1: Gill Grunt`, `the prompt is gone`, `closed`, `the game took Gill Grunt`
  ([../runtime/host/portal.md](../runtime/host/portal.md)).
- **A villain captured in play** [TrapTeam]: `TRAPTEAM_ISO=<disc> tools/regress/trapteam_capture.sh` (~25 min, `build-trapteam/`, isolated under `private/runs/trapteam_capture`): the recorded route (`trapteam_capture.route`) from the tutorial's start to Sheep Creep on the game's frame clock (`SKY_TOYBOX_CLOCK=frames`), the capture into an empty Life Trap, then a restart that loads the held villain ([boot.md](../../games/trapteam/docs/boot.md#the-first-capture-2026-11-02-v)).
- **The first chapters** [TrapTeam]: `TRAPTEAM_ISO=<disc> tools/regress/trapteam_chapters.sh` (~16 min, `build-trapteam/`, isolated under `private/runs/trapteam_chapters`): a fresh save from the logos through the opening movie (ended with Start) into the tutorial, a relaunch through "Select a save slot" and Continue Game, then chapters 2 and 3 and the Academy through `SKY_LEVEL` with a scripted walk; log checks, no fault ([010.md](../../games/trapteam/docs/010.md)).
- **A swap with a trap on the portal** [TrapTeam]: `TRAPTEAM_ISO=<disc> tools/regress/trapteam_swap.sh` (~3 min, `build-trapteam/`, isolated under `private/runs/trapteam_swap`): the Toy Box, then the wheel; each figure read, no sound block taken for a command.
- **A swapper through the Toy Box** [SwapForce]: `SWAPFORCE_ISO=<disc> tools/regress/swapper_toybox.sh` (~2 min,
  `build-swapforce/`, isolated under `private/runs/swapper_toybox`; also the last row of `tools/test_all.sh`'s Swap Force
  section): a linked swapper and a loose bottom in a scratch collection, the Toy Box at the portal prompt puts the
  swapper card on P1 (both tags: slots 0 and 1), takes it off, puts it back, then "Change the bottom"; the log lines it
  checks are listed at the top of the script. `SWAPPER_ART_DATA=<scratch data folder>` (where `ArkeyanArtTest` ran with
  `SWAPFORCE_ISO`, or the launcher read the disc) copies the halves' pictures in, and the run checks the stacked picture
  (`[toybox] swapper picture ...: stacked`); `search_toybox.sh` takes it too.
- **Search, Filters and sort in the Toy Box** [SwapForce]: `SWAPFORCE_ISO=<disc> [LANGUAGE=fr] tools/regress/search_toybox.sh`
  (~2 min, `build-swapforce/`, isolated under `private/runs/search_toybox_<en|fr>`): 14 dumps (two swappers) in a scratch
  collection; at the portal prompt it types into the field ("fire", "niveau 20", a typo), opens Filters by the pad's Start,
  sets Fire, Swap Force and Sort by Name, removes a tag by click and Clear all, and saves the window's screenshots
  (`shots_*.png`); the log's `[toybox] shelf: N found for ...` lines are what it checks.
- **The Toy Box's focus** [TrapTeam]: `TRAPTEAM_ISO=<disc> tools/regress/toybox_nav.sh` (~75 s, `build-trapteam/`, isolated under
  `private/runs/toybox_nav`): ten scratch figures, keys, Tab and the pointer through `SKY_TOYBOX_SCRIPT`; each scripted key logs
  `[toybox] script: focus: ... home <row> <place> seat <n>`, and 31 checks read them (rows, the kept column, remembered places,
  LB/RB, no wrap, the pointer) ([toybox_ux.md](../runtime/host/toybox_ux.md), "Navigation"); 12 shots `shots_*.png`.
- **Pad remapping and rumble** [every game]: `tools/regress/pad_qol.sh <ssa|giants|swapforce> <disc>` (~1 min a game, isolated under
  `private/regress/pad-qol-<game>`): a virtual Xbox 360 pad holds RB, LT and D-pad up while `SKY_INPUT_LOG=1` logs what the title reads, with the
  game's `[<game>.input]` giving RB B's job, LT A's, D-pad up RT's and Player 1 strong rumble; then a run with no settings. Details:
  [input.md](../runtime/xbox360/input.md), "Remapping and rumble". In `test_all.sh` (each game's section).
- **Out of health** [every game]: `tools/regress/out_of_health.sh <ssa|giants|swapforce> <disc>` (~1 min, `private/regress/out-of-health-<game>`):
  the Toy Box's `prompt=1d` and `level` test aids; the first shot must hold the cross's red and the second none
  ([toybox_ux.md](../runtime/host/toybox_ux.md), "Out of health"). In `test_all.sh`.
  **Timing-sensitive** (2026-11-05) [V]: the first shot is taken at a fixed 30 s, 2 s after the prompt; under load the
  Toy Box's opening (it reads 81 portraits) can end just before it, and the shot reads back the last presented frame,
  still the game's (one full `test_all` at load ~20: `shots_out.png` the title screen, the log's `[toybox] open` before
  `shot=out`). The same build alone: 4 of 4 passes, 1892 red pixels then 0.
- **Nightmare from the start** [SwapForce] [Imaginators]: `SWAPFORCE_ISO=<disc> tools/regress/nightmare_menu.sh` or
  `IMAGINATORS_ISO=<disc> tools/regress/nightmare_menu.sh imaginators` (~4 min, `private/regress/nightmare/<game>/{off,on}`): the real
  difficulty menu of a new game, Nightmare locked, then offered by `SKY_NIGHTMARE_FROM_START=1` and accepted (Swap Force's tutorial, Imaginators'
  first level starts); `menu.ppm` of each run is the frame to look at. In `test_all.sh` (the Swap Force and Imaginators sections).
- **The shelf and the player's wheel** [Giants]: `GIANTS_ISO=<disc> tools/regress/wheel_toybox.sh` (~1.5 min, `build-giants/`, isolated under
  `private/runs/wheel_toybox`; the figure tool comes from `build/`): Spyro, Whirlwind and a Swap Force Scratch (Giants does not read it:
  not on the shelf); Spyro is added to the wheel from its page, Whirlwind is put on Player 1, then Tab held, `1` and Tab released put Spyro
  on it. The log lines are what it checks; look at `shots_home.png`, `shots_versions.png` (the plain toy then only Giants' versions) and
  `shots_wheel.png`. In `test_all.sh` (Giants section).
- **A HUD shot with a Giants figure** [Giants]: `-DARKEYAN_GAME=giants`, `SKY_PORTAL_PLACE=new:100:1206` (a never-played
  LightCore Jet-Vac), `SKY_GPU_SNAPSHOT=<prefix> SKY_GPU_SNAPSHOT_FROM=3000 SKY_GPU_SNAPSHOT_EVERY=450`, presses
  `a:20,a:30,...,a:130`. A figure at level 15 with a hat: [../runtime/host/figures.md](../runtime/host/figures.md).
- **Strict Vulkan drivers** [SSA]: a macOS test build on Vulkan (MoltenVK) with the Khronos validation layer,
  `tools/regress/vulkan_validation.sh <name> <seconds> "<presses>"` (`SKY_LEVEL`, `SYNC=1` for synchronization
  validation; summary of the layer's messages by kind in `private/runs/<name>/summary.txt`); build and results:
  [../runtime/gpu/plume_backends.md](../runtime/gpu/plume_backends.md#strict-vulkan-drivers-the-validation-layer-run-v-2026-10-28).
- **Glyphs**: `SKY_GLYPHS_SCRIPT` ([../runtime/xbox360/input.md](../runtime/xbox360/input.md), "Glyphs").

## SuperChargers to the Academy, new game and continue [SuperChargers]

`SUPERCHARGERS_ISO=<disc> tools/regress/superchargers_lobby.sh [<run name>]` (about 11 min of game time, through `../gamelock.sh`):
a new game from the title through the menus, the opening movie, the intro and its rift tunnel ride (Fiesta `new:3400` and Crypt
Crusher `new:3227` on the emulated portal, RT held; the game waits for it) to the Academy with `SKY_LEVEL=academy`, then a second run
on a copy of its saves that continues into the Academy with no `SKY_LEVEL`. Each run checks the Academy's archive and zone, no fault
(`game.log`, the stubs against `private/test-refs/superchargers/`), the fps floor (`REGRESS_FPS_FLOOR`, 25), the sound, the save and
the last shots ([games/superchargers/docs/boot.md](../../games/superchargers/docs/boot.md) "SKY_LEVEL and the test"). Not yet a
section of `tools/test_all.sh`.

**The player's owner id across sessions** (Arkeyan Launcher issue #28): `SUPERCHARGERS_ISO=<disc> tools/regress/superchargers_owner.sh
[<run name>]` (about 10 min): the same new game registers Fiesta and Crypt Crusher (`--make` dumps) with the save's owner id, then a
second session on that save and those dumps must log the same id, write no owner on them, and the picture
`private/runs/<name>-continue/shot_collection.png` shows the game's collection (Portal Master > Collection > Skylanders: Undead 1)
([figures.md](../runtime/host/figures.md) "Owner").

## The regression harness (`tools/test_all.sh`)

One command says whether a change broke SSA, Giants or Swap Force. **Run it before every merge.**

```sh
export SSA_ISO="<SSA disc>.iso" GIANTS_ISO="<Giants disc>.iso"   # read only; no default path in the script
export SWAPFORCE_ISO="<Swap Force disc>.iso"                      # optional: without it the Swap Force section is a SKIP row
tools/test_all.sh                 # builds, unit tests, then the game runs (SSA 1, Giants 3, Swap Force 1)
tools/test_all.sh --quick         # builds and unit tests only
tools/test_all.sh --only ssa      # (or giants, swapforce, trapteam) one section; unknown/missing names exit 2
tools/test_all.sh --long          # also SSA with a level-15 Giants figure (120 s), one toy of each kind through tools/regress/ssa_figures.sh (5 runs), and Giants' whole level tour
tools/test_all.sh --record        # on a good build: write the allow-lists and reference frames instead of comparing
```

Exit code 0 = all PASS. Output: one table per game (PASS or FAIL with the reason), the load average at the start and
the end, the total time. Logs and run folders: `.scratch/regress/` (local scratch; never share it). Times (2026-10-25, M1 Pro, builds up
to date): `--quick` ~10 s (a cold build of both games ~15 min more); the full run ~8 min.

Trap Team has its own section: set `TRAPTEAM_ISO` and provide its translated code in `private/trapteam/ppc/`.
It builds `build-trapteam/` and runs the shared units with that disc, including both achievements tests; without the
disc the section is a visible `SKIP`. Its game route and picture references remain unvalidated, so a full section
shows a runtime `SKIP` instead of claiming game coverage. `--quick --only trapteam` runs the build and units only.

**Builds and unit tests.** `build/` (SSA) and `build-giants/` (`-DARKEYAN_GAME=giants`) are configured Release when
missing and built with `--target all` plus every unit test by name (the `EXCLUDE_FROM_ALL` ones too, so a stale or
non-linking test fails here). Then each test runs in a scratch folder with the isolation variables:
`ArkeyanPortalTest ArkeyanPortalAsyncTest ArkeyanFigureTest ArkeyanArtTest ArkeyanCollectionTest ArkeyanSettingsTest ArkeyanDatadirTest
ArkeyanSavesTest ArkeyanGuestFormatTest ArkeyanProgressTest ArkeyanAchievementsKernelTest ArkeyanGuestThreadTest ArkeyanGuestLayoutTest ArkeyanTexturePackTest ArkeyanRecompGpuTest` in every build folder (the two achievements
tests read that section's own disc), and `ArkeyanRuntimeLauncherTest` in SSA's only (it checks SSA's disc and game entry; it needs `SSA_ISO`). What each test checks:
[../porting/debugging.md](../porting/debugging.md#unit-tests).

**The game runs.** Each run uses the isolation variables (plus `SKY_FIGURES`) in its own folder, the emulated portal
with `new_14.sky`, `SKY_SKIP_MOVIES=1`, `SKY_TRACE_FILES=1`, `SKY_RENDER_STATS=1`, `SKY_FRAME_TIMES`, `SKY_AUDIO_DUMP`,
`SKY_GPU_SNAPSHOT` every 60 frames, and `--vsync 0 --fps-limit 30 --anisotropy 4`. The harness stops the game by PID.

| Run | Seconds | What it plays | Extra checks |
|---|---|---|---|
| SSA | 90 | the fast recipe, `SKY_LEVEL=level_027`; `keys.ini` (Giants' own Y key) | SSA keeps its keys (`keycaps: a=Space b=X x=C y=V start=Enter back=Bksp`) |
| Giants 1 | 100 | title to Chapter 1's tutorial and play (`giants_press.py`), an SSA figure (Gill Grunt), a virtual PS5 pad at 8 s (`SKY_GLYPHS_SCRIPT=ps5:8`) | glyphs follow the pad (`[glyphs] playstation`, `a_button 64 drawn as playstation`, `font atlas 1024 drawn as playstation`); the save is written (`Save_Slot_1.xsg` not empty) |
| Giants 2 (`giants-save`) | 50 | the first run's save folder again | the slot is opened and the new-game movie `fmv01` does not play again |
| Giants 4 (`giants-coin`) | 170 | `SKY_LEVEL=level_213_tutotial`, a Giant (Swarm, `new:101`): A through the cutscenes, the stick right to the rock, Y every 2 s from 142 s lifts it; `SKY_GLYPHS_SCRIPT=ps5:8,switch:100,xboxone:108,ps3:116,xbox360:124,key:141`; `keys.ini` (`[giants.input] key_y=G`) | the rock's coin as a PlayStation button (`y_coin 64 drawn as playstation`), one style per family (`drawn as nintendo`, `xboxone`, `playstation3`, `[glyphs] xbox360`), then after the key press the "Mash!" prompt's coin as a keycap (`[glyphs] keyboard`, `y_coin 128 drawn as keyboard`) with Giants' own key (`keycaps: a=Space b=X x=C y=G`) |
| Giants 3 (`giants-level`) | 60 | `SKY_LEVEL=level_203_the_machine`, a never-played LightCore (`SKY_PORTAL_PLACE=new:100:1206`) | the figure is placed (toy type 100) and the game lights the portal |
| Swap Force [SwapForce] | 100 | `build-swapforce/` (`-DARKEYAN_GAME=swapforce`); title to the tutorial (A every 2 s from 14 to 50 s, "Swap Force to the tutorial" above), Eruptor (`SKY_PORTAL_PLACE=new:9`), through the Greeble ships' cutscene | the fonts' buttons follow the PS5 pad plugged at 8 s (`(guest memory) drawn as playstation`); the title's START is a picture (`START shown as the Start button's picture`, `the info font's Start character made`) |
| `--long`: SSA | 120 | the fast recipe with a level-15 Giants figure (`ArkeyanFigureTest --make`, xp, hat, gold) | the figure's Giants data (extra areas) comes back byte for byte |
| `--long`: Giants tour | ~3 h | every level of the disc (`tools/pipelines/tour_giants.sh`) | faults, stops and levels not reached against `private/test-refs/giants/tour.txt` (one level per line, `#` comments give each group's reason; `--record` rewrites it without them) |

Checks on every run:

| Check | What it needs to pass |
|---|---|
| **launcher update from published macOS releases** | `bash tools/regress/launcher_update.sh`: local `private/update/{v0.6.1,v0.7.0,candidate}/ArkeyanLauncher-macOS.zip`; proves both old versions update to 0.8.0 and restart, preserving settings, fake install records, collection and desktop shortcuts; wrong checksum and missing asset must refuse and leave the old app running. No game or network; scratch homes under `private/update/run/`, screenshots under `private/update/shots/`; requires macOS, `sandbox-exec` and Apple Vision via `xcrun swiftc`. Exit 2 means isolation blocked, with no launcher started; clicking Restart now and the non-writable-folder DMG fallback stay manual. |
| **Windows kit install and Vulkan picture** | `WINDOWS_KIT="<Windows kit>" WINDOWS_TOOLCHAIN="<extracted Windows llvm-mingw>" bash tools/windows/install_test.sh <game> "<disc>.iso" .scratch/windows-install`: the kit's own `Arkeyan-Builder.exe` under Wine in the existing `ssarecomp-d3d12-test` image, `--jobs 2`, then its installed runtime on Vulkan for 180 s (`RUN_SECONDS`, minimum 120). Requires `@done`, actual run-time containers and their translated cache entries at least `movie_shaders` from the kit's `game.txt`, zero `no translated` lines, and two recent nonblack presented images including one within 15 s of the end (centre luma >= 3, >= 1% pixels above 16, range >= 16). A known Rosetta `invalid gdt selector` crash resumes once across install/play, prints `@warn` and preserves attempt logs; a second crash fails. Requires a new/empty output below `.scratch/`, an existing image and compiler; downloads nothing. `INSTALL_SECONDS` defaults to 3600. Synthetic proof: `python3 -I tools/windows/install_test_test.py`; real Wine/GPU proof is still required. |
| **build** | `build/` (SSA) and `build-giants/` (`-DARKEYAN_GAME=giants`) build with every unit test target, the `EXCLUDE_FROM_ALL` ones included (they are named explicitly, so a stale or non-linking test fails here) |
| **unit tests** | `ArkeyanPortalTest ArkeyanPortalAsyncTest ArkeyanFigureTest ArkeyanArtTest ArkeyanCollectionTest ArkeyanSettingsTest ArkeyanDatadirTest ArkeyanSavesTest ArkeyanGuestFormatTest ArkeyanGameDataTest ArkeyanFileListTest ArkeyanTexturePackTest ArkeyanRecompGpuTest` in both build directories (and Swap Force's), plus `ArkeyanProgressTest` and `ArkeyanAchievementsKernelTest`, which read each section's own disc (the achievements' counts and gamerscore: SSA 38, Giants 45, Swap Force 39, 1000 each); `ArkeyanRuntimeLauncherTest` in SSA's (it checks SSA's disc and game entry; it needs `SSA_ISO`). The row shows each test's own "N checks, M failures" |
| **run** | the game is still running when the harness stops it (an early exit is a crash or a quit: FAIL with the log's last lines) |
| **no fault, [guard] or [stub] regression** | no `Fault (`, `Indirect call to unrecompiled`, SIGSEGV/SIGBUS/SIGABRT line; every `[guard]` target and `[stub]` name already in `private/test-refs/<game>/{guards,stubs}.txt` |
| **reaches the expected point** | SSA: `level_027\mission.str` opened; Giants 1: `\level\level_213_tutotial.arc` (Chapter 1); Giants 2: `Save_Slot_1.xsg`; Giants 3: `\level\level_203_the_machine.arc`; Swap Force: `open \archives\tutorial_a.pak`, `create save: "SWAPFORCE_0"` (the new game) and `open \archives\eruptor.pak` (the figure read from the portal and loaded) |
| **fps and hang** | at least `REGRESS_FPS_FLOOR` (27) fps, SSA over level +10..+40 s, Giants and Swap Force over the last seconds of the run; a hang is a last frame more than 12 s before the end (the end counted from the first present: `SKY_FRAME_TIMES` starts there, the harness notes the second it appears) |
| **frame close to the reference** | one of the last 20 snapshots (a frame every 60 frames, `SKY_GPU_SNAPSHOT`) has a PSNR of at least 20 dB (`REGRESS_PSNR_SSA`, `REGRESS_PSNR_GIANTS`, `REGRESS_PSNR_GIANTS_LEVEL`) against a reference on a 160x90 greyscale reduction. SSA's last scene is the "visiting from another adventure" dialog, Giants' is Chapter 1 with Gill Grunt standing still: both steady, one reference (`private/test-refs/<game>/frame.png`). `giants-save`, `giants-level` and Swap Force (jumps, the camera, the figure's flash; 17 dB, `REGRESS_PSNR_SWAPFORCE`) show scenes that move with the run's timing (dialogue, opening camera): their reference is a **set** (`frame.png` + `frame_NN.png`, up to 16) and the best of the 20 snapshots x the set counts. Swap Force's canyon has a lava vortex that moves the whole sky: two snapshots of one run a second
apart are only 12-17 dB apart, so the best match moves with the run's timing (four runs of one build, 2026-11-05:
18.2, 20.3, 19.4 with the write watch off, and 19.1; earlier 21.8 and 22.5), and its PS5 pad (plugged at 8 s by design)
draws PlayStation buttons where the 2026-10-27 references have the Xbox ones. See "Frame references" below. A snapshot that cannot be read (truncated -- the game was stopped while writing it) is skipped and named on a line of its own, and the check uses the others; if none can be read it fails with "no shot can be read" |
| **glyphs follow the device (PS5 pad)** | Giants only: the run plugs a virtual PS5 pad at 8 s (`SKY_GLYPHS_SCRIPT=ps5:8`); the log shows `[glyphs] playstation`, `a_button 64 drawn as playstation` (the title's A) and `font atlas 1024 drawn as playstation` (the menus' prompts; the 512 atlas is only drawn before 8 s, on the legal screen and the title) |
| **audio does not repeat** | on the `SKY_AUDIO_DUMP` capture: at least 100 non-silent blocks and at most 1 % of 1024-sample blocks identical to the previous one (the Giants cadence bug's signature: 75.8 % before the fix, 0.0 % after) |

**The references** (`private/test-refs/<game>/stubs.txt` and `guards.txt`, `private/test-refs/<run>/frame.png`) come from the game, so they
are never in git: a new clone has none and the checks FAIL with "run once with --record". Record them from a good build,
and again when a change legitimately alters a scene or adds a `[stub]`/`[guard]` (read the allow-list diff first: a new
stub is a regression until understood). Frames are compared by content, not by index.

**The machine-load guard.** When the fps check fails and the load average is at least `REGRESS_HIGH_LOAD` (default:
twice the logical cores), the run is repeated once ("running once more for the fps check", a "second run, load N"
row); a second failure fails. With a low load the fps check fails at once.

**Frame references** [V] (2026-10-26). Before, `giants-save` scored 29.2 dB then 17.0 dB against the same single frame, and
`giants-level` 19-27 dB (its threshold had been lowered to 15, which only catches a black frame). The scene moves, so one
frame is the wrong reference. Now (`analyze.py frame`): the two moving runs compare with a set of frames recorded from
several runs, and keep 20 dB; a broken or black frame scores under 10 dB against every frame of the set, so the check
stays meaningful. `--record` **adds** the run's distinct frames (every 4th of the last 20, skipping any within 35 dB of one
already in the set); record 3 times on main's code as it is, and `rm private/test-refs/<game>/frame*.png` first to start a
set over. Proof (3 full runs in a row, all PASS): `giants-save` 47.4 / 24.7 / 27.1 dB, `giants-level` 33.2 / 27.0 / 30.8 dB (min 20); SSA 23.0-32.0 dB.
SSA and Giants' first run use `--single` (the old check: `frame.png` only, replaced at each `--record`).
Swap Force [SwapForce] [V] (2026-10-27): a set of 15 frames from 3 `--record` runs (the ship's deck, Eruptor, "Press A to
Attack", the figure's flash); then 2 runs PASS at 20.3 and 26.9 dB, 29.6 fps, 0.00 % repeated audio, 18 stubs (all
start-up `Vd*`, `KeInitializeDpc`, `XamUserReadProfileSettings`...), no guard. Wrong scenes score 8-12 dB against the set
(title 9.6, legal 8.6, loading 11.6, a menu 10.0, black 8.2, another level 11.6), so the threshold is 17 dB.

Proof it fails (2026-10-25): with `RedirectLevel` (`kernel/files.cpp`) sending `mission.str` to a missing file (a
scratch commit), `--only ssa` gave 3 FAIL and exit 1 ("missing in the log: level_027\\mission.str", "no level frame
(>= 1000 draws)", "PSNR 11.1 dB vs the reference (min 20)"). On the good build: SSA 29.8 fps, PSNR 30.4 dB; Giants
0.00 % repeated audio blocks, PSNR 35.5 dB. Its first run found that `ArkeyanRuntimeLauncherTest` did not link (it lacked
`host/collection/datadir.cpp`).

## Coverage of every game (`coverage_all.sh`)

`. ../isos.env; tools/regress/coverage_all.sh <work dir below private/> [--games a,b] [--fsr] [--resume] [--jobs N] [--play S] [--only <regex>]`
runs the whole catalogue of the six games, unattended: each game's catalogue is read from its disc
(`tools/regress/coverage_catalogue.py`: every level or zone, hub, Heroic Challenge, bonus mission, arena, race track,
mode and the credits; every toy type of its accept list in `resources/collection/catalogue.json`, swappers paired, every
trap and every villain in a trap of its element), then each entry is one isolated run (`tools/regress/coverage_run.py`,
the isolation block above, under `../gamelock.sh`, `--jobs` at once, default 2; a slot taken serves up to 8 runs
in a row for `COVERAGE_LEASE` seconds, 600, `tools/regress/coverage_lease.py`, so a busy lock's queue is waited for
once per lease, not once per entry). A run is driven live: the game's
menus, the entry's own proof line awaited in the log (the level's file opened, `[level] <zone> starts`, the challenge's
mesh, the arena's mission), then `--play` seconds (30; Swap Zone pieces and lockpick puzzles 20; an entry that cannot count as covered, 15) with
the hero moving (the stick, attacks, jump, every 4 s), then the game stopped by its own PID: shorter than 45 s so that
the ~500 entry runs and ~1100 figures fit in about 7 hours on two slots. Figures go 20 to a run
(`COVERAGE_FIGURE_BATCH`): the portal cleared and the figure's dumps placed by the Toy Box script
(`place=`/`remove=all`), the game's read of the slot awaited, 8 s of play each (`COVERAGE_FIGURE_SECONDS`).
Routes per game: SSA's levels start the game's way (`SKY_NEXT_LEVEL`, the hero spawns) and its Battle Mode goes through
the menus with two pads (`ssa_arena.sh`'s presses); Giants' Heroic Challenges are chosen at Cali on the hub with a save
that unlocks 39 of the 45 (`giants_unlock.py`, made once per work dir; an entry past her list is not reached); SuperChargers continues a save made once (a new
game to the Academy) and renames the Academy's request (`SKY_LEVEL_FROM=academy/academy`: ~25 s to any zone instead of
~320 s); its race tracks go through the racing menus, Single Race, at fps_limit 30 (route `scrace`: the
track's pack trophy, a vehicle of its type, the map's cursor moved by the stick; at 60 its races fault, office/STATE.md); the credits are SSA's two movies swapped in (`SKY_MOVIE_SWAP`) or the credits level until the next level loads.

Verdicts (`<work dir>/<game>/coverage.tsv`: kind, name, verdict, lowest_fps, detail, area, seconds): `ok` (reached,
played, no fault, `[guard]` line or hang), `crash` (a fault or `[guard]` line, or the game ended by itself), `hang`
(no new frame for 45 s), `not_reached` (with why; also every entry whose catalogue row has a *cap*: it ran, for its
faults and frame rate, but not the game's way, e.g. a challenge loaded alone without its challenge state), and
`not_testable` (no run; the catalogue's reason). A run that never shows its proof line is run once more. `--resume`
keeps the rows already written (a stop with Ctrl-C or `kill` stops the games in progress); a figure batch cut by a
crash leaves its later figures for a new batch. Kept per entry: the log lines that matter (`logs/`, 300 lines for a
crash or hang) and one frame (`shots/`); each run's folder is deleted. The end prints one line per game and the wall
time (also in `<work dir>/summary.txt`). `--fsr` adds `SKY_UPSCALER=fsr3` to every run. On macOS the run holds off idle sleep (`caffeinate -i`): a Mac that slept for 6 hours in the first run (2026-11-07) froze every game in progress, whose entries then timed out and ran again.

The report: `tools/regress/status_report.py <work dir>/*/coverage.tsv --catalogue <work dir> --platform "<name>"
--build <id> --out docs/status/STATUS.md --coverage docs/status/COVERAGE.md`: per game and area the entries, ok, crash,
hang, not reached, not testable and not run, the share covered over the catalogue (never over what ran), and
COVERAGE.md, every entry not covered with its reason. Checks with no game: `tools/regress/status_report_test.py`,
`tools/regress/coverage_scan_test.py`.

## Coverage of levels, figures and credits (`coverage.sh`, `status_report.py`)

`tools/regress/coverage.sh <ssa|giants|swapforce|trapteam> "<disc>.iso" <work dir> [--levels a,b] [--figures n,m] [--no-credits]`
runs one game at a time, isolated and `nice`: the game's own level tour (45 s per level; `COVERAGE_LEVEL_SECONDS`
overrides, and Giants needs more than 30 s because its tutorial follows the title at ~29 s), then
`tools/regress/figures.sh` (every toy the game accepts, or the listed types; a Swap Force swapper is `2004+1004`), then
`tools/pipelines/ending.sh` for the credits (`--no-credits` skips them). It writes `<work dir>/coverage.tsv`:
`# game <id>`, then `kind name verdict lowest_fps detail` for every level, figure and credits run. The verdict is `ok`,
`crash` (a fault or `[guard]` line in the run's log) or `not_reached`; `lowest_fps` is the smallest number of frames in
any one-second window (from the first to the last frame drawing at least 60 % of the run's busiest draw counts; every
frame for credits); `detail` lists the guards, faults, script errors and missing files of that run.
A gap of at least one second within those bounds gives `lowest_fps` 0 and `not_reached`, unless the run crashed.

`tools/pipelines/ending.sh <ssa|giants|swapforce> "<disc>.iso" <work dir>` requires a new or empty work directory, or one
it previously marked with `.ending-work`. It refuses other non-empty directories before changing their contents.
Keep the marker to reuse a work directory; a reused directory's run files can be replaced.

`tools/regress/status_report.py <coverage.tsv>... --platform "<name>" --build "<id>" --out docs/status/STATUS.md`
writes a public page with one table per game (levels, figures, credits: checks, passed, not reached, crashed, lowest
fps), the platform and build, and a table of the rows that did not pass; it draws `docs/status/status.png` with Pillow.
`--subset` marks a partial run, and `--state-lines` prints one line per crash or `[guard]` for `office/STATE.md`.
`tools/regress/status_report_test.py` checks the page, picture and state lines with an invented coverage file, so no
disc is needed.

### Trap Team tours (scripts prepared; game runs pending) [TrapTeam]

`tools/pipelines/tour_trapteam.sh` discovers standalone `.arc` names from the disc's level directory at run time,
using the read-only XDVDFS directory reader. It excludes the title, challenge voice/mesh packs, dialogue/global/music
support packs and duplicate arena challenge copies as in the Giants tour. No archive extraction or stored level list
is needed. A challenge or horde level may need a session its standalone load does not provide; the tour records the
failure. The discovery helper is `tools/regress/trapteam.py`.

```sh
# Safe discovery only: no executable, launcher, figure generator or game is started.
bash tools/pipelines/tour_trapteam.sh --list "<Trap Team disc>.iso"
bash tools/regress/coverage.sh trapteam "<Trap Team disc>.iso" private/runs/tt-coverage --list

# For a later session when level selection works in the runtime (these launch the game):
bash tools/regress/coverage.sh trapteam "<Trap Team disc>.iso" private/runs/tt-coverage --no-credits
bash tools/regress/figures.sh trapteam "<Trap Team disc>.iso" private/runs/tt-figures --seconds 130
```

Trap Team work directories must be below `private/`, including when using the level or figure tour directly. Every run uses the isolation block above, plus `SKY_FIGURES`; figure generator and roundtrip
checker invocations also get those variables. Each figure starts with a freshly generated tag; no real collection or
figure dump is read. Trap Team runs are `nice` and stop only their own PIDs, sequentially. The executable defaults to
`"build-trapteam/ArkeyanRuntime/Skylanders Trap Team"`, with a macOS fallback inside `"Skylanders Trap Team.app"`; `TOUR_EXE` / `FIGURE_EXE` override it.
The default duration is 130 s (`COVERAGE_LEVEL_SECONDS`, `TOUR_SECONDS`, or the figure tour's `--seconds`). The input
recipe is reused from Giants and still needs timing validation in Trap Team. All older games keep their defaults.

The tour requests each discovered level through `SKY_LEVEL` and checks that the requested archive opened. Trap Team's
adapter currently has no first-level redirect: until runtime support is supplied, other levels will be reported as
not reached. Runtime changes are separate from these scripts. Outputs are the existing `summary.tsv`, retained
`runs/` logs/frame times and, if any pipeline lists were written, `merged.bin`; coverage still writes `coverage.tsv`.
The figure tour uses catalogue id `trapteam`, includes earlier accepted toys, pairs swappers and includes known
Trap Team-era mini and Elite tags. Unsupported traps and unknown requests are explicit `SKIP` rows in `results.tsv`
(see [figures.md](../runtime/host/figures.md#trap-team-figure-tour)); coverage represents skips as `not_reached` with
their reason. Credits have no verified route yet: coverage records that reason without starting a credits run.

Checks requiring no game: `for script in tools/pipelines/tour_trapteam.sh tools/regress/coverage.sh tools/regress/figures.sh; do bash -n "$script" || exit; done`,
`python3 -I tools/regress/trapteam_test.py` (invented disc, catalogue selection, isolation paths and discovery-only CLI),
and `python3 -I tools/check_docs.py`. The real-disc `--list` mode has been checked; no Trap Team game run was made to
validate level selection, presses, figure acceptance, roundtrips or rendering.

## Frame times (`tools/perf`)

`tools/perf/level_run.sh <exe> <out dir> [scale] [level]` runs this recipe headless with `SKY_FRAME_TIMES` and prints
`tools/perf/frames.py`'s summary: worst gaps of the level's first 5 s, fps and min/median/p95/p99 frame gaps and work
from 10 to 40 s after the level's first frame. On a shared machine, run the two builds being compared at the same time.
It sets every isolation variable (since 2026-10-26: before, it left `SKYLANDERS_DATA`, `SKY_COLLECTION` and `SKY_LOG`
at the player's defaults). `SKY_GPU_SNAPSHOT_SECONDS=49,53` saves frames at the same wall-clock moments at any frame
rate (a game-speed check).

**Giants** **[Giants]**: `tools/perf/giants_run.sh <game executable> <out dir> [scale, 2] [level, level_213_tutotial]` is the same
for Giants: a new game in `SKY_LEVEL=<level>` with the harness's presses (`tools/regress/giants_press.py`: walking,
attacking, the pause menu at the end), 100 s, isolated; it prints the summary over 50..70 s (`frames.py --from 50 --to 70`:
a fixed wall-clock window, Giants' levels have fewer draws than SSA's 1000-draw level test) with the ten worst gaps, the
CPU time per frame of the process and its busiest threads over the window (`tools/perf/thread_cpu.py`, from `ps -M` at
the window's ends; far less inflated by other processes' load than the work time, a wall time), and the load average at
the start and the end. Level `title`: no presses (the title screen); `MOVIES=1`: the Bink movies
play (`SKY_BINK_LOG=1`); `FPS_LIMIT=60`; `WINDOW="<from> <to>"`; `SAMPLE=1`: macOS `sample` for 10 s from the window's end into
`<out dir>/sample.txt` (`SAMPLE_AT=<s>`, `SAMPLE_SECONDS=<s>` move it, e.g. onto a level's first seconds; inside the
window it adds 2-5 ms to each frame's work, 2026-10-27); `FIGURE=new:<toy type>[:<variant>]`: another
figure than `new_14.sky`. The scenarios of the 2026-10-26 performance pass: `level_213_tutotial` (Chapter 1),
`level_203_the_machine`, `level_hub_2_stage_2`, `arena_challenge_201_000`, `title`, `title` with `MOVIES=1`
([renderer_threads.md](../runtime/gpu/renderer_threads.md), "Giants").

**Swap Force** **[SwapForce]**: `tools/perf/swapforce_run.sh <game executable> <out dir> [scale, 2] [zone, barrens/brn2/brn2]`
(2026-10-28) is the same for Swap Force: a new game in `SKY_LEVEL=<zone>` with the zone tour's presses, 75 s, window
45..65 s, `FPS_LIMIT`, `WINDOW`, `SAMPLE`, `FIGURE` as above (default Eruptor, `new:9`).

**The lava shimmer** (2026-10-29) **[SwapForce]**: Swap Force's tutorial run also saves 40 consecutive presents from
50 s (`SKY_GPU_SNAPSHOT_SECONDS=50,50.001,...`: one per present, the ship in the lava canyon), and
`tools/regress/analyze.py shimmer <snapshot dir> <frames.txt> <from s> <max %>` measures, on the canyon walls (the
upper 45 % of the frame), the share of pixels whose luma flips with the frame's parity by more than 16 levels (a lock-in
at the Nyquist frequency: smooth motion averages out, a patch drawn on one frame and not the next stays). FAIL above
1.5 % (good build 0.5-0.8 %, with `SKY_POLY_OFFSET=0` 3.2 %), or when the frames are not the canyon (orange) or not at a
steady 30 or 60 FPS. The cause it guards: [shaders.md](../../games/swapforce/docs/shaders.md#the-lava-shimmers-cause-the-polygon-offset-2026-10-29-swapforce-alchemy-xbox-v).

**The lava's gradients** (2026-10-30) **[SwapForce]**: `tools/regress/analyze.py banding <snapshot dir> <max ratio>` reads the
same 40 frames: the worst bin of the green channel's histogram over the upper part against its neighbours. FAIL above 3 (good
build 1.2-2.3, a table target scaled with the frame 6-39). It only sees the bug at scale 2+, the default on Apple silicon.
The cause it guards: [shaders.md](../../games/swapforce/docs/shaders.md#the-lavas-posterized-gradients-2026-10-30-swapforce-ssa-xbox-v).

**Edge smoothing** (2026-10-31) **[SwapForce] [TrapTeam]**: `tools/regress/analyze.py sharpness <min gain %> <reference image> <image>...` is the detail (the mean
absolute Laplacian of the grey 3D area) of each frame against the reference, for the same static frame at `--edge-smoothing game`, `light` and `off` (an offscreen run with `SKY_DUMP_FRAME`, or a window capture
with `screencapture -l <window id>`). Light gains 24-30 % and Off 27-34 % in the two games; in SSA and Giants (no known FXAA pass) nothing but the scene's own movement (0-5 %). Its test is `tools/regress/analyze_test.py`.

**Load times** (2026-10-28): the three scripts also print `tools/perf/load_times.py <frames.txt> <game.log> [--file NAME]`:
the first frame with 100 draws or more, then each load (a stretch of frames under 100 draws: the loading screens) with
its length, and with `--file` the time from the level's own file (`level_027\mission.str`, `level_213_tutotial.arc`,
`\brn2.pak`) to the level. `[file] open` log lines and `SKY_FRAME_TIMES` both count from start-up for it.

`tools/perf/sample_threads.py <sample.txt> [--top N] [--min N] [--thread ID] [--under a,b] [--callers NAME]` reads a
`sample` report per thread: samples spent running (not blocked in the kernel), the hottest functions by self time,
the time under given functions (`--under __imp__NtReleaseSemaphore`) and the call paths into one (`--callers XXH3`).
That is how the kernel's wake-up cost was found ([threads_sync.md](../runtime/xbox360/threads_sync.md)).
`SKY_VBLANK_LOG=1` logs how late the emulated vertical blanks come ([video_vblank.md](../runtime/xbox360/video_vblank.md)).

**Window resize** (macOS, a real window: not offscreen): `tools/perf/resize_run.sh <exe> <iso> <out dir> [wait s] [drag s]
[env...]` drags the game window's corner with the mouse (`tools/perf/live_resize.swift`) and prints the frame gaps
before, during and after the drag, with captures of the game window only
([gpu/README.md](../runtime/gpu/README.md#window-and-resize-xbox-host), "Window and resize").

## Discipline

- Run before/after pairs **at the same time** and compare runs of the same day only: a load average of 20-50 from
  other jobs skews everything. Two runs of the same build must differ less than the change measured.
- Compare frames by content, not by frame index (the same index is another moment in another run).
- Measure pipeline builds with a cold-cache app copy (the `.app` has its own Metal cache, [platforms/macos.md](platforms/macos.md)).
- Run `tools/perf/level_run.sh` after each merge that touches the renderer or the generated code (a 30 -> 21 fps
  regression once went unnoticed for a day); rebuild a prebuilt `libArkeyanRecompLib.a` after `ppc/` changes.
- Keep the Mac awake during long runs: `caffeinate -u -t 30`, renewed.
- Grep `[stub]` after every new screen: a stub that runs must write its outputs.
- After a merge that touches patches: `git -C thirdparty/{plume,XenosRecomp,XenonRecomp} checkout -- .`, then cmake and
  ninja.
- **`EXCLUDE_FROM_ALL` tests are not rebuilt by a plain build** (`ArkeyanRuntimeLauncherTest`, `ArkeyanTexturePackTest`, Arkeyan Launcher's
  `ArkeyanLauncherTest` and `FakeArkeyanBuilder`): running one after `ninja` alone tests a **stale binary** (it happened on
  2026-10-25). Build it by name first (`cmake --build build --target ArkeyanLauncherTest FakeArkeyanBuilder`); the harness does.
