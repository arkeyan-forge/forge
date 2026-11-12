# The multithreaded renderer, caches and performance findings

> **Purpose:** how is the host renderer threaded and cached, and what did the performance work find? · **Status:** current (2026-11-04) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [TrapTeam]

Code: [`gpu/worker.cpp`](../../../runtime/gpu/worker.cpp), [`gpu/gpu.cpp`](../../../runtime/gpu/gpu.cpp), [`gpu/draw.cpp`](../../../runtime/gpu/draw.cpp),
[`gpu/buffer_cache.cpp`](../../../runtime/gpu/buffer_cache.cpp), [`d3d/render.cpp`](../../../runtime/d3d/render.cpp). Class **[Xbox]** (host renderer) with **[Lux]** cost drivers.

## Threads

| Thread | Work |
|---|---|
| The **game's render thread** (guest) | guest code, the ring scan, state translation into draw packets (`BuildDraw`), texture and vertex content hashes, copying guest data it must snapshot |
| **GPU thread** (`worker.cpp`, `SKY_GPU_THREAD=0` / `[gpu] thread=off` disables) | everything that records or submits: draws, clears, copies, depth resolves, texture copies, Present (and its GPU waits), readbacks |
| **Texture threads** (up to 4: `hardware_concurrency - 3`, 4 to 7 cores) | untile, byte-swap, convert new/changed guest textures straight into upload memory (`gpu::UploadTextureAsync`) |
| **Pipeline threads** (4, at startup) | build the listed pipelines while the intro movies play ([pipeline_cache.md](pipeline_cache.md)) |
| Main thread | window, input, movie frames (`PresentDisplayFrame`), Toy Box |

Correctness rules: the public `gpu::` API is unchanged; calls queue their command-list work in call order (**batches of 64, one lock per batch**). Producers (the render thread, the main thread for movies, the Toy Box)
hold `g_producerMutex` in every call, the buffer cache's `CacheVertices` / `CacheIndices` included (**[V]** 2026-10-26: they took
none, and a main-thread movie present ran `BufferCacheFrameDone` during the game's draws: a rare crash in `Acquire`, [problem_catalogue.md](../../porting/problem_catalogue.md)); **the GPU thread never takes it** (no lock-order inversion; the draw context is created in `Init` because creating it uploads). A queued draw is a copy in a **per-frame arena** (4 MB chunks recycled after the
GPU thread runs that frame's Present). Guest float constants: the ring keeps a version per group of 16 registers; only changed groups are queued into the GPU thread's own 8 KB copy.
**Guest memory is read only at the call**: vertex/index ranges and texture data go into *producer upload memory* tagged with the producer frame and reused once the GPU finished that frame (`CompletedFrame`); textures are
snapshotted (`sliceBytes` per subresource) before the texture threads decode them. `Present` ends a producer frame; the caller waits only while the GPU thread is still on the frame **before** (one frame of run-ahead).
Readbacks (`SKY_DUMP_FRAME`) wait for the GPU thread.

## Caches

- **Buffer cache** (`gpu/buffer_cache.cpp`): converted (byte-swapped once) vertex/index data keyed by the **range a draw actually reads** `[vertexStart, vertexEnd)` (from the index range `minIndex..maxIndex` + base vertex,
  or `first..first+count`), **not** `(address, size)`: the engine streams dynamic vertices through a **2.56 MB ring at `0xAA5E0000`** and every draw's fetch constant says "size = the rest of the ring", so a
  `(address, size)` key created ~290 MTLBuffers/frame (~2 MB converted each; D18). A range seen for the first time is staged into upload memory; it becomes **resident** (32 MB blocks, LRU above 256 MB) only when drawn again in a later
  frame; ring data never enters the cache; residents are re-validated with XXH3 at most once per submission, by 4 KB chunks that overlapping ranges share (`gpu/range_hash.h`, 2026-11-03: SSA's `level_008` hashed 142 MB a frame for 12 MB of distinct bytes; "Where the frame goes" below). Verified: 6.0e6 checks, 0 mismatches. Index cache also gives min/max index. Vertex data swapped 8-in-32, indices 8-in-16.
  Assumption: within one submission the game does not rewrite a range it has drawn from (dynamic draws go through BeginVertices). Log `[buffers] frame N, per frame: acquire .. staged .. promoted .. hashed ..` (`SKY_BUFFER_STATS=1`).
  Since 2026-11-04 a resident range whose pages nothing wrote since its last hash is not hashed again (the **write watch**, below).
  Threads: the render thread calls it, under the producer lock; `BufferCacheFrameDone` runs in `gpu::Present`, which the main thread also calls until the game presents its first frame (`PresentDisplayFrame` stops once `SetGameRendering(true)`). A main-thread present between a draw's `CacheVertices` and its `gpu::Draw` still leaves that draw with a range staged in the frame just ended: at worst one draw with wrong vertices, before the game's first visible frame (**[?]** never seen). `ArkeyanRecompGpuTest`'s "producers" case presents on one thread while another fills the cache.
  Swap Force's zone tour (2026-10-27) [SwapForce] [V]: before the lock ~5 % of its runs crashed at start-up (`unordered_map::find` in
  `BufferCacheFrameDone`); none of 9 re-runs with it did ([games/swapforce/docs/status.md](../../../games/swapforce/docs/history.md), "Zone tour").
- **Sampler states** hashed (`SamplerSlot`): 5% -> 1% of the render thread; a collision only makes a new slot.
- **Constants**: both 4 KB float banks, byte-swapped, **deduplicated within the frame** (identical banks uploaded once; consecutive draws with the same `guestConstantsId` skip the hash). Uploading only the registers a shader uses would
  need XenosRecomp metadata (hash+swap ~114 samples; 25% of draws hit the dedupe, only 12.5% repeat the previous draw's banks, first differing byte at ~1160 of 8192).
- **Textures**: see [textures_formats.md](textures_formats.md). Each texture a frame uses is checked once a frame against guest memory; from 64 KB by a
  **4 KB sample** (64 lines of 64 bytes, one per 1/64th of the storage), the whole storage being hashed when the sample changed, every 8 frames
  (staggered by the texture's key so a level's textures do not all come due together) and the frame after a change (`CheckGuestContent`,
  `runtime/d3d/render.cpp`). Giants `level_203`: 222 textures a frame, **21.9 -> 4.3-4.6 MB hashed**; the texture hash 12-14 % -> 3-3.5 % of the render
  thread ("Giants (2026-10-27)" below). Staleness bound: a write the sample misses shows at most 8 frames late. The guest hash itself is unchanged (glyph
  replacement and texture packs key on it). `SKY_TEXTURE_STATS=1` hashes every sampled texture whole too and logs each change the sample missed.
  Since 2026-11-04 a texture whose pages nothing wrote since its last whole hash keeps that hash, exactly (the write watch, below); the sample
  and the 8-frame rule apply only to textures on pages the game writes.
- **Chunked submission**: the frame is sent to the GPU in parts where a render pass ends anyway (after 128 draws at a framebuffer change, or every 1024 draws), each part with its own command list and fence, so the GPU works while the CPU records
  the rest. On Metal this needs `plume_queue_order.patch` ([plume_backends.md](plume_backends.md)). GPU wait at present 8.4-12.6 ms -> 3.4-5.1 ms.
- **One frame in flight** (`Present` waits for the frame *before*, not its own): at scale 2 25% of the render thread waited for the GPU. Per-frame upload memory, command lists, present descriptor sets, acquire semaphore (`Context::previous`
  swapped in `EndFrame`); the buffer cache frees ranges once the frame that last used them completed. Kept as full waits: a frame that released textures (framebuffers may be in flight; Vulkan cannot destroy those), readbacks / frame
  dumps, swapchain/offscreen resizes, shutdown. "**2 frames in flight**" (an earlier try) freed staging and buffers the GPU still read: left out.
- **Bind only on change**: Metal/Vulkan `setFramebuffer` only when it changes; D3D12 `Context::bound` (root signature + four descriptor heaps once per command list because `SetGraphicsRootSignature` unbinds every root argument,
  then pipeline, viewport, scissor, blend factor and the four root descriptors when they differ): "rest" 30-41 ms on the console was the guest CPU, but re-binding everything per draw cost real time.

## Performance findings (all M1 Pro unless noted)

| Change | Effect |
|---|---|
| bind a framebuffer only when it changes | level_027 2.4-3.3 -> ~30 fps (160-227 ms GPU wait -> 8-9 ms) |
| buffer cache keyed by the range read | 6 -> 25-30 fps; 0 MTLBuffers created per frame (was ~290) |
| host zlib for the game's `inflate` ([lux/zlib.md](../../engines/lux/zlib.md)) | mission.str 3.4 -> 0.3-0.5 s; load to first full frame 4.5 -> 0.95 s |
| pipeline list + baseline | render-thread pipeline time 4666 -> 0 ms |
| `PhysicalToGuest` per-thread cache | 247 -> 106 samples |
| late frame shown at once (`WaitForFlip`) | worst-window 25.2 -> 29.0 fps |
| `SendInheritedShadows` walked 10 536 registers per run | **22 -> 30 fps by itself** (came in with a commit and went unnoticed for a day) |
| GPU thread + texture threads | render-thread work 28 -> 18-20 ms at scale 2; level 30.5 -> 55.9 fps at limit 60 with one instance |
| Xbox build `x86-64-v3` + bind-on-change | console 15-20 fps CPU-bound |

Where a level frame still spends time on the render thread (scale 2, limit 60; before the 2026-10-27 texture sample): XXH3 ~16% (texture hash 2/3, vertex validation 1/3), ring parse ~5%, constants rebuild ~4%, guest code ~30%. `IssueDraw` ~40% of wall time earlier.
**First level frame 150-290 ms** is the game's own main thread running its first update (the render thread idles ~170 ms in the job loop `sub_82635740`: `WaitForSingleObject(0x82BB4F28, 1 ms)`); it cannot move into the loading screen
without changing the game. The first frame also decodes ~2857 textures (25.6 MB, ~35-50 ms for 390 on the render thread before the texture threads).
Not done: hashing on workers (would show CPU-written textures a frame late), parallel command-list recording, a texture preload per level, display pacing check on a real window (Metal presents when the present command buffer is
scheduled: the emulated vblank is not tied to the display's; next step `presentAfterMinimumDuration`).

## Optimizations per game (inventory, 2026-10-28) [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce]

What has been done for speed, where it lives and which games have it. "Runtime" = generic host code every game gets;
the rest is per engine or per game and must be redone (found, toured) for each new game. Measurements: "Loading and
frame time per game (2026-10-28)" below.

| Optimization | Where | SSA | Giants | Swap Force |
|---|---|---|---|---|
| **Host decompression** of what loads unpack | Lux: `runtime/inflate.cpp` (zlib's `inflate`, [lux/zlib.md](../../engines/lux/zlib.md)); Alchemy: `engines/alchemy/lzma.cpp` (the LZMA SDK's `LzmaDec_DecodeReal`, [alchemy/lzma.md](../../engines/alchemy/lzma.md)) | ✅ zlib (mission.str 3.4 -> 0.3-0.5 s) | ✅ LZMA (**new** 2026-10-28; zlib 1.2.3 is linked but no load calls it) | ✅ LZMA (**new**; same) |
| **Pipeline baseline** (built into the executable) | `games/<id>/pipelines/baseline.zst`, `tools/pipelines/tour*.sh`, [pipeline_cache.md](pipeline_cache.md) | ✅ 1570 (70 levels, one figure) | ✅ 2363 (91 levels, Tree Rex, Hot Dog, 13 more characters) | ✅ 711 -> **738** (**new**: + Blast Zone and Scorp through the 16 story zones, 7 more figures in two zones) |
| Pipelines built during the intro movies (4 threads), `pipelines.bin` per player | runtime, `gpu/draw.cpp` `PrecompilePipelines` | ✅ | ✅ | ✅ |
| **Loading wait** on the pipeline list (the loading screen covers the builds) | runtime, `kernel/files.cpp` (trigger per game) | ✅ `levels\*\mission*.str` | ✅ `level\*.arc` | ✅ `archives\common_ents_zone.pak` |
| D3D12 pipeline library (`pipelines.d3d12`) | runtime, `gpu/d3d12_pipeline_cache.cpp` | ✅ (Windows; off on Xbox UWP) | ✅ same | ✅ same |
| Shader cache (AIR / SPIR-V / DXIL, zstd) | `private/<id>/ppc/shader_cache.cpp`, [shaders.md](shaders.md) | ✅ 1122 | ✅ 4443 | ✅ |
| GPU thread, texture threads, chunked submission, one frame in flight, bind on change | runtime, `gpu/worker.cpp`, `gpu/gpu.cpp` (above) | ✅ | ✅ | ✅ |
| Buffer cache keyed by the range read, its producer lock; 16-bit pair streams converted in it (2026-11-04) | runtime, `gpu/buffer_cache.cpp` | ✅ | ✅ | ✅ |
| Texture check by a 4 KB sample | runtime, `d3d/render.cpp` `CheckGuestContent` | ✅ | ✅ (measured there) | ✅ |
| Constants deduplicated, sampler slots hashed, `PhysicalToGuest` cache | runtime | ✅ | ✅ | ✅ |
| Kernel: one sleeper per waiting thread, yield back-off | runtime, `kernel/` ([threads_sync.md](../xbox360/threads_sync.md)) | ✅ | ✅ (measured there: 22 -> 30 fps class fix) | ✅ |
| Real-time vertical-blank thread, a late frame shown at once (`WaitForFlip`) | runtime, `kernel/video.cpp` ([video_vblank.md](../xbox360/video_vblank.md)) | ✅ | ✅ | ✅ |
| Presents per blank / fps limit | `game.h` flags per game | interval 2 (fps 60 lifts it) | `presentsAtEveryBlank` (limit 30 shows every second frame) | `presentIntervalPacesGame` (the runtime writes interval 1 at 60) |
| **The render thread's wait for its last swap** made a sleep | `games/swapforce/swap_wait.cpp`, `kernel/video.cpp` `WaitForVBlankHandled`; Trap Team: `games/trapteam/swap_wait.cpp` (a mid-instruction hook, 2026-11-03); SuperChargers and Imaginators: `engines/alchemy/swap_wait.cpp` (2026-11-04, below) | - (no such spin) | - (no such spin seen) | ✅ **new** |
| Thread priorities | only the interrupt thread (real-time); the guest's priority calls are ignored | same | same | same |

**Gaps left** (2026-10-28): Giants' and Swap Force's loads are bound by the games' own loader threads (guest code,
below), not by decompression any more; the baselines grow with figures no tour used (Giants: SSA figures other than
`new_14.sky`, Bouncer, LightCore variants; Swap Force: 11 of the 16 swappers and most cores, Giants and SSA figures,
the Swap Zones and arenas with them); Swap Force at 60 fps (52-56 fps, its own work: another session's).

## The perf log (`[perf]` lines every 10 s for the whole session; `SKY_PERF_LOG=0` turns off)

`[perf] <fps> fps; frame median M ms, p95 P ms; per frame: N draws taking D ms, GPU wait G ms, present, command lists, rest (guest CPU, renderer outside Draw, waits)`; then `[perf] render thread per frame: renderer R ms (ring, textures,
gpu::Draw), vblank wait, GPU wait and present, the rest = the guest's own code`; then `[perf] GPU thread busy X% ...; the game's render thread waited Y ms per frame for it; N texture threads busy Z%`. If Y is large and busy ~100% the
D3D12 recording is the limit; if GPU wait is large, the GPU. Per frame, with the GPU's own busy time, game-thread CPU, latency and hitch causes: the F3 debug
screen at Full and its performance log (`gpu/frame_stats.h`, [gamemenu.md](../host/gamemenu.md), "Debug screen"); the
renderer time's counters keep counting (the `[perf]` window is their difference). `SKY_FRAME_TIMES=<file>` writes `<ms since start> <work ms> <draws>` per swap (`tools/perf/frames.py` summarises: worst gaps in the first 5 s of the level and fps/median/p95/p99
over +10..+40 s). Rules: run before/after pairs **at the same time** on a shared machine (load average 20-50 skews everything), cold app clones for pipeline measurements, `tools/perf/level_run.sh` after every merge.

## Giants (2026-10-26) [Giants] [Alchemy] [Xbox]

The owner's run (kit, M1 Pro, Chapter 1, the Mac at load 63) showed a p95 of 33-36 ms at 46-48 fps and "guest code
12-17 ms per frame". Measured again headless with `tools/perf/giants_run.sh` (scale 2, a new game in the level, the
harness's walking and attacking presses, window 50..70 s; frame gap = time between presents, work = the render thread's
time without the vblank wait; load average = start/end of each run, 1-minute value).

**The cause: not the guest's own code.** `sample` (`tools/perf/sample_threads.py`) on Chapter 1: the render thread was busy
3871 of 5709 samples, **1559 of them in `NtReleaseSemaphore`** (1285 in `__psynch_cvbroad`): every kernel signal was a
`notify_all` on one condition variable where ~25 Giants threads sleep (5 JQ workers, audio, streaming, network); the woken
threads then fought over `g_kernelLock` (`__psynch_mutexwait`, 4204 samples over all threads). The render thread releases
the job semaphore many times a frame (`sub_822797F8` -> `sub_82409670` -> `NtReleaseSemaphore`). SSA has fewer waiting
threads and was not bound by it. Fix: per-thread sleepers ([threads_sync.md](../xbox360/threads_sync.md)), **[Xbox]**.
Second fix: the emulated vertical blanks came up to 8 ms late on macOS (timer coalescing), so frames were 25-41 ms apart
with 9 ms of work; the interrupt thread now runs with the real-time policy ([video_vblank.md](../xbox360/video_vblank.md)), **[Xbox]**.

| Scenario (scale 2) | Before (`b1c23e7`): fps, gap median/p95/p99/max, work median/p99 | After (`0a8e9f7`) | Load |
|---|---|---|---|
| Chapter 1 `level_213_tutotial`, limit 30 | 29.7 fps; 33.2/39.3/43.8/117 ms; work 17.2/43.7 ms | **30.0 fps; 33.3/33.4/33.4/33.7 ms; work 8.3/12.7 ms** | 2-9 |
| `level_203_the_machine`, limit 30 | 29.9; 33.1/39.5/41.1/95; 15.5/34.5 | 30.0; 33.6/38.8/40.0/40.7; 8.9/12.1 (fix 1 only) | 6 |
| `level_hub_2_stage_2`, limit 30 | 29.5; 33.3/40.1/50.1/139; 16.0/50.0 | 30.0; 33.6/39.0/40.5/41.6; 9.2/13.2 (fix 1 only) | 6-9 |
| `arena_challenge_201_000`, limit 30 | 29.0; 33.3/42.4/68.0/95; 23.7/67.9 | 30.0; 33.5/38.5/40.2/40.8; 7.0/11.7 (fix 1 only) | 3-8 |
| Chapter 1, limit 60 | 59.2; 16.7/19.3/23.7/39.8; 14.5/23.7 | **60.0; 16.7/16.7/16.7/16.8; 8.4/9.9** | 2-7 |
| `level_203_the_machine`, limit 60 | 58.8; 16.6/19.4/27.7/92.8; 13.9/27.6 | **60.0; 16.7/16.7/16.7/16.7; 8.3/10.9** | 3-6 |
| `level_hub_2_stage_2`, limit 60 | 55.2; 16.9/22.9/44.6/206; 16.3/44.6 | **60.0; 16.7/16.7/16.7/16.7; 4.1/5.4** (a lighter view: 231 draws, 571 before) | 3-4 |
| `arena_challenge_201_000`, limit 60 | 43.2; 20.8/37.3/64.6/105; 20.8/64.5 | **60.0; 16.7/16.7/16.7/16.7; 6.4/9.3** | 3-5 |
| title, limit 30 (20..55 s) | 30.0; 33.3/38.7/40.3/41.0; 3.9/6.6 | 30.0; 33.3/33.4/33.4/34.6; 2.2/7.5 | 1-2 |
| intro movies (Bink 2), limit 30 (1..30 s) | 29.3; 33.3/39.0/40.6/438; 5.7/10.7 | 29.9; 33.3/33.3/33.4/102 (the first frame at 1.1 s) | 2-3 |
| menus and the level load (15..50 s), limit 30: worst gaps | 158-296 ms | 41-48 ms | 3-9 |

"Fix 1 only" rows: the same-time pair before the vblank fix; with it every level run since shows the Chapter 1 pacing
(gaps within 33.3 +- 0.5 ms). Before the first fix, under the owner's kind of load (13-29), Chapter 1's work was 22.7 ms
median, 51 ms p99 (now 10.8 and 18.3 at load 15-26).

**SSA** (`tools/perf/level_run.sh`, scale 2, same-time pairs, load 2-6): level_027 work 18.6 -> 19.7 ms median, p99 22.5 ->
22.7 (noise), gaps p99 40.1 -> **33.4 ms**; level_010 work 9.6 -> 9.5, gaps p99 41.1 -> 33.4 ms, max 49.5 -> 33.8; at limit
60 level_027 stays at 54.4 fps (render-thread bound: work 18.5 ms, the "Where a level frame still spends time" list above).

**What was left in Giants on 2026-10-26** (Chapter 1 at 60 fps, `sample` 10 s; the texture hash part is done, next section): the render thread is busy 62 % of the time (9.9 ms of
16.7): `IssueDraw` 27 % of its busy time, of it the texture content hash (`XXH3`, `GuestContentHash`, every texture
once a frame) 14 %; `steady_clock::now` for the perf timers 3 %; the guest's own code the rest, spread (no function over
4 %: `sub_821DFB08`, `sub_82269E08` under the main update `sub_821C7558`). The audio thread (`sub_8238F178`...) 6 %, the
GPU thread 11 %, the JQ workers ~2 % each (1 ms timed waits). No spin-wait, sleep, memcpy, decompression or file stream
showed in the profiles. 60 fps holds on this Mac with ~40 % margin; a slower CPU (Windows iGPU PC, Xbox) would first
meet the texture hash and the guest code.

## Giants (2026-10-27): the texture check and first visits [Giants] [Alchemy] [Xbox]

Measured with `tools/perf/giants_run.sh` (scale 2, limit 60, window 50..70 s, `SAMPLE=1`), base = `main` `d51f3be`, pairs run
one after the other; load average = the 1-minute value at the start and end of each run. Texture-hash share = `XXH3`
samples directly under `BuildDraw` over the render thread's busy samples (`tools/perf/sample_threads.py --callers`).

**1. The texture check.** Per frame in `level_203_the_machine` the renderer checked 222 textures and hashed **21.9 MB**,
19.9 MB of it in the 125 textures of 64 KB and more, with no change at all. Now a texture from 64 KB is checked by a
4 KB sample and hashed whole every 8 frames, when the sample changes and the frame after a change ("Caches" above):
**4.3-4.6 MB** a frame. Same-time pairs, quiet machine:

| Scenario (limit 60) | Before: work median/p99, gap p99, texture hash | After | Load before / after |
|---|---|---|---|
| Chapter 1 `level_213_tutotial` | 13.8/27.3 ms, 27.4 ms, **13.7 %** (793 samples) | **10.0/15.2 ms, 17.0 ms, 3.5 %** (152) | 7-10 / 7-10 |
| `level_203_the_machine` | 13.9/30.9 ms, 31.1 ms, **12.4 %** (739) | **11.0/29.1 ms, 29.1 ms, 3.1 %** (160) | 5 / 5-8 |
| `arena_challenge_201_000` | 8.3/13.9 ms, 17.8 ms, 5.7 % (186) | 8.7/18.8 ms, 19.3 ms, 2.1 % (79) | 5-7 / 6-8 |

The same textures under load 22-27: 8-11 % -> 2-3.3 %. Checks (`SKY_TEXTURE_STATS=1`, every sampled texture also hashed
whole): Giants `level_203` 98 windows of 60 frames and title -> Chapter 1 49 windows: **0 changes missed** (and none
found: Giants does not rewrite its big textures there). SSA `level_027` rewrites one 1120x704 RGBA8 texture at
`0x06A60000` almost every frame (1452 changes in a run): the sample missed **1** of ~1600 changes, shown 7 frames late,
before the rule "hash whole the frame after a change"; with it, 1232 changes found, **0 missed**. The perf timer around
`TextureFor` now starts only for a texture not yet checked this frame (two clock reads per texture slot of every draw).
**SSA** pairs (`tools/perf/level_run.sh`, scale 2, limit 30, load 7-9): `level_010` work 13.2 -> 9.1 ms median, 27.5 ->
30.0 fps; `level_027` work 21.5 -> 20.6 ms, the render thread's texture time 3.0 -> 1.5 ms a frame (the after run lost
fps to the GPU: GPU wait 21-26 ms against 0.3-9 ms, another job's game shared it); again at load 4-6: `level_027`
30.0 -> 29.9 fps, work median 19.0 -> 17.9 ms (p99 22.4 -> 39.1: one 3.5 s burst; the after run repeated: 29.8 fps,
17.5 ms, p99 25.3), texture time 2.2 -> 1.2 ms. Under load 15-70: `level_027`
23.4 -> 20.0 and 48.7 -> 34.4 ms, `level_010` 19.0 -> 17.7 ms.

**2. First visits.** The hitches the first time a level is played are **pipelines the baseline lacks**, built at draw
time: 20-67 ms each on a cold Metal cache (a new player's app), then saved in the player's `pipelines.bin` (so a second
visit is smooth). The 1575-pipeline baseline came from a tour with one SSA figure; other figures draw with other
states and shaders: Tree Rex built 17-26 new pipelines per level run, Hot Dog 58-145 (in `level_200`, 52 of its 123 new
keys have a pixel shader the baseline never saw; the rest are known shaders with another alpha test spec constant, cull
mode, vertex layout or blend). The baseline was re-made from the Tree Rex and Hot Dog tours of 23 levels (story, hubs,
arenas), one `level_200` run for each of the 13 other Giants characters (`TOUR_FIGURE`) and these perf runs:
**1575 -> 2363 pipelines** ([pipeline_cache.md](pipeline_cache.md), "Giants"). Checked with a figure no tour used
(Bouncer, `FIGURE=new:110:1206`), warm cache: pipelines built at draw time `level_203` **23 -> 1**, `level_210_thevillage`
**55 -> 5**, `level_hub_2_stage_2` **102 -> 6** (295 / 438 / 1012 ms of builds -> 0 / 2 / 3 ms). Cold cache, a new
player's first launch (an app copy with a fresh bundle id), Bouncer, `level_203`: before 24 builds (350 ms, the slowest 53
ms) and frames of 50, 79 and 151 ms in the level; after 1 build (0.5 ms) and no frame over 40 ms after the level's first.
The bigger list costs nothing measurable at start: ready after 6.9 s cold in both (the title's loading wait 6.0 s with
the movies skipped; the intro movies cover it), 1.6 s warm. **[?]** One earlier cold run (`new_14.sky`) had a 209 ms frame
at 70.9 s and 84-125 ms frames at 87.3 s (the pause menu) with no draw-time build; the repeat was swamped by load 35-73.

**What is left** (render thread, Chapter 1 and `level_203` at 60 fps): the buffer cache's revalidation of resident
vertex/index ranges (`XXH3` under `Acquire`, 4-8 %: the same sampling would apply, with the buffer cache's own rules);
the guest's own code; first visits with figures no tour used (5-6 builds per level with Bouncer) and with SSA figures
other than `new_14.sky` (not toured). The base's work median at the 60 limit, 13.8-13.9 ms against 8.3-8.4 ms on
2026-10-26, was not a regression: the measurement (next section).

## Giants (2026-10-27): no slowdown since 2026-10-26, the measurement was off [Giants] [Xbox]

The work medians above (13.8-13.9 ms for `d51f3be` at the 60 limit) looked 60 % worse than 2026-10-26's 8.3 ms. **[V]
No code since `77c67ea` made frames heavier**: back on a quiet machine, `main` (`75ddcd0`) is at 7.9-8.6 ms, faster than
`77c67ea` in every pair (the texture check above). Two things inflated the numbers, neither in the game:

1. **`sample` inside the window.** `SAMPLE=1` ran macOS `sample` from 60 to 70 s, inside the 50..70 s summary window;
   it interrupts every thread about once a millisecond. Same build, quiet machine (load 4-5), `level_203` at 60: frames
   50..60 s 8.6 ms, 60..70 s **11.8 ms**; the window's median 10.5 ms against 8.1 ms without `SAMPLE` (load 4-5 too).
   Under load 20-50 the sampled 10 s added 3.7-4.9 ms against 0.5-2 ms in the same seconds of runs without it.
2. **The machine's load.** Other jobs build and play on the same Mac (6 performance and 2 efficiency cores): the load
   average moved from 3 to 63 within minutes. The harness runs the game under `nice`, so under load its threads lose the
   performance cores: the render thread's own CPU time per frame grows from 8.5 ms (load 3-5) to 10-11 ms (load 20-50),
   the frame's work (a wall time) to 16-24 ms. A load average taken at the start and the end of a run (the "load"
   columns above) misses spikes in between.

Back-to-back pairs (`tools/perf/giants_run.sh`, scale 2, window 50..70 s, no `SAMPLE`; each pair started only under a
1-minute load of 8; both builds with the same generated code, `private/giants/ppc` of 2026-10-26 21:22): work median,
render thread CPU per frame (`ps -M`, below):

| Scenario | `77c67ea` (round 1 / round 2) | `main` `75ddcd0` (round 1 / round 2) | Load |
|---|---|---|---|
| Chapter 1 `level_213_tutotial`, limit 60 | 9.0 / 8.8 ms; CPU 9.1 / 9.0 | **8.2** / 16.6 ms (*); CPU 8.4 / 10.5 | 4-5; (*) 15 -> 40 during the run |
| Chapter 1, limit 30 | 9.6 / 9.5 ms; CPU 9.6 / 9.6 | **8.6 / 9.0 ms**; CPU 8.7 / 9.2 | 3-5 |
| `level_203_the_machine`, limit 60 | 8.7 / 9.5 ms; CPU 9.0 / 9.9 | **8.4 / 8.1 ms**; CPU - / 8.5 | 3-5 |
| `level_203_the_machine`, limit 30 | 11.0 / 9.2 ms; CPU - / 9.7 | **7.9 / 8.1 ms**; CPU 8.3 / 8.6 | 3-7 |

"-": the thread list changed in the window (a thread ended, so the per-thread figures are shifted; `thread_cpu.py` flags
the impossible ones). Every run held its limit (30.0 / 60.0 fps, gap p99 33.4 / 17.0 ms at most) but (*). The suspects
named for the merges did not show in `sample` (render thread, `level_203` at 60, ~4500 busy samples in 10 s): the Toy
Box 1 sample, the portal, `XamInputGetState` and the language code 0; the buffer cache's producer lock (gfix: a
recursive mutex taken by every `CacheVertices` / `CacheIndices` call from `BuildDraw`, uncontended once the game
presents) 20-30 samples of lock and unlock, under 1 %. The only change in the generated code since 2026-10-26 is XenonRecomp's 32-bit jump-table switch
(323 `switch (ctx.rN.u64)` -> `.u32`, nothing else): `main` built with the older code measured the same (8.3 vs 8.5 ms,
same-time pair).

**What changed in the tools.** `giants_run.sh` now starts `sample` at the window's end by default (`SAMPLE_AT` still moves
it) and prints the CPU time per frame of the process and its busiest threads over the window (`tools/perf/thread_cpu.py`,
`ps -M` at the window's ends). Compare that line between runs at different loads; the work time only within same-time
pairs. **How to verify**: run `main` twice, `SAMPLE=1 SAMPLE_AT=60` then without, at a quiet moment; the first one's
`frames.py --from 60 --to 70` is 2-5 ms higher.

## Loading and frame time per game (2026-10-28) [SSA] [Giants] [SwapForce] [Alchemy] [Xbox]

All on the M1 Pro, scale 2, headless and isolated (`tools/perf/level_run.sh`, `giants_run.sh`, `swapforce_run.sh`, which
now print `tools/perf/load_times.py`); a shared machine (another session building and playing): the load average
(1-minute, start/end of each run) is given, and every before/after is a pair of runs one after the other in the **same
build**, switched by an environment variable (`SKY_HOST_LZMA=0`, `SKY_SWAP_WAIT=0`). Repeat any number that looks off:
under load 40-50 a single run doubles its load time.

**Loading** (the time from the level's own file to the level's first full frame: `level_027\mission.str`,
`level\level_213_tutotial.arc`, `\archives\brn2.pak`; "boot" = start-up to the first frame with 100 draws, movies
skipped, the presses of each script):

| Game | Boot | Level load | What the load spends | Load average |
|---|---|---|---|---|
| SSA `level_027` | 20.7-22.6 s (the level itself: `SKY_LEVEL` starts in it) | 0.3-0.5 s for `mission.str` (2026-10-26, host zlib) | the guest's first update (150-290 ms first frame) | 5-14 |
| Giants Chapter 1 | 17.0-18.0 s (the title, at the script's presses) | translated LZMA **1.73 / 2.33 s** -> host **1.23 / 1.00 s**; 0.86 s at the 60 limit | before the `.arc`, ~4 s of save and fade with no file read (the game's own timing) | 6-27 |
| Swap Force `brn2` | 3.7-7 s (the first menu frame) | translated **5.6 / 6.8 / 8.7 s** -> host **4.9 / 5.0 / 5.3 s** | LZMA on two job workers, the main thread's own loading code | 15-40 |

What the loads decompress and how [V]: SSA zlib (`mission.str`, host since 2026-10-25); **Giants and Swap Force chunked
LZMA** (IGA archives) through the LZMA SDK linked in the title, now host code
([alchemy/lzma.md](../../engines/alchemy/lzma.md)): the same 285 MB (Swap Force, start -> `brn2`) took **8.5-10.1 s**
of the decoder threads translated and **4.8-5.9 s** host, the same 201 MB (Giants, start -> Chapter 1) **9.5-11.0 s**
and **5.3-5.7 s**. Both games also link zlib 1.2.3, never called in these loads (no `inflate` sample). Reads: the disc
reads (`pread` on the ISO) were under 1 % of the loader threads' samples: not a cost. What is left of a load is guest
code: Giants' `tfbLoadThread` (busy ~72 % of a load, its archive manager's request loop `sub_8225B838` the top
function), Swap Force's main thread (~60 %); the decoders are no longer saturated.

**First visit, second visit**: what differs is the pipelines the list lacks (built at draw time, 20-67 ms each on a
cold Metal cache, then in the player's `pipelines.bin`, so 0 on the second visit). Swap Force's baseline came from one
figure (Eruptor): the swapper Blast Zone built **21** in `brn1` on a first visit, 25 in `winterkeep02`, 10-11 in most
story zones (the same ~10 keys: its own effects), Scorp 3; a tour with Blast Zone and Scorp through the 16 story zones and
seven more figures (`new:2000,1000`, `2008,1008`, `2012,1012`, `2015,1004`, cores `3000`, `3006`, `3010`) in `brn2` and
`fg1` found **27 new keys: 711 -> 738**. After: Blast Zone in `brn1` **21 -> 1** build, in `winterkeep02` 25 -> 0. Giants:
1 build in Chapter 1 with `new_14.sky` (2363-key baseline, 2026-10-27). SSA: 0 at `level_027`.

**Frame time** (window 50..70 s Giants, 45..65 s Swap Force, 10..40 s after the level's start SSA; CPU = `thread_cpu.py`,
the process and its busiest threads per frame):

| Game, limit | fps | gap median / p99 | work median / p99 | CPU per frame: process, busiest threads | Load |
|---|---|---|---|---|---|
| SSA `level_027`, 30 | 30.0 | 33.3 / 34.2 | 18.4 / 24.5 | - (`level_run.sh` takes no `ps`) | 5 |
| SSA `level_027`, 60 | 54.4 | 18.0 / 26.2 | 17.9 / 26.1 (render-thread bound, as on 2026-10-26) | - | 14 |
| Giants Chapter 1, 30, translated LZMA | 29.9 | 33.3 / 34.9, 34.0 | 9.4 / 17.3, 9.7 / 18.4 | **18.1, 17.7**; 9.5, 2.7, 2.4 | 6-27 |
| Giants Chapter 1, 30, host LZMA | 30.0 | 33.3 / 33.4, 34.1 | 8.3 / 12.3, 8.6 / 15.4 | **15.1, 15.8**; 8.4-8.6, 2.1-2.2 | 6-26 |
| Giants Chapter 1, 60 | 59.2 | 16.7 / 28.2 | 7.9 / 26.4 | 12.7; 7.8, 1.9 | 6-20 |
| Swap Force `brn2`, 30, spin | 29.9 | 33.3 / 35.0, 42.6 | 16.6 / 19.7, 16.7 / 38.7 | **46.4, 46.2**; render thread **14.6, 13.8** | 12-41 |
| Swap Force `brn2`, 30, swap wait | 29.9 | 33.3 / 34.0 | 16.6 / 17.5 | **41.0**; render thread **9.5** | 12-21 |
| Swap Force `brn2`, 60, spin | 55.4 | 17.6 / 30.8 | 13.0 / 29.9 | 38.6; 10.7 | 22-29 |
| Swap Force `brn2`, 60, swap wait | **58.8** | 16.7 / 26.9 | 16.6 / 25.9 | 37.3; 10.0 | 21-33 |

Giants decodes in the level too (streamed sounds and objects): the host LZMA took 2-3 ms of CPU a frame off the
process and the work's p99 from 17-18 to 12-15 ms. Swap Force's process CPU (37-46 ms a frame: ~1.2 cores at 30 fps)
is mostly its job workers' yields (`swtch_pri` under `NtYieldExecution`, 40 % of three workers' samples in a load and
menu profile) and its own render and update code; Swap Force at 60 fps is another session's work.

## Trap Team (2026-11-02): the pipeline baseline [TrapTeam] [Alchemy] [Xbox] [V]

The same build before and after its baseline (658 pipelines, [pipeline_cache.md](pipeline_cache.md), "Trap Team"), M1
Pro, scale 2 (auto), limit 60, headless and isolated (`tools/pipelines/tour_trapteam.sh`, 130 s, `SKY_LEVEL`), a shared
machine.
Builds = `pipeline built at draw time` lines after the level's `.arc` opens; first frame = the level's first frame with
100 draws or more after that open, its gap; worst = the longest gap in the 10 s after it. Gusto toured every level;
Head Rush (`new:455`) toured none.

| Level, figure | Builds before -> after (ms of builds) | First frame before -> after | Worst gap before -> after |
|---|---|---|---|
| `level_313_tutorial`, Gusto | 125 (27) -> **0** | 17 -> 33 ms | 27 -> 34 ms |
| `level_hub_2014_01`, Gusto | 128 (42) -> **0** | 35 -> 43 ms | 79 -> 41 ms |
| `level_300_birdcage`, Gusto | 142 (38) -> **0** | 18 -> 23 ms | 35 -> 33 ms |
| `level_305_wilikinfactory`, Gusto | 125 (25) -> **0** | 23 -> 33 ms | 36 -> 51 ms |
| `level_313_tutorial`, Head Rush | 136 (35) -> **0** | 33 -> 33 ms | 34 -> 81 ms |
| `level_hub_2014_01`, Head Rush | 142 (53) -> **0** | 36 -> 33 ms | 70 -> 37 ms |
| `level_300_birdcage`, Head Rush | 141 (45) -> **1** (0) | 33 -> 33 ms | 71 -> 35 ms |
| `level_305_wilikinfactory`, Head Rush | 125 (46) -> **0** | 33 -> 31 ms | 53 -> 53 ms |

The Metal cache was warm in every run (the same app: 0.2-0.4 ms a build), so the builds cost under 55 ms a level and
the frame times above are noise at this load; on a new player's cold cache each build is 20-67 ms (Giants, above), so
the 125-142 builds were 2.5-9 s of hitches in a first visit. The 658 are ready 0.1 s after start; the title's and each
level's loading wait then has nothing left to build.

## Swap Force's swap wait (2026-10-28) [SwapForce] [Xbox] [V]

Swap Force's render thread ends each frame (`sub_82515D68`) by spinning on a status call (`sub_82F5CB30`: the swaps
issued minus the swaps shown, device +16968 and +16976) until its last swap is shown, which D3D's vertical blank
callback (`sub_82F5C128`) records at the swap's blank; then it calls `D3DDevice_Swap`. That spin was **54 % of the render
thread's busy samples** in a menu and load profile (`sub_82515D68` 3181 + the status call 2784 of 11027) and ~5 ms of
CPU per frame in `brn2` at 30 fps. Now `games/swapforce/swap_wait.cpp` hooks the status call: while a swap is pending it
waits for the next graphics interrupt's callback (`InterruptsHandled` / `WaitForInterruptHandled` in
`kernel/video.cpp`, read before the check so an interrupt in between ends the wait at once: a first version waited for
the next blank after the check and lost one blank in two at the 60 limit, 31-35 fps), then reads the status as the spin
would. Same build, `SKY_SWAP_WAIT=0` against default: render thread 13.8-14.6 -> **9.5 ms** of CPU a frame at 30
(process 46 -> 41 ms), 60-limit pair 55.4 -> **58.8 fps** (gap p99 30.8 -> 26.9 ms). Neither Giants nor SSA has that
spin (their render threads wait in `WaitForFlip` inside the Swap hook).

## What can still be gained at 60 FPS and scale 2 (2026-10-28) [SwapForce] [SSA] [Xbox] [?]

Where the remaining cost is ours rather than the games' own, from the GPU work branch (measured on a shared
machine: to confirm on a calm one). The games' own share: Swap Force renders its 3D at 1040x620 and 30 FPS on the console;
here, at scale 2 and 60 FPS, 2560x1440 is about 5.7x the pixels twice as often (~11x the console's work), and at scale 1 it
holds 60. Our share, in order of expected gain:

| What | Cost now | Lead |
|---|---|---|
| **Resolves as real copies** (Swap Force) | ~28 a frame, 15-17 of the whole picture: ~300 MB and ~3.6 ms of GPU a frame at scale 2 | read the render target as a texture where nothing draws over it before the read (aliasing) instead of copying; 3 of ~31 repeated copies already skipped |
| **One frame in flight** | the GPU idle ~40 % of a frame | two frames in flight (`SKY_FRAMES_IN_FLIGHT`, on the branch; a regression was fixed there and must be re-checked with `test_all.sh`) |
| **Translated shaders** (XenosRecomp output) | generic code, more ALU than a hand-written shader | per pass GPU timings (Metal timestamps) to find the costly shaders, then fold constants and drop dead paths in the translation |
| **SSA's render thread** (CPU) | ~7 ms a frame of PM4 translation + 8.5 ms of the game's code: 57.4 FPS, ~2 ms short of 60 | move part of the draw translation off that thread |
| **A GPU shared with another program** | Swap Force ~56 FPS instead of 60 | the copies above; nothing else to do on our side |

Next: a GPU audit of Swap Force on a calm machine (time per pass, the costly shaders, each copy's share, scale 1 vs 2:
does the time follow the pixel count, or is part of it our overhead).

## Where the frame goes, per game (2026-11-03) [SSA] [Giants] [SwapForce] [TrapTeam] [Lux] [Alchemy] [Xbox]

The question: why do the games run slowly and cap at 30? Measured on the M1 Pro, default settings (scale auto = 2,
anisotropy 4, `fps_auto_fallback` on), offscreen with vsync off, one game at a time, a shared machine (load average
6-30 from the other lanes' builds: compare pairs only). `tools/perf/game_perf.sh <game> <disc> <level> <fps> <out>`
(`FALLBACK=0` holds 60, `SAMPLE=1` takes a `sample` after the window; window 65-95 s of a 100 s run); the split is the
`[perf]` lines of the window's end, the hot functions `tools/perf/sample_threads.py` on the sample.

| Game, level (draws) | 30: work median / p95 | 60 before (fallback on): fps, drops | 60 held (fallback off): fps; render thread renderer + guest, waits | 60 after (fallback on): fps, drops |
|---|---|---|---|---|
| SSA `level_027` (1780-1930) | 11.1 / 16.0 | 29.9, 2 | 59.3; 5.9 + 6.2, GPU thread 1.1 | **59.8**, 1 |
| SSA `level_008`, no hero (3456) | 18.9 / 26.2 | 29.8, 2 | 55.8; 10.2 + 8.2 | 31.2, 4 (its heaviest view: 41-50 fps at each try) |
| Giants `level_213_tutotial` (550) | 8.2 / 10.5 | 59.6, 0 | = before; 2.2 + 4.8 | 59.9, 0 |
| Giants `level_203_the_machine` (510) | 7.9 / 10.5 | 59.8, 1 | 59.2; 3.7 + 3.6 | 59.6, 0 |
| Swap Force `barrens/brn2` (1960) | 16.6 / 16.6 | 37.1, 4 | 59.5; 6.5 + 10.4 | **59.3**, 3 |
| Swap Force `wildwoods/ww1` (1715) | 16.6 / 17.2 | 36.9, 5 | 59.8; 5.5 + 11.2 | **57.3**, 1 (GPU wait 11.9 ms in its last 10 s) |
| Trap Team `level_313_tutorial` (1410) | 16.6 / 16.6 | 29.9, 4 | 53.1; 7.6 + 5.2, GPU thread 6.1 (GPU wait 16.2); **60.0 at scale 1** | 32.7, 2 (GPU-bound) |
| Trap Team `level_hub_2014_01` (820) | 16.6 / 16.6 | 34.5, 5 | 60.0; 3.8 + 12.8 (the swap spin, below) | **59.1**, 0 |

"Work" is the render thread's time from one flip to the next swap, without the vblank wait; Swap Force and Trap Team wait
for a blank in their own code (the swap wait below), so theirs reads 16.6 ms whatever the frame costs. "Guest" is the
`[perf]` "guest code and other": the game's own code on the render thread **and its waits** (job queues, the swap spin).

**Causes, by what they cost:**
1. **The automatic 60 -> 30 drop** [V] (`host/settings_fallback.h`, [video_vblank.md](../xbox360/video_vblank.md)).
   Three late frames in 30 halved the rate, and 60 came back only after 300 fitting frames **in a row** at 30, a try
   failing within 2 s doubling that up to 5 min. A single late frame at 30 (SSA's streaming hitches: 1-3 a second in
   `level_027`) restarted the count, so a game that holds 59.5 fps (SSA `level_027` held) sat at 30 for the whole run.
   This is the cap the owner sees. Now: 20 late in 120 frames (a frame in six over 2 s, under ~50 fps) to halve; a try at
   60 after 300 frames at 30 unless more than 1 in 20 of them were late; a failed try doubles the wait up to 60 s.
2. **The GPU at scale 2** [V]: Trap Team's tutorial held at 60 is GPU-bound (GPU wait 16.2 ms a frame, the GPU thread
   100 % busy, the render thread waiting 6.1 ms for it: 53 fps, and the `[perf] hint` says so); Swap Force's and Trap
   Team's hub windows swing between GPU wait 0 and 13.7 ms with what else uses the GPU. The owner's Trap Team report
   (GPU wait 8.3 ms, GPU thread 69 % busy) is the same limit. Leads: the resolves as copies, the translated shaders
   ("What can still be gained" above). The same tutorial at `--scale 1`, held: 60.0 fps, GPU wait 0, work p99 16.6 ms.
3. **SSA's render thread** [V] (CPU): in `level_008` (3456 draws) the renderer is 10-12 ms of the frame and the guest 8 ms.
   Its largest single cost was the buffer cache's validation: 23 % of the render thread re-hashed 142 MB a frame (the
   draws of one mesh read overlapping ranges, each its own key) for 12 MB of distinct bytes (`SKY_BUFFER_STATS=1` now
   prints both and the bytes read). Now each chunk is hashed once a frame (`gpu/range_hash.h`, exact: the edges of a range
   are hashed directly; `ArkeyanRangeHashTest`). First with 1 KB chunks and an always-on table: 12.3 MB read, the hash 23 %
   -> 12.5 % of the render thread; same build, `SKY_RANGE_HASH=0` against default, 60 held: `level_008` **50.2 -> 55.8
   fps**, work median 19.7 -> 17.7 ms, renderer 12.3 -> 10.2 ms; `level_027` 59.5 / 59.3 fps (it holds 60 either way).
   Alchemy's ranges do not overlap (Trap Team's tutorial: 27.9 MB hashed for 27.8 MB distinct), and there that table
   doubled the hash's cost (micro-benchmark, 28 MB in 1040 ranges: 0.84 -> 1.59 ms a frame), so the final version uses
   4 KB chunks and drops the table for 120 frames after a frame where under a quarter of its lookups hit (the hash values
   are the same with or without it): no overlap 0.80 -> 0.83 ms, SSA-like overlap 4.0 -> 0.7-1.1 ms. Final version, two
   same-build pairs in a row (`SKY_RANGE_HASH=0`, then default; load 4-12), 60 held:
   `level_008` renderer **11.7 -> 8.6 ms** and 11.9 -> 8.8 ms, work median 19.4 -> 16.2 and 19.5 -> 16.4 ms, 49.4 -> 56.3
   and 49.7 -> 46.1 fps: in the second pair the GPU thread (94 % busy, 30 % of it `WriteBackDepth`, item 5) and the GPU
   set the rate once the render thread is faster. Trap Team's hub 59.3 -> 60.0 fps, renderer 3.4 / 3.3 ms (no cost
   where nothing overlaps).
4. **Trap Team's swap spin** [V], a wait, not work: its render thread ends each frame spinning on the swap counters
   (device +16968 issued, +16976 shown), like Swap Force's (above), but the loop is inlined in the guest
   (`sub_823EE370`, guest `0x823EE484`): 36 % of the render thread's samples in the hub (859 of 2403), a host core busy
   for nothing. No function to hook: the fix is a mid-instruction hook at that loop (XenonRecomp config, so a new
   `private/trapteam/ppc/`), waiting as `games/swapforce/swap_wait.cpp` does: done the same day
   ("A stable 60 (2026-11-03, part 2)" below). It also makes Trap Team's `[perf]` "guest code" (12 ms in the owner's report) mostly waiting.
5. Smaller, on other threads [V]: SSA's depth write-back (`WriteBackDepth`, the depth the light code reads, untiled per
   texel on the GPU thread) is 30 % of SSA's GPU thread, which reaches 82-94 % busy in `level_008`'s heaviest view:
   SSA's next limit there once the render thread got faster (made cheaper the same day, part 2 below); two SSA
   threads spin in the guest's `sub_82520638` (20-25 % of a core each); NtSetEvent's unlock wakes a waiter
   (`__psynch_mutexdrop`, ~10 % of SSA's Game Master in one sample, not in another). Leads [?], not changed.

Giants holds 60 in both levels (work 7-8 ms; nothing to fix). The rest of a Swap Force or Trap Team frame (renderer 4-8 ms,
guest 5-12 ms with the waits) fits in 16.7 ms when the GPU keeps up.

## A stable 60 (2026-11-03, part 2) [SSA] [SwapForce] [TrapTeam] [Lux] [Alchemy] [Xbox] [V]

The owner's recheck at 60 (`recheck60.sh`, `game_perf.sh` per level, the drop switch on) still dropped to 30 once or
twice in 6 of 8 runs. **When** (`frames.txt` and the `[file] open` times, the 2.5 s before each drop): Swap Force at
38 s while it loads Eruptor (`eruptor.pak` and its banks: 22-25 ms frames of 70 draws, a load, not the scene); Trap Team's
tutorial at 66 s after its save and level start (1410 draws at 48-50 fps: GPU-bound, below); SSA `level_027` and
`level_008` in their heaviest view (3700-4000 draws at ~51 fps for 2 s or more: the rule's "under ~50", a real shortfall).
(The recheck passed Swap Force's zones as `barrens_brn2_brn2`, which `SKY_LEVEL` ignores: it measured the menu world;
`game_perf.sh` now reads that spelling as `barrens/brn2/brn2`. Its drop count matched no log line: the message now says
`drop to 30 FPS until they fit again`.)

- **A load does not count** (`host/settings_fallback.h`, `kernel/video.cpp`, `LastFileOpenNs` in `kernel/files.cpp`): the
  frames in the 5 s after any file open are not sampled; after them the miss window starts empty, and a game at 30 is
  back at 60 at once. `ArkeyanSettingsTest` covers both. 2 s was too short: Swap Force's slow frames go on ~4 s after
  Eruptor's last file opens, with no read in between (`SKY_TRACE_FILES=1` now times each read: the zone streams
  ~150 KB/s of sound, 1-2 reads a second, so reads alone cannot say "loading"). `brn2` at 60, two runs: 0 drops,
  60.0 fps.
- **SSA's depth write-back** a tile at a time (`d3d/tiled_depth.h`, `ArkeyanTiledDepthTest`): the guest's light job reads that
  depth every frame (its hook runs every frame in a sunlit level), so it cannot be skipped; `Tiled2D(x, y)` for 32-bit
  texels is the 4 KB tile's index plus an offset that depends on `(x % 32, y % 32)` only, so a 32x32 table (from
  `Tiled2D` itself, the split checked against it over the whole image once per layout, else the old per-texel path)
  replaces a call per texel. `level_008`, 60 held, GPU thread samples in the write-back **350 -> 153** (30 % -> 15 % of
  the GPU thread).
- **Trap Team's swap wait** (`games/trapteam/swap_wait.cpp`, a `[[midasm_hook]]` at `0x823EE484` in
  `config/TrapTeam.toml`; XenonRecomp then changes only `ppc_recomp.46.cpp`, so `private/libs/trapteam` got that one
  object recompiled and replaced, the old library kept as `.before-swapwait`): the hub, 60 held, `SKY_SWAP_WAIT=0`
  against default: render thread **16.2 -> 6.6 ms of CPU a frame** (busy 99.7 % -> 58 %), process 28.1 -> 18.7 ms,
  60.0 fps either way, worst frame 87 -> 21 ms.
- **Trap Team's tutorial on the GPU** (Metal System Trace, `xctrace record --template "Metal System Trace" --attach`,
  3.7 s at 52.5 fps, scale 2): per second, fragment 448 ms, **blits 220 ms**, vertex 98 ms; per frame ~48 render
  passes (plume already folds a clear into the pass's load action) and **~39 blit encoders, 4.2 ms of the ~19 ms GPU
  frame**. The blits are ours: the game's 25 resolves a frame (`SKY_DUMP_FRAME`), 14 of them the whole 2560x1440
  target, each a copy of 140-200 us at scale 2, plus the uploads. The fragment work (8.5 ms) is the game's shaders at
  4x the console's pixels. Cutting the copies means sampling the render target where nothing draws over it before the
  read (aliasing), the lead of "What can still be gained" above: not done (a frame-wide hazard analysis) [?]. So the
  tutorial stays GPU-bound at 2 (52.6 fps), and **scale auto steps down at the next start**
  ([settings.md](../../getting-started/settings.md), `resolution_scale`; `gpu/auto_scale.h`, `ArkeyanAutoScaleTest`): the
  run writes `auto_scale.txt`, the next start renders at 1: **60.0 fps**, work p99 16.7 ms; `--scale 2` given by the
  player stays 2 (45.6 fps in that run, nothing written).

- **What is left: SSA's heaviest views** [V]. `level_027`'s opening view (2900-3990 draws, the first ~25 s of the level)
  and `level_008`'s run at 46-51 fps, 60 held: the render thread does 17.3 ms a frame there (renderer 9.0 ms, guest
  8.3 ms; `BuildDraw` 45 % of its samples: the vertex and index caches 12 %, `TextureFor` 8 %, the float constants 6 %,
  ~25 ring register reads a draw under the ring's mutex 7 %). That is the rule's "under ~50 fps" for 2 s, a real
  shortfall: the drop is the owner's choice there (a steady 30 over an uneven 46-50). Each of those costs is 1-3 % of
  the thread; holding 60 there needs ~3 ms off the frame: moving the draw translation off the render thread (the lead
  of "What can still be gained") [?].

**The recheck** (`recheck60.sh`: every level 100 s at 60, the switch on, a new data folder each run, load 4-6): window
65-95 s fps, and drops to 30 in the whole run (`fallbacks to 30` of each `<game>-<level>.txt`).

| Level | Before (the owner's run) | After |
|---|---|---|
| SSA `level_027` | 55.1 fps, 2 drops | 57.4, 2 (the opening view, 46-51 fps for ~25 s: above) |
| SSA `level_008` | 59.1, 1 | 56.1, 1 (the same heavy view) |
| Giants tutorial / `level_203` | 59.7 / 59.7, 0 / 0 | 60.0 / 60.0, 0 / 0 |
| Swap Force `brn2` | 59.3, 1 (the menu world: the zone name was ignored) | **59.9, 0** (the zone itself) |
| Swap Force `ww1` | 59.5, 1 (the menu world) | 59.9, 1 at 97 s: 60-212 ms hitches after the window, no file open, the machine's load going 5.7 -> 12.6 [?] |
| Trap Team tutorial | 44.9, 1 | 52.8, **0** (GPU-bound at 2; a second start renders at 1: 60.0, 0) |
| Trap Team hub | 60.0, 0 | 60.0, 0 |

## The last pass (2026-11-03, part 3) [SSA] [SwapForce] [TrapTeam] [Lux] [Alchemy] [Xbox]

- **SSA's heaviest view** (`level_027`'s opening, 2850 draws a frame, 60 held, ~54 fps) [V]: `BuildDraw` is 45 % of
  the render thread; inside it the vertex and index caches 12 %, `TextureFor` 8 % (its once-a-frame texture checks:
  XXH3 96 samples), the float constants 6 %, the ring register reads 7 % (a third of it their mutex). Two cheap cuts were
  written and verified, then measured against the part-2 build in alternated pairs (`private/before3`, the same
  machine, load 7-22): `GpuFloatConstants` visiting only the dirty or invalidated groups through a 128-bit mask (~10 a
  draw, all clean and written by the stream, instead of 128; `SKY_RING_CHECK=1` compared it with the per-register path:
  0 differences), and a per-slot memo of the 16 pixel texture slots within a frame (`SKY_SLOT_CHECK=1`: 0
  differences). Result: renderer 8.4 -> 8.2 and 8.3 -> 8.2 ms, 54.4 / 54.4 and 54.2 / 54.9 fps: under the noise, so
  both were taken out again. Kept, being trivial: the ring's lock as an `os_unfair_lock` on macOS and the renderer timer
  on `mach_absolute_time` (6 ns a call against 19 for `steady_clock`). Each remaining cost is 0.1-0.5 ms; the ~3 ms a
  steady 60 needs there is the draw translation off the render thread, which needs a harness proving the draws' order:
  not done [?].
- **Trap Team's copies** [V]: copy-on-write was written (a colour resolve of a whole target into a texture of its size
  and format kept the target as the texture until the next draw or clear into it, which then made the copy first) and
  measured (`SKY_RENDER_STATS`): of 44 resolves a frame, 39 could be read in place but **38 were copied later anyway**,
  8 at a whole clear of the target and 30 at a draw into it that keeps what it holds. The game reuses its EDRAM targets
  pass after pass, as on the console, so the copies are needed; the change saved one copy a frame and was taken out. Lead:
  at a whole clear, give the texture the target's storage and the target a new one (8 copies, ~1.2 ms of GPU a frame)
  [?]. The tutorial stays GPU-bound at 2; scale auto steps down at the next start (part 2).
- **Swap Force `ww1`**, on a quiet machine (load 3-5), twice: 60.0 fps, 0 drops, no frame gap over 60 ms after 30 s
  (the part-2 drop at 97 s came with the load going 5.7 -> 12.6) [V].
- `recheck60.sh` passes Swap Force's zones as `area/map/map`, counts the drops on the runtime's own line (`drop to 30 FPS
  until`) and prints each level's window fps.

**The recheck, part 3** (`recheck60.sh` fixed, 2026-11-03 23:33 and 2026-11-04 00:00; window 65-95 s fps, drops to 30
in the run). Neither run was quiet: the first at load 5-16 (a chat app using CPU and GPU), the second while another
lane built and ran SuperChargers (load 7-16).

| Level | Part 2 | Part 3, run 1 | Part 3, run 2 |
|---|---|---|---|
| SSA `level_027` | 57.4, 2 | 55.6, 2 | 56.8, 2 (the heavy opening view, above) |
| SSA `level_008` | 56.1, 1 | 54.6, 1 | 54.9, 1 (the same) |
| Giants tutorial | 60.0, 0 | 59.9, 0 | 59.7, 1 (load 6.9 -> 15.7 in that run) |
| Giants `level_203` | 60.0, 0 | 59.9, 0 | 60.0, 0 |
| Swap Force `brn2` | 59.9, 0 | 58.5, 0 | 59.9, 0 |
| Swap Force `ww1` | 59.9, 1 | 58.1, 0 | 58.5, 1 (120-200 ms hitches, no file open, as in part 2; none in the two quiet runs) |
| Trap Team tutorial | 52.8, 0 | 36.9, 2 | 36.8, 2 (GPU-bound at scale 2, the GPU shared; a second start renders at 1) |
| Trap Team hub | 60.0, 0 | 59.9, 0 | 59.6, 0 |

## SuperChargers and Imaginators at 60 (2026-11-04) [Alchemy] [XDK] [Xbox] [V]

M1 Pro, scale 2 (auto), limit 60, offscreen, one game at a time, a shared machine (load 4-24: compare pairs only).
`tools/perf/game_perf.sh superchargers <disc> academy 60 <out>` with `SAVE_SEED=<a save in the Academy>` (the lobby test's
continue), and `... imaginators <disc> level_508 60 <out>` (a new game reaches its first level, `2016/level_508`, by itself;
`SECONDS_TO_RUN=150`). Held 60 = `FALLBACK=0`; the hot functions from `SAMPLE=1` and `tools/perf/sample_threads.py`.

**What the render thread did** (both games, the same three causes):
1. **16-bit pair streams copied and swapped per draw** (`d3d/render.cpp` `PrepareVertices`). Since `e5991dc` (0.8.2) a
   stream with any 16-bit pair input that is not a texture coordinate (FLOAT16_4 normals, SHORT4N positions) left the
   buffer cache: every draw copied the **whole** stream from guest memory, swapped it, swapped the pairs back, then
   uploaded it. In SuperChargers' Academy that was `SwapEndian` 18 % and `memmove` 14 % of the render thread. Now the
   cache converts them: its key carries the vertex stride and a mask of the vertex's 4-byte words holding pairs
   (`d3d::PairWords`), and `d3d::SwapVertexWords` swaps those words 8-in-16 and the rest 8-in-32 in one pass, so the
   result is resident and re-validated by hash like any other stream (`ArkeyanVertexPairsTest`: equal to the old two passes,
   a cut last vertex too). Only an element the mask cannot describe (unaligned, past 256 bytes) still takes the CPU copy.
   That copy now also starts at the draw's first vertex: the draw's base is moved by it for every non-dynamic stream, so
   a CPU-copied stream starting at 0 read the wrong vertices whenever the first vertex was not 0.
2. **The swap spin** (as Trap Team's, "A stable 60", part 2): the scene's end spins on the device's swap counts (+16968
   issued, +16976 shown), inlined twice in each game: SuperChargers `0x8267196C` (`sub_826718D0`) and `0x82603324`
   (`sub_82603218`), Imaginators `0x8268AADC` (`sub_8268AA40`) and `0x82616ACC` (`sub_826169C0`). One shared
   mid-instruction hook, `engines/alchemy/swap_wait.cpp` `AlchemySwapWait` (r10 issued, r11 the device), named in each
   game's `config/<Game>.toml`; XenonRecomp then changes only the objects holding those loops (SuperChargers
   `ppc_recomp.36/46`, Imaginators `35/45`), which were recompiled and replaced in `private/libs/<id>` (the old library kept
   as `.before-swapwait`). `SKY_SWAP_WAIT=0` keeps the spin.
3. **Imaginators did not pace on its interval**: its day-1 adapter said `presentsAtEveryBlank` [?]; the game asks for
   interval 2 and steps on it, as SuperChargers, so at limit 60 it held 33.3 ms a frame. Now
   `presentIntervalPacesGame` (`games/imaginators/adapter.cpp`). The game clock stays right: the level's load and
   opening segments last the same wall time at 30 and at 60 (7.7 / 7.5 s between the same draw-count changes).

| Run (60 held) | fps (window) | gap p95 / p99 ms | renderer ms | guest + waits ms | process CPU ms a frame |
|---|---|---|---|---|---|
| SuperChargers Academy, before (1276 draws) | 58.3 | 19.7 / 23.7 | 11.2 | 5.5 | 38.6 |
| + pair streams cached | 57.5 | 17.7 / 35.6 | 6.4 | 11.3 (the spin grows) | 34.4 |
| + swap wait: `SKY_SWAP_WAIT=0` / default (same build) | 58.1 / **59.9** | 20.9 / 32.2 -> **17.0 / 19.0** | 6.4 / 6.0 | 10.8 / 10.6 | 38.0 -> **31.6** |
| Imaginators `level_508`, before (the day-1 build, 1009 draws) | 29.7 | 33.5 / 34.3 | 15.0 | 21.0 | 58.7 |
| Imaginators `level_508`, after | **59.5** | 16.8 / 17.3 | 3.9 | 12.8 | 26.4 |

Imaginators' window is 115-145 s; the level ran at 60.0 fps in every 10 s from its start (~57 s) to the run's end.

**With the drop to 30 on** (the player's default; `game_perf.sh` without `FALLBACK=0`, window = the last 35-5 s; a
SuperChargers zone other than the Academy is a new game, the lobby test's presses, with `SKY_LEVEL=<map>`,
`SECONDS_TO_RUN=470`):

| Zone (draws) | fps | drops to 30 | renderer ms |
|---|---|---|---|
| SuperChargers Academy (1276) | **60.0** | 0 | 5.2 |
| SuperChargers `intro1` (1318) | **59.2** | 0 | 4.5 (a first run at 8 fps after 400 s was the machine at load 36-58: every part of the frame 10x slower; rerun at load 4-5) |
| SuperChargers `sky1` (1177) | **59.1** | 0 | 4.3 |

**Imaginators with its full-resolution scene** (2026-11-05, `--scale 2`, drop on; day 2's ultrawide hooks make the scene the
frame's 1280x720 instead of the console's 1040x620, as the other Alchemy games): `game_perf.sh imaginators <disc> <map> 60 <out>
--scale 2`. The hub is GPU-bound at full resolution and holds 60 at the original one; the cause of the GPU's jump (about 3x for
1.5x the pixels) is not found [?] (stream P).

| Zone (draws) | scene | fps | drops to 30 | GPU wait / GPU thread busy |
|---|---|---|---|---|
| Imaginators hub `hub_2016` (1382) | full, 2x 1280x720 | **58.4** (58.2-58.6 in every 10 s) | 0 | 13.2 ms / 100 % |
| Imaginators hub `hub_2016` (1372) | `--original-resolution 1`, 2x 1040x620 | **60.0** (p99 16.9 ms) | 0 | 0.0 ms / 30 % |
| Imaginators `level_508` (1013) | full | **59.6** (median 16.7 ms; load 30 at its start) | 0 | 13.3 ms / 98 % |

**The tour, 60 with the drop on, scale auto = 2** (2026-11-05, [Alchemy] [V]; `game_perf.sh`, one game alone on the Mac, window = the
last 35-5 s; SuperChargers `SECONDS_TO_RUN=470` from a new game with `SKY_LEVEL=<map>`, Imaginators 200 s from a new game with
`SKY_LEVEL=<map>`; `private/perf/tour090/`). Every one of the 24 zones holds **59.8-60.0 fps** with no drop to 30 in its window
(gap p99 16.8-17.5 ms). Renderer ms: SuperChargers 0.9-3.6, Imaginators 0.5-3.4; the hub (1382 draws) that read 58.4 before the
Metal lossless compression is now **60.0**, renderer 3.1 ms, GPU thread busy 22 %.

| Game | Zones (fps; draws median) |
|---|---|
| SuperChargers | sky2 60.0 (1308), sky3 60.0 (1525), nat1 60.0 (1324), nat2 60.0 (1543), nat3 60.0 (1141), quest1 60.0 (1055), quest2 60.0 (319), quest3 60.0 (1567), mag1 60.0 (1572), dark1 60.0 (1291), final1 60.0 (654), final2 60.0 (533) |
| Imaginators | `hub_2016` 60.0 (1382), `level_500` 60.0 (1892), 501 60.0 (966), 502 60.0 (1121), 503 60.0 (1453), 504 60.0 (1054), 505 60.0 (723), 506 59.8 (621), 507 59.9 (1287), 508 60.0 (1008), 509 60.0 (1346), 510 60.0 (125), 512 60.0 (906) |

One drop in the whole tour, outside a window: SuperChargers `nat1` dropped to 30 at ~314 s, the zone's load (gaps 79-243 ms),
and was back at 60 by itself before the window. Imaginators' zones are `2016/level_500`-`510`, `512` and `hub_2016` (the disc's
`Level_5xx.pak` list); `level_511` does not exist. Not run: a story path beyond each game's first chapter.

**The released games** with this build (`recheck60.sh`'s levels, drop on, a shared machine; earlier: "The recheck, part
3"): SSA `level_027` 56.9 fps, 2 drops (55.6-57.4, 2); Giants tutorial and `level_203` 60.0 / 60.0, 0 / 0; Swap Force
`brn2` 60.0, 0 and `ww1` 58.7, 0; Trap Team tutorial 37.0, 2 (GPU-bound at scale 2, as before) and hub 59.9, 0.
SSA `level_008` read 28.7 fps, 2 drops, in a run at load 18-22 with two other lanes' games running: SSA draws no 16-bit
pair stream there (a build logging each pair element printed none in 3451-draw frames), so its path is the one before
this change; that run is the machine's load. `tools/test_all.sh` (pictures against the references for SSA, Giants and
Swap Force): all pass.

## The write watch (2026-11-04) [SSA] [Giants] [SwapForce] [TrapTeam] [Lux] [Alchemy] [Xbox] [V]

**Why.** After the chunked range hash, re-checking guest data was still the render thread's largest single cost:
SuperChargers' Academy (1280 draws, 60 FPS, scale 2) hashed 16.8 MB of vertex and index ranges a frame and never found
one changed (`rewritten 0.0`), plus its textures; `XXH3` was 17 % of the render thread's samples (vertices 11 %,
textures 6 %). XXH3 runs at 25-35 GB/s on one core here, so only not reading the bytes helps.

**How** (`runtime/write_watch.{h,cpp}`, `ArkeyanWriteWatchTest`). Before hashing a range, the renderer makes its host pages
(16 KB on Apple silicon) read-only and takes a stamp; the first write to such a page faults, the handler
(`PlatformInstallWriteFaultFilter`, ahead of the crash report) stamps the page and makes it writable, and the write goes
on. A later frame whose range has no page stamped after its own stamp keeps its hash without reading a byte: the buffer
cache's resident ranges (`Acquire`) and the textures' whole hashes (`CheckGuestContent`, which makes the textures that are
not rewritten exact instead of sampled). The order makes it exact: protect, stamp, then hash, so a write during the hash
is stamped after it; protecting and stamping share a spin lock with the handler (never held while touching guest memory).
- **Pages the game keeps writing** (its own data beside a vertex buffer, a buffer rewritten each frame, SSA's 1120x704
  texture) would fault every frame, and a fault (~5 us, plus ~1.6 us to protect again) costs more than hashing the page
  (~0.6 us): a page written within 8 frames of being protected stays writable for 240 frames, and the ranges on it are
  hashed as before.
- **System calls** cannot write a protected page (they fail with `EFAULT` instead of faulting): `NtReadFile` holds a
  `writewatch::HostWrite` on its buffer for the whole read, which unprotects and stamps it and keeps it writable: a
  `Watch` of a range on those pages returns 0 (hashed) until the read ends, logged once as `[writewatch] a check met a
  file read on its pages`. Until 2026-11-12 it was `WillWrite` alone, and a texture's `Watch` on the render thread could
  protect a page again while the read ran; the read then failed and the guest saw the end of its file (issue #26; the
  failure reproduced in `ArkeyanWriteWatchTest` [V]; not seen in a Swap Force run on the Mac [?]). It is the only path
  where the host's kernel writes guest memory (the portal, zlib, XMA and every other host write is user code, and
  faults like the game's).
- The physical windows (0xA/0xC/0xE) do not alias each other (`memory.h`): one view of each byte, so one protection.
- Windows: a vectored exception handler (`platform_win32.cpp`, write faults only; a system call into a read-only page
  fails with `ERROR_NOACCESS`, the same `WillWrite` covers it). `ArkeyanWriteWatchTest` cross-built with llvm-mingw 20260922
  (`-DSKY_TESTS_ONLY=ON`, which also compiles the changed runtime files for Windows) passes under Wine 10 (x86-64
  through Rosetta) [V]; **not run on a real PC** [?]. The UWP build does not call `writewatch::Init`: off there.
  Linux uses the POSIX code (SIGSEGV), not run.
- `SKY_WRITE_WATCH=0` turns it off; `SKY_WRITE_WATCH=check` hashes anyway and logs every change the watch missed;
  `SKY_BUFFER_STATS=1` adds `write watch: N unwritten, N faults, N pages protected, N missed` to the `[buffers]` line and
  `SKY_TEXTURE_STATS=1` the textures' count.

**Measured** (`game_perf.sh`, 60 held with `FALLBACK=0`, scale 2, the same build with `SKY_WRITE_WATCH=0` then without;
the render thread's renderer ms a frame, mean of the window's `[perf]` lines; a shared machine, load 6-30, except the
first Academy pair at 25-100). `SKY_WRITE_WATCH=check` in SSA `level_027`, Swap Force `brn2` and Imaginators
`level_508`: 0 changes missed (725, 914 and 569 ranges a frame found unwritten; 0.4-0.5 faults a frame). The Academy with
`SKY_BUFFER_STATS=1`: 0 bytes hashed a frame (16.8 before), 1152 ranges unwritten, 0 faults.

| Game, level (draws) | Watch off: renderer ms, fps | On |
|---|---|---|
| SuperChargers Academy (1278) | 6.0, 59.5 | **3.8**, 59.8 |
| Imaginators `level_508` (1012, the game at 30) | 3.9, 30.0 | **2.5**, 30.0 |
| SSA `level_027` (1925) | 7.7, 49.7 | **6.1**, 54.2 |
| SSA `level_008` (3456), runs alone, two pairs | 13.0 / 11.8, 40.5 / 44.1 | **9.2 / 9.5**, 47.2 / 48.1 (a first pair, shared: 11.0 -> 10.4) |
| Giants `level_203` (510) | 3.2, 59.9 | **1.5**, 59.9 |
| Swap Force `brn2` (1960) | 7.5, 50.9 | **4.9**, 59.5 |
| Trap Team tutorial (1409) | 11.0, 43.9 | **5.2**, 49.3 (now waits for the GPU: GPU wait 14 ms) |
| Trap Team hub (819) | 4.2, 59.8 | **2.4**, 59.8 |

**The rest of the per-draw path, the same day** [V]. From the Academy's samples, what each draw still paid for nothing:
- the GPU thread: `SDL_GetTicksNS` twice a draw for the perf log's draw time (54 of its 742 samples in `DrawNow`; now
  `PlatformNowNs`, `platform/clock.h`, as the render thread's timers), a map and an unmap of the upload buffer per upload (57:
  now mapped once for the buffer's life, `Context::stagingMemory`), the 8 KB XXH3 of the float constants for their
  per-frame deduplication (93: now a hash kept per group of 16 registers by `gpu::Draw`'s mirror, only the changed groups
  hashed, `DrawDesc::guestConstantsHash`), and the sampler state's hash and map lookup (27: now compared with the slot the
  same register had at the last draw);
- the render thread: three `g_textures` lookups per bound texture (`TextureFor` returns its entry now);
- both threads' perf-log timers: `mach_absolute_time` puts an instruction barrier before its counter read, which drains
  the pipeline; `PlatformNowNs` reads the counter (`CNTVCT_EL0`) itself on Apple silicon (SSA `level_008`: 55 -> 1 of the
  render thread's ~2230 samples, 53 of the GPU thread's 1231 before);
- `GpuFloatConstants` (`d3d/ring.cpp`) visits only the groups dirty or not cached (a 128-bit mask in the device's bit
  order) instead of testing all 128 a draw: SSA `level_008` 137 -> 91 samples (its draws write many constants). The
  same idea was measured under the noise on 2026-11-03 next to the hashing; with the hashing gone it shows.

**Against the branch's base** (`980e8c7`, built from the same sources, `GAME_EXE`), the pass's three changes together,
pairs run alone on a quiet machine (load 6-9), 60 held, scale 2: renderer ms a frame and the 65-95 s window's fps.

| Game, level (draws) | Base: renderer, fps | This pass |
|---|---|---|
| SuperChargers Academy (1276) | 5.4, 59.0 | **3.1**, 60.0 |
| SSA `level_008` (3456) | 9.7, 54.1 | **6.8**, **58.9** |
| SSA `level_027` (1935) | 6.0, 60.0 | **4.3**, 60.0 |
| Swap Force `brn2` (1945) | 6.2, 59.9 (p99 gap 18.4 ms) | **3.7**, 60.0 (p99 17.0) |

The same build alone, the other games (no base run): Giants `level_203` 1.2 ms, 60.0 fps; Trap Team hub 1.9 ms, 60.0;
Imaginators `level_508` 2.4 ms (the game at 30 in this branch).

**The GPU side, measured the same day** (Metal System Trace, `xctrace record --template "Metal System Trace" --attach`,
2-3 s after the window, 60 held, scale 2; per second of trace, this process only; `GPUTRACE=1` in a private copy of
`game_perf.sh`). The channel xctrace calls "Compute" holds the blit encoders: the game's resolves, copied.

| Game, level | fps | Fragment ms/s | Blits ms/s (per frame) | Vertex ms/s |
|---|---|---|---|---|
| Trap Team tutorial | 50 | 505 | 225 (4.5 ms, 40 blits) | 136 |
| SuperChargers Academy | 58 | 370 | 149 (2.6 ms, 47) | 142 |
| Swap Force `brn2` | 57 | 355 | 206 (3.6 ms, 27) | 77 |
| SSA `level_008` | 47 | 341 | 156 (3.3 ms, 33) | 93 |

The resolves are a fifth to a quarter of the GPU's busy time in every game, the rest the game's own shaders at four
times the console's pixels. Trap Team's tutorial is GPU-bound (GPU busy ~87 %); the others are not at 60. Two ways to
save copies were counted before writing them (`SKY_RENDER_STATS=1`, a diagnostic build): **a resolve that clears its
whole target in the same call** (its storage could be exchanged with the texture's instead of copied) happens in none of
SuperChargers', SSA's or Trap Team's 42-56 resolves a frame and about once a second in Giants; **a resolve into a texture
nothing reads before the next resolve into it** (the copy could be skipped) is 3 of Trap Team's 47 a frame, one of them the
presented frame, which the swap reads. Neither was kept: the copies are the games' own, and what is left is the 2026-11-03
lead (a copy deferred until its target is next written, exchanged at a whole clear: 8 of 39 in Trap Team) [?].

## The GPU at scale 2 (2026-11-05) [SSA] [Giants] [SwapForce] [TrapTeam] [Xbox] [V]

**Lossless compression of the targets** (`patches/plume_metal_compression.patch`, [plume_backends.md](plume_backends.md)).
plume created every render and depth target with shader-write usage, which turns off the Apple GPU's lossless
compression: every pass load and store, every resolve copy and every sample of a target moved its whole size. Without
it (pairs of runs alone, 60 held, scale 2, Metal System Trace; GPU ms a frame = fragment + vertex + blits):

| Game, level | Before: fps, GPU ms (fragment / blits) | After |
|---|---|---|
| Trap Team tutorial (1409 draws) | 52.4 and 50.7, 16.7 (9.8 / 4.3) | **60.0** and 60.0, 12.6 (7.4 / 2.5) |
| SuperChargers Academy (1274) | 60.0, 9.5 (5.4 / 2.0) | 60.0, **8.2** (4.6 / 1.4) |
| SSA `level_008` (3456) | 42.5, 19.4 (11.3 / 5.0), load 12-14 | **60.0**, 9.7 (5.3 / 2.0), load 8 |
| Swap Force `brn2` (1960) | 59.3 (GPU wait 6 ms), 14.3 (7.9 / 4.6) | 59.9, **9.6** (5.5 / 2.2) |
| Swap Force tutorial (417), scale 2 | 58.4 (GPU wait 14.6 ms), 13.9 (8.6 / 4.1) | **60.0**, 10.5 (6.9 / 2.4) |
| Swap Force tutorial, scale 3 | 28.1 (GPU wait 32 ms), 33.8 (20.8 / 10.9) | **43.2**, 20.5 (14.0 / 4.7) |

(Trap Team's row is the patch alone; the other rows' runs had the deferred copy below as well.) A copy
of a whole 2560x1440 target fell from 110 to 62 us in Trap Team, from 299 to 133 us at scale 3. The GPU-bound scenes
(Trap Team's and Swap Force's tutorials, the owner's report) now hold 60 at scale 2; Swap Force's tutorial at scale 3 is
still GPU-bound, on its fragment work (14 ms a frame).

**The deferred resolve copy** (`gpu/draw.cpp`, `CopyTextureNow`; `SKY_RESOLVE_DEFER=0` copies at once). A resolve of a
whole target into the corner of a texture is not recorded at once: it waits until the GPU thread is about to read the
texture (a draw binding it, a copy from it, the present, a readback) or write the target (a draw into it, a partial
clear, a copy or upload into it, its destruction), and is recorded there, which leaves every picture as it was. Two cases
need no copy: a **whole clear of the target** gives the texture the target's storage instead (the textures exchange
storage, heap slots and views; the framebuffer cache is keyed by storage for that), and a **second resolve into the
texture** before anything read it drops the first. `SKY_RENDER_STATS=1` prints `[gpu] resolve copies, per frame: N
deferred, N copied, N by a storage swap, N dropped`. Trap Team's tutorial: 40.5 deferred, 38.5 copied, **2.0 swapped**,
0 dropped a frame; its blits 2.5 -> 2.3 ms a frame with both changes. Copies saved a frame in the other games (swapped + dropped, of the
resolves deferred): SuperChargers' Academy 1 of 45, SSA `level_008` 1 of 37.5, `level_027` 4 of 36.5, Giants `level_203` 2.9
of 21, Swap Force `brn2` 3.9 of 39, Trap Team's hub 3 of 39.5, Imaginators `level_508` 1 of 51: every one of those runs
held its frame rate (60.0, Trap Team's hub 59.4, Imaginators at its 30) with the whole pass, and the full `test_all`
passes (2026-11-05).

**Looked at, not changed** [?]: render-pass load and store actions (plume stores every attachment and loads every one
it does not clear; a `DontCare` needs to know the attachment's next use, the same look-ahead as the deferred copy, and
with compression a load or store costs about half what it did); the game's FXAA pass (`edge_smoothing.h`: Light at
scale 2 only changes its thresholds, the pass still runs over the frame); SSA's `k_2_10_10_10` G-buffer and light
targets kept as `R16G16B16A16_UNORM` (8 bytes a pixel, plume has no 10:10:10:2 format; a patch would add it and bring
the console's precision back); the most expensive translated shaders (this template's shader-profiler table is empty:
needs the "Metal GPU Counters"/shader timeline recording). What is left at scale 3 is the games' own fragment work
(Swap Force's tutorial: 14 ms a frame).
