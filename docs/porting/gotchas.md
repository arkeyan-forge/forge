# Gotchas D1-D44: cause, fix, class, where it is explained

> **Purpose:** the numbered index of the traps that cost days (D1-D44, the numbers other pages cite), each with its fix, its class and the page that explains it · **Status:** current (2026-10-26) · **Tags:** [Xbox] [XDK] [Lux] [Alchemy] [SSA] [Giants]

The numbers come from the old ENGINE_NOTES section 6 ([engine_notes.md](../engines/lux/engine_notes.md) §6) and stay
stable. Class: **[Xbox]** carries over to every game, **[XDK]** every title linked with the same XDK library, **[Lux]** /
**[Alchemy]** expected in that engine's games, **[SSA]** / **[Giants]** one game. Every technical problem by subsystem is in
[problem_catalogue.md](problem_catalogue.md). Commit hashes quoted in older pages may not match the current log.

| D | Gotcha | Cause -> fix | Class | Details |
|---|---|---|---|---|
| D1 | rare null fault in the heap coalesce `sub_828A2360` at frontend load (1 run in 10-94), ARM64 only | XenonRecomp emitted nothing for `lwsync/sync/eieio`; the spin-lock release `lwsync; stw 0` was visible early -> real fences (1773 sites) | [Xbox] | [toolchain/xenonrecomp.md](../tools/toolchain/xenonrecomp.md), [xbox360/threads_sync.md](../runtime/xbox360/threads_sync.md) |
| D2 | "pink/violet 3D", "stencil EQUAL 2 never passes" | not rendering bugs: NGL/D3D write state into the ring; shadows lie -> ring mirror | [Lux] | [gpu/ring_mirror.md](../runtime/gpu/ring_mirror.md) |
| D3 | menus without backgrounds | `RunCommandBuffer` was a no-op -> record/replay with inherit tags | [Lux] | [gpu/command_buffers.md](../runtime/gpu/command_buffers.md) |
| D4 | white scene | clears are draws in `0x822ADDE0` invisible to draw hooks -> hook the worker | [XDK] | [gpu/edram_resolve_tiling.md](../runtime/gpu/edram_resolve_tiling.md) |
| D5 | weak stubs that "succeed" without outputs (country from stack, muted music, pending XNADDR, uninitialised enumerator, empty save names, unwritten TIME_FIELDS) | implement outputs; grep `[stub]` per screen | [Xbox] | [xbox360/stubs.md](../runtime/xbox360/stubs.md) |
| D6 | `WaitForMultipleObjects` on zero handles hangs the main thread (WideLoad) | NT fails count 0 with `STATUS_INVALID_PARAMETER_1` -> fail, don't block | [Lux] | [lux/str_levels_loading.md](../engines/lux/str_levels_loading.md) |
| D7 | every `Read<float>` garbage, viewports 0 | `XenonUtils::ByteSwap(float)` converts numerically -> swap bits | [Xbox] | [xbox360/memory.md](../runtime/xbox360/memory.md) |
| D8 | Bink planes read 4 KB too low | the `0xE0000000` physical window addresses physical +0x1000 | [Xbox] | [xbox360/memory.md](../runtime/xbox360/memory.md) |
| D9 | title music silent, SFX fine | Wwise mutes the music bus unless `XMPTitleHasPlaybackControl` reads 0/0 (and system version != 0) | [Xbox] | [xbox360/xam.md](../runtime/xbox360/xam.md) |
| D10 | resolve parameters always 0 | `RB_COPY_*` shadows reset before Resolve returns -> call arguments (10th on the stack) | [XDK] | [gpu/edram_resolve_tiling.md](../runtime/gpu/edram_resolve_tiling.md) |
| D11 | shader setters labelled the wrong way round | `0x82290250` = SetPixelShader, `0x822905F8` = SetVertexShader: check against the object type bits | [XDK] | [gpu/d3d9ltcg_device.md](../runtime/gpu/d3d9ltcg_device.md) |
| D12 | wrong overlapped / options / clear stencil | arguments 9+ are on the stack at `r1+0x54`, `+0x5C` | [Xbox] | [xbox360/threads_sync.md](../runtime/xbox360/threads_sync.md) |
| D13 | "Indirect call to unrecompiled guest code (ctr=0x826854A8 ...)" at the save-slot menu | packed leaf functions without `.pdata` -> `find_switch_tables.py` `packed_leaves` pass | [Xbox] | [toolchain/xenonrecomp.md](../tools/toolchain/xenonrecomp.md) |
| D14 | shader cache never matches | `Create*Shader` rewrites the container in place -> hash first | [XDK] | [gpu/shaders.md](../runtime/gpu/shaders.md) |
| D15 | Bink movies without shaders | YUV shaders built at run time, in no archive -> `SKY_DUMP_SHADERS` into the cache | [Lux] | [gpu/shaders.md](../runtime/gpu/shaders.md) |
| D16 | terrain comes and goes (depth EQUAL) | positions not bit-identical across pipelines -> Metal `-fpreserve-invariance` (worked) | [Xbox] | [gpu/render_bugs.md](../runtime/gpu/render_bugs.md) |
| D17 | G-buffer/light pass never ran | MRT draws dropped: pointers compared instead of sizes | [Xbox] | [gpu/edram_resolve_tiling.md](../runtime/gpu/edram_resolve_tiling.md) |
| D18 | ~290 MTLBuffers/frame | the vertex ring at `0xAA5E0000` is reported as "the rest of the ring" in every fetch constant -> key the cache on the range read | [Lux] | [gpu/renderer_threads.md](../runtime/gpu/renderer_threads.md) |
| D19 | texcoords garbled | 16-bit pairs swapped by the 32-bit vertex swap -> `g_SwappedTexcoords` | [Xbox] | [gpu/textures_formats.md](../runtime/gpu/textures_formats.md) |
| D20 | Z-prepass lost | depth-only passes have **no pixel shader**: don't skip them | [Lux] | [gpu/shaders.md](../runtime/gpu/shaders.md) |
| D21 | depth wrong | reversed depth (GEQUAL, far = 0); drop the old "clear depth to 0 at Swap" workaround once real clears arrive | [Xbox] | [gpu/xenos_registers_pm4.md](../runtime/gpu/xenos_registers_pm4.md) |
| D22 | wrong sizes | the 3D scene renders at 1120x704 (not 1280x720); half-res passes 560x352 with 4xMSAA | [Lux] | [lux/level_frame_lightprepass.md](../engines/lux/level_frame_lightprepass.md) |
| D23 | stencil via aliased colour target | EDRAM aliasing: observed, never needed once the ring state was right | [Xbox] | [gpu/edram_resolve_tiling.md](../runtime/gpu/edram_resolve_tiling.md) |
| D24 | crash at movie start | NGL crashes on frames with nothing to draw (no scene / null render list) -> two guards in `patches.cpp` | [Lux] | [lux/ngl_renderer.md](../engines/lux/ngl_renderer.md) |
| D25 | "20 s wait after START" | STATE_GAME_FINALCHECKS: pending XNet call + unwritten XConfig outputs | [Xbox] | [lux/boot_saves.md](../engines/lux/boot_saves.md) |
| D26 | 120-500 fps title, movies at 6 fps | fences retire instantly; wait on the emulated vblank at the present interval | [Xbox] | [xbox360/video_vblank.md](../runtime/xbox360/video_vblank.md) |
| D27 | 91% of the main thread in `sched_yield` | the job system idles by yielding -> back-off after 32 fast yields (200 us sleeps) | [Lux] | [xbox360/threads_sync.md](../runtime/xbox360/threads_sync.md) |
| D28 | quit crash `pthread_mutex_lock+12`, truncated report | `SDL_Quit` freed the audio stream under the audio thread -> `std::_Exit(0)` | [Xbox] | [xbox360/threads_sync.md](../runtime/xbox360/threads_sync.md) |
| D29 | weird visuals reported (START glyph huge, torn title background, split dim background, random-language strings, nickname lost on HUD) | most fixed by D2/D3; French strings and nickname still open [?] | [SSA] | [gpu/render_bugs.md](../runtime/gpu/render_bugs.md) |
| D30 | `perl alarm` does not bound a run | use a background kill timer (`kill`, then `kill -9`) | none (process) | [debugging.md](debugging.md) |
| D31 | Mac in Maintenance Sleep during long runs | `caffeinate -u -t 30`, renewed | none (process) | [platforms/macos.md](../getting-started/platforms/macos.md) |
| D32 | timings through the symlink lie | the `.app` has its own Metal cache; measure with a cold app clone | none (process) | [gpu/pipeline_cache.md](../runtime/gpu/pipeline_cache.md) |
| D33 | XMA ahead of output, loops broken | FFmpeg's xma2 holds back 4096 samples -> wrap each frame as a WMA Pro packet | [Xbox] | [xbox360/audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md) |
| D34 | Wwise drops voices | error 4 on a frame split across buffers -> wait for the next buffer | [Xbox] | [xbox360/audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md) |
| D35 | deadlock in XMA lock | engine 1's lock enters critical section `0x82A3CC30` first -> same lock order | [SSA] | [xbox360/audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md) |
| D36 | the binary becomes GPL | Homebrew FFmpeg is GPL -> static LGPL wmapro-only build; configure refuses GPL | [Xbox] | [xbox360/audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md), [release.md](../release/release.md) |
| D37 | x64 crash that ARM64 hides | `divw` by zero / INT_MIN/-1 trap on x64 -> divides patched | [Xbox] | [toolchain/xenonrecomp.md](../tools/toolchain/xenonrecomp.md) |
| D38 | content "errors" in logs | `flags 0x13 -> 3` is a probe for a missing slot: normal | [Lux] | [xbox360/xam_content_saves.md](../runtime/xbox360/xam_content_saves.md) |
| D39 | offscreen run on macOS loads OpenGL / EGL fails | SDL's offscreen driver has no Metal view -> `SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN` | [Xbox] | [platforms/macos.md](../getting-started/platforms/macos.md) |
| D40 | one unexplained fault in `sub_822AE460` <- scene traversal during `tfb.bik` (guest `0x24`), once in ~25 runs | never reproduced | [Lux] [?] | [lux/ngl_renderer.md](../engines/lux/ngl_renderer.md) |
| D41 | windowed "crash after slot choice" | most likely the quit crash (D28); headless saving works [?] | [Xbox] [?] | [xbox360/threads_sync.md](../runtime/xbox360/threads_sync.md) |
| D42 | 32-bit index buffers | D3D marks them with Common bit 31; the renderer assumes 16-bit (none seen) [?] | [XDK] [?] | [gpu/xenos_registers_pm4.md](../runtime/gpu/xenos_registers_pm4.md) |
| D43 | Giants figures: "the Toy Box and the portal are for SSA" (Giants seats, accepts, locks and the portal id were SSA's) | the runtime hard-coded `"ssa"` (`portal.cpp`'s game id and locks, `Model::game`) -> use the build's `ARKEYAN_GAME_ID`; `figures::Catalog(game)` per game | [Xbox] | [runtime/host/figures.md](../runtime/host/figures.md) |
| D44 | a figure with Giants' extra area is "a problem toy" / progress above level 10 is lost | the second pair of areas (blocks 17-21 and 45-49) has its own checksum (CRC-16 of 64 bytes, field = `06 01`) and the first area's `0x16` bit 0; a wrong checksum makes the title refuse the figure, and clamping to 33000 drops Giants' part -> `figure_tag` keeps both parts | [Alchemy] [Giants] | [runtime/host/figures.md](../runtime/host/figures.md) |

## Additional traps found later (not numbered)

| Trap | Cause -> fix | Class | Where |
|---|---|---|---|
| the pad stays deaf after closing the Toy Box | sticks never read exactly 0: dead zones (8000 / 30) | [Xbox] | [xbox360/input.md](../runtime/xbox360/input.md) |
| a real HID portal freezes the game ~1 s every ~2 s on Windows | blocking `WriteFile` on the title's portal thread, and the wrong write method (the portal wants SET_REPORT) -> `AsyncPortal` + `HidD_SetOutputReport` | [Xbox] | [platforms/windows.md](../getting-started/platforms/windows.md) |
| the portal is never listed on Windows | hidapi lists game controllers only; usage page `FF00` -> lift the enumerate-only-controllers hint | [Xbox] | [platforms/windows.md](../getting-started/platforms/windows.md) |
| an inactive portal that answers status resets the protocol | any `S` with active = 0 resets: stay silent until `A 01` | [Lux] | [lux/portal_title_side.md](../engines/lux/portal_title_side.md) |
| a placed render target read before init removes the D3D12 device | commit render/depth targets | [Xbox] | [platforms/xbox_uwp.md](../getting-started/platforms/xbox_uwp.md) |
| a typeless depth SRV with no view removes the D3D12 device | give it its depth plane's format | [Xbox] | [gpu/plume_backends.md](../runtime/gpu/plume_backends.md) |
| a shader cache built on Linux has no Metal AIR (breaks the macOS build) | build it on the Mac | [Xbox] | [platforms/README.md](../getting-started/platforms/README.md) |
| `SKY_PRESS` has no `lb`/`rb` | only `start back a b x y up down left right lup ldown lleft lright` | [Xbox] | [xbox360/input.md](../runtime/xbox360/input.md) |

Process traps that used to be listed here (a stale prebuilt `libArkeyanRecompLib.a`, a stale shader cache after a
XenosRecomp change, editing a running script) are not repeated here.
