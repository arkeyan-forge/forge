# Debugging: environment variables, log tags, recipes, capture and dump tools

> **Purpose:** every environment variable the runtime and its tests read, how to read each log tag, and the capture, dump and search tools for a bug · **Status:** current (2026-10-29) · **Tags:** none (runtime aids); game-specific values are tagged [SSA], [Giants] or [SwapForce]

Read from the code (`skyenv::Get` in `runtime/host/env.h`, `fprintf(stderr, "[tag] ...")`). The variables are `SKY_*` for
every game since 0.6.0 (the launcher's `SKY_LAUNCHER_*`); **the older `SSA_*` / `SKYLAUNCHER_*` names still work** (the new name
is read first). Kept: `SSA_ISO` (per game, with `GIANTS_ISO`, `SWAPFORCE_ISO`), `SKYLANDERS_DATA`, `SDL_*`. The command line, `launcher.ini` and which source wins: [../getting-started/settings.md](../getting-started/settings.md).
The isolation block every run starts from, the fast recipe and the harness: [../getting-started/testing.md](../getting-started/testing.md).

## Environment variables (complete catalogue)

### Isolation and paths
| Variable | Effect |
|---|---|
| `SKYLANDERS_DATA=<folder>` | replaces the `Skylanders` data root (log, `pipelines.bin`, `portal.txt`, collection, kits...); **disables the data-folder migration** |
| `SKY_SETTINGS=<file>` / `SKY_LAUNCHER_INI=<file>` | settings file instead of the player's `launcher.ini` (missing/empty = defaults); also disables the migration |
| `SKY_SAVE_DIR=<dir>` | saves (`<dir>` is the save root), `pipelines.bin`, `portal.txt`; also disables the migration. Always a **new empty** folder |
| `SKY_COLLECTION=<folder>` (older alias `SKY_FIGURES`) | the figure collection; disables the migration |
| `SKY_MIGRATE_FROM=<folder>` | the only old figure folder to migrate (nothing is migrated without it when `SKY_COLLECTION` is set) |
| `SKY_PORTRAITS=<folder>` | the disc pictures' cache |
| `SKY_LOG=<file>` | the log file instead of the player's (empty = no file) |
| `SKY_IMPORT_FOLDER=<folder>` | Toy Box Manage > Import source (else `<collection>/Import`) |
| `SKY_LAUNCHER_DATA=<folder>` | the launcher's kits folder; also disables the migration |
| `SSA_ISO`, `GIANTS_ISO` | the discs for `tools/test_all.sh`, `ArkeyanRuntimeLauncherTest` (SSA) and `ArkeyanArtTest` (each game's pictures read from its disc); read only |
| `SKY_KEEP_COLLECTION=1` | `ArkeyanCollectionTest` keeps its scratch collection and prints where |

### Game flow and test aids
| Variable | Effect |
|---|---|
| `SKY_HEADLESS=1` | no window/SDL video/dialogs (the launcher prints usage without a path) |
| `SKY_PRESS="btn:sec[:hold],..."` | scripted pad presses: `start back a b x y up down left right`, `lup ldown lleft lright` (left stick); 150 ms unless a hold is given ([xbox360/input.md](../runtime/xbox360/input.md)) |
| `SKY_PRESS2="btn:sec[:hold],..."` | the same script for the second controller (Player 2, XAM user 1), which then exists ([xbox360/input.md](../runtime/xbox360/input.md)) |
| `SKY_PRESS_FILE=<file>` | presses read from lines appended to the file during the run (`a`, `lup:2`, `2 a` = pad 2); [`tools/regress/live_run.sh`](../../tools/regress/live_run.sh) wraps an isolated Giants run with it (another game: `LIVE_EXE`, `LIVE_ISO`); the first controller is connected from the start |
| `SKY_LOG_SHADER=<hex hash>` | logs once where that pixel shader is first drawn (frame, primitive, vertex shader, target size) |
| `SKY_POLY_OFFSET=0` | the guest's polygon offset (`PA_SU_POLY_OFFSET_*`, the host's depth bias) off: decals depth-fight the surface under them (Swap Force's lava shimmer, [shaders.md](../../games/swapforce/docs/shaders.md#the-lava-shimmers-cause-the-polygon-offset-2026-10-29-swapforce-alchemy-xbox-v)); to see the fault or to check that `analyze.py shimmer` catches it |
| `SKY_SKIP_PS=<hash>[,<hash>...]` | test only: drops the draws of these pixel shaders (a hash may be cut short, `123B15C5`), logged once per shader; bisects a frame by pass. The target keeps its old contents: clean for a pass that writes the target it read from, stale for one that writes another target |
| `SKY_SKIP_MOVIES=1` | every Bink movie ends after its first frame ([SSA] the Lux `Movie_Update` hook, `engines/lux/movie.cpp`; [Giants] the `BinkOpen` hook makes it one frame long, `d3d/bink_display.cpp`) |
| `SKY_LEVEL=<level>` | a new game starts in `<level>`. [SSA] **A level other than `level_027` may have no Skylander** [V]: in `level_hub`, `challenge_level_000`, `level_037`, `038`, `039` and `046` the redirect loads the level but no hero is created (no HUD; the camera stays on its first view); `level_036` showed an NPC's dialog and the others were not tried: such a level loads and draws but may not be played ([games/ssa/docs/100.md](../../games/ssa/docs/100.md), "Modes"). [SSA] [Giants] the first level's files come from `<level>` (`RedirectLevel`, `kernel/files.cpp`; the first level's path is the adapter's `firstLevel`, `runtime/game.h`). [SSA] `level_027`'s files: `level_000..046`, `level_hub`, `challenge_level_000..031`, `pvp_level_001..010`, `dnftest`. [Giants] `level\level_213_tutotial.arc`/`.bld`: e.g. `level_203_the_machine`, `pvp_level_200_two_towers`, `title`. [SwapForce] `<area>/<map>` or a full zone name, the `_zoneName` of a `CZoneInfo` of `zoneinfos.pak` (`barrens/brn1`, `multiplayer/pvp/pvp_sandcastle/pvp_sandcastle`; the 195 loadable ones: `tools/pipelines/swapforce_zones.py <iso> <scratch>`) [V] (the zone tour, [history.md](../../games/swapforce/docs/history.md) "Zone tour"): the game's zone lookup renames the tutorial's zone (`engines/alchemy/zone.cpp`, hook `Zone_IndexByName`; a longer name is written over the engine name table's last string and the table's end moved, `game::NameTable`); `[level] a new game starts in zone ...`; a `multiplayer/survival/` zone also gets Solo Survival's game mode (`[level] the next zone's game mode 1 -> 64`; [history.md](../../games/swapforce/docs/history.md) "Zone tour"). [SuperChargers] `<map>` (`academy`): the new game plays its intro and tunnel ride, then enters `<map>/<map>` instead of `intro1/intro1` (`engines/alchemy/zone_request.cpp`: the game's zone request is pointed at our text, `[level] the zone asked for, intro1/intro1, is now academy/academy`) [V] ([boot.md](../../games/superchargers/docs/boot.md) "SKY_LEVEL and the test"); a full zone path (`actionpacks/apa1/apa1`, `2015/ezones/airzone/airzone`) is taken as it is (`engines/alchemy/zone_name.h`), and `SKY_LEVEL_FROM=<zone>` renames that zone's request instead (`academy/academy`: a continued save reaches the zone in ~25 s instead of the new game's ~320 s) [V] (the coverage runs, [testing.md](../getting-started/testing.md)). `SKY_LEVEL_FROM` in the other games [V] (2026-11-07): [SSA] [Giants] [TrapTeam] that level's files are redirected instead of the first level's (`kernel/level_redirect.h`): Battle Mode's first arena (`pvp_level_007`, `pvp_level_200_two_towers`) becomes any arena, in the game's own battle; [SwapForce] that zone is renamed instead of the tutorial's, a trailing `/` takes any zone under it (`multiplayer/ringout/`: Arena Modes' first arena) |
| `SKY_MOVIE_SWAP=<from>:<to>[,...]` | test switch (scripts only): a movie the title opens as `movies\...\<from>.bik` is read from `movies\<to>.bik`, logged `[movie] <path> read from movies\<to>.bik (SKY_MOVIE_SWAP)` (`SwapMovie`, `kernel/files.cpp`). [SSA] `legal:sl_sc30,atvilogo:credit` plays the ending and the credits at start-up ([`tools/pipelines/ssa_ending.sh`](../../tools/pipelines/ssa_ending.sh)); [Lux] every movie that plays to its own end logs `[movie] <name> ended after <n> updates` (`engines/lux/movie.cpp`) |
| `SKY_TRACE_FILES=1` | log every file read/write with timestamps |
| `SKY_TEST_READ_FAIL=<name>:<n>[:<k>]` | test aid: the n-th read (from 1) of the disc files whose name holds `<name>` (any case) fails its first k attempts (default 1); k = 9 exhausts the retries ([bink.md](../engines/lux/bink.md#movie-data-that-stops-arriving-issue-26)) |
| `SKY_CLOCK_STEP=<hex guest address>` | test aid: a float the title keeps its frame step in (60ths of a second; SSA `82B62CA4`): its sum over the flips against wall time, `[clock] N s of game in M s: R s/s` every 5 s ([video_vblank.md](../runtime/xbox360/video_vblank.md)) |
| `SKY_NO_FPS_CAP=1` | test aid: a game's `maxFpsLimit` (Giants: 60) is not applied, to measure above it |
| `SKY_ALLOC_LOG=1` | [Xbox] one `[alloc]` line per call of `MmAllocatePhysicalMemoryEx` (size, protect, alignment), `NtAllocateVirtualMemory` (base, size, type), the memory queries `MmQueryAllocationSize`, `MmQueryAddressProtect`, `NtQueryVirtualMemory` and `MmQueryStatistics` (address or totals, result) and Trap Team's five new imports (their arguments), with the guest thread and the **result**; a result of 0 is marked `(ZERO)`. Off by default, nothing read or logged (`kernel/alloc_log.h`). A game's own pool allocator is not an import: hook it in `games/<id>/` ([Trap Team's](../../games/trapteam/docs/boot.md)) |
| `SKY_XMA_TRACE=1` | [Xbox] test switch: `[xma-loop]` (a context armed with a hardware loop, each wrap), `[xma-frame]` (RMS of the 4 subframes of each loop end/start frame), `[xma-wrap]` (the step across a wrap and the silent samples after it), `[xma-dec]` (a frame the decoder returned trimmed or not at all) on stderr: the loop audit of [audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md#hardware-loops-in-levels-2026-10-28-ssa) |
| `SKY_LANGUAGE`, `SKY_VOICE_LANGUAGE` | text / voice language (`en fr de es it nl sv no da fi`); settings, over `launcher.ini` |
| `SKY_TOYBOX_AUTO_OPEN=0\|1` | the Toy Box opens at the game's portal prompt (setting `[portal] toybox_auto_open`) |
| `SKY_WINDOW_SIZE=<w>x<h>` | the window at that size (test runs on a display of another shape, e.g. `2560x1080` offscreen; `host/display.cpp`) |
| `SKY_WINDOW_SCRIPT="<s>:<w>x<h>,<s>:min,<s>:restore,..."` | resizes, minimises or restores the window at those seconds (offscreen too: the output follows the window); the live ultrawide test (`main.cpp`, [ultrawide.md](../runtime/gpu/ultrawide.md)) |
| `SKY_DISPLAY_SIZE=<w>x<h>` | the window's display size for ultrawide (the scene is made for it when it is wider than the window; offscreen runs have a 1024x768 display): a 16:9 window on a wide display (`main.cpp` `WindowDisplay`, [ultrawide.md](../runtime/gpu/ultrawide.md)) |
| `SKY_UPSCALER=fsr3\|metalfx\|off`, `SKY_FSR_QUALITY=<preset>`, `SKY_UPSCALE_SCALE=<n>`, `SKY_FSR_DEBUG=1\|2` | FSR 3 upscaling as the `upscaler` / `fsr_quality` settings (`metalfx`: MetalFX, an internal comparison on Metal); the frame's scale forced to n; FSR's debug view in the output (its motion, depth, masks in a grid), 2 also logs the first dispatches and the inputs pass's first values; `[upscale]` and `[fsr]` log lines ([upscaling.md](../runtime/gpu/upscaling.md)) |
| `SKY_GPU_TIMES=1` | Metal: the GPU's busy time per frame in the `[perf]` log (`GPU busy X ms per frame`), from each command buffer's GPU start and end |
| `SKY_GLYPHS_SCRIPT="ps5:25,key:45,..."` | at those seconds, plugs a virtual SDL gamepad of that family (`ps5`, `ps4`, `ps3`, `switch`, `xboxone`, `xbox360`; the previous one unplugged) and presses its Guide button half a second later, or (`key`) sends an F12 press; with `glyphs=auto` the glyphs follow (`[glyphs] playstation`; `host/glyphs.h`, [input.md](../runtime/xbox360/input.md)) Also `btn:<name>` / `btnup:<name>` (hold / release a button of the virtual pad: a b x y lb rb lt rt start back ls rs dpad_*), `vibrate` / `vibrate0` (the title asks Player 1's pad to rumble; the virtual pad logs `[input] virtual pad rumbles`) ([input.md](../runtime/xbox360/input.md), "Remapping and rumble") |
| `SKY_INPUT_LOG=1` | `[input] user N: the title reads buttons 0x.., triggers L/R` at each change of what `XamInputGetState` returns (the pad remapping test) |
| `SKY_NIGHTMARE_FROM_START=0\|1` | [SwapForce] [Imaginators] setting `[<game>] nightmare_from_start`: the difficulty menu offers Nightmare; `[nightmare]` lines in the log |
| `SKY_PORTAL=auto\|real\|hid\|usb\|emulated\|none\|stall` | portal backend; `stall` = a fake real portal, always found, whose every write blocks 1 s then fails, as the first Windows PC's portal did (`host/portal_stall.h`): with `AsyncPortal` the game stays at the limit (60 fps, max gap 30 ms; before it, 31 fps and 989 ms gaps) |
| `SKY_PORTAL_PLACE=a.sky,new:<toy type>[:<variant hex>],...` | the emulated portal holds exactly these figures (file names relative to the collection; `new:100:1206` creates once and places a never-played figure of that type) |
| `SKY_PORTAL_MODEL=<id>` | the emulated portal answers `R` with this model of the identity table (`0128`, `01 48`, `020A0302`) instead of the game's own ([portal.md](../runtime/host/portal.md), "The portal's identity") |
| `SKY_PORTAL_LOG=1` | trace the portal protocol (commands, answers, status reports only when they change), `[portal] light:` and `[toybox] portal prompt: mode N, text hash 0x...`, and echo the title's own portal log |
| `SKY_PORTAL_USB=VVVV:PPPP`, `SKY_LIBUSB=<path>` | pick another USB portal; path of libusb-1.0 |
| `SKY_TOYBOX_SCRIPT`, `SKY_TOYBOX_SHOTS`, `SKY_TOYBOX_REAL_VIEW=1` | drive the Toy Box unattended / screenshots / show the real-portal view over the emulated portal The script also takes `prompt=1` / `prompt=2d` (the game asks that player for a Skylander for 3 s; `d`: and says its Skylander died) and `level` (a level starts), for the out-of-health test ([toybox_ux.md](../runtime/host/toybox_ux.md), "Out of health"); `place=<dump>`, `remove=<slot>`, `remove=all` change the emulated portal while the game plays (`host/portal_script.h`; the coverage runs' figures, [testing.md](../getting-started/testing.md)) [V] |
| `SKY_LAUNCHER_SHOTS=<dir>`, `SKY_LAUNCHER_TEST_DROP=<iso>`, `SKY_LAUNCHER_FONT_DIR` | launcher screenshot test (writes `01-no-iso.png`, `02-verified.png`, `03-settings.png`, 1280x800) and fonts folder |

### Renderer, GPU, performance
| Variable | Effect |
|---|---|
| `SKY_GPU_BACKEND=vulkan\|d3d12` | force a backend on desktop Windows (default Vulkan then D3D12) |
| `SKY_GPU_THREAD=0` | no GPU thread: the caller records and submits (bisect) |
| `SKY_CRASH_TEST=fault\|abort` | after the 400th flip the guest's render thread faults on a null pointer (or aborts), to check the `[crash]` lines ([gamemenu.md](../runtime/host/gamemenu.md)) |
| `SKY_REPORT_DIR=<folder>` | the F3 menu's "Report a problem" writes its file there alone and opens no file manager (tests) |
| `SKY_PERF_LOG=0` | turn the `[perf]` lines off |
| `SKY_RENDER_STATS=1` | `[render] frame N: D draws, R resolves, skipped S (shader) T (target) [F fps], ring jumps J, simulation passes since start-up: N run, M skipped` (the last two only count a game's `stepPassShaders`) every 60 frames |
| `SKY_FRAME_TIMES=<file>` | one line per swap: `ms-since-start work-ms draws` (`tools/perf/frames.py`) |
| `SKY_BUFFER_STATS=1` | `[buffers] frame N, per frame: acquire .. staged .. promoted .. hashed ..; write watch: N unwritten, N faults, N pages protected, N missed` |
| `SKY_RESOLVE_DEFER=0` | every resolve copy recorded at once instead of deferred to the first read of its texture or write of its target ([renderer_threads.md](../runtime/gpu/renderer_threads.md), "The GPU at scale 2"); with `SKY_RENDER_STATS=1` the GPU thread prints `[gpu] resolve copies, per frame over 600: N deferred, N copied, N by a storage swap, N dropped` |
| `SKY_WRITE_WATCH=0\|check` | `0`: no write watch, every resident range and texture is hashed as before (to measure, or to rule it out); `check`: also hash what the watch calls unwritten and log `the write watch missed a change` for each difference ([renderer_threads.md](../runtime/gpu/renderer_threads.md), "The write watch") |
| `SKY_TEXTURE_STATS=1` | `[textures] last 60 frames, per frame: N checked, N hashed whole (MB), N by sample; N changes of sampled textures found, N missed by the sample`; also hashes every sampled texture whole to count (and log) the changes its sample missed ([renderer_threads.md](../runtime/gpu/renderer_threads.md), "Textures") |
| `SKY_GPU_SNAPSHOT=<prefix>`, `_EVERY=<n>` (default 300), `_FROM=<frame>` | save the presented image as PPM every n frames (for 36 000 frames), offscreen output only; `_EVERY=1` + `_FROM` = consecutive frames |
| `SKY_GPU_SNAPSHOT_SECONDS=<s>,<s>,...` | also save the first image presented at or after each of these seconds as `<prefix>_t<s>.ppm`: the same moments at any frame rate (the game-speed check, [frame_pacing.md](../engines/lux/frame_pacing.md)) |
| `SKY_SNAPSHOT=<prefix>` | CPU-frame PPM every 100th frame (up to 20; Bink stopgap, before the GPU path) |
| `SKY_D3D_CAPTURE=<s>` or `f<N>` | log every draw/clear/resolve/swap of the first frame after `<s>` seconds, or of the frame after the N-th swap ([below](#frame-capture)) |
| `SKY_DUMP_FRAME=<s>` or `f<N>`, `SKY_DUMP_PREFIX=<p>`, `SKY_DUMP_DRAWS=1` | save every resolve target of one frame (and the presented one; with `_DRAWS` the target after every draw) as `<p>_<n><tag>_<w>x<h>.ppm`; also logs `[dump]` lines with depthctl/stencil/targets, textures, constants; `f<N>` picks the same frame as `SKY_D3D_CAPTURE=f<N>` |
| (a player's recording) | the distinct pictures a second (`ffmpeg -i <clip> -vf mpdecimate -f null -` or a per-frame difference) tell the frame rate the game really ran at: Swap Force at `fps_limit=30` shows at most 30 (interval 2); more means the game ran at 60 whatever the launcher showed ([SwapForce], 2026-10-28) |
| `SKY_DUMP_FRAMES=<n>` | with `SKY_DUMP_FRAME`, n consecutive frames (default 1), each opened by `[dump] frame <swap> at <seconds>`: the frame-to-frame change of a constant (an animation time, previous-frame matrices) or of a target; without `SKY_DUMP_PREFIX` only the `[dump]` lines are written (lighter) |
| `SKY_DUMP_FLOATS=1` | with `SKY_DUMP_FRAME`, every texture the dump would save (with `SKY_DUMP_DRAWS`, every target of every draw, not only the first) is read back as floats, one channel at a time, and scanned: `[dump] floats file NN<tag> fmt F WxH [BAD] r nan N inf +N/-N neg N den N [min, max] mean M g ... b ... a ... [first at X Y]` (`BAD`: a NaN or an infinity, the first one's pixel). A cube or array texture is read as its first slice (face 0, level 0; before 2026-11-10 Metal returned garbage for a cube, Trap Team's `#132`). A NaN written into an 8-bit target is stored as 0: only float targets can show `BAD`. No picture without `SKY_DUMP_PREFIX`, so a whole frame is a log of a few hundred KB: what to ask a tester whose GPU we do not have; `tools/regress/compare_dump_floats.py <ours> <theirs>` lists the first targets whose means differ. Compare two runs with the same `fps limit`, scene timing (intro movies played or skipped) and anisotropy (the `[settings]` line): the Mac's 30 fps log against a PC's 60 fps one differs from the first moving model on (its velocity targets halve), which is not a GPU difference. `[dump] clear target i fmt F WxH to r g b a` lines show the frame's clears; `[dump]   target i blend E S/D mask M [(the pixel shader does not write it)]` after a draw's line, each target whose blend or write mask is not target 0's, or that its pixel shader leaves alone (masked: it keeps its contents) |
| `SKY_DUMP_VS_CONSTANTS=<n>` | with `SKY_DUMP_FRAME`, also the vertex shader's float constants c0 to c<n-1> per draw (`[dump]   vs c0-c<n-1> ...`): matrices, bone palettes, the previous frame's matrices of a motion blur |
| `SKY_DUMP_ALPHA=1` | with `SKY_DUMP_FRAME`, also each dumped texture's alpha as grey (`<p>_<n><tag>_<w>x<h>a.ppm`): Swap Force keeps the depth of field's weight and the FXAA luma in alpha; the `[dump]` lines also give pixel shader constants c0-c5 (`ps c0-c5`) |
| `SKY_DUMP_SHADERS=<dir>` | keep every shader container the game creates as `<XXH3>.vs/.ps` (once each), including run-time-built ones |
| `SKY_PRESENT_PROBE=<n>` | every nth swap, `[present]`: the texture the swap presents (address, fetch words: size and format) and a readback of it (pixels lit, mean); `NOT FOUND` when the texture is unknown and nothing is presented. Reads from the GPU: a test aid, slows the frame. Offscreen frames do not prove a window ([render_bugs.md](../runtime/gpu/render_bugs.md)) |
| `SKY_AUDIO_DUMP=<path>` | the stereo float32 48 kHz output as a raw file |
| `SKY_TEXTURE_DUMP=0\|1`, `SKY_TEXTURE_PACKS=0\|1` | settings (`[video] texture_dump`, `texture_packs`): write every texture once as DDS to the game folder's `textures-dump/`; use the pack in `textures/` ([texture_packs.md](../runtime/gpu/texture_packs.md)) |
| `SKY_TEXTURE_DUMP_LOG=1` | with the dump on, logs `[texpack] dumped <file>: guest hash <hash>, WxH`: the hash the glyph tables use (finding a game's glyphs) |
| `SKY_BINK_LOG=1` | once a second, `[bink] t=<s> frame <n> of <total>` for the last opened movie, to check its pace against the wall clock ([Giants] Bink 2, [bink.md](../engines/lux/bink.md)) |

### Launcher / build (Arkeyan Launcher, Arkeyan-Builder)
`SKY_LAUNCHER_DATA`, `SKY_LAUNCHER_KIT`, `SKY_LAUNCHER_RUNTIME`, `SKY_LAUNCHER_NO_UPDATE_CHECK`, `SKY_LAUNCHER_UPDATE_URL`, `SKY_LAUNCHER_LOG_DIR`, `SKY_LAUNCHER_BUILD_JOBS`, `SKYLAUNCHER_VERSION` (CMake cache variable), `FAKE_SKYBUILD_FAIL`, `FAKE_SKYBUILD_DELAY`, `SKY_LAUNCHER_TEST_INSTALL=1|real`, `SKY_LAUNCHER_TEST_EVERY`, `SKY_LAUNCHER_TEST_UPDATE=1` ([launcher/](../release/launcher/README.md)). Build-side: `JOBS`, `LLVM_MINGW_ROOT`, `FFMPEG_WINDOWS`, `WINE`, `WINEPREFIX`, `EXE`, `VALIDATE=1`, `SAVE_FROM`, `ISO`, `SECONDS_TO_RUN`, `ARKEYAN_UWP_MARCH`, `ARKEYAN_SOURCE_FONT_DIR` (compile definition); Xbox packaging: `SKY_PACKAGE_NAME`, `SKY_DISPLAY_NAME` (`tools/xbox/package.sh`), `SKY_XBOX_TOOLS` (`tools/xbox/setup_tools.sh`).

Arkeyan-Builder's own options (`--iso --out [--kit] [--jobs] [--toolchain]`): [../tools/arkeyan-builder.md](../tools/arkeyan-builder.md).

## Log line tags and how to read each

All logs go to stderr (= `Arkeyan.log`, flushed per line). Grep, don't read.

| Tag | Where | Meaning / how to read |
|---|---|---|
| `[log]`, `[version]`, `[system]`, `[settings]`, `[platform]` | start-up | file path; `ArkeyanRuntime 0.1.0, build <rev>[-dirty]`; OS/CPU/RAM (AVX flags); effective settings; `[platform] cannot commit N MB up front: guest memory is committed on first access` = the lazy-commit fallback |
| `[loader]` | `loader.cpp` | `xboxkrnl.exe!KeTimeStampBundle (ordinal N) -> 0x...`: variable imports resolved |
| **`[stub]`** | `kernel/stubs.cpp` | `[stub] Name (lr=.. r3=.. r4=..)` **once per unimplemented import**: grep after every new screen; each stub that runs must write its outputs ([xbox360/stubs.md](../runtime/xbox360/stubs.md)) |
| `[kernel]` | `kernel/*` | `unsupported dispatcher object type`, `0x... is not an event`, `ObReferenceObjectByHandle: no guest object`, `NtAllocateVirtualMemory: out of memory`, `MmAllocatePhysicalMemoryEx: out of memory / exceeds max address`, `XexLoadImage: cannot load`, `XexGetProcedureAddress: ordinal 0x.. not provided` (**a new run-time lookup**), `RtlRaiseException 0x.. ignored` |
| `[thread]` | `kernel/threads.cpp` | `N created: start 0x.. context 0x..[ (suspended)]`, `N resumed`, `N named "JQ Worker"` (the SetThreadName exception) |
| `[file]` | `kernel/files.cpp`, `vfs.cpp` | `open <path> -> 0xSTATUS` (0xC0000034 name not found is normal for probes), `read/write ...` with `SKY_TRACE_FILES`, `read <disc name> @P len N: K failed attempt(s), system error E; read in full on a retry` or `...; the rest is missing (the game sees the end of the file)` (a host read that failed: issue #26; E is errno or `GetLastError`, -1 for the test aid), `symbolic link A -> B`, `unsupported class N` |
| `[content]` | `kernel/xam_content.cpp` | `device selector (...) -> device 1`, `create lux: "<name>" type 0x.. flags 0x.. -> result (disposition n)` (`0x13 -> 3` = probe), `close lux -> 0` |
| `[xconfig]`, `[xmsg]`, `[achievement]` | XAM | `setting c/s not available` (a new XConfig the title asks), `unhandled message 0x.. to app 0x..`, `user U unlocked achievement N` |
| `[xma]` | `kernel/xma.cpp`, `apu/` | `out of contexts`, `input buffer 0x.. is not in physical memory` (XMA buffer from the wrong allocator), `cannot open FFmpeg's WMA Pro decoder` |
| `[audio]` | `host/audio_xaudio2.cpp` | UWP: `XAudio2 unavailable: no sound` |
| `[language]` | `games/<id>/language.cpp` | SSA: `text eng, voices eng` at audio init; Giants: `voices fr: <archive>.bld:/FRENCH.pak loaded before ...` (once), `movie voice track 9 (fr)` |
| `[heap]` | `patches.cpp` | `free of 0x.. skipped (block already free \| next block corrupt \| previous block corrupt), lr ..` (max 32): **keep the log**: payload and caller |
| `[guard]` | `indirect_call_guard.cpp` | `bad indirect call to 0x.. skipped: lr .. r3 .. (*r3 ..) r31 .. (*r31 ..) thread N "name" (K so far)`: first word of r3/r31 = vtable of a C++ object; r31 reads 0 since the translations keep it as a local ([xenonrecomp.md](../tools/toolchain/xenonrecomp.md#registers-as-locals)) |
| (no tag) `Indirect call to unrecompiled guest code (ctr=.. lr=..)` | `loader.cpp` | **aborts**: a missing function boundary; `Fault (...) at guest address 0x..`; `Main thread exited with 0x..` |
| `[render]` | `d3d/render.cpp` | `no translated vertex\|pixel shader <hash> (primitive N): its draws are skipped`, `run of command buffer 0x.., which was not recorded`, `resolve with exponent bias N: not applied`, `computed-index fetch of vertex format N: not supported, reads 0`, `polygon offset first used: frame N, pixel shader <hash>, slope scale S, offset O` (once a run), `frame N: ...` stats |
| `[gpu]` | `gpu/*` | `Vulkan\|D3D12\|Metal on <name>, swapchain output`, adapter/window/scale lines, `shader cache: N shaders`, `pipeline list: N pipelines (A from pipelines.bin, B more of the baseline's C)`, `pipeline list: N pipelines ready after X ms`, `loading (<file>) waited X ms for N of the M listed pipelines`, `pipeline built at draw time: X ms (N built, T ms in all)` (**should never print on a warm baseline**), `draw waited X ms for a listed pipeline`, `no GPU thread (...)`, `first present WxH`, `step N: <what>`, `D3D12 device removed: reason 0x.. ...` + DRED lines + `video memory ...`, `D3D12 pipeline cache ...`, `texture WxH xN, format F, M mips: X MB (T MB in all)` (>= 16 MB, every backend since 2026-11-12; T: every texture alive, a third more for mip chains), `a texture heap is full (16384 textures): the next ones sample black` (once), `built without a shader cache ...: the game's draws are skipped` |
| `[perf]` | `gpu/gpu.cpp` | every 10 s: fps/median/p95/draws/GPU wait; render-thread split; GPU thread busy; `hint: the GPU limits the frame rate ... resolution_scale=1` ([gpu/renderer_threads.md](../runtime/gpu/renderer_threads.md)) |
| `[buffers]`, `[capture]`, `[dump]` | debug aids | see recipes below |
| `[portal]` | `host/portal*`, `kernel/portal.cpp` | `mode auto (SKY_PORTAL)`, `hid: enumerated 1430:0150 "..." usage page FF00`, `hid: opened`, `wrote the first command, 'R'`, `read the first report`, `no real portal plugged in: emulating one`, `emulated portal`, `figure folder: ...`, `N lock(s) left by programs that are gone removed`, `emulated portal ignores command 0x..` (a command of a later game), `< / ->` protocol lines with `SKY_PORTAL_LOG`, `placed <file>`, `N figure(s) back on the portal from <portal.txt>` |
| `[collection]` | `host/collection` | `built-in catalogue: ...`, `catalogue additions from ...`, `migrated <old>: N copied, M already there, K failed (originals kept)`, `import: ...`, `tidy: N never-played figure(s) ... moved to the trash`, `<file>: <reason>; kept as ...` (a `collection.json` that failed to load) |
| `[toybox]` | Toy Box | `P1: the game took <name> (slot n, x s after the change)` |
| `[glyphs]`, `[input]` | host | `[glyphs] playstation` (a style change), `[input] gamepad <name> (<GUID>)` |
| `[movie]`, `[bink]` | `engines/lux/movie.cpp`, `d3d/bink_display.cpp` | `[movie] skipped <name>` (intro skip); `[bink] open <name> WxH, N frames`; `[bink] t=.. frame n of N` with `SKY_BINK_LOG` |
| `[video]` | `kernel/video.cpp` | `present interval 0x.. seen (fps limit N)`: the title's own present interval |
| `[shader]` | `d3d/shaders.cpp` | `vertex\|pixel object 0x..: no container at +0x..`: a shader object the runtime cannot read |
| `[ultrawide]` | `engines/lux/ultrawide.cpp`, `games/giants/adapter.cpp` | the Hor+ changes: `texture WxH -> WxH` [SSA], `camera aspect a -> b` [Giants] |
| `[texpack]` | `host/texture_pack.cpp`, `d3d/render.cpp` | pack index problems (`ignored <file>: the name is not 16 hex digits`), dump writes; `dumped ..: guest hash ..` with `SKY_TEXTURE_DUMP_LOG` |
| `[saves]` | `kernel/files.cpp` | `backup <time>` before a save is replaced; `<path>: <error>` when the atomic write fails |
| `[art]` | `host/art/game_art.cpp` | the disc pictures: `cannot write ..`, `the built-in manifests do not load: ..` |
| `[uwp]` | UWP host | `view activated (kind N)`, `window visible/hidden`, `disc image <path>, swap chain WxH`, `figure collection <folder>`, `no Activated event after 5 s: activating the window anyway`, `SystemNavigationManager unavailable: B may send the app to Home`, `Windows.Gaming.Input unavailable: no gamepad`, `device form <Xbox\|...>` |

## Recipes

- **Fast run to level_027 (~75 s)** [SSA], its variants and Giants' presses: [testing.md](../getting-started/testing.md#the-fast-recipe-ssa-to-level_027-in-75-s-ssa).
- **Frame times**: `tools/perf/level_run.sh <exe> <out> [scale] [level]` (headless fast recipe, `SKY_FRAME_TIMES`) then
  `tools/perf/frames.py`. **Pipeline tours**: `tools/pipelines/tour.sh` (SSA), `tools/pipelines/tour_giants.sh`.
- **Platforms**: `tools/windows/d3d12_test.sh` (D3D12 under vkd3d-proton), `tools/windows/wine_smoke_test.sh`,
  `tools/linux/smoke_test.sh`, `tools/linux/kit_test.sh`, `tools/shaders/check_dxil.sh` ([platforms/](../getting-started/platforms/README.md)).

## Unit tests

All are targets of `runtime/CMakeLists.txt` (`ArkeyanRecompGpuTest`: `runtime/gpu/CMakeLists.txt`), need no game data unless
stated, and print `N checks, M failures` (exit code 0 = pass). `tools/test_all.sh` builds and runs every one marked H, in
both build folders (`ArkeyanRuntimeLauncherTest` in SSA's only).

| Test | Checks | H |
|---|---|---|
| `ArkeyanPortalTest` | the emulated portal's protocol: the command sequence SSA sends, placing and removing figures, block reads and writes, write-back; both games' portal prompt classifiers | H |
| `ArkeyanPortalAsyncTest` | real-portal I/O off the title's threads with a fake device that stalls: game-side calls stay under 5 ms, retries back off 2x up to the maximum | H |
| `ArkeyanFigureTest` | the figure library: crypto test vectors, tag encryption and progress round trips, the catalogue, cross-game round trips (SSA, Giants, SSA); `--make` writes a figure for a run, `--same-extra` compares two dumps' extra areas | H |
| `ArkeyanArtTest` | the disc pictures: texture, archive and image readers on synthetic data; with `SSA_ISO` / `GIANTS_ISO`, each game's pictures read from its disc into a scratch `SKYLANDERS_DATA` | H |
| `ArkeyanCollectionTest` | the collection library in a scratch collection (never the player's) | H |
| `ArkeyanSettingsTest` | the settings parser: `launcher.ini`, the environment, the command line, precedence and warnings | H |
| `ArkeyanDatadirTest` | the data folder helper and its migration on fake trees (not built on Windows) | H |
| `ArkeyanSavesTest` | save backups, atomic writes, restoring, the running game's marker (not built on Windows) | H |
| `ArkeyanGuestFormatTest` | the guest's printf (`kernel/guest_format.h`): flags, width, precision, `*` arguments, the Microsoft conversions, on a fake guest memory | H |
| `ArkeyanProgressTest [<disc>...]` | the XDBF achievements reader (synthetic data, damaged data, each given disc's counts and gamerscore), the launcher's cache, the unlocks' round trip, the play time (not built on Windows) | H |
| `ArkeyanAchievementsKernelTest <disc>` | the achievement calls of a title on a fake guest context against a disc: unlock, notice, enumeration, a second process reading it back (not built on Windows) | H |
| `ArkeyanGuestThreadTest` | the guest threads with the game's code range and no game code: host calls to an address outside the code refused and logged (thread start, APC), 2000 thread create/exit cycles on a 256 MB system heap, a re-initialised dispatcher header replacing its object (not built on Windows) | H |
| `ArkeyanTexturePackTest` | the DDS reader and writer, the content hash and the texture pack index (`EXCLUDE_FROM_ALL`) | H |
| `ArkeyanRecompGpuTest [<dir>]` | draws test images through the present pass offscreen and compares them with a CPU reference (bilinear, clamped, letterboxed); writes `gpu_<case>_{actual,expected}.ppm` when given a folder | H |
| `ArkeyanRuntimeLauncherTest <iso> <scratch dir> [<damaged copy>...]` | the runtime launcher's disc verification and settings file, without UI; needs SSA's disc (`EXCLUDE_FROM_ALL`) | H (SSA) |
| `ArkeyanPresentTest` | the Xbox present test package (`tools/xbox/presenttest`), UWP builds only, built on request: run on a console, not by the harness | - |

## Capture, search and triage

### Frame capture
`SKY_D3D_CAPTURE=<s>` prints, for one frame starting `<s>` seconds after launch (`[capture] frame starts after swap at ..`): each draw `[capture] draw N DrawIndexedVertices prim P base B start S count C ib ADDR (common .. addr ..)` / `DrawVertices` / `BeginVertices`, the shaders `vs=ADDR(HASH) ps=ADDR(HASH)`, `declaration ADDR` (once per declaration), `stream S vb ADDR fetch .. .. stride`, `tex S ADDR common .. fetch ...`, `viewport .. scissor ..`, the **shadow** registers (`regs surface .. color .. depth .. mask .. depthctl .. blend0 .. colorctl ..`) **and a `gpu` line with the mirror values**, `clear flags .. rect .. color`, `resolve flags .. rect ..`, `swap front .. ; end of capture (N draws)`. **Diff the shadow and `gpu` lines before chasing any wrong blend/depth/stencil.**
`SKY_DUMP_FRAME=<s> SKY_DUMP_PREFIX=<p>` writes every resolved texture of the frame as PPM; `[dump]` lines per draw with viewport, scissor, blend, depthctl, stencil, targets, the guest shader hashes (`vs`, `ps`), `sumode` (`PA_SU_SC_MODE_CNTL`: cull, polygon offset enables in bits 11-13) and the four `polyoffset` registers (front scale, front offset, back scale, back offset), inputs, streams, textures (size, format, mip levels, min/mag/mip filter, anisotropy, LOD range, bias, the fetch constant's dwords 3-5), VS constants c0/c4, and per resolve `flags src WxH rect dest depthsurf (file NN)` (the PPM it wrote). An `R32_FLOAT` texture (depth resolves, linear depth) is read back raw: `[dump] float file NN: min, 10%, median, 90%, max` and its PPM is grey from min to max.
**One frame, both aids** (2026-10-27, the Swap Force level frame): `SKY_D3D_CAPTURE=f2300 SKY_DUMP_FRAME=f2300 SKY_DUMP_PREFIX=<dir>/d` in one run interleaves the `[capture]` lines (state on entry: the `gpu` registers lag one draw, the targets do not) with the `[dump]` lines and the file numbers; `private/swapforce/capsum.py <log>` (not tracked) folds them into passes (targets as `tileNNN/f<format>/WxH`, textures read, resolves with their file). Shader hash -> HLSL: match `private/shaders/dump/<hash>` with `private/shaders/exp/in/*`. `SKY_DUMP_SHADERS=<dir>` saves the shaders built at run time for the cache (`tools/prepare_game.sh --shader-dump <dir>`).

### Finding a function without symbols
`tools/ppcinfo.py` (`find-string`, `callers`, `callees`, `strings`, `asm`) over `private/image.bin` (`--game giants`: `private/giants/`); RTTI names by regex; vtables from RTTI; `private/<id>/ppc/*.cpp` is greppable (instruction comments); write-protect a page to find its writer/reader; hook the engine's printf-like loggers; `sigmatch` for a known function of another game ([toolchain/analysis_tools.md](../tools/toolchain/analysis_tools.md)). **Strings that identify engine functions** are listed in [lux/binary_layout.md](../engines/lux/binary_layout.md).
Profilers: macOS `sample <pid> 10` (never `4 1`); `xctrace record --template 'Time Profiler' --attach <pid>`.

### Finding who writes a value (store watch) [Xbox]
When a value in guest memory is known (a memory scan found it) but not the code that writes it, build a diagnostic
runtime in its own folder whose generated code reports its stores. `ppc_context.h` defines `PPC_STORE_U32` only
`#ifndef`, so a header given with `-include` can define it first: call a function with the address, the value and
`__func__` (the guest function's name), then store as usual. Log the first store per (function, address, value) for a
list of addresses (`SSA_WATCH=A5B3688C,...`) and for a value (Giants: the float 16:9, `0x3FE38E39`):
```sh
cmake -S . -B build-watch -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_GAME=giants "-DCMAKE_CXX_FLAGS=-include $PWD/private/watch.h"
nice cmake --build build-watch -j4 --target ArkeyanRuntime   # a full rebuild of the generated code (~30 min)
```
It runs at playable speed. In Giants it named in one run the writers of the cameras' aspect (`sub_82198DB0`), of the
display-aspect global (`sub_8219B900`) and of the scene size (`sub_82196600`): [ultrawide.md](../runtime/gpu/ultrawide.md). In Swap Force (build ~8 min, `-DARKEYAN_GAME=swapforce`) a header that logs the stores of 1040, 620 and their floats, only when the other of the pair sits next to it, named the render-target pass that computes the scene's size (`sub_8258EBE8`) in one run.
Protecting the host page instead (`mprotect`) stops at the first write to any object of the 16 KB page, usually
another one. Keep the header and the build folder in `private/` (they hold nothing of the game, but the build does).

### Aids that are game-specific (redo them for a new game)
The press timings and the first level's name (`SKY_PRESS` recipes, `tools/perf/level_run.sh`, `tools/pipelines/tour.sh`);
the movie skip hook (`Movie_Update` in Lux, `BinkOpen` for Giants); the first level for `SKY_LEVEL` (the adapter's
`firstLevel` file path for SSA and Giants; Swap Force's `Zone_IndexByName` hook and `game::FirstZone`); the text and voice
language hooks (the game's adapter); the pipeline baseline (`games/<id>/pipelines/baseline.zst`, shader hashes); the glyph
texture hashes (`host/glyphs.cpp`); the portal prompt reader (`games/<id>/<id>_portal_prompt.*`); the harness's expected
log lines and references. The D3D capture and dump aids need the game's D3D hooks. Steps: [day1.md](day1.md). Swap Force's press recipe and `SKY_LEVEL` [SwapForce]:
[testing.md](../getting-started/testing.md#swap-force-to-the-tutorial-in-25-60-s-swapforce).

### Crash triage
1. Which kind: `Fault (...) at guest address` (guest memory), `Indirect call to unrecompiled guest code` (function boundary), `[guard]`, `[heap]`, a hang (grep `[stub]`, check `[thread]`, `sample` the process), a black screen (frames counted? `[render] no translated ... shader`, `[gpu] built without a shader cache`).
2. POSIX fault reports print thread name, sp/fp, pc/lr symbolised, a frame-pointer walk, the load address (`atos -o <exe> -l <addr> <addresses>`); Windows prints module+offset per frame.
3. A crash that never reproduces: apply fences, ship the guards, keep logs, move on.
