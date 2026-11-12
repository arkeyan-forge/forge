# Swap Force: dated history of the port

> **Purpose:** the dated news and session entries of the Swap Force port, moved out of [status.md](status.md) on 2026-10-29; the current state is in status.md · **Status:** archive (history, not maintained) · **Tags:** [Xbox] [XDK] [Alchemy] [SwapForce] [V]

## Status
- **Loads and frames faster** (2026-10-28) [V]: the archives' LZMA decoding runs as host code (`sub_82476EF8`,
  [lzma.md](../../../docs/engines/alchemy/lzma.md); 262 144 calls checked, 0 differences): 285 MB decode in 4.8-5.9 s of
  the workers' time instead of 8.5-10.1 s, `brn2` loads in 4.9-5.3 s instead of 5.6-8.7 s; the render thread's spin on
  its last swap is a wait (`swap_wait.cpp`: 13.8-14.6 -> 9.5 ms of CPU a frame at 30 fps); the pipeline baseline grew
  711 -> 738 with the swapper Blast Zone, Scorp and seven more figures (Blast Zone's draw-time builds in `brn1` 21 -> 1).
  Numbers: [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md), "Loading and frame time per game".
- **Day 1 done** (2026-10-26): identity, libraries, sigmatch against SSA's and Giants' inventories, engine verdict
  (below).
- **Toys and variants from the disc** (2026-10-26): `/archives/toydata.pak` (IGA v10, XML `.igx` per toy family) read by
  `tools/toy_tables.py`; the catalogue's Swap Force rows agree with it (136 toys, 123 variants; swapper tops 2000-2015,
  bottoms 1000-1015: the public list had them the wrong way round); catalogue id `swapforce`
  ([collection.md](../../../docs/runtime/host/collection.md)). No pictures yet: the C++ reader lacks IGA v10 / IGZ v7
  ([art.md](art.md)).
- **Scaffolding and XenonRecomp config** (2026-10-26): `-DSSA_GAME=swapforce` builds `SSFRecomp`; 25587 functions, 454
  jump tables, no unknown instruction ([boot.md](boot.md), "XenonRecomp").
- **Imports** (2026-10-26): the 44 imports SSA and Giants lack are implemented; `ninja -C build-swapforce` links
  ([boot.md](boot.md), "Imports").
- **D3D, XMA and Bink hooks** (2026-10-26): 21 hooks; the device layout is Giants', `DrawIndexedVertices` takes a base
  vertex and `Resolve` a level again (XDK 21250). The game boots through the logos, the title, a new save, the opening
  movie and the tutorial level with a figure read from the emulated portal, at 30 fps with sound; nothing drawn yet (no
  shader cache) ([boot.md](boot.md), "Direct3D").
- **Shader cache and first pictures** (2026-10-27): 556 shaders from run-time dumps, all translated
  ([shaders.md](shaders.md)); the title screen ("PRESS START"), the portal prompt, the loading screen and the level HUD
  render like the console; the level's 3D scene does not yet.
- **The tutorial's 3D scene** (2026-10-27): its pass list written ([boot.md](boot.md), "What broke on the way to a
  level"); three generic fixes (inverted viewport depth ranges kept, unnormalized texture fetches in XenosRecomp,
  scaled for resolves) and the tutorial draws like the game: the ship's deck, Eruptor lit with his shadow, the lava
  canyon, bloom and the HUD.
- **Test aids and the harness** (2026-10-27, guide steps 6 and 12): the tutorial in 25-60 s with an A every 2 s;
  `SKY_LEVEL=<area>/<map>` (e.g. `barrens/brn1`) starts a new game in another zone (`engines/alchemy/zone.cpp`);
  `SWAPFORCE_ISO=<disc> tools/test_all.sh --only swapforce` (one 100 s run, its references in
  `private/test-refs/swapforce/`).
- **The freeze ~58 s into the tutorial fixed** (2026-10-27) [V]: the Wwise voice service spun on a 4-context XMA voice
  whose contexts 5-7 were never kicked (the hooked writers saw only the last of the register stores sharing a word);
  the writers now act on every context of their voice ([boot.md](boot.md), "XMA"). The tutorial plays past the
  Greeble ships' cutscene to the next zone's loading (175 s run).

- **Figures and the portal** (2026-10-27, guide step 11) [V]: the emulated portal answers the Swap Force portal
  (`02 03`; older portals get "WRONG PORTAL VERSION"); every figure type plays: SSA, Giants, Swap Force cores,
  LightCores, swappers (two tags, mixed halves too), magic items; the Toy Box opens at the game's "place your Skylander"
  prompt (the CMagicMoment reader). Below, "Figures and the portal".
- **In the player kit** (2026-10-27, guide step 13, macOS) [V]: `KIT_GAMES="ssa giants swapforce" tools/make_kit.sh` packs Swap Force
  (`games=ssa,giants,swapforce`; the default stays `ssa giants` until the user decides to ship it); Arkeyan-Builder installs `SSFRecomp` from the disc in ~5 min and the installed game plays
  the tutorial. Below, "Installing Swap Force". One run-time shader of 7 is not caught yet ([shaders.md](shaders.md)).
- **Parity with SSA and Giants** (2026-10-27) [V]: [parity.md](parity.md) has every row with its proof. Ultrawide works (the scene pass and the
  world camera, 2560x1080 and 3440x1440 in the tutorial); HD texture packs work (per-game folder); the game's own icon comes from the XDBF at
  install; the portal prompt and the saves with their backups were checked; button glyphs work by redrawing the fonts' button images in guest memory ([art.md](art.md), "Button pictures"), and the 60 fps option works (the game reads its present interval from its device; the runtime shows it 1 at 60, [boot.md](boot.md), "Frame rate"). Details: [boot.md](boot.md),
  "Ultrawide, frame rate, glyphs, texture packs, icon, saves".
- **The zone tour and the pipeline baseline** (2026-10-27, guide step 10) [V]: every loadable zone of the disc (195)
  started alone with `SKY_LEVEL`: **136 reached and ran to the end, 20 fault (all survival arenas, one game spot), 39
  black** (37 turbozone staging areas, `brn3`, `ww1`); `games/swapforce/pipelines/baseline.zst` (686 pipelines); a
  loading wait on every zone load; 30 fps locked in three heavy zones; a runtime start-up race fixed (it crashed ~5 % of
  runs). Below, "Zone tour".
- **The survival arenas, the black zones and the save-slot stop** (2026-10-27) [V]: the 20 survival arenas started
  alone now play (they need Solo Survival's game mode, which `SKY_LEVEL` sets); `brn3` and `ww1` are not black with the
  movies on (`SKY_SKIP_MOVIES` stalled their intro cutscene; the tour plays their movies); the 37 black staging areas
  are pieces of their Swap Zone, never entered alone; the stop at the save slots under load was a race in the game's
  save tasks, fixed (`games/swapforce/save_tasks.cpp`). The tour: **158 of 195 zones reached and ran to the end, 0
  fault, 37 black (the staging areas)**. Below, "Zone tour" and "The stop at the save slots".
- **The blur and the green lava fixed** (2026-10-27) [V]: with the `anisotropy` setting above 1 the runtime filtered
  render targets and colour tables anisotropically: the whole 3D view smeared, the lava and Eruptor turned green. Now
  only textures with mipmaps get it. The passes that remain soft by design (a 1040x620 scene stretched to 1280x720, FXAA,
  depth of field, motion blur): [shaders.md](shaders.md#the-post-processing-passes-and-the-blur-swapforce-alchemy).
- **The flickering lava and crystals at 60 fps fixed** (2026-10-28) [V] (headless; on the player's display [?]): the user's
  "epileptic" tutorial was judder of the experimental 60 fps at ~45 fps (equal scene steps shown for 1 or 2 refreshes), on
  the two fastest scenes (the lava canyon, the crystal tunnel); no draw was wrong. At 60 the runtime now shows frames every
  2 blanks while they miss their blank (steady 30), and 60 again once they fit
  ([shaders.md](shaders.md#the-lava-and-the-crystals-flicker-2026-10-28-frame-pacing-not-a-draw-swapforce-alchemy-v),
  [video_vblank.md](../../../docs/runtime/xbox360/video_vblank.md#pacing-waitforflippresentinterval-in-the-swap-hook)).
- **The 3D at full resolution by default** (2026-10-27) [V]: the scene is 1280x720 (the window's shape when wider)
  instead of 1040x620 stretched; `[swapforce.video] original_resolution=1` (`--original-resolution 1`) keeps the
  console's. Three hooks: the chain's size, no predicated tiling, the console's EDRAM plan
  ([boot.md](boot.md#the-scene-at-full-resolution-2026-10-27-alchemy-swapforce-v)).

- **Text and voice language, the intro movies** (2026-10-27) [V]: the disc has English and French only (text and voices);
  `--language en|fr` works through the console language, any other falls back to English in the title; `voice_language`
  (`en`, `fr`) plays the voices in the other language (sound package suffix and the movies' Bink track, 3 hooks found by
  hand), and `skip_intro_movies` skips the logos. Proof and the addresses in [boot.md](boot.md) ("Text and voice language,
  the intro movies"); rows in [parity.md](parity.md).

- **Arena modes with two players, the story's start, the fall check** (2026-10-27) [V]: from the Arena Modes
  menu with pad 2 (`SSA_PRESS2=start:5000`) and a second figure placed through the Toy Box's Player 2 prompt, Battle Arena
  (Rampart Ruins) and Ring Out play with two HUDs and no fault; the survival modes need an arena unlocked by the story (all
  locked on a new save); a new game's story plays the tutorial into the first Barrens level. The stuck-falling threshold
  is scaled by 30 / fps above 30 fps (`fall_check.cpp`). What stays open: [100.md](100.md).

## Zone tour (2026-10-27) [Alchemy] [SwapForce]
`tools/pipelines/tour_swapforce.sh <iso> <work dir> [<zone>...]` ([testing.md](../../../docs/getting-started/testing.md#swap-force-to-the-tutorial-in-25-60-s-swapforce))
runs each zone alone: a new game with `SKY_LEVEL=<zone>`, Eruptor on the emulated portal (`new:9`), A every 2 s to 22 s,
then walking, attacking and jumping, 40 s, offscreen, isolated, one run at a time. One line per zone in
`<work>/summary.tsv`: the zone's `.pak` opened, draw-time builds after it, faults, last frame, median fps, max draws, the
verdict, and the mean brightness of a snapshot 4 s before the end (`black` below 6). The tour of 2026-10-27 is in
`private/swapforce/tour/summary.tsv` (local only).

**The zones** [V]: `tools/pipelines/swapforce_zones.py <iso> <scratch>` lists the `_zoneName` of every `CZoneInfo` in
`/archives/zoneinfos.pak` (200; the name the game looks up, e.g. `multiplayer/pvp/pvp_sandcastle/pvp_sandcastle`)
whose map has its own `/archives/<map>.pak`: **195**. Left out, no `.pak` of their own (pieces of other zones):
`golden/golden`, `templates/lockpickminigame_template`, `townarchetypes/adventurepack1edc`, `adventurepack2edc`,
`town_statics`. 158 of the 195 names are longer than the tutorial's (30 characters, up to 72): `SKY_LEVEL` now writes a
longer name over the engine's name table ([debugging.md](../../../docs/porting/debugging.md), `SKY_LEVEL`;
`engines/alchemy/zone.cpp`).

**Results** (the last run of each zone; the start-up crashes and the slot stops of the first pass were run again with
the fixed build, below; the 20 survival arenas, `brn3` and `ww1` again on 2026-10-27 with their fixes):

| Group | Zones | Reached, ran to the end | Fault | Black |
|---|---|---|---|---|
| story zones (`barrens`, `frostguard`, `wildwoods`, `outlands`, `verdant`, `finale`, `tutorial`) | 18 | 18 (`brn3`, `ww1` with their movies) | | |
| `town` states and `townarchetypes` | 16 | 16 | | |
| `adventurepacks`, `ui/mainmenubackground` | 3 | 3 | | |
| `heroicchallenges` | 20 | 20 | | |
| `lockpick_minigame` | 24 | 24 | | |
| `turbozones` (the Swap Zones) | 42 | 42 | | |
| `turbozones` `*_staging` | 42 | 5 (`ap1_*`, `ap2_*`) | | 37 |
| `multiplayer/pvp`, `multiplayer/ringout` | 10 | 10 | | |
| `multiplayer/survival` | 20 | 20 (in Solo Survival's mode) | | |
| **All** | **195** | **158** | **0** | **37** (pieces of a Swap Zone) |

"Reached, ran to the end" is the zone's `.pak` opened, frames to the end of the run, no fault, a picture that is not
black: the median fps in the zone was 30 (the game's own rate) in every one, 450-2150 draws a frame. Not every zone was
looked at: a lobby or a cutscene counts too (the `pvp`/`ringout` arenas end on a dim frame, brightness 36).
- **The 20 survival arenas, fixed** [V] (2026-10-27): they faulted (`Fault (signal 10) at guest address 0x00000000`,
  `pc __imp__sub_82A5D168 + 92`, from `sub_82A5D788` <- `sub_82A128A0` <- `sub_82A147E8` <- `sub_829A72F0`) at the
  first wave. The arena's objective component starts in `sub_82A621F8`: it reads the game mode (`sub_82989648`, the
  game object's +1184) and builds its rules object at its +28 only in modes 4, 32, 64, 128 and 256; `sub_82A5D168`
  calls that object. `SKY_LEVEL` starts a new game, so the mode was Story Mode's (1) and +28 stayed null. The way the
  game does it, seen in a live run through the menus (Arena Modes > Solo Survival; a new save has every arena locked, so
  the menu cannot go further): the menu item calls SetGameMode (`sub_829A8928`) with **64**; entering a zone
  (`sub_829C6220`) then calls SetGameMode with the game object's "next mode" (+1188, `sub_82989658` stores it). So
  `SKY_LEVEL` on a `multiplayer\survival\` zone stores 64 as the next mode when the zone is looked up
  (`engines/alchemy/zone.cpp`, `game::GameInstance`, `NextGameModeOffset`, `GameModeSoloSurvival` in
  `games/swapforce/addresses.h`), and the game applies it itself: `[level] the next zone's game mode 1 -> 64`. All 20
  ran 75 s with no fault at 30 fps, 615-1595 draws a frame, the arena's score HUD and the waves of enemies on screen
  (`survival_barrens`, a snapshot). Team Survival (2 players) and Rival Survival were not tried: Team Survival's menu
  item asks for a second controller. Not a recompilation or runtime fault.
- **37 staging areas are black: pieces of a Swap Zone, not zones of their own** [V] (2026-10-27,
  `turbozones/*_staging` but the `ap1_*`/`ap2_*` ones). From `zoneinfos.pak`: each of the 37 is listed in the
  `_subZones` of its Swap Zone's `CZoneInfo` (`turbozones/brn1_digger` lists `TurboZones/BRN1_Digger_Staging`); none
  has a `_turboZoneType`, `_isTurboZone`, `_targetLevel` or objectives of its own; each has
  `_mapSharingMode=ePST_ShareAssetIfPresentInAnotherPackage` (it borrows what its Swap Zone's package holds) and no
  sky (`CDistantGeometryData`: empty `_farList` and `_nearList`). Its map holds `Intro_PeriphStaging*` static
  entities, scenery for the Swap Zone's intro, and a player start. Loaded alone, the game runs (the HUD's "JOIN"
  prompt, the game's 30 fps) over a black, skyless view. The 42 Swap Zones all play when started alone (the tour),
  and a Swap Zone loads its staging area itself: `turbozones/brn1_digger` opens `brn1_digger_staging.pak` and
  `brn1_digger_staging_characters.pak` after its own archives (its tour log).
  The five `ap1_*`/`ap2_*` staging areas (adventure packs) show their scenery from their start. Nothing to fix; the tour
  keeps them as the expected `black`.
- **`barrens/brn3` and `wildwoods/ww1`: black only with `SKY_SKIP_MOVIES`** [V] (2026-10-27). Both load with a story
  movie (`_loadMovieName` `fmv05`, `fmv02`), then play their intro **inside a cutscene** (`brn3_intro.pak`,
  `ww1_intro.pak`: a `movie_stage` and its audio) that shows the Bink movie (`BRN3_Intro.bk2`, 1063 frames;
  `WW1_Intro.bk2`, 1567). The frame dump of a black `ww1` frame (`SKY_DUMP_FRAME=75`): the scene is drawn in full
  (the composite resolve is bright), then the last pass draws a full-screen quad textured with the 1040x620 movie
  surface over it. `SKY_SKIP_MOVIES` cuts every movie to one frame: the cutscene keeps showing that black surface and
  never ends (black from 65 to 200 s; START does not skip it). With the movies on (`SKY_SKIP_MOVIES=0`, no press during
  them), `ww1` plays `fmv02` then `ww1_intro`, and Eruptor is in the level with the HUD at ~200 s; `brn3` likewise
  (`fmv05`, `brn3_intro`, Eruptor on the ship's deck at ~190 s). A test aid's limit, not a port bug: a player sees the
  movies. The tour runs these two with the movies on (`TOUR_MOVIE_SECONDS`, 240 s, A every 2 s to 70 s): both `ok`
  (brightness 110 and 125, 30 fps, 1825 and 870 draws a frame).
- **Start-up crashes, fixed** [V]: 10 runs of the first pass (~5 %) died before the zone, all before the game's first
  Swap: `Fault (signal 10)` in `unordered_map::find` under `gpu::BufferCacheFrameDone` <- `gpu::Present` <-
  `PresentDisplayFrame` <- `main` (6), `mfm_alloc` in `plume::MetalCommandList::setGraphicsPushConstants` (1),
  `std::bad_alloc` (1), an exit with no line after the first draws (2). The game's render thread fills the vertex/index cache (`gpu::CacheVertices`, `CacheIndices`)
  while the main thread still presents the display frames and cleans that cache up: the two now take the producer
  lock (`runtime/gpu/buffer_cache.cpp`) [Xbox]. Every one of them ran again with the fix and reached its zone (the
  survival one then faulted at the arena spot above).
- **Stops at the save slots** (3 runs of the first pass, `lp_puzzle10..12`, load average ~70): one or two
  `SL_LOAD_SLOTS Thread` lines, no `create save`. The race of "The stop at the save slots" below, fixed.

**Pipelines** [V]: the 212 runs' `pipelines.bin` merged: **686 pipelines**, then **711** with the arenas, `brn3` and
`ww1` run again (2026-10-27), `games/swapforce/pipelines/baseline.zst`
(14 KB; hashes and states only) ([pipeline_cache.md](../../../docs/runtime/gpu/pipeline_cache.md), "Swap Force").
Draw-time builds after the zone opened, from an empty list to the baseline (90 s runs): `barrens/brn2` 130 -> 3,
`heroicchallenges/hc_verdant_01` 274 -> 7, `town/town_state2` 199 -> 7. Over the tour, 116 on average per reached zone
(the most: `hc_verdant_01` 274, `fg1` 253, `town_state9` 244).

**Performance** [V] (M-series Mac, load average ~5, scale 1, `--vsync 0 --fps-limit 60`, the baseline built in, 90 s,
`tools/perf/frames.py --from 40 --to 85`):

| Zone | Draws a frame (median) | fps | Frame gap median / p99 / max | Zone start (first 5 s, worst gap) |
|---|---|---|---|---|
| `barrens/brn2` | 1944 | 30.0 | 33.3 / 33.4 / 49 ms | 122 ms |
| `heroicchallenges/hc_verdant_01` | 1249 | 30.0 | 33.3 / 33.4 / 49 ms | 94 ms |
| `town/town_state2` | 1269 | 30.0 | 33.3 / 33.4 / 37 ms | 91 ms |

Locked at the game's 30 fps (measured before the 60 fps fix of 2026-10-27, which lifted that lock: [boot.md](boot.md), "Frame rate"): per frame the renderer takes 2-4.5 ms of the 33 (`[perf]`: draws 2.3-4.5 ms, GPU wait 0);
the only spikes are one 90-120 ms frame when the zone starts and 49 ms ones a few times a minute. Nothing to optimize
yet.

## The stop at the save slots (2026-10-27) [Xbox] [Alchemy] [SwapForce] [V]
**Symptom**: under load, after the title's A, the title stays up for ever: `[content] device selector`, one (or two)
`[thread] N named "SL_LOAD_SLOTS Thread"`, `enumerate type 0x1 on device 1: 0 package(s)`, then nothing (no
`SL_SAVE_OPTIONS`, no `create save`). Seen in 3 of 195 tour runs at a load average of ~70, twice before; reproduced in
a live run at a load average of ~45 (`tools/regress/live_run.sh`; the slot thread had ended, `sample` showed no thread
left in the save code).

**Cause: a race in the game's own code.** The storage object runs each save task ("SL_LOAD_SLOTS", "SL_SAVE_OPTIONS",
"SL_SAVE_GAME", ...: `sub_829CC7F0` names the thread by task) on an igThread and polls for its end every frame:
- the start, `sub_829CF218`: starts the thread (`sub_823F3CE0`: CreateThread suspended, the igThread's running byte
  +16 set, ResumeThread), **then** clears the global done byte (`0x8320ABC1`) and sets the storage's busy byte (+185);
- each worker (`sub_829CAA90` ...) sets the done byte as its last store; the thread's trampoline (`sub_823F3B70`)
  then clears the running byte;
- the poll, `sub_829CF1C0`: busy and done -> `sub_829CC358` joins the thread, clears busy.

A short task (no save yet: 0 packages) can end before the starting thread clears the done byte: the done byte is
cleared after the worker set it and the poll waits for ever. On the console the starting thread gets there first (a
few instructions); on a loaded host the new thread may run to its end first.

**Fix** (`games/swapforce/save_tasks.cpp`, hook `SaveTask_Poll`; addresses in `games/swapforce/addresses.h`): before
the game's poll, if the storage is busy, the done byte is clear and the task's igThread (storage +188) is no longer
running (+16 clear: the worker returned, so it had set the done byte), the done byte is set again and
`[save] the task's thread ended before its start cleared the done flag: set again` is logged. The poll then finishes
the task as the game would have.

**Proof**: with a 300 ms sleep added after `NtResumeThread` (a local, uncommitted test patch that makes the new
thread win every time), the tutorial recipe stops after the first `SL_LOAD_SLOTS Thread` without the fix and reaches
`tutorial_a.pak` with it (the `[save]` line twice: two short load-slot tasks). No tour or harness run has stopped at
the slots since.

## Installing Swap Force (2026-10-27, kit 0.5.0) [Xbox] [Alchemy] [SwapForce]
As for SSA and Giants, a player drops the disc, presses INSTALL, then PLAY. Design and contract:
[kit contract](../../../docs/release/launcher/kit_contract.md) ("Several games in one kit").
- **In the kit**: `KIT_GAMES="ssa giants swapforce"` (`tools/make_kit.sh`; the default is still `ssa giants`); the kit gets
  `config/swapforce/` (`game.txt`, `SwapForce.toml`, the switch tables), `commands/swapforce/`,
  `lib/swapforce/runtime/` (and its own `libSSARecompGpu.a`, which differs from SSA's, as Giants' does:
  `kit_commands.py` keeps it per game), `res/swapforce/Info.plist` and
  `ppc_config_sha256.swapforce` in `kit.txt`. Nothing else in Arkeyan-Builder needed a change: the disc is told by
  `default.xex` (`5d9a3bc3...`), the shaders by `shaders=igz:/archives/shaders_xenon.pak`, the name by
  `runtime=SSFRecomp`. Building the kit needs `private/swapforce/ppc/ppc_config.h` and `ppc_context.h` (XenonRecomp on
  `games/swapforce/config/SwapForce.toml`; the context is SSA's, byte for byte).
- **The macOS kit** (`JOBS=4 tools/make_kit.sh macos`, 7.6 min from clean build folders): 845 files, 98 MB,
  `ssa-kit-macos-arm64.zip` 39 MB; `tools/kit_scan.sh` clean. Its SSA and Giants parts are **byte-identical** to a
  `KIT_GAMES="ssa giants"` kit of the same tree (`diff -rq`: only `kit.txt` and the `swapforce` folders differ).
  SSA and Giants install from the three-game kit as before [V]: SSA 223 s (3 run-time shaders), Giants 603 s (50 run-time
  shaders; shaders 453 s then 56 s), both `@done`.
- **The install** [V] (the zip extracted, `Arkeyan-Builder --iso <disc> --out <game folder> --jobs 4`, M-series Mac, load
  average 5-20, another kit build running for part of it; scratch `SKY_SETTINGS`, `SKY_LAUNCHER_INI`,
  `SKY_COLLECTION`, `SKY_SAVE_DIR`, `SKYLANDERS_DATA`, `SKY_LOG`): `game: Skylanders: Swap Force`; disc 0 s
  (`/archives/shaders_xenon.pak: 62 entries, 61 IGZ, 1074 shaders (580 vertex, 494 pixel)`), recompile 5 s (323
  files), shaders 122 s (1074 of 1074, 0 without DXIL), compile 151 s, link 0 s (`SSFRecomp.app`, 84 MB), movies
  10 s (6 shaders built at run time), shaders 9 s (1080 of 1080), compile 6 s, link 0 s: **306 s**, `@done
  .../SSFRecomp.app/Contents/MacOS/SSFRecomp`.
  **Since the movie pass reaches the loading screen** [V] (2026-10-27, a kit rebuilt with `movie_portal`, `movie_press`,
  `movie_shaders` in `game.txt`, load ~4): shaders 104 s, compile 143 s, **movies 33 s (7 shaders built at run time,
  `811CCF44C1A9FC42.ps` among them)**, shaders 8 s (**1081 of 1081**), compile 6 s, `@done`.
- **The installed game** [V]: the recipe of [testing.md](../../../docs/getting-started/testing.md#swap-force-to-the-tutorial-in-25-60-s-swapforce)
  (100 s, offscreen): `[gpu] shader cache: 1080 shaders`, `create save: "SWAPFORCE_0"`, `open \archives\tutorial_a.pak`,
  `open \archives\eruptor.pak`; 30 fps in the level, no fault; the last snapshot shows Eruptor on Flynn's ship with
  the HUD and "Press X to Attack". The 1080-shader install drew no `811CCF44C1A9FC42` on the loading screen; the
  1081-shader one has `[gpu] shader cache: 1081 shaders`, reaches `eruptor.pak` and logs no `no translated` shader
  ([shaders.md](shaders.md), "From the disc").
- **The Windows and Linux kits** (2026-10-27, `KIT_GAMES="ssa giants swapforce" JOBS=4 tools/make_kit.sh windows linux`; the default
  stays `ssa giants`) [V]: Windows kit 867 files, 95 MB, zip 37 MB; Linux kit 863 files, 109 MB, zip 39 MB; `kit_scan` clean on both. Three
  small fixes were needed, all generic:
  1. **The Linux kit had no Swap Force**: the Docker run of `make_kit.sh` did not pass `KIT_GAMES` on, so the container built the
     default two games (Arkeyan-Builder then said "this disc is not one of the games these tools build"). It passes it now.
  2. **The Windows game faulted at start on `extrq`** (exception 0xC000001D, an illegal instruction): `-march=btver2` includes
     AMD's SSE4a, and the compiler used `extrq`/`insertq` for byte extracts in Swap Force's translated vector code (96 of them in
     `SSFRecomp.exe`; Wine on Rosetta rejects it, and so would any Intel PC). `cmake/toolchains/x86_64-w64-mingw32-clang.cmake` now adds
     `-mno-sse4a` (the kit's `compile.args` get it; 0 left in the executable). SSA and Giants were not affected in the tests but
     get the flag too; the Windows kit was rebuilt from clean (13 min).
  3. `tools/linux/kit_test.sh` and `tools/windows/wine_smoke_test.sh` take `SKY_PORTAL_PLACE` from the environment (`new:9`: Swap Force has
     no `new_14.sky` figure to place).
- **Linux, Docker** [V] (`tools/linux/kit_test.sh ssa-player-ubuntu24`, amd64 under Rosetta on an M-series Mac, lavapipe, the kit mounted, the
  disc read only, `SKY_PORTAL_PLACE=new:9`, A every 3 s from 14 s): Arkeyan-Builder from the disc to `@done /game/out/SSFRecomp` (852 MB game
  folder): `game: Skylanders: Swap Force`, 1074 of 1074 shaders (33 s), compile 566 s (Rosetta), link 41 s, movies 38 s (7 shaders built at
  run time), second pass 13 s: `1081 of 1081`; about 11 min in all. The game, offscreen, 600 s: `[gpu] Vulkan on llvmpipe`, `shader cache: 1081
  shaders`, `create save: "SWAPFORCE_OPTIONS"` then `"SWAPFORCE_0"`, `open \archives\tutorial_a.pak`, `eruptor.pak`, 535 draws a
  frame at 1.5-2.3 fps (software GPU under emulation), no fault, no `no translated` line (killed by the 600 s timeout). A 150 s run only
  reached the loading screen: at 2 fps give it 300 s or more.
- **Windows, cross build and Wine** [V] (the macOS kit's `Arkeyan-Builder --kit <Windows kit> --toolchain <llvm-mingw 20260922>` over the `ssabuild-work`
  of a native macOS install of the same disc, 8 min under load: shaders 156 s, compile 166 s, movies 120 s): compile 642 s (a clean cross build, load ~5), link 1 s, `SSFRecomp.exe`. Wine 10, headless
  (`tools/windows/wine_smoke_test.sh`, `EXE=`, A every 2 s from 14 to 110 s, 150 s): `create save: "SWAPFORCE_OPTIONS"` and
  `"SWAPFORCE_0"`, `open \archives\tutorial_a.pak`, `eruptor.pak`, no `Fault`, no `[guard]` line, alive until the timeout. The cross
  build skips the movie pass, so the 7 movie shaders are missing on the GPU path (Wine runs without a GPU here). Before the `-mno-sse4a`
  fix the same run faulted at ~35 s (`0xC000001D`). **Not tested**: Swap Force on a real Windows or Linux PC, a GPU path under Wine, the
  Arkeyan Launcher game-table row (marked unavailable), the game's icon on Windows and Linux (none).

## Day 1 (2026-10-26) [Xbox] [XDK] [Alchemy] [SwapForce] [V]

### The XEX
- Original PE name `nova-xenon-fin.pe`, link date 2013-08-17. Sections as Giants' plus a `.XBMOVIE` and `BINK*`/`RAD*`
  sections (Bink 2 linked in: `BINK` code at `0x8307E000`).
- **267 imports** (Giants 220, SSA 218).
- Static libraries (XEX optional header `0x200FF`): `XAPILIB 2.0.21250.6`, `D3D9`, `D3DX9`, `XGRAPHC`, `XAUDIO2`,
  `XAPOBA`, `XMCORE`, `XNET`, `XONLINE`, `XHTTP`, `XAUTH`, `XMP`, `XMIC`, `XPARTY`, `XBOXKRNL` all `2.0.21250.0`;
  `LIBCPMT 2.0.11886.0`; `LINK 10.0.11886`, `C1`/`C2 16.0.11886` (the same compiler as Giants).
  New against Giants: **XDK 21250**, `D3D9` (Giants: `D3D9LTCG`), `XMCORE` (Giants: `XMCOREL`), `LIBCPMT` (Giants:
  `LIBCMT`), and `XHTTP`, `XAUTH`, `XPARTY`. So **every `runtime/d3d/device.h` offset is to re-derive** (the guide, step 9).
- The disc: `/archives/` only: 2231 `.pak`, 92 `.bk2` (Bink 2), 2 `.bik` (Bink 1: the Activision and Alchemy logos),
  plus `default.xex`, `cachenonce.txt`, `nxeart`. A `.pak` is an **IGA v10, big-endian** archive (Giants: IGA v8,
  little-endian, in `.arc`/`.bld`): every Alchemy reader (`runtime/host/art/alchemy_archive.cpp`, `tools/toy_tables.py`,
  Arkeyan-Builder's shader extractor) needs the new version. Strings in the XEX name `.igz` files inside the paks
  (`fonts/ProximaNova-*.igz`, `UI/*.igz`), `igImage2`, `igObject`: Alchemy.

### sigmatch
| Inventory | Found | At 1.0 | What matched |
|---|---|---|---|
| SSA's (`docs/giants/ssa_hooks.json`, 297) | 176 | 52 (51 strict + 1 data) | CRT and XDK only: save/restore helpers, setjmp/longjmp, D3D (`DrawVertices`, `DrawIndexedVertices`, shader constants, `D3DVertexShader_Init`, dirty registers...), XMA engines 1 and 2, `XAudio2_RenderDriverCallback`. None of the Lux engine hooks (`inflate`, NGL, `Movie_Update`, languages, `LightTile_ClassifyJob`) |
| Giants' (`tools/sigmatch/giants_hooks.json`, 38) | 34 | 21 | the ten `[main]` addresses (the same as SSA's inventory gives), XMA (7 of 8 at 0.9-1.0; `Xma2_Init` fuzzy, it is a voice set-up in Giants), D3D shader inits, `DrawVertices`, `ClearF_Worker`, `InsertCallback`, `BinkOpen` (0.9); the Giants-only ultrawide hooks and the display aspect global are not found |

Giants for comparison: 171/293 from SSA's inventory (2026-10-25). The ten `[main]` addresses (both inventories agree):
`restgprlr_14 0x82FF2D50`, `savegprlr_14 0x82FF2D00`, `restfpr_14 0x82FF401C`, `savefpr_14 0x82FF3FD0`,
`restvmx_14 0x82FFB578`, `savevmx_14 0x82FFB2E0`, `restvmx_64 0x82FFB60C`, `savevmx_64 0x82FFB374`,
`longjmp 0x82FFAE90`, `setjmp 0x82FFC090`.

### Engine verdict
**Alchemy**, a newer build than Giants': the XDK and CRT code matches (SSA's and Giants' inventories agree), no Lux
engine code exists, Giants' engine-level hooks (ultrawide) do not match byte for byte, the archives are IGA v10
big-endian, and the disc carries the Alchemy logo movie. The Alchemy work carries over at the design level (IGA/IGZ,
Bink 2 on the GPU, XMA, no command buffers); every engine address is re-found, every format reader takes the new
version.

## Figures and the portal (2026-10-27) [Alchemy] [SwapForce] [V]

Runs: `private/swapforce/run.sh` (the isolation block), the tutorial presses, `SKY_PORTAL=emulated`,
`SKY_TRACE_FILES=1`, `SKY_PORTAL_LOG=1`, `SKY_GPU_SNAPSHOT_SECONDS`; real figures are copies of the owner's dumps in
`private/swapforce/figures/` (the runs write their copies only).

### Level 20 [SwapForce] [V]
The game's own experience table has 20 levels (level 20 = 197500 = 33000 + 63500 + 101000, the third part in the extra
area's +0x08, which the game adds its earned experience to): the HUD of a figure made with 197500 shows "MAX 20", one
with 132400 shows 17 ([figures.md](../../../docs/runtime/host/figures.md#swap-forces-level-cap-20-alchemy-swapforce-v)).

### The portal model
- **What the game checks.** The title's portal driver (`sub_82A91BF0`, "[TFB Portal] Portal firmware version %u.%u.")
  has Spyro's Adventure's check: a version `(b1 << 8 | b2)` under `0x011F` logs "Portal firmware out of date". Then
  `sub_82A7B550` sorts the portal by its id: first byte 1 is kind 0 (second byte under 5, or 10-49: Spyro's Adventure's
  `01 28`, `01 29`, `01 2C`) or kind 1 (5-9 or 50 and up: Giants' `01 34`, `01 3C`, `01 3D`, `01 48`); first byte 2 or
  more is kind 2, 0 is kind 3. The game wants kind 2 or more.
- **Seen in runs:** `01 28` and `01 48` (`SKY_PORTAL_MODEL`): "WRONG PORTAL VERSION. An old Portal of Power is connected
  to the console. Please connect a SWAP Force Portal to your console." over the title, and the game stays there; `02 03`
  plays. So every Swap Force or later portal works and every Spyro's Adventure or Giants portal is refused
  (`PortalVerdictFor("swapforce", ...)`, `SSAPortalTest`).
- **The emulated portal answers `02 03`** (the Swap Force portal, `EmulatedPortalModel`); it answered `02 1B` (Traptanium)
  before, which the game takes too. The log: `[portal] model: 02 03 Swap Force portal; Swap Force: works (emulated)`.

### Figures of each type
The portal reports 16 slots (2 status bits each, `EmulatedPortal::SlotCount`); `SKY_PORTAL_PLACE` takes a list, each in
its own slot. Each row: the archives the game opened (`[file] open \archives\<name>.pak`) and a frame in the tutorial with
the character's name on the HUD.

| Figure | Placed as | Archives | Seen |
|---|---|---|---|
| SSA character | `new:9` (Eruptor) | `eruptor.pak` | plays (the harness run) |
| Giants character | a real Jet-Vac (`100`, variant `0x1000`) | `jetvac.pak` | "JET-VAC" plays |
| Swap Force core | a real Scratch (`3000`); `new:3003` (Scorp) | `catgryphon.pak`; `scorpion.pak` | "SCRATCH", "SCORP" play |
| LightCore | a real Countdown LightCore (`3010`, variant `0x2206`) | `dabomb.pak` | "COUNTDOWN" plays |
| Swapper | the real Wash Buckler top and bottom (`2015`, `1015`) | `shapeshifter.pak`, `body_pirate.pak`, `legs_pirate.pak` | "WASH BUCKLER" plays |
| Swapper, new tags | `new:2004,new:1004` (Blast Zone) | `body_fireknight.pak`, `legs_fireknight.pak` | "BLAST ZONE" plays |
| Swapper, mixed | `new:2015,new:1004` | `body_pirate.pak`, `legs_fireknight.pak` | "WASH ZONE" plays |
| Magic item | a real Battle Hammer (`3200`), with Blast Zone | `magicitem_hammer_itemdata.pak` | the hammer's icon on the HUD |

- **Never-played tags** (`new:<type>`, `CreateBlankFigure`) work for every type, Swap Force's (1000-3299) included,
  with variant 0 (the real Swap Force figures have `0x2000`; the game does not mind).
- **A lone half** (`new:2015` only) is not a character: the game asks for a Skylander (CMagicMoment state 5) and the
  Toy Box opens with the top on P1. **Two characters for player 1** (Jet-Vac and Countdown): "THERE ARE TOO MANY
  SKYLANDERS FOR Player 1 on the PORTAL OF POWER".
- **What the game writes** (the runs' copies against the dumps): the first and extra area pairs (blocks 8-21 and 36-49;
  on a never-played figure only the B copies 36-49), and **block 6** of the plain sector 1: `01` and four bytes, the
  same four on both halves of a swapper written together, different on each figure and run [?]. Each half of a
  swapper keeps its own progress (both had their play time written). `SSAFigureTest <dump>` reads every written figure
  as valid (first and extra areas, checksums right).
- **A swapper through the Toy Box** (2026-10-27, [V] `tools/regress/swapper_toybox.sh`): a swapper is one card (two
  linked dumps, `docs/runtime/host/collection.md` "Swappers") on one seat; putting it on places both tags (top in slot 0,
  bottom in slot 1 for player 1; player 2 would use 2 and 3), taking it off removes both, and the Figure page's "Change the
  bottom" swaps one half. The run (Blast Zone, `body_fireknight.pak` and `legs_fireknight.pak` open, the game took it)
  passes; player 2 with a swapper and two characters at once were **not run** [?], so is the title's way of telling
  which player a pair belongs to (slot order is assumed). Design: `docs/runtime/host/toybox_ux.md` section 4, "Swappers".

### The portal prompt (the Toy Box opens at it)
Swap Force runs every figure request through the **CMagicMoment** singleton (public facts: `tsapZ/SwapForceRecomp`
`src/game/magic_moment_watch.cpp`, MIT; checked here: `sub_82858F70` stores the instance at `0x83131BE0`, and
`MM_pleaseplaceText` sits at `0x82110260` as that source says). `games/swapforce/swapforce_portal_prompt.*` and the
adapter's `ReadPortalPrompt` read three words a frame: the instance, its state (`+0x20`), its player (`+0x148`);
`SKY_PORTAL_LOG=1` prints `[toybox] portal prompt: magic moment state S, player P, reason R` on every change.
- States seen with no figure (run `prompt1`): `0` in the front end, `1` and `0` once at the title, then after the
  difficulty `1, 2, 3, 5` (player 0): the prompt. `5` (Idle) and `13` (Dead) ask that player for a Skylander, `16`
  (Error) is a "too many" screen (`ClassifySwapForce`, `SSAPortalTest`).
- The run: `[toybox] prompt for player 1`, `open (1 figures, emulated portal)`, `SKY_TOYBOX_SCRIPT=Return:75` puts Scratch:
  `P1: Scratch`, `the game took Scratch (slot 0, 0.2 s after the change)`, state 7, `the prompt is gone`, `closed`,
  `catgryphon.pak` opens and Scratch plays the tutorial.
- [?] Player 2's prompt, the Dead state and the error screen were not run.

## Nightmare difficulty and "Nightmare from the start" (2026-10-28) [Alchemy] [SwapForce] [V]
The difficulty menu (a new game's NEW slot, and the pause menu's Options) has four rows, Easy, Medium, Hard and **Nightmare**.
A new save shows Nightmare with a padlock and "Beat the game to unlock Nightmare difficulty." [V] (shot of the menu).
- **How the game locks it** (read in the recompiled code, found by the names in guest memory: `tryUnlockNightmareDifficultyLevel_Internal`,
  a script function the story's end calls, and `nightmare_difficulty_unlocked`, a property of the slot's data): `sub_82C12BC0`
  answers whether a difficulty row is available: any index under 3 yes, else **`sub_829CC788`**, which reads that property
  from the profile (`sub_829D48F0`, `sub_823E96C0`) and answers whether it is 1. It has no other caller. The property is
  written by the story's end only; nothing in the menu writes it.
- **`[swapforce] nightmare_from_start=1`** (`--nightmare-from-start 1`, `SKY_NIGHTMARE_FROM_START=1`; the launcher's Game tab,
  Difficulty): `engines/alchemy/nightmare.cpp` (Swap Force's own file until 2026-11-05, now shared with Imaginators) hooks `sub_829CC788` (`GAME_HOOK_NightmareUnlocked`), calls the original and
  answers 1 when the option is on. The save is not touched: the property stays what it was, so with the option off the
  padlock is back at once (the next time the menu asks), and a slot that chose Nightmare keeps playing it (the choice is the
  slot's own). The log says `[nightmare] offered from the start`, then once per case `[nightmare] the menu asked: the save says
  locked, the option is on: offered`. The game asks when the Nightmare row is highlighted, not when the menu opens.
- **Proof** [V]: [`tools/regress/nightmare_menu.sh`](../../../tools/regress/nightmare_menu.sh) (about 4 min, real menus, a new
  save): option off, three Down, the padlock and `the option is off: locked`; option on, the same, the description "Villains
  wield devastating power. Only for experienced Skylander armies. Major bonus to Loot and XP." and no padlock, then A: the
  tutorial starts. It runs in `tools/test_all.sh` with Swap Force.
- **The other two games** [V] (menus run 2026-10-28): Giants' difficulty screen (Story, a new slot) has three choices, shown with a
  growing silhouette: the first, "Cyclops Crusher (Medium)" (the default) and "Heavy Hitter (Hard)"; Right at Hard stays on
  Hard (no fourth, no padlock), nothing is locked on a new save, and its disc has no Nightmare text. Spyro's Adventure starts a
  new game with no difficulty choice, its Options (General, Audio, Controls) hold no difficulty either (General has only "Visiting
  Nicknames"), and the only "Nightmare" strings of its disc are upgrade names ("Nightmare Huggers", "Nightmare Touch"). So the
  option exists for Swap Force only; the other runtimes log `<game> has no locked difficulty, ignoring nightmare_from_start`, and the
  launcher shows the option on Swap Force's page only. (2026-11-05: Imaginators has it too, SuperChargers offers Nightmare from the
  start, Trap Team hides it until the story is beaten: each game's status page, "Nightmare".)

## Moved from the status page: the other disc and two solved problems

The other disc (row of the Disc table):

| Other disc [V] | Redump "Skylanders - Swap Force (Europe) (Es,It)" (2026-10-27): `default.xex` `3db455f6...` (full: `3db455f6ee17e8cb3a4d58873d96583d9160396d360a0130e5eb88adad8a5518`), same size; decrypted, **the same loaded image byte for byte** (only the XEX header differs: media id `0x74FDDE06`, version `0x1C`, region), so the code, addresses and hooks are the reference disc's. Its files: the same 2476, 424 of them language pairs (`_en`/`_fr` become `_es`/`_it`), 73 localised in place (movies, a few levels); `shaders_xenon.pak` identical. Accepted by `info.cpp` and `game.txt`; `test_all.sh --only swapforce` on it: ALL PASS. With the game's language, it picks Spanish (`_es` archives). |

Solved problems (they were in Open problems):

- **[V] The "stall at the save slots" with no portal is the game's own "PORTAL DISCONNECTED" screen** (2026-10-27):
  the title's A gives the device selector and two `SL_LOAD_SLOTS Thread` lines, then the game shows that screen and
  waits for a portal (`SKY_PORTAL=none`, headless or not; a snapshot of an offscreen run shows it). It is what the
  kit's movie pass hit (it ran with `SKY_PORTAL=none`); with `SKY_PORTAL=emulated` and an empty collection, headless
  runs pass the slots. Not a runtime bug: as on a console, Swap Force needs a portal past the title.
- **[V] The stall under a heavy load is fixed** (2026-10-27): a race in the game's own save tasks (below, "The stop
  at the save slots").
