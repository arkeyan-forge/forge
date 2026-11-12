# Pipeline list, `pipelines.bin`, the baseline, loading waits, the D3D12 library

> **Purpose:** how are host pipelines keyed, listed, precompiled and waited for (`pipelines.bin`, the baseline, the D3D12 library)? · **Status:** current (2026-11-02) · **Tags:** [Xbox] [SSA] [Giants] [SwapForce] [TrapTeam]

Code: [`gpu/draw.cpp`](../../../runtime/gpu/draw.cpp) (`PipelineKey`, `PipelineRecord`, `LoadPipelineList`, `PrecompilePipelines`, `WaitForPipelineList`),
[`gpu/d3d12_pipeline_cache.cpp`](../../../runtime/gpu/d3d12_pipeline_cache.cpp), [`games/ssa/pipelines/baseline.zst`](../../../games/ssa/pipelines/),
[`tools/pipelines/`](../../../tools/pipelines/). Class **[Xbox]** (host) except the baseline content (**[SSA]**).

## Why

A pipeline the system shader cache lacks costs **~20 ms on a cold Metal cache** (more from DXIL on D3D12; ~12 ms on the Xbox), and the game asks for **~120 at once when a level starts**
(33 for the first 3D frontend screen): 667 ms and 2.6 s frames; 4666 ms of render-thread time compiling on a cold run. **The `.app` bundle has its own Metal cache**
(`$(getconf DARWIN_USER_CACHE_DIR)<bundle id>`), so anyone running the app starts cold while symlink runs share a warm user-wide cache: **timings through the symlink lie** (D32; measure with an app
clone with a fresh bundle id).

## `PipelineKey` (everything a pipeline is built from, shaders by hash)

```
u64 vertexShader (XXH3 of the guest container), u64 pixelShader (0 = depth only), u32 specConstants, topology, RasterState{cull, frontFace, depthBias, slopeScaledDepthBias},
u32 colorTargetCount, Format colorFormats[4], BlendState blend[4], Format depthFormat (UNKNOWN: none), DepthStencilState,
u32 inputCount, VertexInput inputs[32] {location, stream, offset, format}, u32 streamCount, u32 strides[16], u32 perInstance (bit per stream)
```
`KeyOf` zero-fills then copies field by field so padding never reaches the hash. **Backend-independent** (verified by reading): the shader hashes are of the guest container; formats come from `xenos.cpp` tables with no
backend branch. What differs per backend (DXIL variants, the point-sprite GS, root descriptors) is derived from the key in `CreatePipeline`, not stored in it.
`PipelineRecord = { PipelineKey key; u64 check (XXH3 of the key) }`, **832 bytes**, pinned by `static_assert(sizeof == 832)` (no `long`, enums are `int`, same alignment on arm64 macOS and x64 mingw). **If `PipelineKey`
changes meaning bump `PipelineListVersion` and regenerate the baseline** (an old one is ignored with a log line, never misread). Stale records (shaders no longer in the cache) are skipped cheaply.

## `pipelines.bin` (per player) and the baseline (built into the executable)

- `pipelines.bin` (in `SKY_SAVE_DIR` or the app's pref path): 12-byte header **magic "SSAP", version, record size**, then records, appended the first time the game uses an unlisted pipeline.
- **Baseline** `games/ssa/pipelines/baseline.zst`: same format, zstd-compressed (1.24 MB -> 26 KB), **1570 pipelines** (1566 from a tour of all 70 levels + 4 added by a later check; 119 vertex + 136 pixel shaders
  of the 1118), embedded by `gpu/embed.cmake` as `pipeline_baseline.h`, compiled in `gpu/game_inputs.cpp` (an object of the executable, not of `libArkeyanRecompGpu.a`, which is then the same bytes for every game: the kit shares it). **It holds render states and the XXH3 of shader containers only: no game content**; it cannot rebuild anything without the player's disc. It is
  the **one disc-derived artifact the user allowed to ship**. Baseline keys are not written to `pipelines.bin`.
- `LoadPipelineList`: `pipelines.bin` first (what this player saw), then the baseline's keys it lacks. Log: `[gpu] pipeline list: N pipelines (A from pipelines.bin, B more of the baseline's C)`.
- **Making the baseline**: `tools/pipelines/tour.sh <iso> <work dir> [figures] [parallel]` runs the fast recipe in **every level** (`SKY_LEVEL=<level>`, 70 of them: `level_000..046`, `level_hub` first stage,
  `challenge_level_000..031`, `pvp_level_001..010`, `dnftest`), a different figure per run, with presses (menus, B through the cutscene, then from 62 s: A, left stick in 4 directions, X, Y, B; pause menu at
  100-112 s), ~2 min 10 s per run, 2 in parallel (~80 min); each run's `pipelines.bin` is merged: `tools/pipelines/merge_lists.py <out> <lists...>` (first occurrence wins, run order);
  then `zstd -19 --rm merged.bin -o games/ssa/pipelines/baseline.zst`. Coverage still grew at the end (last levels added 5-44 keys): rare effects, hub stages 2-6, bosses, other figures' upgraded attacks, the Toy Box screens
  are built at draw time once and appended to the player's list. The Toy Box overlay and Bink movies use only the present pass.

## Building the list

- At startup **up to 4 threads** build the listed pipelines **while the intro movies play** (`PrecompilePipelines`; shader and pipeline creation use their own mutexes, never `g_mutex`). A draw needing one that is still
  being built waits for it (`[gpu] draw waited X ms for a listed pipeline`).
- **Loading wait**: `gpu::WaitForPipelineList(reason)` is called by `OpenFile` after a successful open of `\levels\...\mission*.str` (every level, the frontend, the hub's `mission_stage<n>.str`): it builds keys on the loading
  thread plus `hardware_concurrency - 1 - running` extra threads and waits for the background ones. Log `[gpu] loading (<file>) waited X ms for N of the M listed pipelines (U extra threads)`. Finite (never waits for a pipeline nobody
  builds), unlike the WideLoad hang. The other candidates (WideLoad waits `sub_82846398`, which can have zero events; the startup state machine only covers the frontend) were rejected.
- Measured (M1 Pro, cold): built at draw time **312 -> 0**; first 3D frontend frame 667 -> 133 ms, first level frame 2564 -> 334 ms; loading added ~2.9-4.4 s only when movies are skipped (the intro movies cover it: legal screen 5 s alone).
  Log lines at draw time: `[gpu] pipeline built at draw time: X ms (N built at draw time, T ms in all)`.

## D3D12: the persistent pipeline library (`pipelines.d3d12`)

D3D12 has no driver cache we control: the console would build ~1566 pipelines from DXIL at every launch. `ID3D12PipelineLibrary` in `LocalState\pipelines.d3d12` (`gpu/d3d12_pipeline_cache.cpp`), header **magic "SSPL"** + the identity of
the shader cache; plume loads a pipeline by `RenderGraphicsPipelineDesc::cacheKey` (the `PipelineKey` XXH3) or builds and stores it (`patches/plume.patch`). Saved when the list is built and, after draw-time builds, at most every 30 s
off the render thread (temporary file then rename). D3D12 rejects a library from another driver/adapter (logged, starts empty). Checked under vkd3d-proton: first run "0 loaded, 1566 built; saved", second "1566 loaded, 0 built".
**On the Xbox Series X opening the library removed the device** (run 8): off by default on UWP (`[gpu] pipeline_library=off`), on elsewhere; a `pipelines.d3d12.loading` marker is written before opening and removed after, a
marker left behind makes later runs skip the cache (the file renamed `.failed`). Only used when `D3D12_FEATURE_SHADER_CACHE` reports library support. An alternative not shipped: per-PSO `GetCachedBlob` / `CachedPSO`.
Log: `[gpu] D3D12 pipeline cache <state>: N loaded, M built; saved X MB`.

## Rules for a new game

1. Start with an empty baseline (the SSA one hashes SSA's shaders); the list works from the first run. Tour with several figures (one of each kind the game sells).
2. Re-run the tour (needs `SKY_LEVEL` working: the first level's file paths) and ship the merged baseline; bump `PipelineListVersion` when the key changes.
3. Never ship `pipelines.bin` from a player's machine (it holds hashes of their disc's shaders: same as the baseline in principle, but only the baseline was approved).

## Giants [Alchemy] [Giants]
- The loading wait is on `\level\<name>.arc` (Alchemy has no `mission*.str`), see `OpenFile`; the baseline is `games/giants/pipelines/baseline.zst`: **2363** pipelines (2026-10-27; 1575 from the first tour of 91 levels with `new_14.sky`, `tools/pipelines/tour_giants.sh`,
  then 788 from tours with other figures: `TOUR_FIGURE=new:112:1206` (Tree Rex) and `new:105:1000` (Hot Dog) through the 23 story, hub and arena levels, one `level_200` run
  for each of the 13 other Giants characters, merged with `merge_lists.py`). Results per level: `games/giants/docs/status.md`.
- **Tour with several figures**: each figure draws with its own shaders and states; with only an SSA figure in the tour, a Giants figure built 17-145 pipelines
  at draw time per level (20-67 ms each on a cold Metal cache: the first-visit hitches). With the 2363 a figure no tour used (Bouncer) built 1-6 per level
  (was 23-102). Numbers: [renderer_threads.md](renderer_threads.md), "Giants (2026-10-27)". Figures still worth touring: SSA figures other than
  `new_14.sky`, Bouncer, the LightCore and Series 2 variants.
- `SKY_LEVEL=<name>` works for Giants too (`level_203_the_machine`, `pvp_level_200_two_towers`...): `level\level_213_tutotial.arc/.bld` are read from that level. Draw-time builds per level: 215 -> 2 (level_203), 135 -> 2 (level_008), 34 -> 3 (challenge_level_020).
- The tour needs `SKY_SAVE_DIR` to exist before the run (the list file is opened at the first draw).

## Swap Force [Alchemy] [SwapForce]
- The baseline is `games/swapforce/pipelines/baseline.zst`: **738 pipelines** (14 KB; 2026-10-28). 711 from the zone tour
  of 2026-10-27 (`tools/pipelines/tour_swapforce.sh`: the 195 zones of `zoneinfos.pak` with a `.pak`, 212 runs, each run's
  `pipelines.bin` merged: 686; then the 20 survival arenas in their mode and `brn3`, `ww1` with their movies: +25), then
  **27** from tours with other figures (2026-10-28, `TOUR_FIGURE`): the swapper Blast Zone (`new:2004,new:1004`) and
  Scorp (`new:3003`) through the 16 story zones but `brn3` and `ww1`, and `new:2000,new:1000`, `new:2008,new:1008`,
  `new:2012,new:1012`, `new:2015,new:1004`, `new:3000`, `new:3006`, `new:3010` in `brn2` and `fg1` (101 runs, no fault),
  merged with `merge_lists.py` after the 711. Blast Zone built 21 pipelines at draw time in `brn1` before, **1** after
  ([renderer_threads.md](renderer_threads.md), "Loading and frame time per game"). `runtime/gpu/CMakeLists.txt` embeds it when the file exists **at configure time**: re-run
  `cmake build-swapforce` once after adding it; the log then says `[gpu] pipeline list: 738 pipelines (0 from
  pipelines.bin, 738 more of the baseline's 738)`.
- **Loading wait** on `\archives\common_ents_zone.pak` (`files.cpp`): every zone load opens it (the menu background's
  too, 2-3 times a load); Swap Force has no `level\` or `levels\` folder. Log `[gpu] loading
  (\archives\common_ents_zone.pak): the 711 listed pipelines are built; ...` [V].
- Draw-time builds after a zone opens, empty list -> baseline: `barrens/brn2` 130 -> 3, `hc_verdant_01` 274 -> 7,
  `town_state2` 199 -> 7 (90 s runs). The first-frame stall of a zone is then 90-120 ms.
- Grows with the game: the tour starts each zone alone for 40 s; figures no tour used (11 of the 16 swappers, most
  cores, the Giants and SSA figures), later parts of the zones, the survival arenas (they fault alone) and the Toy Box are built at draw time
  once and appended to the player's list.

## Trap Team [Alchemy] [TrapTeam]
- The baseline is `games/trapteam/pipelines/baseline.zst`: **658 pipelines** (12 KB; 2026-11-02), from 115 runs
  of `tools/pipelines/tour_trapteam.sh` merged with `merge_lists.py`: the 56 levels of the disc with Gusto (`new:450`,
  130 s each): 559; then 100 s runs with other figures: the swapper Blast Zone (`new:2004,new:1004`), the Giant Crusher
  (`new:102`), the mini Bop (`new:502`), the core Fling Kong (`new:452`), Gusto with a Water Trap holding its villain
  played with LT (`TOUR_PRESS=lt:50:0.5,...`, the dump of `ArkeyanFigureTest --make-trap-pair` placed by its absolute path)
  in the tutorial, `level_hub_2014_01` and four story levels (300, 303, 309, 316); Snap Shot with the evolved villain's
  trap and one figure of each other element (Trap Masters 454, 462, 470, 478, 482; cores 460, 468, 476, 485) in the hub,
  300 and 303: **+99**. The figure runs still added 0-16 keys each; the elements' third level only 2.
- **Loading wait** on `\level\<name>.arc`, as Giants (`files.cpp`: games whose `LevelOfFile` is `LevelArcOfFile`). Log
  `[gpu] loading (\level\level_313_tutorial.arc): the 658 listed pipelines are built; 0 built at draw time so far` [V];
  the list is ready 0.1 s after start (warm Metal cache).
- Draw-time builds after the level opens, empty list -> baseline: Gusto 125-142 -> **0** in the tutorial, the hub, 300
  and 305; Head Rush (`new:455`, a Trap Master no tour used) 125-142 -> 0-1 ([renderer_threads.md](renderer_threads.md),
  "Trap Team (2026-11-02)"). The log then says `[gpu] pipeline list: 658 pipelines (0 from pipelines.bin, 658 more of
  the baseline's 658)`.
- Not covered: the arenas (`arena_challenge_*` stay on a 126-draw screen with `SKY_LEVEL`; 309, 311 and 361 fault there,
  the redirect's artefact), the later parts of each level (130 s runs), the other villains and Kaos Traps, the older
  games' figures but Crusher and Blast Zone: built at draw time once and appended to the player's list.
