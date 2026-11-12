# Problem catalogue: every problem we hit porting SSA and Giants, its cause and fix

> **Purpose:** every technical problem met while porting SSA and Giants, by subsystem: symptom, cause, fix, files and class · **Status:** current (2026-10-29) · **Tags:** [Xbox] [XDK] [Lux] [SSA] [Giants] [SwapForce]

Distilled from [engine_notes.md](../engines/lux/engine_notes.md) (the detail, with addresses and evidence),
the getting-started pages and the git log. Paths are repository paths under
`runtime/` unless they start with another top folder (`d3d/render.cpp` = `runtime/d3d/render.cpp`). "Dn" = gotcha n in
[gotchas.md](gotchas.md).

Class column: **Xbox** = Xbox 360 generic (kernel, XAM, the XDK's D3D9, Xenos, XMA, Bink, XenonRecomp/XenosRecomp/plume,
our host runtime): expect the fix to carry over as is, or with re-found addresses. **Lux** = the engine (lux, NGL, job
system, loaders, renderer habits): expect the same behaviour in another Lux game, at other addresses. **SSA** = this
game's addresses, menus, first level, assets: re-find or redo.

Rule zero: **never reuse an SSA address.** Re-find each with the string / pattern / call-graph clue of
[engine_notes.md](../engines/lux/engine_notes.md) (section 2's grep table, and the "Next game" notes). The per-game
addresses live in `games/<id>/addresses.h` ([../architecture/hooks.md](../architecture/hooks.md)).

Not here: the order of work from a disc to a playable game (worked
example [day1.md](day1.md)); the test-run rules and aids
([../getting-started/testing.md](../getting-started/testing.md), [debugging.md](debugging.md)).

## Problems we hit (symptom / cause / fix / files / class)

### Recompiler and XenonRecomp config
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Rare null fault in heap coalesce `sub_828A2360` at frontend load (1 in 10-94 runs) | no fences emitted for lwsync/sync/eieio; spin-lock release visible early on ARM64 | real fences (1773 sites) | `patches/xenonrecomp.patch` | Xbox |
| "Indirect call to unrecompiled guest code (ctr=0x826854A8)" at slot choice | packed leaf functions without .pdata | `packed_leaves` pass | `tools/find_switch_tables.py`, `games/ssa/config/SSA.toml` | Xbox |
| Switch tables missing | compiler's jump-table shape unlike XenonAnalyse patterns | concrete simulation per index | `tools/find_switch_tables.py` | Xbox |
| `__builtin_debugtrap` / wrong results | missing instructions, `blrl`, `vupkd3d128` FLOAT16 | added | `patches/xenonrecomp.patch` | Xbox |
| x64 crash that arm64 hides | `divw`/`divd` by 0 or INT_MIN/-1 trap on x64 | divides return as on ARM64 | `patches/xenonrecomp.patch` | Xbox |
| Fault at start on x86 only (Windows Giants under Wine): a jump-table load 4 GB off in `sub_821C7A70` | `switch (ctx.r11.u64)` on an index whose upper half is set by `addi` on a negative word (the game's `cmplwi` is 32-bit); undefined behaviour, arm64 hid it | `switch (ctx.rN.u32)` | `patches/xenonrecomp.patch` | Xbox |
| Every `Read<float>` garbage, viewports 0,0,0,0 | `XenonUtils::ByteSwap(float)` converts numerically (D7) | swap bits | `memory.h` | Xbox |
| SIGBUS / garbage host pc in `sub_829B71D0` (billboard render) | bad indirect target (freed particle Action?) not reproduced | guard: log and skip | `ArkeyanRecompLib/indirect_call_guard.h`, `indirect_call_guard.cpp` | Lux (cause ?) |
| HEAD crashes at main menu after an innocent change | stale prebuilt `libArkeyanRecompLib.a` | rebuild when `ppc/` changes | build | Xbox |
| UWP `_setjmp` binds to a private CRT | translated code imports `_setjmp` | redirect to ucrtapp's | UWP build | Xbox |

### Kernel, XAM, XConfig, XNet
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Movies stall | KeTimeStampBundle not ticking (Bink, GetTickCount read it) | tick every ms | `kernel/` | Xbox |
| Events/objects misbehave | inlined Ke*Initialize overwrite headers | bind via 'SSAK' marker in wait list | `kernel/sync.cpp` | Xbox |
| Wwise banks never load | ReadFileEx completions need user APCs in alertable waits | APC queue | `kernel/` | Xbox |
| Main thread hangs after first level load (holding portal lock) | WideLoad waits on 0 handles; NT fails that call | zero-count wait fails (D6) | `kernel/sync.cpp` | Lux |
| 91% of main thread in `sched_yield` | job system idles by yielding | back-off after 32 fast yields | `kernel/threads.cpp` (c433233) | Lux |
| 22 s wait after START (STATE_GAME_FINALCHECKS) | XNet XNADDR pending forever, XConfig outputs unwritten | offline XNet, real XConfig | `kernel/xconfig.cpp`, `kernel/net.cpp` (4ee1adc) | Xbox |
| Country from stack garbage, empty save names, crash at guest 0x8 | weak stubs return 0 without writing outputs (D5) | implement; grep `[stub]` | `kernel/stubs.cpp` and callers | Xbox |
| Achievement check crash | uninitialised enumerator handle | enumerator that returns nothing | `kernel/xam_achievements.cpp` | Xbox |
| Title music at -96 dB, SFX fine | XMPTitleHasPlaybackControl read garbage; also system version 0 took the pre-2009 path | XMP answers title control; version 2.0.17559 | `kernel/xmp.cpp`, `kernel/xconfig.cpp` | Xbox |
| Save stamped wrong / crash | TimeZone XConfig 3/1-3/7, RtlTimeToTimeFields missing | implemented | `kernel/` (d422434) | Xbox |
| Device selector loops | overlapped InternalLow published early | publish InternalLow last | `kernel/` | Xbox |
| 120-500 fps title, movies at 6-7 fps | fences retire instantly; no vblank pacing; VBlank status bit 0x7FC86544 | WaitForFlip on vblank + interval | `kernel/video.cpp` | Xbox |
| Content "errors" `flags 0x13 -> 3` | game probing for a missing slot (D38) | none: normal | - | Lux |

### Threads, sync, memory, heap
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Heap crash at frontend load after `hudmenu.str` (~1 in 40 logs) | corrupt free-list neighbour; source not found in ~300 runs | free skipped and logged when neighbour is corrupt | `engines/lux/patches.cpp` (`sub_828A25F8`) | SSA (Lux heap) |
| Bink planes 4 KB too low | 0xE0000000 window maps physical +0x1000 (D8) | honour the offset | `memory.*` | Xbox |
| XMA silent | XMA buffers must come from MmAllocatePhysicalMemoryEx; MmMapIoSpace must return guest address | done | `kernel/xma.cpp` | Xbox |
| Giants' render thread 27 % in `NtReleaseSemaphore`, frames 15-24 ms of work, spikes to 100-300 ms | every kernel signal was `notify_all` on one condition variable shared by ~25 waiting threads; they all woke and fought over `g_kernelLock` | each waiter sleeps on its own condition variable, listed in the objects it waits for (`KernelObject::sleepers`) | `kernel/objects.*`, `sync.cpp`, `thread.cpp`, `overlapped.cpp` (f4264be) | Xbox |
| 30 fps frames 25-41 ms apart with 9 ms of work (gap p99 ~40 ms, SSA and Giants) | macOS coalesced the vblank thread's timed wait: blanks 2-8 ms late, even unloaded | real-time thread policy for the interrupt thread (`PlatformSetThreadTimingCritical`); `SKY_VBLANK_LOG=1` | `kernel/video.cpp`, `platform/` (0a8e9f7) | Xbox (host) |
| PhysicalToGuest hot (4000/frame) | two mutexes and maps per call | per-thread cache | af63b9c | Xbox |
| Windows cannot commit 4 GB | commit charge | reserve + commit 64 KB on first fault, kernel writes commit first, race retry | `platform` Win32 (30453bc) | Xbox |
| Quit crash `pthread_mutex_lock+12`, report cut short | SDL_Quit freed audio stream under the audio thread (D28) | `_Exit` at once on quit; also on guest main exit and window hidden | `main.cpp` (fd05ed6, 5bb2976) | Xbox |
| `GuestHeap::Free` zeroes the wrong address | physical offset used as virtual | not fixed (harmless) | `memory` | Xbox |
| Swap Force stops at the title after one `SL_LOAD_SLOTS Thread` line under load (3 of 195 tour runs) | a race in the game: the save task's start (`sub_829CF218`) starts the worker thread, then clears the done byte the worker sets last; a short task ends first on a loaded host and the poll waits for ever | the poll hook sets the done byte again once the task's igThread is no longer running | `games/swapforce/save_tasks.cpp` ([history.md](../../games/swapforce/docs/history.md) "The stop at the save slots") | SwapForce (the pattern: Xbox, a game race the console's timing hid) |

### GPU: PM4, Xenos, D3D, shaders
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Menus without backgrounds | RunCommandBuffer was a no-op; engine caches one buffer per mesh/material (D3) | record/replay with inherit tags | `d3d/command_buffers.cpp` | Lux |
| Huge opaque START glyph on autosave panel | Run leaves inherited fetch constants on the GPU; shadows keep old texture | keep inherited state after runs; then the ring mirror | `d3d/command_buffers.cpp`, `render.cpp` | Lux |
| Pink/violet 3D, "stencil EQUAL 2 never passes" (level dark for ~2 work blocks) | NGL writes render state and D3D writes literal constants straight into the ring; shadows lie (D2) | ring mirror for NGL registers + float constants | `d3d/ring.cpp` | Lux |
| Title logo vanishes with the first ring mirror | mirror kept Resolve's internal copy-draw registers | ring only for NGL set; shadows elsewhere | `d3d/ring.cpp` (01f71bc) | Lux |
| Flat blue staircase on title clouds, black menu header | material packet `sub_828FCBA8` sets blend/alpha in the ring | mirrored | `render.cpp` | Lux |
| White level scene | clears are draws in ClearF's worker, invisible to draw hooks (D4) | hook clear worker | `render.cpp` (3169f63) | Xbox |
| Resolve parameters always 0 | Resolve wipes its RB_COPY_* shadows (D10) | read call arguments (10th on stack) | `render.cpp` (6ba6ff2) | Xbox |
| DepthMap/shadow maps black | depth resolves skipped, no k_24_8 format | `ResolveDepthPS` into R32F | `render.cpp`, `gpu/` | Xbox |
| G-buffer and light pass never ran | MRT draws dropped: pointers compared, not sizes (D17) | compare sizes | 6fdbb56 | Xbox |
| Terrain flickers in and out | depth EQUAL needs invariant positions; Metal needs `-fpreserve-invariance` (D16) | flag in XenosRecomp | `patches/xenosrecomp.patch` | Xbox |
| Wrong shader hashes, cache never matches | Create*Shader rewrites containers in place (D14) | hash first | `d3d/shaders.cpp` | Xbox |
| Bink movies without shaders | YUV shaders built at run time (D15) | `SKY_DUMP_SHADERS` into the cache | `tools/shaders/build_cache.sh` | Xbox |
| Particles missing | point sprites: VS export 63, param_gen point coord | XenosRecomp patch; D3D12 geometry shader | `patches/xenosrecomp.patch`, `gpu/shaders/point_sprite.hlsl` | Xbox |
| Texcoords garbled | 16-bit pairs swapped by 32-bit vertex swap (D19) | swap back | `g_SwappedTexcoords` | Xbox |
| Vertex cache explodes (~290 buffers/frame) | dynamic vertex ring: fetch size = rest of ring (D18) | key on range read | `gpu/` (f72c7ce) | Lux |
| Rare SIGSEGV at host 0x8 in `std::vector<Key>::push_back` <- `Acquire` <- `gpu::CacheVertices` <- `BuildDraw` (Giants, `giants-save` at title.arc after the movies, ~1 run in 5; shipped in 0.4.0) | `CacheVertices` / `CacheIndices` took no producer lock: until the game presents its first frame the main thread presents too (`PresentDisplayFrame`), and its `gpu::Present` ran `BufferCacheFrameDone` (swaps and clears the staged-range vectors, prunes `g_entries`, frees blocks) during the render thread's draws; the upload ring (`AllocateUpload`) was raced too. SSA has the same path, its window is shorter | both take `ProducerLock`, like every other producer call; `ArkeyanRecompGpuTest` "producers" case (crashed 3 of 3 without, TSan clean with) | `gpu/buffer_cache.cpp`, `gpu/selftest.cpp`, [renderer_threads.md](../runtime/gpu/renderer_threads.md) | Xbox (host) |
| Shader setters mislabelled | first symbol file swapped them (D11) | check object type bits | `docs/runtime/gpu/d3d9ltcg_device.md` | Xbox |
| Sun light: 770 sprites on one spot | VS must start with vertex index in r0.x | XenosRecomp | 8fa8da4 | Xbox |
| Light pre-pass holes | per-tile computed-index fetches + kill-vertex flag | `GuestShader::pulled` | 392b5ef | Xbox |
| vPos replaced by point coord | param_gen register holds position XY, point coord ZW | XenosRecomp | f0dada8 | Xbox |
| Saturated yellow grass in some frames | param_gen register from stale mirrored SQ_CONTEXT_MISC | read it from the PS header | 1dc1d8c | Xbox |
| Sky clouds missing at x1, flicker at x2 | param_gen enable bit from stale shadow (FlushShaders writes ring only) | enable bit from PS header too | 44bed06 | Xbox |
| Perf drop 30 → 21 fps unnoticed for a day | Run's inherited sends walked 10536 registers per run | dirty-group walk | `d3d/ring.cpp` (ed8c094) | Xbox |
| Alpha test function ignored (always GEQUAL) | not implemented; right only because the ring gives the real function | open | `render.cpp` | Xbox |
| Swap Force level: the 3D view one colour wash, the character a faint ghost | inverted viewport depth range (MinZ 1 > MaxZ 0.2, reversed depth through the viewport) swapped by `PrepareDraw`: the sky dome won every depth test | keep MinZ > MaxZ as it is | `d3d/render.cpp` | Xbox |
| Swap Force level: lit scene black, then a zoomed top-left quarter of the G-buffer | unnormalized texture coordinates (`texCoordDenorm`) ignored by XenosRecomp; then not scaled for resolves at resolution scale > 1 | `tfetch2DDenorm`, `g_ScaledTextures` | `patches/xenosrecomp.patch`, `gpu/draw.*`, `d3d/render.cpp` | Xbox |
| Swap Force: the lava canyon's and crystal tunnel's walls shimmer in patches, at a steady 60 and at 30 (2026-10-29) | the walls' G-buffer decals (VS `1A20887C`, GEQUAL, no depth write) ask for a polygon offset (`PA_SU_SC_MODE_CNTL` bits 11-13, `PA_SU_POLY_OFFSET_*` slope 16/48) the runtime ignored: they depth-fought the wall, a different patch winning each frame | the offset becomes the host pipeline's depth bias (Xenia's units: slope / 16 x the resolution scale, offset x 2^19 or 2^23); `SKY_POLY_OFFSET=0` shows the fault | `d3d/render.cpp` (`PrepareDraw`), check `tools/regress/analyze.py shimmer` ([shaders.md](../../games/swapforce/docs/shaders.md#the-lava-shimmers-cause-the-polygon-offset-2026-10-29-swapforce-alchemy-xbox-v)) | Xbox |

### Light pre-pass and shadows (level frames)
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Dark/flat lighting, grey balloons, black shadowed sides | `AmbientMap` is a cube rendered face by face; Resolve made a 2D texture | cube destination, copy into face `slice` | `render.cpp` (432bc82) | Lux (D3D cube resolve: Xbox) |
| Blocky stair-stepped shading, no sun shadows | CPU job `sub_821404D8` classifies 32x32 tiles from the scene depth resolve in guest memory; resolves only existed on the host | write that resolve back (tiled, 20e4) via async readback | `render.cpp`, `gpu/` (f270e40, c760f3d) | Lux |
| Stencil from EDRAM aliasing (colour target over depth) | looked necessary; was not once the ring state was right | not emulated | - | Xbox (trap) |
| Violet sky | correct: matches real 360 capture | none | - | SSA |

Lesson: a 360 GPU result read back by the CPU (memexport, depth-resolve readers) does not exist in guest memory
unless written back. In Giants, find CPU readers of resolve addresses early (write-protect the page, as done here).

### Host backends: D3D12, Metal, Vulkan
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Every draw its own render pass (2.4 fps level) | plume `setFramebuffer` ends Metal encoder | bind on change | `gpu/draw.cpp` (785e687) | Xbox (host) |
| Render target read early after a flush | plume Metal resources untracked; no order between command buffers | per-queue MTLEvent | `patches/plume_queue_order.patch` | Xbox (host) |
| Cold `.app` stalls (2.6 s level start) | `.app` has its own Metal cache (D32) | pipeline list + built-in baseline | `gpu/draw.cpp`, `games/ssa/pipelines/baseline.zst` | Xbox (host) |
| lavapipe crashed on first pipeline | cache SPIR-V is smol-v | decode | `gpu/draw.cpp` (b4d2dca) | Xbox (host) |
| D3D12 drew nothing | no DXIL; no spec constants | one signed DXIL per specialization; root CBVs/SRV | `patches/xenosrecomp.patch`, `gpu/draw.cpp` | Xbox (host) |
| Terrain black under vkd3d-proton | switch-based jumps gave invalid SPIR-V via dxil-spirv | `if (pc <= N)` blocks | afeef89 | Xbox (host) |
| dxc on macOS cannot sign DXIL | no dxil.dll | own hash (`embed_dxil.cmake`, `signDxil`) | | Xbox (host) |
| Xbox: splash forever, then DEVICE_REMOVED | placed RT/depth targets read before init | committed targets, DRED | 0a871d8 | Xbox (host) |
| Xbox: removal at frontend load | null-desc SRV on typeless depth | depth-plane format | 9011418 | Xbox (host) |
| Xbox: presents never shown | CoreWindow activation and swap chain unlike MS template; Presents off the UI thread | template flags, UI-thread presents | bf2ee88, 373a5e4 | Xbox (host) |
| Xbox: removal at start | ID3D12PipelineLibrary; GPU thread | both off by default, crash marker, step log | 269b33a, 8d5646d | Xbox (host) |
| Xbox: 4 fps then 15-20 fps CPU-bound | rebinding root signature per draw | bind only what changed | 1dd6f76 | Xbox (host) |
| Resizing the window froze the game for good, then the window turned magenta (levels, all three games; title screens fine) | `WaitForSubmissions` reset the submission count under a command list still being recorded: with chunked frames the present pass went into a list already sent, the frame's list was never committed, the GPU waited forever (Metal "GPU Timeout Error"; the magenta is macOS's, not a colour of ours) | send the recording list before waiting | `gpu/gpu.cpp`; [gpu/README.md](../runtime/gpu/README.md#window-and-resize-xbox-host), `tools/perf/resize_run.sh` | Xbox (host) |
| MoltenVK rejects some pipelines | vertex attribute type mismatch | open | | Xbox (host) |

### Bink, XMA, audio
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| XMA ahead of output, loops broken | FFmpeg xma2 holds 4096 samples back (D33) | each frame as its own wmapro packet | `apu/`, `kernel/xma.cpp` | Xbox |
| Wwise drops voices | error 4 on a frame split across buffers (D34) | wait for next buffer | `apu/` | Xbox |
| Deadlock | engine 1's XMA lock enters a critical section first (D35) | same lock order | `kernel/xma.cpp` | Xbox |
| A thread spins in the Wwise voice service, the game stops presenting (Swap Force, ~58 s into the tutorial) | the writers store the Kick/Clear register once per context; read after the writer, only the last store per word survived: 3 of a 4-context voice never decoded | the hooked writers also act on every context of the voice they were given | `kernel/xma.cpp` | Xbox |
| Binary becomes GPL | Homebrew FFmpeg (D36) | static LGPL wmapro-only build, configure refuses GPL | `tools/ffmpeg/build.sh` | Xbox |
| Fault once in ~25 runs during `tfb.bik` (`sub_822AE460`) | not found (D40) | none | | Lux ? |

### Disc files, STR containers
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Long loads (3.4 s level) | translated zlib at ~30 MB/s on the game thread | host zlib per guest `z_stream` | `inflate.cpp` | Lux |
| Windows loads slower | no zlib found in mingw build | open (miniz is in tree) | | Xbox (host) |
| Assets found by format | `.str` = `strm` + one zlib stream of records; images `APKF` (`\0XET`, `SYHP`); shaders `\0ASL` `.lsa` | readers | `tools/shaders/lsa.py`, `host/figures/` | Lux |
| Giants: challenge levels fault at `0x78` (`sub_82246390`) or stop, only in the level tour | the `SKY_LEVEL` redirect loads a challenge level alone: no base mesh (`challengeMesh_*.bld`, which holds the Havok world, null at `0x82673738`) and no challenge session | none needed (not a runtime bug; the six base meshes run alone with 0 faults, 2026-10-27: a real run of challenge 016 through Cali and the select UI: 0 faults; the 44 locked ones were not run); the tour's allow-list says why ([history.md](../../games/giants/docs/history.md), "Challenge levels in the tour") | `private/test-refs/giants/tour.txt` | Giants (test aid) |
| Trap Team: `arena_challenge_309/311/361_000` fault at `0x8C` (`sub_8264B3F0`), only in the level tour | the `SKY_LEVEL` redirect loads an arena alone: no base mesh (`Arena_Mesh_*.bld`, which holds the Havok world, null at `0x8284EB50`) | none needed (not a runtime bug); the coverage row says so ([status.md](../../games/trapteam/docs/status.md), "Arenas in the tour") | `tools/regress/coverage.sh` | Trap Team (test aid) |

### Portal and figures
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Portal "out of date" / protocol resets | version must be ≥ 0x011F; any S with active = 0 resets; inactive must stay silent | emulated answers | `kernel/portal.cpp`, `host/portal_emulated.cpp` | Lux |
| Ordinals missing | title looks up xam 0x4A1/0x4A2 at run time | `XexGetProcedureAddress` + `CreateHostThunk` | `kernel/portal.cpp` | Xbox |
| HID portal not listed on Windows | SDL hidapi lists game controllers only; usage page FF00 | lift the hint for the portal | `host/portal_hid.cpp` (15f6f75) | Xbox (host) |
| UWP had no real portal | - | GIP factory + HidDevice routes, untested | `host/uwp/portal_backends.cpp` | Xbox (host) |
| Pad deaf after Toy Box | sticks never read exactly 0 | dead zones | d247210 | Xbox (host) |
| Nickname lost on HUD after re-place | unknown | open | | SSA ? |
| Figure crypto | AES key = MD5(block0, block1, n, Activision string) | `host/figures/` | | Lux (series) |
| Swap Force "stalls" after the title's A (two `SL_LOAD_SLOTS Thread`, then nothing; every headless kit movie pass) | no portal (`SKY_PORTAL=none`): the game's own "PORTAL DISCONNECTED" screen, not a hang | the kit's movie pass gets an empty emulated portal for Swap Force (`movie_portal=emulated` in `game.txt`) | `tools/arkeyan-builder/main.cpp`, `games/swapforce/game.txt` | Alchemy (SwapForce) |

### Saves, UI, language
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| Empty save names | `vsprintf` was a stub (only caller) | implemented | `kernel/` | Xbox |
| Some strings in random languages with French | unknown (D29) | English text default | 114fe5e | SSA |
| Voice language tied to text | one index drives text, movies, Wwise | hook Wwise language and Bink track | `language.cpp` | Lux |
| Glyphs per pad | glyphs are whole 128x128 DXT5 textures | replace by content hash | `host/glyphs.cpp` | Lux (hashes SSA) |
| Swap Force survival arenas started alone (`SKY_LEVEL`) fault at the first wave in `sub_82A5D168` | the objective builds its rules object only in an arena game mode; a new game is in Story Mode (1) | `SKY_LEVEL` stores Solo Survival's mode (64) as the next zone's mode; the game applies it | `engines/alchemy/zone.cpp`, `games/swapforce/addresses.h` | SwapForce (Giants' challenge levels started alone: the same kind) |
| Swap Force `brn3`, `ww1` black with `SKY_SKIP_MOVIES` | their intro movie plays inside a cutscene that a one-frame movie stalls on its black surface | none needed (a test aid); the tour plays their movies | `tools/pipelines/tour_swapforce.sh` | SwapForce |
| SuperChargers: every presented frame black, draws not skipped | empty device scissor applied with the scissor test off | scissor only with the test on | `runtime/d3d/render_scale.h`, `render.cpp` | Xbox |
| SuperChargers: world black under the HUD, UI trails | SHORT4N position read with its 16-bit halves swapped (w = 0) | swap the halves back for non-texcoord 16-bit pair inputs | `runtime/d3d/vertex_pairs.h`, `render.cpp` | Xbox |
| SuperChargers: `No code padding left for a host thunk` at start | one code section, no run of 16 zero words | a lone zero word no function starts at | `runtime/thunk_slot.h`, `loader.cpp` | Xbox |

### Perf, settings, platforms, packaging
| Symptom | Cause | Fix | Files | Class |
|---|---|---|---|---|
| The user: "even with the option on, the movies are not skipped" / "even with 60 FPS on, not at 60 FPS" (0.6.0, macOS, from the launcher) | **not reproduced** in the launcher's flow (PLAY, `--play`) for any game: the logos are skipped and the present runs at 60 on 0.6.1 builds. Two gaps closed: the runtime read the video keys from `[video]` only (filled in by PLAY), so a game started any other way ignored its page; and a machine that cannot hold 60 shows 40-50 uneven FPS, now a steady 30 with a message | the runtime reads `[<game>.video]` after `[video]`; `HalfRate` fallback + `ToyboxNotice`; `tools/regress/play_settings.sh` (in `test_all.sh`) | `host/settings.cpp`, `kernel/video.cpp`, `host/toybox.cpp` | Xbox (host) |
| Tests skewed (60 fps) | player's `launcher.ini` had `fps_limit=60` | `SKY_SETTINGS` / `SKY_LAUNCHER_INI` | `host/settings.cpp` (a72802b) | Xbox (host) |
| 60 fps without game patch | game steps by measured time (F clamp 1/20 s); scene time counts vblanks | present interval-2 frames every vblank; vblank stays 60 Hz | `kernel/video.cpp` | Lux |
| Swap Force at `fps_limit=60`: "the lava and the crystals flicker, the picture is epileptic" (user's recording: 44-48 fps, each frame shown 1, 2 or 3 refreshes, the scene moving by the same step whatever the time shown) | a frame that misses its 60 Hz blank is shown at once, so a game at ~45 fps runs at a steady ~22 ms while the display's vsync shows frames for 16.7 or 33.3 ms: judder, worst on fast scenes (the tutorial flies through a lava canyon and a crystal tunnel). No draw was wrong (lava and tunnel draws, constants, velocities and resolves checked frame by frame) | while 3 of the last 30 frames miss their blank, show every 2 blanks (steady 30 fps: the engine measures 33.3 ms and shows 33.3 ms); back to 60 after 300 frames in a row ready within one blank (doubled up to 5 min each time 60 fails again within 2 s) | `kernel/video.cpp` (`HalfRate`), [video_vblank.md](../runtime/xbox360/video_vblank.md#pacing-waitforflippresentinterval-in-the-swap-hook) | Xbox |
| First level frame 150-290 ms | game's own first update on main thread | none possible | | Lux |
| Integrated GPU lags at x2 | fill-bound | `resolution_scale=auto` | `gpu/gpu.cpp` | Xbox (host) |
| Light passes break at x2 | old shader cache without `g_ResolutionScale` | rebuild cache after any XenosRecomp change | `tools/shaders/build_cache.sh` | Xbox |
| msix deflates the ISO | makemsix store list lacks `.iso`; >4 GB needs zip64 | patch, zip64 | `tools/xbox/patches/makemsix-store-iso.patch` | Xbox (host) |
| App gets small budget on Xbox | Dev Mode app type | set App type: Game | | Xbox (host) |
| Xbox One cannot run Series build | AVX2/FMA | btver2 build; Series build `-ffp-contract=off`; cpu check | `host/uwp/cpu_check.cpp` | Xbox (host) |
| Players cannot run bash/CMake | - | Arkeyan-Builder + player kits | `tools/arkeyan-builder`, `make_kit.sh` | Xbox (host) |
| `ArkeyanRecompGpuTest` aborts at exit (134), ~1 run in 6 | `gpu::Shutdown` left textures queued by `DestroyTexture` (`g_pendingDestroy`); the next `Init`'s first frame deleted them against the destroyed device, and `MetalTexture::~MetalTexture` locks the device's mutex: `std::terminate` | `DrawShutdown` frees the queue while the device exists. The game never calls `Shutdown` (it leaves by `_Exit(0)`: no destructor runs) | `gpu/draw.cpp` | Xbox (host) |
| Giants "60 % slower since yesterday" (work median 13.8 ms against 8.3 ms, 2026-10-27) | not the code: `giants_run.sh`'s `SAMPLE=1` ran `sample` inside the summary window (+2-5 ms a frame), and other game instances' load (3 to 63 within minutes) pushes the niced game off the performance cores; `main` measured 7.9-8.6 ms at load 3-5, faster than the older build in every pair | `sample` starts at the window's end by default; the run prints CPU time per frame per thread (`ps -M`, `tools/perf/thread_cpu.py`), which load inflates far less than the wall-time work; compare work only in same-time pairs | `tools/perf/giants_run.sh`, `tools/perf/thread_cpu.py`, [renderer_threads.md](../runtime/gpu/renderer_threads.md) | Xbox (host, test aid) |
| A kit-built game wrote `SSARecomp.log` into the real data folder although every scratch variable was set (2026-10-27) | not the game: Arkeyan-Builder's headless run (movie shaders) of the game it just linked set `SKY_SETTINGS`, `SKY_SAVE_DIR`, `SKY_COLLECTION` but not `SKYLANDERS_DATA` or `SKY_LOG`, and the player's data folder is the default; it also unset the caller's values after | the run sets `SKYLANDERS_DATA`, `SKY_LAUNCHER_INI`, `SKY_LOG` in its work folder and restores the caller's values; `tools/regress/kit_isolation.sh` proves it under `sandbox-exec` | `tools/arkeyan-builder/main.cpp`, `tools/regress/kit_isolation.sh`, [testing.md](../getting-started/testing.md) | Xbox (host, test aid) |
| Giants frame checks flip between runs (29 dB then 17 dB) | the scene (dialogue, camera) moves with the timing; one reference frame | a set of reference frames, best match counts | `tools/regress/analyze.py`, [testing.md](../getting-started/testing.md#regression-harness-toolstest_allsh) | Giants |

---

## Giants leads written before the Giants boot (2026-10-25)
What happened to them: [games/giants/docs/boot.md](../../games/giants/docs/boot.md).
- SSA's code already accepts Giants-era sidekicks 505, 514, 519, 526 ([engine_notes.md](../engines/lux/engine_notes.md) §3.13); the hat field at 0x14 starts
  with Giants [?]; the catalogue lists 22 Giants toys, unverified (`resources/collection/catalogue.json`).
- Later games add portal commands (J, L, M, V: lights, speaker) [?]; HID VID/PID stays 1430:0150.
- Expect the same light pre-pass (sun tiles, ambient cube) and the same NGL ring writes: capture one level frame
  first and compare with [engine_notes.md](../engines/lux/engine_notes.md) §3.6.
- The launcher already has a greyed Giants entry (`LauncherGameCatalog()`, `host/launcher_ui.cpp`).
