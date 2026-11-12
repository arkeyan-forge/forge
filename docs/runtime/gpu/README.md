# GPU: replacing the game's Direct3D (Option B)

> **Purpose:** how does the runtime turn the title's Direct3D calls into host GPU work (Metal, Vulkan, D3D12), and where is each part documented? · **Status:** current (2026-10-27) · **Tags:** [Xbox] [XDK] [Lux] [SSA] [Giants] [SwapForce]

Class tags: **[Xbox]** generic, **[XDK]** the linked D3D9LTCG library, **[Lux]** / **[Alchemy]** the engine's habits,
**[SSA]** / **[Giants]** one binary.

## What it is

**Decision (the user, 2026-10-13):** keep the game's own statically linked D3D9LTCG running and **do not emulate the
PM4 command processor**; hook the functions that emit work, read the state from the device structure plus a mirror of
the PM4 ring, and translate each draw, resolve, clear and swap to the host through **plume** (Metal, Vulkan, D3D12),
with shaders translated offline by **XenosRecomp**. The hooks are `PPC_FUNC(GAME_HOOK(Name))`, present only in a game
whose `games/<id>/addresses.h` defines the name ([architecture/hooks.md](../../architecture/hooks.md)).

### One draw, end to end [XDK] [SSA] (addresses are SSA's)

1. The game calls `DrawVertices` / `DrawIndexedVertices` (`0x8229AD58` / `0x8229B170`), `BeginVertices` (`0x8229A850`), or a command buffer is run.
2. Our hook records the draw if a command buffer is being recorded; else runs the **guest function** (it flushes dirty state into the ring), `ScanRing`, then `IssueDraw`.
3. `PrepareDraw` reads: shaders from `device+0x3290/0x328C`; render targets from the surfaces; blend/depth/stencil/cull from `GpuRegister` (ring or shadow); viewport from the `PA_CL_VPORT_*` shadows + `PA_CL_VTE_CNTL`; scissor from `device+0x327C`;
   alpha test; the 16 sampler fetch constants -> `TextureFor` -> host textures; booleans; param_gen from the pixel shader header.
4. `PrepareVertices` maps the declaration to inputs (formats, DEC3N, swapped texcoords, defaults), caches the converted range on the GPU, gathers computed-index inputs; rectangle/quad lists are expanded.
5. `gpu::Draw` copies the draw into the frame arena (GPU thread) -> pipeline from `PipelineKey` (listed or built) -> bind on change -> draw.
6. `Resolve` copies host targets into host textures (`resolved = true`); `Swap` waits the vblank and presents the front-buffer texture.

### What is **not** emulated

PM4 execution, EDRAM aliasing / stencil-via-colour-write-mask, MSAA samples, exponent bias, `k_8_8_8_8_GAMMA` curves, memexport (no shader uses it), tessellation, 1D/3D textures, alpha function, resolve-to-memory (textures
resolved on the GPU stay on the host; the exceptions are the depth write-back for one CPU reader and a colour resolve
written back when a game hook says the CPU is about to read it, `d3d/resolve_writeback.h`).

## Where the code is

| Code | What |
|---|---|
| `runtime/d3d/` | The D3D side: `device.{h,cpp}` (device layout, dirty groups), `render.cpp` (draws, resolves, clears, swap), `command_buffers.cpp`, `ring.cpp` (the mirror), `shaders.cpp`, `xenos.cpp` (formats), `null_gpu.cpp` (fences, kick-offs), `bink_display.cpp`, `ultrawide.cpp`, `capture.cpp` |
| `runtime/gpu/` | The host renderer on plume: `gpu.{h,cpp}` (init, textures, present, overlay, readback), `draw.{h,cpp}`, `worker.cpp` (GPU thread), `buffer_cache.*`, `shader_cache.h`, `d3d12_pipeline_cache.cpp`, `d3d12_diagnostics.cpp`, `shaders/` (the present pass), `selftest.cpp` |
| `thirdparty/plume`, `thirdparty/plume.cmake`, `patches/plume.patch` | plume (MIT) and our patches ([plume_backends.md](plume_backends.md)) |
| `engines/lux/ultrawide.cpp`, `engines/alchemy/ultrawide.cpp` | each engine's ultrawide hooks ([ultrawide.md](ultrawide.md)) |
| `runtime/d3d/upscale.*`, `runtime/gpu/upscale.*` | temporal upscaling of the scene: AMD FSR 3.1 on every backend (`gpu/fsr/`), [upscaling.md](upscaling.md) |

## How to test it

- `ArkeyanRecompGpuTest [dir]` (`runtime/gpu/selftest.cpp`, in `tools/test_all.sh`): draws test images through the present
  pass into an offscreen target, reads them back and compares with a CPU bilinear reference; with `dir`, writes
  `gpu_<case>_{actual,expected}.ppm`. Its "producers" case presents on one thread while another fills the buffer cache
  (the main thread and the game's render thread before the first frame; [renderer_threads.md](renderer_threads.md)).
- A game run with `SDL_VIDEO_DRIVER=offscreen` runs the whole GPU path into an offscreen target;
  `SKY_GPU_SNAPSHOT=<prefix>` saves every 300th presented image read back from the GPU (`SKY_GPU_SNAPSHOT_EVERY`,
  `SKY_GPU_SNAPSHOT_FROM`; `runtime/host/display.cpp`). Capture and dump tools: [debugging.md](../../porting/debugging.md).
- A live resize in a real window (macOS): `tools/perf/resize_run.sh` ("Window and resize" below).
- The pipeline baseline and the level tours: [pipeline_cache.md](pipeline_cache.md),
  [testing.md](../../getting-started/testing.md).

## Window and resize [Xbox] (host)

Who presents: the **GPU thread** (`gpu::detail::PresentNow`, [renderer_threads.md](renderer_threads.md)), not the main
thread, on macOS, Windows and Linux; the main thread presents only until the game's first frame
(`PresentDisplayFrame`, a black frame) and on UWP (`gpu::ServicePresents`). So the modal loops that block the main
thread while the player drags the window's edge (AppKit's live resize on macOS, `WM_SIZING`/`WM_SIZE` in
`DefWindowProc` on Windows) **do not stop the game's frames**: no SDL event watch or `SDL_EVENT_WINDOW_EXPOSED` redraw
is needed. **[V]** macOS: the frames keep their pace during a drag (table below). **[?]** Windows and Linux: same code
path, built (llvm-mingw, Docker) but not dragged on a real desktop; Wine's runs have no window
([windows.md](../../getting-started/platforms/windows.md)).

The swap chain follows the window at the next present: `PresentNow` asks plume `needsResize()` (the window's size in
pixels against the swap chain's) and `ResizeSwapChain` waits for the GPU (`WaitForSubmissions`), resizes, rebuilds the
framebuffers. On Metal this is `CAMetalLayer.drawableSize`, which SDL's Metal view also sets on the main thread at every
pixel-size event (`SDL_HINT_VIDEO_METAL_AUTO_RESIZE_DRAWABLE`, on by default); plume reads the window's size from a
cache the main thread refreshes (`CocoaWindow`, `dispatch_async`). **No debounce**: a drag resizes the swap chain at
almost every present (~350 times in 7 s of Giants at 60 fps) and that costs at most one frame (gaps below); deferring it
would draw old-size framebuffers into new-size Metal drawables (SDL resizes them anyway) and Vulkan reports an
out-of-date swap chain until it is recreated.

**The freeze and the magenta (fixed 2026-10-27).** Dragging the window's corner in a level froze SSA, Giants and Swap
Force **for good** (not "a bit"), and after 2-5 s the window turned solid magenta. Cause: `WaitForSubmissions` reset
the frame's submission count while a command list was still being recorded; levels send a frame in several command
lists ("chunked submission", 1.3-5 per frame), so after a mid-frame resize the present pass was recorded into a list
already sent and the frame's own list was never committed. The GPU then waited forever (Metal: the command buffer ends
with `kIOGPUCommandBufferCallbackErrorTimeout`, "Caused GPU Timeout Error"), the GPU thread waited for its fence
(`EndFrame` -> `waitForCommandFence`) and the game for the GPU thread. The title screens send one list per frame: they
never froze. Fix: `WaitForSubmissions` sends the recording list first (`runtime/gpu/gpu.cpp`); this covers
`ResizeOffscreen` too. **The magenta is not ours**: no clear, layer background or debug fill in the runtime, SDL or
plume is magenta, and a black `CAMetalLayer.backgroundColor` did not change it: it is what macOS shows for a layer whose
GPU work timed out. Black at worst before the first frame (the present pass clears to black; SDL's view fills black).

How it was found: `sample` of the frozen game (the GPU thread in `waitForCommandFence`, Metal's
`CommandQueueDispatch` thread blocked in `IOGPUCommandQueueSubmitCommandBuffers`), then a completed handler on
plume's command buffers printing `status()` and `error()` (the timeout). Metal's API validation (`MTL_DEBUG_LAYER=1`)
reported nothing: every command was valid, a wait was never satisfied.

**The check** (macOS, a real window, never a screen capture): `tools/perf/resize_run.sh <executable> <iso> <out>
[wait s] [drag s] [env...]` runs the game isolated, then `tools/perf/live_resize.swift` brings it to the front and
drags its bottom-right corner (shrinks and grows it twice by up to 240 points, then holds the button still), capturing
**only the game window** (`screencapture -l`); it prints the capture widths (did the window really change size?) and the
frame gaps before, during and after the drag from `SKY_FRAME_TIMES`. The mouse moves for the drag's seconds: the
terminal needs the Accessibility permission. `SAMPLE=1` samples the game for 3 s after the drag.
Results (M1 Pro, scale 2, vsync, limit 60, a 6 s drag, load average 10-30 with builds running):

| Game, place | Before the fix: frames during the drag | After: frames during the drag, gap median / p95 / max |
|---|---|---|
| Giants, Chapter 1 (`level_213_tutotial`) | 5-6 in 7.1-7.5 s, then none (frozen, magenta) | 429 in 7.2 s; 16.7 / 18.2 / 30.3 ms (before the drag 16.8 / 30.1 / 50.1) |
| SSA, `level_027` (render-thread bound at this load) | 5 in 7.3 s, then none | 171 in 7.1 s; 39.5 / 71.8 / 140 ms (before the drag 36.0 / 89.2 / 125) |
| Swap Force, the tutorial (paced at 30 by the game) | 4 in 7.0 s, then none | 218 in 7.3 s; 33.3 / 33.7 / 39.1 ms (before the drag 33.3 / 33.4 / 34.4) |
| Giants, the title (one command list per frame) | 146 in 4.9 s; 33.3 / 34.5 / 37.8 ms (limit 30): no freeze | - |

## Gotchas

- The shadows lie: inherited command-buffer state and direct ring writes are not in them
  ([ring_mirror.md](ring_mirror.md), [command_buffers.md](command_buffers.md)).
- `Create*Shader` rewrites the container in place: hash it before calling the original
  ([d3d9ltcg_device.md](d3d9ltcg_device.md#resources)).
- SDL's offscreen video driver has no Metal: offscreen runs render into a target paced at 60 Hz, nothing appears.
- Every rendering bug with its cause and fix: [render_bugs.md](render_bugs.md).

## Pages

| Page | Answers |
|---|---|
| [xenos_registers_pm4.md](xenos_registers_pm4.md) | Which Xenos registers do we read (and from where), which PM4 packets do we parse; primitive types, depth format |
| [d3d9ltcg_device.md](d3d9ltcg_device.md) | The 0x5F80-byte device layout, dirty masks, resources, the D3D function address table, what we hook |
| [command_buffers.md](command_buffers.md) | Begin/End/RunCommandBuffer, inherit tags, our replay |
| [ring_mirror.md](ring_mirror.md) | Why the shadows lie, `ScanRing`, `GpuRegister`, rules and traps |
| [edram_resolve_tiling.md](edram_resolve_tiling.md) | EDRAM surfaces as host targets, Resolve/Clear/Swap, tiling, MSAA, CPU reads of resolves |
| [textures_formats.md](textures_formats.md) | Fetch constants, the format table, tiling/endian/mips, vertex formats |
| [shaders.md](shaders.md) | Containers, `.lsa`, the cache, XenosRecomp semantics, the translated-shader ABI, specialization constants, DXIL |
| [pipeline_cache.md](pipeline_cache.md) | `PipelineKey`, `pipelines.bin`, the baseline, loading waits, the D3D12 library |
| [plume_backends.md](plume_backends.md) | Metal/Vulkan/D3D12 backends, every plume patch, limits |
| [renderer_threads.md](renderer_threads.md) | GPU thread, texture threads, caches, performance findings, the perf log |
| [render_bugs.md](render_bugs.md) | Every rendering bug with cause and fix, open items, the lessons |
| [ultrawide.md](ultrawide.md) | How does `aspect=auto` widen the scene, the frame and the HUD? |
| [texture_packs.md](texture_packs.md) | How do I dump the game's textures and replace them (HD packs)? |
| [steamdeck_startup.md](steamdeck_startup.md) | Why might the Steam Deck's Game Mode show nothing, and what does the game log? |

See also: [lux/ngl_renderer.md](../../engines/lux/ngl_renderer.md),
[lux/level_frame_lightprepass.md](../../engines/lux/level_frame_lightprepass.md),
[xbox360/video_vblank.md](../xbox360/video_vblank.md), [toolchain/xenosrecomp.md](../../tools/toolchain/xenosrecomp.md),
[platforms/xbox_uwp.md](../../getting-started/platforms/xbox_uwp.md) (the D3D12-on-console history).
