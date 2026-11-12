# Guide: from playable to perfect, every fix a game gets

> **Purpose:** the checklist a new game goes through after it boots: every fix, patch and optimisation SSA, Giants and Swap Force needed, where it lives and how to verify it · **Status:** current (2026-10-29) · **Tags:** [Xbox] [XDK] [Lux] [Alchemy] [SSA] [Giants] [SwapForce]

The new-game steps take a disc to **playable**. This page takes it from playable to **perfect** (the owner's bar,
2026-10-28: "every little thing", depth before breadth, 60 FPS for real, a sober UI). It is an inventory, not a story:
one row per fix, the detail stays in the canonical page it links. Next games: Trap Team, SuperChargers, Imaginators
(last section).

How to read a row. **Where**: *runtime* = shared host code every game gets for free (check it, do not redo it);
*engine* = Lux or Alchemy code, expect it in the engine's other games at other addresses; *game* = `games/<id>/`, re-found
per game (rule zero: never reuse an address). **Games**: SSA / Giants / Swap Force (SF), ✅ has it, ❌ not done or not
proved, n/a does not apply. The proof of a row is a run or a test ([parity.md](../porting/game_template/parity.md) rule:
a row is ✅ only with one). Sources: [problem_catalogue.md](../porting/problem_catalogue.md),
[gotchas.md](../porting/gotchas.md), pitfalls.md, each game's `parity.md` and `status.md`,
`git log`.

## 1. Boot and saves
| Symptom (player sees) | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| Rare crash at the first menus, ARM64 only | real fences for `lwsync/sync/eieio` (D1) | runtime: `patches/xenonrecomp.patch` | `tools/test_all.sh` soaks | ✅ ✅ ✅ |
| Crash on x64 only (Windows, Wine) | divides by 0 / INT_MIN/-1 as ARM64; jump tables switch on the 32-bit index (Giants faulted at start) | runtime: same patch | Windows kit run under Wine | ✅ ✅ ✅ |
| "Indirect call to unrecompiled guest code" at a menu | packed leaf functions without `.pdata`: `packed_leaves` pass; function starts cross-checked with `.pdata` [?] | tools: `find_switch_tables.py`, `games/<id>/config` | log has no such line | ✅ ✅ ✅ |
| Long wait after START, empty save names, wrong country, silent music | stubs that "succeed" without writing outputs (D5): XNet offline, real XConfig, `vsprintf`, TIME_FIELDS, XMP answers "title has control" (D9, system version 2.0.17559) | runtime: `kernel/` ([stubs.md](../runtime/xbox360/stubs.md)) | grep `[stub]` on every new screen | ✅ ✅ ✅ |
| "Save content is damaged" on a new game | a missing save gives `ERROR_PATH_NOT_FOUND` from `XamContentGetCreator`; content probes `flags 0x13 -> 3` are normal (D38) | runtime `kernel/xam_content.cpp` ([xam_content_saves.md](../runtime/xbox360/xam_content_saves.md)) | new game from empty `SKY_SAVE_DIR` | ✅ ✅ ✅ |
| Achievement check crashes / hangs | enumerator that enumerates nothing | runtime `kernel/xam_achievements.cpp` | title reaches the menu | ✅ ✅ ✅ |
| Game stops at the save slots under load | a race in the game's own save tasks (the start clears a byte the worker sets last): hook the poll | game: `games/swapforce/save_tasks.cpp` | zone tour, 195 runs | n/a (not seen) n/a (not seen) ✅ |
| Quit crashes, report cut short (D28) | `_Exit` at once on quit | runtime `main.cpp` | quit from a run, log complete | ✅ ✅ ✅ |
| Save lost on a crash or power loss | atomic writes (`.savetmp`, fsync, rename), backups before each overwrite, restore from the launcher | runtime `kernel/files.cpp` ([settings.md](../getting-started/settings.md) "Saves and their backups") | `ArkeyanSavesTest`; `.savetmp` absent after a run | ✅ ✅ ✅ (a kill mid-save proved on none but SSA's tests [?]) |
| Heap or render crash 1 run in 40 | guards that log and skip (`[heap]`, `[guard]`); ship them on day one, do not hunt more than an hour | engine Lux: `engines/lux/patches.cpp`, `indirect_call_guard.cpp` | log lines | ✅ n/a n/a |
| Rare start crash after the first movies | buffer cache takes the producer lock (D-class race with the main-thread present) | runtime `gpu/buffer_cache.cpp` | `ArkeyanRecompGpuTest` "producers" | ✅ ✅ ✅ |
| Data in the wrong folder | the game's own folder name (`GameFolderName`: `SwapForce/`), one data root, window title "name + version" | runtime + `games/<id>/game.txt` | `ArkeyanDatadirTest`, `kit_isolation.sh` | ✅ ✅ ✅ |

## 2. Rendering and resolution scale
Day-1 renderer bugs are in the catalogue ([problem_catalogue.md](../porting/problem_catalogue.md), "GPU"); this table is
what is left once the scene draws.

| Symptom | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| Dark or pink 3D, menus without backgrounds | the ring mirror and command-buffer replay (the engine writes state into the ring) | engine Lux: `d3d/ring.cpp`, `d3d/command_buffers.cpp` | level shot against a real-360 shot | ✅ n/a n/a (Alchemy has no command buffers, [?] no ring writes) |
| White scene, resolve parameters 0, shadow maps black | hook the clear worker, read Resolve's call arguments (D10), depth resolves into R32F | runtime `d3d/render.cpp` | level shot | ✅ ✅ ✅ |
| Blocky lighting and no sun shadows | write a GPU resolve back to guest memory when a CPU job reads it (Lux sun tiles). In a new game, find CPU readers of resolve addresses first | engine Lux, `render.cpp`, `gpu/` | shot of a sunny level | ✅ n/a n/a |
| Terrain flickers (depth EQUAL) | Metal `-fpreserve-invariance` (D16) | runtime `patches/xenosrecomp.patch` | level shot, 60 s | ✅ ✅ ✅ |
| Particles, sun sprites, grass missing | point sprites, `param_gen`, vertex texture fetch (grass 1122/1122) | runtime XenosRecomp patch | `SKY_LOG_SHADER`, shot | ✅ ✅ ✅ |
| Whole 3D one colour wash (Swap Force) | keep the inverted viewport depth range | runtime `d3d/render.cpp` | SF level shot | ✅ ✅ ✅ |
| Lit scene black, zoomed quarter of the G-buffer | unnormalized texture fetches, scaled for resolves | runtime XenosRecomp patch, `gpu/draw.*` | SF level shot at scale 1 and 2 | ✅ ✅ ✅ |
| Blur and shadow softness change with the scale | fetch offsets in guest texels, multiplied by the scale (all games, default scale 2) | runtime XenosRecomp patch | shots at scale 1 and 2 equally soft | ✅ ✅ ✅ |
| Light passes break at scale 2 | rebuild the shader cache after any XenosRecomp patch change | `tools/shaders/build_cache.sh`, Arkeyan-Builder | cache rebuilt, shot | ✅ ✅ ✅ |
| Blurry textures, green lava | anisotropy only for textures with mipmaps | runtime `SamplerFor` | SF lava shot | ✅ ✅ ✅ |
| Decals depth-fight the surface under them: patches shimmer from frame to frame (Swap Force's lava canyon and crystal tunnel) | the polygon offset (`PA_SU_POLY_OFFSET_*`) as the pipeline's depth bias, times the resolution scale | runtime `d3d/render.cpp` (`PrepareDraw`) | `SKY_DUMP_FRAME`: `[dump]` lines with `sumode` bits 11-13; `analyze.py shimmer` on consecutive frames (harness) | ✅ ✅ ✅ (SSA's `level_027` and Giants' harness runs never ask for it; SF's tutorial and `brn2` do) |
| Stretched 3D (SF draws 1040x620 and stretches) | full-resolution scene by default, `original_resolution=1` keeps the console's | game `games/swapforce/ultrawide.cpp`, adapter `fullScene` | log `[settings]`; shot | n/a n/a ✅ |
| Frontend broken in two tiles | `ExpandTiledTargets` for predicated tiling | game adapter | title shot | n/a ✅ n/a ([?] SF) |
| An Alchemy shader never translates | the game builds shaders itself: hook `D3DVertexShader_Init` / `D3DPixelShader_Init`; extract from the IGA with Arkeyan-Builder; run-time shaders (Bink 2) dumped with `SKY_DUMP_SHADERS` | game + `tools/arkeyan-builder/igz_shaders.cpp` | cache count equals the disc's (SSA 1122, Giants 4443, SF 1074 containers) | ✅ ✅ ✅ |
| 1D-texture pixel shader draws nothing | 1D fetch in XenosRecomp | runtime patch | Giants menu shot | n/a ✅ n/a |
| `auto` scale too heavy on a laptop GPU | scale 2 on Apple and discrete GPUs, 1 on integrated; log says why | runtime `gpu/gpu.cpp` | `[gpu] resolution scale auto` | ✅ ✅ ✅ |
| Window resize freezes the game, then magenta | `WaitForSubmissions` flushes the piece being recorded | runtime `gpu/` | `tools/perf/resize_run.sh` (a real window; owner-run, never drive the real mouse) | ✅ ✅ ✅ |
| Vulkan or D3D12 only: black terrain, device removed | strict Vulkan validation run, committed targets, typeless depth plane format | runtime `gpu/`, [plume_backends.md](../runtime/gpu/plume_backends.md) | `tools/regress/vulkan_validation.sh` | ✅ ✅ ✅ |

## 3. Ultrawide
Hor+ camera, wider scene targets and frame, the HUD and menus centred at their own 16:9, movies with bars
([ultrawide.md](../runtime/gpu/ultrawide.md)). All of it is per game hooks over one runtime rule.

| Piece | Where | Verify | SSA / Giants / SF |
|---|---|---|---|
| Camera aspect (the global the cameras read) | game hook (SSA camera-aspect global; Giants display-aspect global `0x826565C0`; SF world camera) | `SKY_WINDOW_SIZE=2560x1080` shot, then 3440x1440 | ✅ ✅ ✅ |
| Scene targets at the window's shape, same pixel budget (EDRAM limit on Alchemy) | game/engine: SSA targets, Giants `min(back buffer, 1000x600)`, SF `sub_8258EBE8` | shots; no crash at save load | ✅ ✅ ✅ |
| Wide frame, composite, sun-tile grid | runtime + game | shot | ✅ ✅ ✅ |
| Movies stay 16:9 with bars | runtime `bink_display.cpp` | `fmv01` shot at 2560x1080 | ✅ ✅ ✅ |
| HUD anchored to the window edges (today: centred) | needs the UI's screen width, which brings parked off-screen notices into view | game | 21:9 shot | ❌ ❌ ❌ (known limit) |
| Shape read once at start; menus stay 16:9 | by design | - | ✅ ✅ ✅ |

## 4. Performance
Rule: measure on an idle machine, with a cold app clone (D32), `SKY_SETTINGS` set (a player's `fps_limit=60` skewed tests),
and run an older build beside HEAD before bisecting (pitfalls.md, time sinks 7 and 14).
Inventory with numbers: [renderer_threads.md](../runtime/gpu/renderer_threads.md) ("Optimizations per game").

| Symptom | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| Hitch at each new level or figure (a pipeline built at draw time, 20-67 ms on cold Metal) | pipeline list built by 4 threads during the intro movies, per-player `pipelines.bin`, and a **baseline** built into the executable from a tour, waited for at the loading open | runtime `gpu/draw.cpp`; trigger in `kernel/files.cpp` per game; `games/<id>/pipelines/baseline.zst`, `tools/pipelines/tour*.sh` ([pipeline_cache.md](../runtime/gpu/pipeline_cache.md)) | draw-time builds per level (log `[gpu] loading ...`): target 0-7 | 1570 / 2363 / 738 |
| Baseline misses a figure no tour used | tour with several figures (one of each kind the game sells), `TOUR_FIGURE`, `merge_lists.py`; re-run `cmake` after adding the file | tools | a figure no tour used builds 1-6 per level | ✅ ✅ ✅ (SF: 11 of 16 swappers untoured) |
| Level load 3-9 s | **host decompression**: Lux zlib `inflate` (`inflate.cpp`, miniz on Windows), Alchemy LZMA decoder loop (`engines/alchemy/lzma.cpp`). Find the decoder on day 1 (error strings, sigmatch) | engine | `tools/perf/load_times.py` | 3.4 -> 0.4 s / 1.7-2.3 -> 1.0-1.2 s / 5.6-8.7 -> 4.9-5.3 s |
| Render thread saturated | GPU thread (record and submit), texture threads (up to 4), chunked submission, bind framebuffer / root signature only on change, buffer cache keyed by the **range read** (not address+size, D18), constants deduplicated, sampler slots hashed, `PhysicalToGuest` per-thread cache | runtime `gpu/` | `[perf]` lines, `SKY_FRAME_TIMES` + `tools/perf/frames.py` | ✅ ✅ ✅ |
| Texture hashing 12-14 % of the render thread | 4 KB sample, whole hash every 8 frames | runtime `d3d/render.cpp` `CheckGuestContent` | `SKY_TEXTURE_STATS=1` | ✅ ✅ ✅ |
| Resolves and CPU readbacks cost frame time | write back (async readback) only the resolves a CPU job reads; count draws and resolves per frame and compare between games | runtime / engine | `SKY_RENDER_STATS=1` (draws, resolves, skipped) | ✅ [?] [?] |
| GPU wait at present 25 % of the thread | **one frame in flight**: Present waits for the frame before; full waits only for texture release, readbacks, resize, shutdown. Two in flight was tried and dropped (freed buffers the GPU still read) | runtime `gpu/` | wait 8-12 -> 3-5 ms | ✅ ✅ ✅ |
| 30 -> 21 fps nobody noticed | inherited shadows walked per register; walk dirty groups only. Keep a frame-time number in every merge | runtime `d3d/ring.cpp` | `tools/perf/level_run.sh` before and after | ✅ n/a n/a |
| Kernel wake storms (27 % in `NtReleaseSemaphore`) | one condition variable per waiting thread; `sched_yield` back-off after 32 yields | runtime `kernel/` | `tools/perf/thread_cpu.py` | ✅ ✅ ✅ |
| Render thread spins on its last swap | turn the spin into a wait (13.8 -> 9.5 ms of CPU at 30 fps); Trap Team's line inlines it: a mid-instruction hook (`engines/alchemy/swap_wait.cpp`, named in the game's config; find it by `lwz r9,16976(r11)` in a loop) | game `games/swapforce/swap_wait.cpp` | `tools/perf/swapforce_run.sh` | n/a (no such spin) n/a (not seen) ✅ |
| Slower after a "regression" that was not one | `sample` inside the window, machine load; `giants_run.sh` prints the load. Compare under the same load | tools | `tools/perf/giants_run.sh` | - |
| Slow first install | incremental second shader pass, parallel compression, game updates redo only what changed (hashes) | Arkeyan-Builder, XenosRecomp | kit install time | ✅ ✅ ✅ |

## 5. Frame pacing, 30 and 60 FPS
The engines step by **measured** time, so 60 needs no game patch if the vblank stays 60 Hz
([frame_pacing.md](../engines/lux/frame_pacing.md), [video_vblank.md](../runtime/xbox360/video_vblank.md)).

| Symptom | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| 120-500 fps title, movies at 6 fps (D26) | `WaitForFlip` on the emulated vblank at the present interval; fences without a GPU | runtime `kernel/video.cpp` | `SKY_FRAME_TIMES` gap 33.3 ms at 30 | ✅ ✅ ✅ |
| Frames 25-41 ms apart with 9 ms of work | the interrupt thread asks for real-time scheduling (macOS); a late frame is shown at once | runtime `kernel/video.cpp`, `PlatformSetThreadTimingCritical` | `SKY_VBLANK_LOG=1`: p99 33 ms | ✅ ✅ ✅ |
| The 60 FPS option does nothing | the game's interval: SSA and SF ask for 2 (limit 60 lifts it to 1); Giants presents at every blank (limit 30 shows every second frame); **SF paces itself by the interval it reads from its device**, so the runtime writes 1 there before each swap | game flags: `presentsAtEveryBlank`, `presentIntervalPacesGame` | `fps_limit=60` run: 60.0 fps and the game's clock at 1.00 game second per wall second | ✅ ✅ ✅ (SF 52-56 fps, its own work) |
| Machine too slow for 60: uneven 40-50 fps, "epileptic" lava and crystals | **steady-30 fallback**: at 60, when 3 of the last 30 frames miss their blank, frames are shown every 2 blanks; 60 returns after 10 s that fit (doubling on a quick relapse); a Toy Box toast says "30 FPS: 60 not held" once per run | runtime `HalfRate` in `kernel/video.cpp`, `ToyboxNotice` | median gap 33.3 ms std < 4 ms on a loaded Mac; message once | ✅ ✅ ✅ |
| Long fall respawns the player at 60 (stuck-falling check counts distance per frame) | scale the distance threshold by 30/fps | game `games/swapforce/fall_check.cpp` | scripted long fall at 60 and 120: **not made yet** | ❌ [?] ❌ [?] 🔧 (hook runs, `[fall]` line) |
| Counted-frames code at 60: knockback, projectile range/lifetime, jump apex, raft acceleration, camera speed, spinning props | audit with a same-input comparison 30 vs 60 (apex, push distance) and scale what differs | game | the step-dependence list in office/lessons_cemu_2026-10-28.md (the office's Cemu notes) [?] | ❌ ❌ ❌ |
| Audio bank-load throttle runs twice as often, movies present each frame twice at 60 | harmless | - | - | ✅ ✅ ✅ |
| Wall-clock timeouts (attract, menus) | already frame-rate independent | - | - | ✅ ✅ ✅ |
| Above 60 | not implemented (immediate present with a 60 Hz vblank) | - | - | ❌ ❌ ❌ |

## 6. Movies and the intro skip
| Symptom | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| Movies stall | `KeTimeStampBundle` ticks every ms (Bink reads it) | runtime `kernel/` | movie pace log `SKY_BINK_LOG=1` | ✅ ✅ ✅ |
| Movies without shaders or garbled planes | YUV shaders built at run time dumped into the cache (D15); the `0xE0000000` window maps physical +0x1000 (D8) | runtime | `SKY_DUMP_SHADERS`; movie shot | ✅ ✅ ✅ |
| Bink version | SSA Bink 1; Giants and SF Bink 2 drawn on the GPU by the game (50 run-time shaders) | runtime `bink_display.cpp` ([bink.md](../engines/lux/bink.md)) | 30.00 fps against the header's 30, audio in real time | 1 / 2 / 2 |
| "Skip intro movies" does not skip | only the start-up logos, never the story: SSA `Movie_Update` (`legal atvilogo xpeclogo tfb`), Giants `BinkOpen` (`atvinewlogo_640`, `tfb_logo_640x456`), SF `BinkOpen` (`gsbp_atvi`, `gsbp_vvisions`); makes the movie one frame long. Read the **game's own** `[<game>.video]` section first | game hook + `host/settings.cpp` | log `[movie] skipped <name>`; `tools/regress/play_settings.sh` | ✅ ✅ ✅ |
| Black level after a movie skip in a test | a movie inside a cutscene stalls on its black surface: a test aid problem; tours play those movies | `tools/pipelines/tour_swapforce.sh` | - | n/a n/a ✅ |
| The end of the game's movies | scripted check of the ending and credits | `tools/pipelines/ssa_ending.sh`, `SKY_MOVIE_SWAP` | script passes | ✅ ❌ ❌ |
| Voice track follows the voice language | `Movie_Open` / `BinkOpen` selects the Bink audio track | game `language.cpp` | track id in the log | ✅ ✅ ✅ |
| Lip-sync | a person must check it | owner | - | ❌ 🔧 ❌ |

## 7. Audio
| Symptom | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| XMA silent, loops broken, voices dropped | XMA buffers from `MmAllocatePhysicalMemoryEx`; each frame as its own WMA Pro packet (FFmpeg holds back 4096 samples, D33); wait for the next buffer on a split frame (D34); same lock order as the game (D35); static **LGPL** wmapro-only FFmpeg, configure refuses GPL (D36) | runtime `apu/`, `kernel/xma.cpp`, `tools/ffmpeg/build.sh` | audio dump `SKY_AUDIO_DUMP`; the hash of the LGPL build | ✅ ✅ ✅ |
| A voice never decodes (a game stops presenting at ~58 s) | every context of a voice is kicked and cleared (the writers store one register word per context) | runtime `kernel/xma.cpp` | SF tutorial past 60 s | ✅ ✅ ✅ |
| Loops at level music | audit with `SKY_XMA_TRACE=1` (wraps, silent samples, tail leaks on context reuse) | runtime | SSA: ~128 silent samples at the wrap, inaudible | ✅ ❌ ❌ |
| Title music at -96 dB | XMP title control + system version (see 1) | runtime | audio dump level | ✅ ✅ ✅ |
| Choppy, metallic sound (Alchemy: one XAudio2 voice loops a 1024-sample 5.1 buffer, play cursor `SamplesPlayed mod 1024`) | driver cadence fix | runtime + [boot.md](../../games/giants/docs/boot.md) | audio dump | n/a ✅ ✅ [?] |
| Voice language separate from the text | hook the language (SSA Wwise language, Giants pack loaded before the text's, SF sound-package suffix), 10 / 12 / 2 languages by disc | game `language.cpp` | files read, audio dump | ✅ ✅ ✅ |

## 8. Input
| Symptom | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| Keyboard play | one binding table; keys are SDL scancodes = a key's **place** (AZERTY safe); **per game** `[<game>.input]` over `[input]`; Escape always back | runtime `host/settings.cpp`, `input` | `ArkeyanSettingsTest`; log `[settings] ... keys` | ✅ ✅ ✅ |
| Pad deaf after the Toy Box | dead zones (sticks never read 0): 8000 / 30 | runtime `input` | pad test | ✅ ✅ ✅ |
| Wrong button pictures per pad | **one style per pad family**: `xbox360`, `xboxone`, `playstation` (touchpad = BACK), `playstation3`, `nintendo` (letters by position), `keyboard` (keycaps of the bound keys); `auto` follows the last device | runtime `host/glyphs.cpp`, own pictures (never Sony / Nintendo / Microsoft art) | `SKY_GLYPHS_SHEET`, `SKY_GLYPHS_SCRIPT`, a check per family in `test_all.sh` | ✅ ✅ ✅ |
| Where the game's buttons are | SSA: whole 128x128 textures by content hash; Giants: cells of the font atlases + a few textures + the world prompts' large coins; SF: the fonts' `button` images redrawn **in guest memory** (the CPU text renderer copies them) | game adapter `glyphTextures`, `glyphImages` | dump with `SKY_TEXTURE_DUMP=1 SKY_TEXTURE_DUMP_LOG=1` (30 min), shot in 5 styles | ✅ ✅ ✅ |
| Words in texts name a button the keyboard lacks | SSA: the stick's noun becomes the arrows' picture (`stick_text.cpp`); SF, SuperChargers, Imaginators: "PRESS START", "Appuie sur Start"... interned with START as a picture (`engines/alchemy/prompt_text.cpp`); Giants: the Mash! coin | game text hooks | keycap shot; harness check | ✅ ✅ ✅ |
| 3D world prompts keep the 360's letters | meshes with colour maps, not textures | - | - | n/a ❌ ❌ [V] |
| Keys shown in the prompts are not the player's | the keycaps read the game's own section | runtime | `keycaps:` log line, `key_y=G` for Giants only | ✅ ✅ ✅ |
| Player 2 | XAM user 1 pad path, second figure through the Toy Box's Player 2 prompt | runtime + adapter | `SKY_PRESS2`, `ssa_arena.sh` | ✅ ✅ ✅ (a real second pad ❌) |
| Mouse in the game menus | much later (owner) | - | - | ❌ ❌ ❌ |

## 9. Toy Box, swappers, variants, the wheel
Model: [toybox_ux.md](../runtime/host/toybox_ux.md). The owner's rule: short labels, one sentence, the existing style.

| Symptom | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| "The Toy Box is for SSA" (seats, locks, portal id hard-coded) | the build's game id; `figures::Catalog(game)`; seats and `accepts` per game in `catalogue.json` | runtime `host/` | `ArkeyanCollectionTest` | ✅ ✅ ✅ |
| The prompt must open the Toy Box for the right player | smart portal: SSA prompt object + text hash; Giants "Portal Control" script variables found in the physical heap; SF `CMagicMoment` state | game `*_portal_prompt.cpp` | log `prompt for player 1`, `SKY_TOYBOX_SCRIPT` | ✅ ✅ ✅ ([?] SF player 2) |
| The game says "PORTAL DISCONNECTED" in headless runs | an emulated portal with the right model id (`R` answer: SF `02 03`); kit movie pass key `movie_portal=emulated` | runtime `portal_emulated.cpp`, `game.txt` | `SKY_PORTAL_MODEL` | n/a n/a ✅ |
| Portal "out of date", protocol resets | version >= 0x011F; any `S` with active = 0 resets; inactive stays silent | runtime `kernel/portal.cpp` | `ArkeyanPortalTest`, `--portal-probe` | ✅ ✅ ✅ |
| Swapper = two dumps | one card (both halves on the portal, Change top / bottom, one card in Add a figure), named as the game names it ("Nitro Magna Jet"), picture joins the halves on their shared frame | runtime `collection` (Pair, ChangeHalf) | `tools/regress/swapper_toybox.sh` | n/a n/a ✅ |
| One row per plain toy, only the variants a game tells apart, foreign figures hidden in game | catalogue "versions" per game from the game's own toy tables | catalogue + `figures` | `ssa_figures.sh`, `ArkeyanFigureTest --roundtrip` | ✅ ✅ ✅ |
| "I could not load Spyro" | **the player's own wheel**: Add to the wheel / On the wheel on a figure's page; pins per game and seat | runtime `host/toybox*` | `tools/regress/wheel_toybox.sh` | ✅ ✅ ✅ |
| Cannot find a figure | one smart search (names, elements, games, types, versions) in the Toy Box and the launcher | runtime + launcher | `tools/regress/search_toybox.sh` | ✅ ✅ ✅ |
| Seat limits | Giants `p1 p2 item`; SF one seat per player for swappers | catalogue `seats` | harness | ✅ ✅ ✅ |
| Compatibility | which toys each game reads: SSA toy type only, Giants adds variants (`0x1206`, hats, level 15, extra area with its own CRC-16, Wow Pow bit), SF level cap 20, LightCore = ordinary figures that glow | `figures.md` | all 54 types and 16 variants through the game, 0 failures | ✅ ✅ ✅ |

## 10. The figure collection
| Item | Fix / rule | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| A figure's identity | one dump = one physical figure; the dump carries the progress of every game; foreign games keep their areas byte-identical | runtime `host/figures/` ([figures.md](../runtime/host/figures.md)) | cross-game round trips in `ArkeyanFigureTest` | ✅ ✅ ✅ |
| Catalogue from the game's own tables | toy types, variants, upgrade bits, names and prices from the disc (`toydata.pak` for SF, Giants' `Init_Setup.bld`), later-game variants from public lists | `resources/collection/catalogue.json`, new-game step 11 | catalogue rows agree with the game | ✅ ✅ ✅ |
| Level, XP, gold written and read back | a level play script proves XP and gold; checksums valid; title refuses a wrong CRC | tools + `figures` | `tools/regress/ssa_level_play.sh` | ✅ ❌ ❌ |
| Created, edited, deleted, trash, backups | shared collection library | runtime `host/collection*` | `ArkeyanCollectionTest` | ✅ ✅ ✅ |
| Portraits and artwork from the disc | see 14 | - | - | ✅ ✅ ✅ |

## 11. Settings: the game's own section
| Item | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| Launcher setting does not reach a game started without PLAY | the game's own `[<game>.video]` (and `.input`) wins over `[video]` for every key | runtime `host/settings.cpp` | `tools/regress/play_settings.sh` | ✅ ✅ ✅ |
| Test run reads the owner's settings | `SKY_SETTINGS`, `SKY_LAUNCHER_INI`, `SKY_COLLECTION`, `SKY_SAVE_DIR`, `SKYLANDERS_DATA`, `SKY_LOG` in every run; old `SSA_*` names still read | `host/env.h` | `env_names.sh`, `kit_isolation.sh` | ✅ ✅ ✅ |
| Text and voice language lists | by the disc's file names (SSA 10, Giants 12, SF 2: others show English) | game `language.cpp`, `info.cpp` | `--language`, French menus seen | ✅ ✅ ✅ |
| HD texture packs | dump `<data>/<Game>/textures-dump`, pack `textures/`; **the game's own folder** (Giants' was always `SSA/`) | runtime `gpu` ([texture_packs.md](../runtime/gpu/texture_packs.md)) | `ArkeyanTexturePackTest`, replacement seen | ✅ ✅ ✅ |
| Keys that do not apply | no effect in a game that lacks the feature (`original_resolution` outside SF): say so in [settings.md](../getting-started/settings.md) | docs | - | - |

## 12. Achievements and play time
| Item | State | SSA / Giants / SF |
|---|---|---|
| The enumerator that enumerates nothing (no crash) | ✅ shared | ✅ ✅ ✅ |
| Real achievements per game (each game's list, the unlock log line `[achievement] user U unlocked achievement N`) and play time | queued for 0.6.2 | ❌ ❌ ❌ |

## 13. Icons and art from the disc
| Item | Fix | Where | Verify | SSA / Giants / SF |
|---|---|---|---|---|
| Logo, background, portraits, artworks | per-game manifest (names only, no game data) read by one versioned IGA/IGZ reader; cached in the data folder | `games/<id>/art.json`, `runtime/host/art/` ([game_art.md](../runtime/host/game_art.md)) | `ArkeyanArtTest` with the disc | ✅ ✅ ✅ |
| Alchemy pictures are upside down; Giants' logo is a sprite sheet | `compose` steps coded once ("sprite_sheet_logo"); SF's logo from its lossless 32-bit picture | art reader | logo shot | n/a ✅ ✅ |
| The game's icon | the disc's title logo fitted in a transparent square, made once by Arkeyan-Builder; macOS bundle, Windows exe (`ico.h`, windres), Linux window icon | Arkeyan-Builder | install, look at the icon | ✅ ✅ ✅ |
| The Toy Box and launcher pictures | a swapper's halves joined; portraits first from the game, else another game's | collection | `[toybox] N portraits` | ✅ ✅ ✅ |

## 14. Crash and report
| Item | Fix | Where | Verify |
|---|---|---|---|
| A player can send something useful | the game log (`<Game>.log` plus `.prev.log`) in the data folder, written on **every** platform before its first test; build number as the first line; `[settings]` line shows the effective config | runtime `log.cpp`, [settings.md](../getting-started/settings.md#the-game-log) | the log of a kit-built game on each OS |
| Unreproducible crash | log-and-skip guards, `[guard]` lines, give a hunt one hour | engine | logs |
| "Report a problem" in the launcher | generic; includes the log | Arkeyan Launcher `support.md` | owner-run |
| A fault in a test only | check whether the test aid caused it (challenge levels alone have no base mesh; survival arenas alone need Solo Survival's mode; `SKY_LEVEL` is not the story) | `SKY_LEVEL` redirects | tour allow-lists with `#` comments |

## 15. Launcher side, per game
| Item | Where | SSA / Giants / SF |
|---|---|---|
| Disc check by `default.xex` SHA-256 (+ other regions, e.g. the European SF disc) | `games/<id>/info.cpp`, `game.txt`; Arkeyan-Builder picks the game from the disc | ✅ ✅ ✅ |
| Game version in `game.txt`, UPDATE button, `<!-- games: ... -->` line of the release notes | kit | ✅ ✅ ✅ |
| Kit: `KIT_GAMES`, Windows `-mno-sse4a`, Linux container, per-game movie pass keys (`movie_portal`, `movie_press`, `movie_shaders`), headless run isolated | `tools/make_kit.sh`, Arkeyan-Builder | ✅ ✅ ✅ (a real PC: ❌ ❌ ❌) |
| The game's main menu theme on its launcher page | `art.json` "music" + `Arkeyan-Builder --menu-music` decoding the player's disc (never shipped; [game_music.md](../runtime/host/game_music.md)) | ✅ ✅ ✅ (Trap Team too; disc decoding checked, listening confirmation pending) |
| Game page art and the game's look in the launcher | `art.json`, `GameStyle` (Arkeyan Launcher, separate repo) | ✅ ✅ 🔧 (SF row unavailable in the launcher) |
| Settings page keys per game; portal prepared for the next start | launcher + `portal.txt` | ✅ ✅ ❌ (SF cannot start with prepared seats) |
| Languages offered, texture pack folder, Uninstall everything | launcher | ✅ ✅ ✅ |

## For Trap Team, SuperChargers and Imaginators

### Order (apply the list once the title draws)
1. **Day 1, before any fix**: isolation env, `[stub]` grep per screen, game log on every platform, the fast recipe
   (`SKY_SKIP_MOVIES`, `SKY_LEVEL`, repeated presses) so a run takes under a minute. Sections 1 and 14.
2. **Find the engine's hooks you can reuse by search** (sigmatch against the Alchemy games' inventories): the LZMA decoder
   (section 4: first load-time win), the language, the ultrawide camera/scene hooks, the glyph sources, the portal prompt.
3. **Decompression and pipelines early**: host decoder the same day, then a pipeline tour with several figures, then the
   loading wait trigger (the file the game opens per level).
4. **Pacing before 60**: find how the game asks for its present interval and reads it back (SSA/SF: 2; Giants: 1; SF
   reads its device). Make 30 right first (section 5), then 60 with the fallback and the step-dependence audit.
5. **Ultrawide, glyphs, languages, intro skip, icon, art, texture packs**: sections 3, 6, 8, 11, 13.
6. **Collection**: the game's own toy tables, seats, `accepts`, variants, then the smart portal prompt and Toy Box checks (9, 10).
7. **Launcher**: disc rows, kit, movie pass, settings (15). Docs and parity rows with every step.

### What the Alchemy games probably share [?]
Giants and Swap Force are Alchemy, so for Trap Team and SuperChargers (probably Alchemy), Imaginators (open) expect, then
confirm with a search, not by address: IGA/IGZ archives (reader versions v8/v6, v10/v7: a new version is a new reader
branch), LZMA loads, shaders built by the game (`*_Init` hooks, no `Create*Shader`), no command buffers, the font-atlas or
CPU-composed glyph strings, the zone/level loader in `engines/alchemy/zone.cpp`, the XAudio2 1024-sample voice, a "stuck
falling" check and counted-frame code at 60 (Imaginators has the longest list of 60 FPS problems on other platforms:
minigames, jump height, selfie camera: use it as the test list).

### Known traps (read before you debug)
- Never reuse an address. A hook the game lacks is compiled out; new XDK builds change D3D signatures (Giants: 21076).
- Stale state: per-shader state from the shader header, inherited state after command buffers, never from a shadow
  (pitfalls 2). Ask for a real-360 screenshot before debugging colours (violet sky was correct).
- Rebuild every prebuilt when `ppc/` changes; rebuild the shader cache after any XenosRecomp change, on the Mac for AIR.
- A test that boots is not a game that plays: tours start levels alone (no base mesh for challenges, no mode for arenas).
- The vblank must stay 60 Hz (scene and shader time count vblanks).
- A change that costs frame time goes unseen for a day: record a frame-time number in every merge.
- A player's `launcher.ini` skews tests: isolation first. No full-screen screenshots, never drive the real mouse.
- Everything derived from the disc stays in `private/`; nothing from it in git.

### Pass criteria (a game is perfect when)
- Every row above is ✅, or ❌ with an owner and a line in the game's [100.md](../porting/game_template/100.md) "Left".
- `tools/test_all.sh` passes for every game; the new game's story/zone tour plays with no fault; the title-to-first-level
  run, a save and reload, a figure through the Toy Box and a second player run in the harness.
- Draw-time pipeline builds per level 0-7; loads at host decoder speed; no frame over 2x the median in a 5-minute level run.
- 30 FPS holds p99 33 ms; 60 FPS holds the game's clock at 1.00 s per wall second or falls back to a steady 30 with the toast;
  the stuck-fall and step-dependence list pass at 60.
- Shots at scale 1 and 2, 16:9 and 21:9, five glyph styles and the keyboard, with the game's own keys.
- Docs: the eight game pages current, [parity.md](../porting/game_template/parity.md) rows each with a proof,
  `python3 -I tools/check_docs.py` prints `0 broken`.
