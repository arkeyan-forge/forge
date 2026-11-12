# EDRAM, resolves, clears, tiling, MSAA

> **Purpose:** how are EDRAM surfaces, resolves, clears, swaps, tiling and CPU reads of resolves handled on the host? · **Status:** current (2026-11-11) · **Tags:** [Xbox] [Lux]

Code: [`d3d/render.cpp`](../../../runtime/d3d/render.cpp) (`TargetFor`, `Resolve`, `Clear`, `WriteBackDepth`), [`gpu/draw.cpp`](../../../runtime/gpu/draw.cpp)
(`CopyTexture`, `ResolveDepth`, `ClearColor`, `ClearDepthStencil`), [`d3d/xenos.cpp`](../../../runtime/d3d/xenos.cpp). Class **[Xbox]** unless marked.

## The console model

Rendering goes into **10 MB of EDRAM** (not addressable): a colour target and a depth target are *tiles* of it (`RB_COLOR_INFO` low 12 bits = base tile; depth likewise).
Nothing is visible to the CPU/shaders until a **resolve** copies a region of EDRAM into a texture in main memory. Several surfaces can alias the same EDRAM tiles
(this is how the console reinterprets data); targets larger than EDRAM are rendered in **tiles** with predication.

## Our model: one host texture per EDRAM surface

- **`TargetFor(surface, depth)`**: key = `(EDRAM base tile (12 bits), format, width, height)` read from the D3DSurface (`+0x1C` info word, `+0x24` size
  `(w-1) << 18 | (h-1) << 3`); a host texture of `width*scale x height*scale` is created on first use (colour: `RenderTarget | Sampled`; depth: `DepthStencil`, format `D32_FLOAT_S8_UINT`).
  Surfaces **aliasing the same EDRAM base with different formats become different host textures** (the aliasing tricks below are **not** emulated).
- **New colour targets start at zero**: `TargetFor` queues a whole-target RGBA `(0,0,0,0)` clear immediately after
  creation, before a draw, blend or resolve can read the target. Reused targets keep their contents, and guest clears
  retain their own colour and rectangle. This defines the previously undefined read of a newly created target
  (Swap Force's unwritten light buffer; [issue #26](https://github.com/mflr0/Arkeyan-Launcher/issues/26)). It does not emulate EDRAM
  aliasing or initialize depth targets, whose clear value belongs to the guest.
- **Resolution scale N** (`resolution_scale`): every host target is N times its EDRAM surface, so guest pixel coordinates (viewports, scissors, resolve rects/points,
  clear rects) are multiplied by N (`Scale()`); screen-space draws (viewport transform disabled) keep converting guest pixels via `g_PositionScale`; shaders get
  `g_ResolutionScale` (point sizes x N, `vPos` / N). Memory grows with N^2.
- **Colour formats** (`ColorTargetFormat(RB_COLOR_INFO.color_format)`): 0 `k_8_8_8_8` and 1 `_GAMMA` -> R8G8B8A8_UNORM (**gamma treated as linear**); 2 `k_2_10_10_10` and 10
  `k_2_10_10_10_AS_10_10_10_10` -> **R16G16B16A16_UNORM** (plume has no 10:10:10:2; 16 bits per channel instead; the CPU decodes real 2_10_10_10 textures); 6 `k_16_16_FLOAT` -> R16G16_FLOAT;
  14 `k_32_FLOAT` -> R32_FLOAT; 15 `k_32_32_FLOAT` -> R32G32_FLOAT; everything else (`2_10_10_10_FLOAT`, 16_16, 16_16_16_16 fixed-point -32..32 and variants) -> wide
  R16G16B16A16_FLOAT.
- **MRT draws need equal target sizes** (compare sizes, not pointers: comparing pointers dropped every MRT draw, so the G-buffer and light pre-pass never ran, D17).

## Resolve (`D3DDevice_Resolve` `0x822A1560` [SSA])

Arguments `(dev, flags, srcRect, destTex, destPoint, level, slice, clearColor (r10), f1 = clearZ, 10th = clearStencil at caller r1+0x5C)`.

- **Flags**: bits 0-2 source (0-3 colour target, **4 depth**); bits 4-6 samples; **0x100 clear the colour target after the copy, 0x200 clear depth-stencil after the copy**
  (with clearColor, clearZ, clearStencil); **bits 26-31 exponent bias** (not applied: `[render] resolve with exponent bias N: not applied`, logged once).
- **The `RB_COPY_*` shadows are reset before Resolve returns (`0x822A2550`): read nothing from them afterwards** (D10): resolve parameters come from the call arguments.
- **Swizzle**: D3D sets `copy_dest_swap` (red/blue) when the destination fetch constant's swizzle does not start with X (ARGB textures, ZYXW; `0x822A17AC`). The host copy keeps
  the target's order; `HostTexture.swapRB = ((dword3 >> 1) & 7) != 0` and **sampling swaps back** (a view with swapped components). Reading `RB_COPY_DEST_INFO` instead swapped every resolved ARGB texture.
- The destination is a host texture keyed by the fetch constant (`TextureKey` = base address >> 12, format, width, height, dimension), marked `resolved = true`: **the host copy is the only valid one**
  (the guest memory holds nothing).
- **Cube destinations**: a destination whose fetch constant says cube (`Dimension() == 3`) gets a host **cube** texture (6 slices) and the copy goes into face `slice % 6` (the engine
  renders and resolves its ambient cube map **face by face every frame**; a 2D one-slice host texture made the ambient pass add 0: dark level).
- **Depth resolves** (formats k_24_8 and k_24_8_FLOAT): `gpu::ResolveDepth` draws the depth into an **R32_FLOAT** texture (X = depth value, as Xenia converts) with `ResolveDepthPS`
  (present.metal / present.hlsl); the destination is then sampled as depth in X.
- The `after-copy` clears: `ClearColor(target, rgba)` / `ClearDepthStencil(depthTarget, true, true, z, stencil)`. The rect is passed only when the resolve did not cover the whole target.

## Clears are draws (D4)

`D3DDevice_ClearF` and **`BeginTiling` / `BeginZPass`** all end in **`0x822ADDE0`**, which clears with a **rectangle draw of its own** (an inline DRAW packet no draw hook sees).
We hook that worker: `(device, flags, rect, color float4, -, stencil, f1 = z)`; flags bits 0-3 colour targets 0-3, **0x10 depth, 0x20 stencil** (confirmed from the
depth-control values 0x76 / 0x8701 built in `0x822AD840`); the rectangle is clipped to the viewport (device `+0x3260`) and, with the scissor test on (`+0x2F80`), to the scissor rect
(`+0x327C`). A whole-target clear is a cheap load-action clear. Without this, additive light buffers accumulated stale data (white scene). The old "clear every depth target to 0 at Swap"
workaround had to go once real clears arrived (D21).

## Swap

`D3DDevice_Swap(dev, frontBufferTexture, ...)` (`0x822A95C0`): wait the vblank ([../xbox360/video_vblank.md](../xbox360/video_vblank.md)), look the front-buffer texture up by its fetch
constant (it was resolved into), `gpu::Present(texture)`, then run the original. NGL passes double-buffered front-buffer textures (`0x82C54698` / `0x82C54720` [SSA]).

## Tiling and Z-pass (not used by the level frame)

`BeginTiling` (`0x822AE778`, tile rects `+0x32CC..`, count `+0x32C4`), `EndTiling` (`0x822AEAD0`, resolves every tile via Resolve), `SetPredication` (`0x822AE608`, `SET_BIN_MASK/SELECT`),
`BeginZPass?` (`0x822AEF88`)/`EndZPass?` (`0x822AF218`) with a fixup list (`+0x34B0`). **Predicated tiling IS used by the frontend path** (`nglResolveTiles` `0x8262D540`: per tile `SetPredication(3 << 2i)` +
Resolve); **a level frame (level_027) used no tiling, no Z-pass and no tessellated draws**. Plan for a host renderer: ignore tiles (clear at Begin, resolve once at End), `SetPredication` a no-op.

## Level-frame EDRAM facts [Lux] [SSA]

At 1120x704: depth D24FS8 at tile `0x000`, RT0 at `0x268`, RT1 at `0x4D0` (0x268 tiles each). Two things looked like emulation work and were not:
1. **Colour target aliasing the depth buffer with write mask R only + copy shader `xex_82092DB8.ps`** (14 rect draws): on hardware this rewrites the depth buffer's **stencil byte** from the
   Z-prepass resolve; then 560x352 **4xMSAA** depth-only draws on the same EDRAM base (Hi-Stencil). **Not emulated, not needed**: once the ring state was right (main pass without stencil test), the level
   rendered without it.
2. 4xMSAA half-resolution depth-only passes (560x352): host targets are single-sample.

## Things the host cannot produce: CPU reads of resolves

A resolve exists only on the host GPU. If the **CPU reads a resolve address** (the light tile classification job, `sub_821404D8`), it sees whatever the guest memory holds (zeros: no sun shadows;
leftovers: blocky random shadow classes). `WriteBackDepth` writes the depth resolve back into guest memory **as the 360 stores it**: tiled (`Tiled2D(x, y, pitch, 2)`), depth as 20e4 in bits 31-8
(`Float32To20e4`), stencil 0, big-endian, one texel per guest pixel at higher scales; the hook on `sub_821404D8` records which physical address the job reads (`g_cpuReadDepth`), only that
resolve is written back (3 `k_24_8_FLOAT` resolves per frame: `0x06A60000` full-res, `0x06090000` 560x352, `0x0BDB0000`; writing all cost ~23 ms), **two frames late** through
`gpu::RequestReadback` / `CollectReadback` (compute `ReadbackRawCS`; a synchronous readback cost 10-12 ms/frame at x2) so the CPU is never stalled. The classes a frame draws
with are thus built from depth 2-3 frames old (the job of the frame before, [level_frame_lightprepass.md](../../engines/lux/level_frame_lightprepass.md));
measured over a walk in `level_027`, that leaves 16 tiles of 770 per frame in another class than that frame's own depth
gives (median 4), against 9 for one frame, about the console's; a GPU-bound frame (scale 4) adds a frame (26)
(2026-11-11, [plume_backends.md](plume_backends.md)). If another CPU reader shows up in Giants, add it the same
way: **find CPU readers of resolve addresses early** (write-protect the page and see who reads: that is how `sub_821404D8` was found).
