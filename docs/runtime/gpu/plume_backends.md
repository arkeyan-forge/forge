# plume: Metal, Vulkan, D3D12 (backends, our patches, limits)

> **Purpose:** how do the Metal, Vulkan and D3D12 backends work, which plume patches do we carry, and what are the limits? · **Status:** current (2026-11-11) · **Tags:** [Xbox]

Code: [`gpu/gpu.cpp`](../../../runtime/gpu/gpu.cpp), `gpu/draw.cpp`, `thirdparty/plume` (renderbag, MIT, RT64's RHI, pinned at `e48a55b`, no upstream tags),
[`thirdparty/plume.cmake`](../../../thirdparty/plume.cmake), [`patches/plume.patch`](../../../patches/plume.patch) (459 lines),
[`patches/plume_queue_order.patch`](../../../patches/plume_queue_order.patch) (59),
[`patches/plume_vulkan_strict.patch`](../../../patches/plume_vulkan_strict.patch) (strict Vulkan drivers, below),
[`patches/plume_swapchain_result.patch`](../../../patches/plume_swapchain_result.patch) (the swap chain's last result, below),
[`patches/plume_d3d12_blend.patch`](../../../patches/plume_d3d12_blend.patch) (D3D12's per-target blend, below).
Class **[Xbox]** (host side; nothing here is game-specific).

## Backends

| Platform | plume backend | Window surface | Present-pass shaders |
|---|---|---|---|
| macOS | Metal (Metal 3 / macOS 13: tier-2 argument buffers) | `SDL_WINDOW_METAL`, `SDL_Metal_CreateView` layer | `present.metal` -> metallib (`xcrun metal`), embedded |
| Windows desktop | the player's choice, **D3D12 by default** (0.9.5; Vulkan came first before): `[<game>.video] gpu_backend=d3d12\|vulkan` (the launcher's Video tab, the F3 menu), the other one when the chosen one makes no device; `SKY_GPU_BACKEND=vulkan\|d3d12` forces one, with no fallback (`gpu/gpu_backend.h`) | HWND | `present.hlsl` -> DXIL (signed by `embed_dxil.cmake`) and SPIR-V, both embedded |
| Linux | Vulkan (`PLUME_SDL3_VULKAN_ENABLED`) | `SDL_WINDOW_VULKAN`, SDL makes the surface | SPIR-V |
| macOS, **test build** (`-DARKEYAN_MACOS_VULKAN=ON`) | Vulkan over MoltenVK, for the validation layers ([below](#strict-vulkan-drivers-the-validation-layer-run-v-2026-10-28)); never shipped | none under `SDL_VIDEO_DRIVER=offscreen` | SPIR-V |
| Xbox (UWP) | D3D12 on a **CoreWindow** swap chain (`PLUME_D3D12_COREWINDOW`) | the app's CoreWindow, size chosen by the caller | DXIL |

`present.hlsl` entry points: `PresentVS` (vs_6_0), `PresentPS` (ps_6_0), `ReadbackCS` / `ReadbackRawCS` (cs_6_0), `ResolveDepthPS`, `OverlayPS`; Vulkan vertex shader compiled with `-spirv -fvk-invert-y`.
Output is a swapchain (vsync on, resized with the window) or, when the window has no native surface (SDL's `offscreen` video driver: no Metal view), an **offscreen render target paced at 60 Hz**: the whole GPU path
still runs with nothing on screen (`SKY_GPU_SNAPSHOT`, `ReadbackOutput`). On Linux SDL's offscreen driver gives plume a Vulkan swapchain, so use `SKY_DUMP_FRAME` there.

### Adapter and scale (`[gpu]` log lines)
`[gpu] Vulkan on <name>, swapchain output` / `[gpu] adapter <name>: vendor XXXX, integrated|discrete..., N MB of video memory, driver V` / `[gpu] window WxH (W'xH' pixels), display WxH at P%` /
`[gpu] resolution scale auto: N (reason)`. **Auto scale**: 2 on Apple GPUs and on discrete GPUs with >= 2 GB; **1** on integrated GPUs (UMA / type integrated), virtual or software adapters, discrete with < 2 GB (a Radeon 840M lags at
x3, a little at x2, is fine at x1). A number from `launcher.ini`/`--scale` always wins; fixed for the session. After 30 s under the frame limit with much of each frame waiting for the GPU at scale > 1 it logs a `[perf] hint`.

Linux startup diagnostics and native SDL driver retries for Steam Deck Game Mode:
[issue #22 analysis and verification](steamdeck_startup.md).

## Patches (`patches/plume.patch`, applied at configure time; after a merge that touches patches: `git -C thirdparty/plume checkout -- .` then cmake + ninja)

| Hunk | What / why |
|---|---|
| `setBlendFactor(rgba)` on `RenderCommandList` and the three backends | the Xenos blend constant (`BLEND_FACTOR`, RB_BLEND_RED..ALPHA): Metal `setBlendColor` (dirty bit mask `0x1FF` -> `0x3FF`), D3D12 `OMSetBlendFactor`, Vulkan **dynamic** blend constants (`VK_DYNAMIC_STATE_BLEND_CONSTANTS`, set to zero at `begin()` so a value exists before any draw) |
| Pipeline creation returns **null** when the backend rejects it | Metal: `state.renderPipelineState == nullptr`; D3D12: `createGraphicsPipeline` returns null (and logs the HRESULT) instead of a pipeline bound as a *null pipeline state* (a device removal on a real device) |
| `PLUME_D3D12_COREWINDOW` | `RenderWindow` = `{IUnknown* coreWindow, width, height}`; `CreateSwapChainForCoreWindow`; **exactly Microsoft's template**: flags 0 (no frame-latency waitable object, no allow-tearing), `DXGI_SCALING_STRETCH`, `DXGI_ALPHA_MODE_IGNORE`, FLIP_DISCARD, 2 buffers, B8G8R8A8, `Present(1, 0)`, **no `ResizeBuffers`** after creation. (With the waitable/tearing flags the presents succeeded and were never shown) |
| `D3D12_HEAP_TYPE_GPU_UPLOAD` | mingw-w64's `d3d12.h` (SDK 602) predates it: `D3D12_HEAP_TYPE(5)` (Agility SDK 1.613 value) |
| Typeless depth SRV | depth-stencil textures are `R32G8X24_TYPELESS`; `CreateShaderResourceView(resource, nullptr)` is invalid on typeless formats (a real driver may remove the device): give them their depth plane's format (`R32_FLOAT_X8X24_TYPELESS`) |
| `Map` / failed `Present` logging | `Map failed with error code 0x..` (first 4), `Present failed ...` (first 4); a null buffer or failed Map returns null instead of crashing |
| `D3D12Device::pipelineLibrary` + `cacheKey` | the persistent pipeline library hooks ([pipeline_cache.md](pipeline_cache.md)) |

### `plume_vulkan_strict.patch` (Vulkan, plus two formats' mappings)
Found by the validation-layer run below; applied after the two files above.
- `RenderFormat::R8G8B8A8_USCALED`, a **vertex input** format: Vulkan `VK_FORMAT_R8G8B8A8_USCALED`; D3D12 and Metal map it to what `R8G8B8A8_UINT` maps to (they convert integer vertex data to float by value anyway).
- A Vulkan graphics pipeline the driver rejects is returned as **null** (like Metal's and D3D12's), so the draw is skipped; before, the null `VkPipeline` was bound and crashed.
- The pipeline's viewport state declares **one** viewport and scissor (it declared one per render target; the runtime sets one, and a device without `multiViewport` allows one).

### `plume_queue_order.patch` (Metal)
plume's Metal backend creates every resource **`MTLHazardTrackingModeUntracked`** and keeps barrier fences **per command list**, so nothing orders command buffer N+1 after N: a render target written before a flush and
sampled after it could be read early. Fix: each Metal command buffer **waits for a per-queue `MTLEvent` value that the previous command buffer signals** when it ends (`encodeWait(orderEvent, orderValue)` at begin,
`encodeSignalEvent(orderEvent, ++orderValue)` at commit): in-order execution as on D3D12/Vulkan (one queue). GPU work of different parts no longer overlaps; the CPU/GPU overlap (the point of chunked submission) remains.
Applied **after** `plume.patch` (a separate file so trees where `plume.patch` is already applied still configure).

### `plume_metal_compression.patch` (Metal)
plume gave every single-sample render and depth target `MTLTextureUsageShaderWrite`, for its compute resolve
(`resolveTextureRegion`), which the runtime never calls. On Apple GPUs that usage turns off the **lossless compression**
of the texture, so every render pass load and store, every resolve copy and every sample of a target moved its full
size. Now shader write only with `UNORDERED_ACCESS`. Trap Team's tutorial at scale 2: 52.4 -> 60.0 fps, the GPU's
fragment work 9.8 -> 7.4 ms a frame and its copies 4.3 -> 2.5 ms (a copy of the whole 2560x1440 target 110 -> 62 us)
([renderer_threads.md](renderer_threads.md), "The GPU at scale 2") [V]. Applied after the three above.

### `plume_swapchain_result.patch` (Vulkan): a swap chain that refuses its images [?] (2026-11-10, issue #20)
**Why:** on Linux, an NVIDIA GT 1030 (driver 580.178) under GNOME on Wayland, fullscreen showed one frame (`[gpu] first
present 1920x1080`) and then nothing while the sound played; windowed was fine. No `[gpu] presents: N in the last 5 s` line
ever came, so every later frame stopped at the acquire: plume's `needsResize()` compares only sizes, the window and the swap
chain were both 1920x1080, and a failed `acquireTexture` only skipped the frame, forever. Most likely the driver answered
`VK_ERROR_OUT_OF_DATE_KHR` (or `SURFACE_LOST`) after the switch to fullscreen; it was not logged, which is the patch's
other half.
**Patch** (applied after the four above): `RenderSwapChain::getLastResult()` (0 by default: Metal), which the
Vulkan swap chain sets from `vkAcquireNextImageKHR` and `vkQueuePresentKHR` (the `VkResult`) and the D3D12 one from
`IDXGISwapChain::Present` (the `HRESULT`, 0.9.5; its `lastResult` field and assignment sit in `plume.patch`, whose
`present()` hunk they touch, so that each patch still reverses on its own at the next configure).
**Runtime** (`gpu.cpp`, `SwapChainFailed`; the rules in `gpu/swapchain_recovery.h`): when an acquire or a present fails
with a result other than 0 on a window that has a size, the swap chain is rebuilt before the next frame (`resize()`, a new
swap chain from the old one); from the second failure in a row, or at once for `SURFACE_LOST`, the swap chain object is
destroyed and made again, which destroys and remakes the window's surface (`SDL_Vulkan_CreateSurface`). Five attempts in a
row, then one every 2 s, until a frame gets through. Both paths: the normal present and frame generation's. A surface SDL
cannot make (plume leaves it null) is never used: the frame is skipped and the next attempt makes it again. One line per
streak (the first five streaks, then every 100th): `[gpu] swap chain: acquire failed (VK_ERROR_OUT_OF_DATE_KHR,
-1000001004) on a 1920x1080 window: rebuilding it`, after five `... still failing after 5 rebuilds, trying again every 2 s`,
and `[gpu] swap chain: frames again after N rebuilds`. Metal reports 0: its behaviour is unchanged. **D3D12** (0.9.5) [?]:
a failed present names its HRESULT (`DXGI_ERROR_INVALID_CALL, 0x887A0001`) and takes the same way back (`ResizeBuffers`,
then a new swap chain on the window); `DXGI_ERROR_DEVICE_REMOVED`, `_RESET`, `_HUNG` and `DXGI_ERROR_DRIVER_INTERNAL_ERROR`
are the device's, not the swap chain's: one line (`... the device is gone, not rebuilt`) and the device-lost report,
no rebuild. Unit rules in `ArkeyanSwapChainRecoveryTest`; no real DXGI failure was made here. F3 >
Fullscreen goes through `SDL_SetWindowFullscreen`; the size change then rebuilds the swap chain (`needsResize`), and a
refusal there takes the same way back.
**Verify** [V] on this Mac, MoltenVK build (`-DARKEYAN_MACOS_VULKAN=ON`, below): `SKY_GPU_HEADLESS_SWAPCHAIN=1` keeps a
swap chain under SDL's offscreen driver (MoltenVK's `VK_EXT_headless_surface`), and `SKY_GPU_TEST_ACQUIRE_FAIL=<n>[:lost]`
makes the next n acquires after the first frame fail as `OUT_OF_DATE` (or `SURFACE_LOST`) without asking the driver:
```sh
SSA_ISO=<disc> SKY_GPU_HEADLESS_SWAPCHAIN=1 SKY_GPU_TEST_ACQUIRE_FAIL=8 BUILD=$PWD/build-vk tools/regress/vulkan_validation.sh acquire 45
```
n=8: the first rebuild, five fast ones, two frames waiting, then `frames again after 6 rebuilds` and 134-151 presents per
5 s to the end. `3:lost`: `frames again after 3 rebuilds` (three surfaces made), then 150-162 presents per 5 s. Unit
rules: `ArkeyanSwapChainRecoveryTest`. On the player's machine [?]: the next log names the result and says whether frames
came back.

### `plume_d3d12_blend.patch` (D3D12): each render target its own blend and write mask [V] (2026-11-10)
plume never set `IndependentBlendEnable`, so on D3D12 every target of a draw took target 0's blend and write mask
(Metal and Vulkan always had them per target; plume's Vulkan device enables every feature the GPU has, `independentBlend`
included). Now `IndependentBlendEnable = !logicOpEnabled` (D3D12 requires it off with a logic op), and the unused targets
get D3D12's defaults. Where it was wrong (the six games' level frames on the Mac, `SKY_DUMP_DRAWS`, the `[dump]   target`
lines): Swap Force's tutorial decals (pixel shader `c3efbd6e0b41ae71`, three targets, blend 5/6) write target 2 with
mask 3 and target 0 with mask 7, so D3D12 also wrote target 2's blue channel; no draw's targets differ in blend. And the
runtime now masks the targets a pixel shader does not write ([render_bugs.md](render_bugs.md), Trap Team's title glow),
which D3D12 would otherwise ignore. `gpu/d3d12_pipeline_cache.cpp`'s `Version` is 2: an older `pipelines.d3d12` is dropped once.
Applied after the five above.

### `plume_present_latest.patch`: VSync and the refresh rate [?] (2026-11-11, 0.9.6)
A 60 FPS game on a 144 Hz display with FIFO VSync: each frame waits for a refresh in the swap chain's queue, the present
blocks while it is full, and that wait feeds back into the game's pace (a player on Windows, RTX 3050, 144 Hz, found
VSync made the game worse). `gpu/present_mode.h` chooses from the window's display (`SDL_GetCurrentDisplayMode`) and
`fps_limit`: VSync off: off; a refresh that is a whole multiple of the frame rate (60, 59.94, 120, 240; 30 on 60 or 90),
an unknown refresh or no limit: strict FIFO; else (60 on 75, 100, 144, 165): **the newest frame at each refresh**,
without queueing: Vulkan MAILBOX (where the surface has it), D3D12 flip model at sync interval 0 without
`DXGI_PRESENT_ALLOW_TEARING` (plume's present-wait path, `desc.enablePresentWait`; `wait()` is not used), with a third
swap chain image. The frames keep the emulated blanks' 60 Hz pace. Metal has no such mode (`CAMetalLayer` presents at the
next refresh): FIFO, and the log says so; ProMotion's 120 Hz is a multiple. No fixed refresh shows 60 on 144 evenly: each
frame stays 2 or 3 refreshes (2.4 on average); a 120 Hz mode or a variable-refresh display (G-Sync, FreeSync) does.
The log says the choice once, and again if the window moves to another refresh (checked every 10 s): `[gpu] vsync on:
display 144.00 Hz, 60 FPS, not a multiple: the newest frame at each refresh`. The patch also reads what the display
showed (`getDisplayStats`): D3D12's frame statistics (present and refresh counts), Metal's presented handlers (times);
the 10 s perf line then gives how long the frames stayed: `[perf] display 144 Hz, vsync ...: 598 frames shown, for 1
refresh 0%, 2: 60%, 3: 40%, 4 or more: 0%` (Vulkan reports nothing). Offscreen runs have no swap chain: this Mac proves
the choice and the count only in `ArkeyanPresentModeTest`; the PC check is [test_096_pacing.md](../../release/test_096_pacing.md).
Applied after the six above.

## When D3D12 shows nothing: Vulkan instead [?] (2026-11-11, issue #27)
A Windows player's Trap Team played its sound on a black screen in D3D12 (the 0.9.5 default) and worked in Vulkan (GPU and
log not known yet). Whatever the cause, `gpu/first_frame.h` now guards D3D12's first frames, on desktop Windows only, when
D3D12 runs as the **setting's choice** (not `SKY_GPU_BACKEND`, not after Init's own no-device fallback) on a window:
- **What counts as a frame**: a present that succeeded and whose command list's fence was then waited for (every fence wait
  goes through `WaitFence` in `gpu.cpp`). A present that returns `S_OK` while the GPU never finishes is no frame.
- **A watch thread** samples the counters every 250 ms, so a GPU thread stuck in a fence wait or in a blocked `Present` is
  seen too. It **switches** when: the device is removed before the first frame or in the first minute after it (any
  reason: `DXGI_ERROR_DEVICE_HUNG`...); one fence wait lasts 5 s; or 6 s of a **visible** window after the first present
  attempt pass with no frame shown (a minimised or hidden window does not count; 20 s of loading before the first present
  neither). Frames shown for a minute, or no present attempt in 2 minutes: the watch ends.
- **The switch** (`main.cpp`, `SwitchToVulkan`): `[<game>.video] gpu_backend = vulkan` and `gpu_backend_note = Direct3D 12
  showed nothing (<why>): switched to Vulkan` in the settings file, the running marker removed, then exit code **75**.
  Started by the launcher (`SKY_LAUNCHER_RESTARTS=1`), the launcher starts it again once with `SKY_GPU_BACKEND=vulkan`;
  started on its own, the game starts a new copy of itself with `SKY_GPU_BACKEND=vulkan` (`PlatformStartAgain`). Both
  restarts force Vulkan, so a settings file that cannot be written never brings D3D12 back: no loop. The exit does not
  wait for the game's threads (they may be stuck on the GPU).
- **Not in-process**: by the time the first frames can be judged, the guest has made its textures and buffers on the D3D12
  device and the GPU thread may be blocked in a fence wait or a present that never returns; nothing can tear that down
  safely. Init's own fallback (no device, no DXIL) stays in-process: nothing exists yet then.
- **The log**: `[gpu] D3D12 is the setting's choice: should no frame reach the screen in its first seconds, this game
  switches to Vulkan`, then either `[gpu] D3D12's first frames reached the screen (N shown in the first minute)` or
  `[gpu] D3D12 shows nothing on this PC: <why>`, `[gpu] this game starts with Vulkan from now on: gpu_backend = vulkan in
  [<game>.video]`, `[gpu] restarting with Vulkan: ...`. The launcher's game log keeps the first run as
  `skylauncher-game.prev.log` (`Arkeyan.prev.log` too).
- **Proof**: `ArkeyanFirstFrameTest` (fake backends: presents that fail, presents whose GPU completes nothing, a device
  lost early and one lost late, a slow boot, a minimised window, a working one; the settings written). On a PC:
  `SKY_GPU_TEST_NO_FRAME=fail|stuck|lost` makes D3D12's presents fail, its first frame never complete, or its device
  report a removal; the game must restart in Vulkan with the note written. Not yet run on a real PC.
- **Not caught**: frames the GPU completes but draws black (a wrong shader result, a skipped pipeline). The paths below.

D3D12 paths that could show black on some GPUs only, and what each leaves in the log:

| Path | What the log shows |
|---|---|
| A texture read through a view of another type (a cube map as a 2D array: NVIDIA read 0; fixed, `gpu/heap_view.h`) | nothing: presents go on (`[gpu] presents: N in the last 5 s`) |
| A pipeline the driver refuses (a DXIL feature, a format as render target) | plume's `CreateGraphicsPipelineState failed with error code 0x...`; its draws are skipped |
| A GPU fault (an uncleared placed target's metadata on AMD, an out-of-bounds read) | `[gpu] the GPU device is lost`, the DRED breadcrumbs and page fault; now the switch |
| A swap chain that refuses its images or presents | `[gpu] swap chain: present failed (DXGI_ERROR_..., 0x...)`, rebuilds; now the switch after 6 s |
| Presents that return while the GPU never finishes (a driver hang without a reset) | nothing before 0.9.6; now `the GPU did not finish its work in 5 s` and the switch |
| The pipeline library (`pipelines.d3d12`) of another driver | `[gpu] D3D12 pipeline cache: ...`; pipelines are rebuilt, not black |
| A shader cache without DXIL | `[gpu] D3D12 did not start (the shader cache has no DXIL): trying Vulkan` |
| No window surface (`GetRenderWindow` fails) | `[gpu] D3D12 on <GPU>, offscreen output`: the window stays black, not GPU-specific, no switch |

## Limits and rules we learned

- **`setFramebuffer` always ends the Metal render encoder**: bind only on change (a level frame went from ~3000 to ~28 render passes; GPU wait 160-227 ms -> 8-9 ms; 2.4 -> ~30 fps). Every pass loads and stores all attachments.
- **No texture-to-buffer copies** on Metal/Vulkan: readbacks use a compute pass (`ReadbackCS` / `ReadbackRawCS`) packing pixels into a buffer.
- The Metal swapchain's present completion handler captures the swapchain: destroying it right after a present can race (nothing destroys it: the game exits with `_Exit`).
- Framebuffers cache their sizes (rebuild after a resize) and are keyed by texture pointers: drop them all before destroying a texture (`DestroyTexture` defers until the frame is done).
- `createShader` takes a **metallib**, not MSL; descriptor set N = `[[buffer(N)]]`, binding = `[[id(binding)]]`.
- plume's own CMakeLists always builds Vulkan and forces `CMAKE_OSX_DEPLOYMENT_TARGET` 10.15: `plume.cmake` builds only the platform's backends.
- **Xcode 26+ ships without the Metal compiler**: `xcodebuild -downloadComponent MetalToolchain` (configure fails with that hint).
- Triangle fans: unsupported on Metal and D3D12 (none seen).
- Pulled vertices and `ParamGen`: every backend since 2026-11-03 (before, SPIR-V read the vertex's own data and had no param_gen: the squares below).
- **A texture's default view follows the resource, not the shader** (2026-11-10): a cube map (six slices, `RenderTextureFlag::CUBE`) or a stacked array bound with `setTexture(..., nullptr)` is a 2D array on D3D12 and Vulkan, and Metal's texture is a 2D array too. The bindless heaps declare `Texture2D`, `Texture3D` and `TextureCube`: a read through another type is undefined (NVIDIA returned 0 from every cube map, AMD and Apple the faces; Apple read the lower mips differently). `gpu/heap_view.h` gives every cube map, array and swizzle a view of the heap's type, the cube placeholder included. The validation layer does not see it in the bindless heaps, with or without GPU-assisted validation.
- D3D12 render/depth targets must be **committed** resources (placed ones must be cleared/discarded/copied before any other use; reading garbage compression metadata is a classic GPU fault on AMD).
  D3D12 upload of block-compressed mips smaller than 4x4: round the footprint up to whole blocks.

## Strict Vulkan drivers: the validation-layer run [V] (2026-10-28)

**Why:** Metal, D3D12 and the Linux/Windows Vulkan drivers forgive things that MoltenVK, or a stricter driver, does not. MoltenVK failed the pipelines whose vertex attribute type
differed from the shader's input ("Vertex attribute type mismatch between host and shader"), and the null pipeline that resulted crashed the game. This build runs the whole game on
Vulkan with the Khronos validation layer on the Mac and lists what the layer says.

**Build and run** (macOS; `brew install molten-vk vulkan-loader vulkan-validationlayers`; never a player's build):
```sh
cmake -S . -B build-vk -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_MACOS_VULKAN=ON -DARKEYAN_VULKAN_VALIDATION=ON && ninja -C build-vk ArkeyanRuntime
SSA_ISO=<disc> [SKY_LEVEL=level_027] [SYNC=1] tools/regress/vulkan_validation.sh <name> <seconds> "<SKY_PRESS value>"
```
`ARKEYAN_MACOS_VULKAN` compiles plume's Vulkan backend instead of Metal (`ARKEYAN_METAL` is the macro that the runtime's Metal-only code is under; the shader cache then uses its SPIR-V entries and the present
pass its SPIR-V), `ARKEYAN_VULKAN_VALIDATION` turns the layer on in a Release build (plume enables it only when `NDEBUG` is off). The script isolates the run like every other (`testing.md`), sets the
loader, the layer and the MoltenVK driver paths (no `nice` in front of the game: it is a protected binary and would drop `DYLD_LIBRARY_PATH`), lifts the layer's "10 copies of a message" limit,
and writes `private/runs/<name>/summary.txt`: the messages counted by kind (`validation.txt` has the text). `SYNC=1` adds synchronization validation (slower). Under SDL's offscreen driver the
build draws offscreen (no swapchain), so `SKY_GPU_SNAPSHOT` works as on Metal.

**Coverage:** the title and frontend, `level_027` and `level_040` (the fast recipe's presses, a slower and longer variant for a loaded machine), the Toy Box opened at the portal prompt (a figure taken),
the intro movies (`SKY_SKIP_MOVIES=0`), all with the baseline's 1570 pipelines built at start (so every listed pipeline is validated, not only the drawn ones); a second pass of `level_027` with synchronization validation.

| Kind | Count (first run) | Cause | State |
|---|---|---|---|
| `VUID-VkGraphicsPipelineCreateInfo-Input-08733`: vertex attribute `R8G8B8A8_UINT` at location 16 vs the shader's `float4` | 8 pipelines (4 shaders), each twice | the game declares **PSIZE as UBYTE4**; the translated shaders read PSIZE as `float4`. Metal and D3D12 convert by value; MoltenVK refuses ("Vertex attribute type mismatch"), the pipeline came back null and was bound: a crash | **fixed**: `gpu/draw.cpp` builds the input element as `R8G8B8A8_USCALED` for every integer-format input except locations 1-3 and 9 (the shaders' `uint4` inputs: normal, tangent, binormal, blend indices). The pipeline key keeps the guest's format, so `pipelines.bin` and the baseline stay valid |
| `UNASSIGNED-GeneralParameterError-RequiredHandle`, `VUID-vkCmdBindPipeline-pipeline-parameter` | 1 + 1 | binding that null pipeline | **fixed**: plume returns null, the draw is skipped (patch) |
| `VUID-vkCmdDraw-None-07831`, `-07832`, `vkCmdDrawIndexed` (same) | 40 (limit 10 each) | dynamic viewport/scissor index 1 never set: the pipeline declared one viewport **per render target** (2 on the two-target draws) and the runtime sets one | **fixed** (patch: one viewport) |
| `SYNC-HAZARD-WRITE-AFTER-WRITE` on `vkCmdCopyImage` | 28 223 in 200 s | resolves copy several times into the same texture; it stays in `COPY_DEST`, so no barrier sat between two writes | **fixed**: `CopyTextureNow` adds a barrier when the destination is already `COPY_DEST` (Vulkan only) |
| `Undefined-Value-ShaderOutputNotConsumed` (warning) | 56 | a pixel shader writes colour 0 in a draw with no colour attachment (the depth-only passes) | **explained**: legal, the write is discarded; D3D and Metal draw such a pass the same way |
| `VUID-vkCreateDescriptorSetLayout-support-09582` | 6 (the 16384-descriptor texture heaps, variable count, update after bind) | MoltenVK's `vkGetDescriptorSetLayoutSupport` reports "unsupported" for the layout against its `maxPerSetDescriptors` (1212), then creates it and renders correctly (frames checked) | **explained**: a MoltenVK limit report, not our code; the heap size is the bindless design, and Vulkan 1.2 descriptor indexing requires update-after-bind limits of at least 500 000 samplers/images per set. Not reachable on a player's Mac (Metal) |
| `VALIDATION-SETTINGS` (warning) | 1 | the script's `VK_LAYER_ENABLES` is a deprecated layer setting | script only |

After the fixes: **no validation error beyond the MoltenVK descriptor-limit report, no synchronization hazard**, no `[mvk-error]`, no failed pipeline, on all runs above. Rendering looked right in the
snapshots (title, menus, `level_027`, Toy Box), except one difference, below.

**Solved (2026-11-03): yellow-green blocks on the grass on Vulkan.** The 32-pixel blocks MoltenVK showed in `level_027` (and a Windows NVIDIA tester at the hub, 0.8.0) were the sun's light tiles
([edram_resolve_tiling.md](edram_resolve_tiling.md), "Things the host cannot produce") shaded with the wrong class: the SPIR-V shaders lacked the computed-index fetch that reads each tile's class
and the param_gen register. The depth write-back was right, as measured then. Fix: [render_bugs.md](render_bugs.md) (level frame) and [shaders.md](shaders.md). [V] on MoltenVK: the fast recipe in `level_027`, frames every 120 from frame 1500
(`SKY_GPU_SNAPSHOT_FROM=1500 SKY_GPU_SNAPSHOT_EVERY=120`); the dialogue frame showed the tester's checkerboard before, none after, and the sun's shadows came back. A grid score (the mean
luminance step across the 32-guest-pixel tile boundaries over the step elsewhere, 1.0 = no grid) gave mean / worst frame: Vulkan before 1.046 / 1.227, after 0.977 / 1.018, Metal 0.976 / 1.029

**Seen again (2026-11-08), solved (2026-11-11) [V] (Mac; Windows [?]):** the same yellow-green blocks on `level_027`'s
grass on MoltenVK at a 2560x1440 window (scale 2), and in a player's Windows video: sunlit tiles in shadowed grass, a
staircase one tile inside the grass-sky edge, while the camera moves. **Neither the scale nor the backend**: the tile
classes were 3 to 7 frames late. The classification job sleeps `Sleep(0)` after each tile and the kernel's yield back-off
made each a 200 us sleep (a job took ~115 ms, not ~3 ms): a tile that was sky a few frames before kept its "no shadow"
class. On Windows a 200 us sleep lasts 1 ms or more, so the lag there was several times longer (not measured).
Cause and fix: [threads_sync.md](../xbox360/threads_sync.md#yielding-and-idle-loops-lux), [render_bugs.md](render_bugs.md).
Measured with a temporary trace (each frame's class grid, and the job that read which frame's depth; the "wrong" tiles of
a frame are those whose class differs from the one its own depth gives), `tools/regress/ssa_grass.sh` (the fast recipe,
then a walk), the same binary with the fix off and on, ~880 frames before and ~2500-2900 after per row:

| Backend, window, scale | job p50 before / after | lag (frames) before / after | wrong tiles per frame, mean / p95, before / after |
|---|---|---|---|
| MoltenVK 2560x1440 x2 | 118 / 4.7 ms | 3-7 / 2-3 | 22.9 / 90 -> 16.6 / 66 |
| MoltenVK 1920x1080 x2 | 119 / 5.7 ms | 3-7 / 2-3 | 22.6 / 88 -> 16.4 / 67 |
| MoltenVK 2560x1440 x3 | 116 / 2.9 ms | 3-7 / 2-3 | 22.9 / 86 -> 16.7 / 68 |
| MoltenVK 2560x1440 x4 (GPU-bound, 25 fps) | 112 / 4.2 ms | 4-7 / 3 | 33.0 / 118 -> 26.0 / 98 |
| Metal 2560x1440 x2 | 112 / 2.5 ms | 3-7 / 2 | 22.2 / 88 -> 16.6 / 60 |
| Metal 1280x720 x1 | 119 / 5.0 ms | 3-7 / 2 | 21.9 / 86 -> 17.2 / 62 |

What is left is the depth read-back's own delay (two GPU frames, so that it never stalls,
[edram_resolve_tiling.md](edram_resolve_tiling.md#things-the-host-cannot-produce-cpu-reads-of-resolves)) plus one frame
(the light passes are drawn before the frame's job ends): one frame of lag gives 9.4 wrong tiles, about the console's.
It grows by a frame where the GPU is the bottleneck (x4 above): a delivery to guest memory as soon as the GPU has the
depth would win that frame back; not done (writing guest memory from the GPU thread, after the resolve, risks memory
reused meanwhile) [?]. The grid score (`tools/regress/grid_score.py`) does not separate the two in this walk (1.02 mean
before and after: `level_027`'s start has little shadowed grass at an edge); the player's video scores 1.066 worst, with
up to 24 block edges a frame.

## MoltenVK and the Alchemy games [V] (2026-11-05)

SuperChargers and Imaginators on the macOS Vulkan build (`-DARKEYAN_MACOS_VULKAN=ON`, no validation layer), run with
`tools/regress/moltenvk_exe.sh` as the hub tests' `LIVE_EXE` ([how-to](../../getting-started/platforms/README.md#proving-a-game-on-every-backend-how-to)):
both hub tests pass, no `[mvk-error]`, no failed pipeline, the shots match Metal's (differences are animation timing).
- **Pipelines built at draw time cost 50-220 ms each** on MoltenVK (SPIR-V to MSL, then Metal's compiler; 1-2 ms on Metal
  with the cache's AIR): a first run stutters; the second start loads `pipelines.bin` (359 pipelines ready in 167 ms).
- **60 held** (`game_perf.sh`, `FALLBACK=0`, scale 2, load 8-14): SuperChargers' Academy 60.0 fps (Metal 59.6);
  Imaginators' `hub_2016` **53.2 fps** (Metal 60.0): GPU-bound, the GPU thread busy 100 %, 14.8-16.8 ms a frame waiting
  for the GPU, the CPU side unchanged. `MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS=0` gives the same 53.4: the GPU's work as
  MoltenVK translates it (its MSL from SPIR-V, its render passes) is slower than our Metal path's; which pass costs it is
  not measured [?]. A MoltenVK-only cost: players on macOS use Metal; a real Vulkan driver is not concerned.
