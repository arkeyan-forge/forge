# Upscaling: AMD FSR 3.1 in every game

> **Purpose:** how does the runtime upscale the 3D scene with AMD FSR 3.1 on Metal, D3D12 and Vulkan, what each engine gives it, how a player switches it, and how to check it? · **Status:** current (2026-11-12, 0.9.6) · **Tags:** [Xbox] [Lux] [Alchemy] [V] [?]

The owner's decision (office/decisions.md, 2026-11-03): **AMD FSR 3.1 in every game**, a setting in the launcher and in
the F3 menu, off by default. AMD's FidelityFX SDK (MIT) is used as a library, unmodified, under `thirdparty/`
(THIRD_PARTY_NOTICES.md, the kit's `COMPONENTS.md`); the backend that runs it is ours. MetalFX stays as an internal
comparison on macOS (`SKY_UPSCALER=metalfx`), never offered as "FSR".

## The settings

| Where | Key | Values |
|---|---|---|
| launcher.ini `[<game>.video]` (or `[video]`) | `upscaler` | `off` (default), `fsr3` |
| | `fsr_quality` | `native`, `quality` (default), `balanced`, `performance`, `ultra_performance` |
| environment (tests) | `SKY_UPSCALER`, `SKY_FSR_QUALITY` | the same; `SKY_UPSCALER` also takes `metalfx` (internal) |
| F3 menu, Video | **Upscaling** (Off, FSR 3), **FSR quality** (Native AA first; shown while on) | applied at the next frame, no restart; written to the game's section |

**Scales** (`gpu/upscale_mode.h`): the game's targets render at a whole multiple of the guest's size, so a preset picks
the scene's scale nearest to the frame's scale over FSR's ratio (1.5, 1.7, 2, 3). The frame's scale is the window's
height over 720, rounded (`SKY_UPSCALE_SCALE=<n>` sets it). At 1440p (frame x2) every preset renders the scene at x1
(FSR 2:1); at 4K (frame x3) quality, balanced and performance at x2 (1.5:1), ultra performance at x1 (3:1); at 720p FSR
runs at 1:1, as anti-aliasing. The F3 menu's **Resolution scale** line then reads "x1, FSR 3 to x2".

**Native** (0.9.4, `fsr_quality = native`, AMD's "Native AA", ratio 1): the scene always at the frame's own scale (x2 at
1440p, x3 at 4K), FSR as temporal anti-aliasing only (8 jitter phases, no LOD bias). The owner's ask: frame generation
without the smaller picture of the four upscaling presets. It is the last value of `upscalemode::Quality`, so the four
keep theirs; the menus list it first. Its cost is the full-size scene plus FSR's passes: Swap Force at 1440p held 24 real
FPS beside another game's run, Giants 30 [V] (below). While FSR is on,
the resolution scale setting is kept for when it is off.

**Output size:** where the composite stretches the scene over the whole frame (Alchemy: Giants' 1000x600, and the
others' 1040x620 into 1280x720), FSR outputs the frame's size and does the whole stretch (1000x600 -> 2560x1440 at
1440p); SSA's composite reads by pixel position, so its output is the scene's size times the frame's scale (1120x704 ->
2240x1408). The jitter phases (8 x ratio², FSR's count) and the texture LOD bias follow the width ratio.

**Full-size scenes** (Swap Force, Trap Team, SuperChargers, Imaginators render their 3D at the frame's 1280x720,
`game::Adapter::fullScene`): the scene cannot be told from the frame by size, and the frame often reuses the scene's
EDRAM, so with FSR on these games render their own smaller scene (1040x620, as `original_resolution`) and FSR
upscales it to the frame (2.46x at 1440p). That choice is made at the start: in these four, the F3 menu's Upscaling
row applies at the next start when the game started with FSR off.

**Live switching** (`upscale::Request`, `d3d/upscale.cpp`): the menu asks; at the next swap the scales change and
`render.cpp` drops every cached render target (`DropTargets`), which the next frame makes again at the new sizes (one
frame's EDRAM content is lost, as after a mode change). FSR's context is made again when its sizes change.

## How it works

```
scene draws (jittered, depth-tested, scene-sized targets)
   -> composite draw found (d3d/ultrawide.h's composite constants): upscale::Composite
      -> gpu::Upscale (queued like a draw) -> gpu/fsr/fsr.cpp on the GPU thread:
         0. the object mask: this frame's moving and skinned draws drawn again over the scene's depth, each writing its
            number (R8; gpu/draw.cpp ReplayObjects): "Per-object motion" below
         1. our inputs pass (fsr_inputs.hlsl / .metal): camera motion, or each object's own (RG16F, texture units),
            depth (R32F, 0 near), and the reactive mask where an object left (R8)
         2. FFX's FSR 3.1 dispatch: 9 compute passes (prepare inputs, luma pyramid, shading change pyramid, shading
            change, prepare reactivity, luma instability, accumulate + RCAS) through our backend over plume
      -> the composite samples FSR's output; HUD and text are drawn after it, at the frame's scale (never jittered)
```

| Input | Where it comes from | Status |
|---|---|---|
| Jittered colour | the scene's depth-tested draws into scene-sized targets add the sub-pixel offset to `g_HalfPixelOffset` (no shader change); Halton (2, 3), 8 x ratio² phases (FSR's count) | [V] |
| Depth | the scene-sized depth target with the most depth-tested draws, its viewport range the widest of those holding a quarter of its draws' weight (`upscale::SceneDepthRange`: not a sky's single value, not a light full-screen pass over the whole range when the world is drawn in a slice of it, Swap Force: the world in 1 -> 0.2, depth-only quads in 1 -> 0); a depth outside that range (another layer's slice, a HUD portrait's) gets no motion; the inputs pass normalises it to 0 near, 1 far: ndc depth is reversed when most of the scene's depth tests are "greater" (reversed Z) unless the viewport's range is reversed too (`invertDepth`); NaN becomes 1 | [V] |
| Camera motion (finite, within the picture: a point near the camera plane would give an infinity that spreads through FSR's history) | the **camera vote** (`d3d/upscale_math.h`): each indexed scene draw's position matrix (`game::UpscalePositionMatrix`, c0-c3 in every game) against the same draw in the previous frame; the heaviest group that agrees is the camera; the inputs pass reprojects each pixel's depth with it | [V] all six games (`[upscale] last 900 frames: the camera's motion found in N`) |
| Camera planes, field of view | fitted from the heaviest scene draw's matrix (`upscale::CameraOf`: clip z = c x view z + e), for FSR's depth tests | [V] Giants, [?] SSA (no fit: defaults) |
| Texture LOD | -log2(frame / scene scale) in every implicit-LOD fetch of the translated pixel shaders (`g_MipBias`, shared constant +316, `patches/xenosrecomp.patch`: Metal ignores a sampler's bias); 0 while off | [V] Metal |
| Sharpening | FSR's RCAS at 0.5; the game's own FXAA pass (edge smoothing) draws the plain copy in a level while FSR is on: FSR anti-aliases, and its picture has no luma in alpha for FXAA 3.11 (Trap Team's composite is its tone map and FXAA: smeared otherwise) | [V] |
| Per-object motion | the moving and skinned draws' own reprojection, per pixel through the object mask (below) | [V] Giants (Metal), [?] the other games' gain |
| Reactive mask | 0.9 where an object was the frame before and none is now (the trail it leaves); 0 elsewhere | [V] Giants (Metal) |

### Per-object motion (0.9.6, issue #56)

With the camera's motion alone, a character running past the camera left a light trail for a frame or two: FSR took
its history from where the camera's motion pointed, where the character had been. Now each moving object gets its own:

1. **Which draws** (`upscale::CameraVote`, `d3d/upscale_math.h`; unit-tested in `ArkeyanUpscaleTest`): a scene draw
   that writes the depth and whose identity moved apart from the camera in the frame before (its own previous x
   inverse(current) not the camera's), or that is skinned (its vertices carry blend indices: the translated shaders'
   input 9), gets a number, 1 to 127 (`upscale::SceneDraw`, `gpu::DrawDesc::object`). An object that starts to move is
   found one frame late. At the composite each number gets its draw's own reprojection (the camera's when the draw has
   no pair in the frame before); a still skinned character gets the camera's.
2. **The object mask** (`gpu::detail::ReplayObjects`, `gpu/draw.cpp`): at the upscale, before the inputs pass, every
   numbered draw is drawn again with its own vertex shader, vertices and constants (the frame's uploads, jitter
   included), our pixel shader (`ObjectMaskPS`: the blend writes the blend factor, number / 255) into an R8 mask, the
   scene's depth tested "or equal" and not written: the mask holds the visible object at each pixel. One render pass,
   in FSR's own command list. A replay's pipeline is built on a thread of its own the first time; until then that draw
   is left out (the camera's motion, as before), never waited for.
3. **The inputs pass** (`InputsCS`, `InputsLayersCS`): a pixel of object n reprojects its depth with object n's matrix
   (a constant buffer of 128 matrices) instead of the camera's; Swap Force's motion layers keep their choice
   elsewhere. FSR's own disocclusion test then drops the history where the object was.
4. **The reactive mask** (FSR's `reactive` input): 0.9 at a pixel where an object was in the frame before, all around
   the place its motion takes it to, and none is now, nor beside it (one pixel's margin for the jitter): the trail of
   what an object's own matrix does not describe (a skinned character's limbs; SSA, whose draws carry the camera alone).
   The two masks swap each frame.

Lux (SSA) skins in world space: its draws' c0-c3 is the camera's alone, so no SSA object moves by its matrix; about a
tenth of its draws are skinned (~110 objects a frame, under the 127). Giants: 20-80 objects a frame (the hero, enemies,
props, birds, leaves, the windmill). The log says every 900 frames `[fsr] last 900 frames: N moving objects a frame,
N drawn into the object mask`.

**Measured** (Giants, Chapter 1, M1 Pro, Metal, 2560x1440, FSR quality, frame generation on, fps limit 30; the
scripted run below, the stick held 3.5 s in each direction with A, 90 s; `SKY_FSR_TRAIL=3`: every third frame, the mean
absolute difference, 0-255, between FSR's output and its input at each input pixel where an object was the frame before
and no longer is, "trail", on the objects, and elsewhere; the second half of the run, the play; two runs each) [V]:

| | trail | on the objects | elsewhere |
|---|---|---|---|
| the camera's motion (`SKY_FSR_OBJECTS=camera`: the same mask, every object given the camera's motion) | 10.0, 10.2 | 11.9, 11.9 | 2.42 |
| own motion, no reactive mask (`SKY_FSR_REACTIVE=0`) | 8.2, 8.5 | 10.5, 10.5 | 2.42 |
| own motion and the reactive mask (the default) | 7.8, 6.3 | 10.4, 10.2 | 2.42 |

Looked at (crops of the output, the same moments of the two runs): the intro's windmill blades and birds no longer
ghost into the sky; the hero behind the fence keeps a clean face and body while it moves; flying debris stays solid
(was an orange smear). A fast attack's effects still smear (particles write no depth: not objects). SSA (level_027,
the same measure): no measurable change (its trail region is ~100 pixels a frame) [?].

**Cost** (`SKY_FSR_TIMING=1`: the mask's pass and the inputs pass each sent and waited for alone, an empty wait taken
off; Giants at 1440p, 32-77 objects a frame): the mask's pass 0.41 ms against 0.36-0.39 ms with no object
(`SKY_FSR_OBJECTS=0`), so its draws cost about 0.05 ms; the inputs pass the same with or without (0.33-0.39 ms, most of
it the wait's own cost in that mode). In the frame both are recorded in FSR's list: no extra command list. The CPU adds
one replay record per object (a copy of the draw's bindings). FSR off: nothing changes (no number is given).

Test aids: `SKY_FSR_OBJECTS=0` (no objects, the camera's motion alone, as before 0.9.6), `=camera` (the mask made,
the camera's motion given: the measurement's "before"); `SKY_FSR_REACTIVE=<0-1>` (the reactive value, 0.9; 0 none);
`SKY_FSR_TRAIL=<n>` (the measure above, logged as `[fsr] trail: ...` with the run's means; slow, it waits for the GPU)
with `SKY_FSR_TRAIL_DUMP=<prefix>` and `SKY_FSR_TRAIL_DUMP_FROM=<n>` (8 measured frames as PPM: the input with the
mask in red and the frame before's in blue, the output, the mask); `SKY_FSR_TIMING=1` (the cost above).

**Shader caches:** the bias changes every translated pixel shader, so a game's cache must be translated again
(`tools/shaders/build_cache.sh`; a player's install does it, Arkeyan-Builder's `shaders` step). With an older cache FSR
works without the bias (textures at the scene's mip levels).

## The backend (gpu/fsr/)

| File | What |
|---|---|
| `fsr.cmake` | the SDK's host code as `ArkeyanFfx` (built with `-fshort-wchar` and `ffx_port.h`: the SDK is written for Windows' two-byte `wchar_t`, its opaque context is sized for it; outside Windows the C++ library's `std::wstring` is four-byte, so the optical flow's pass names take `ffx_port.h`'s own two-byte string (`FFX_PORT_WSTRING`): the 0.9.4 candidate's kit, built at -O2, faulted on the first generated frame, and `make_kit.sh` now refuses a kit whose `libArkeyanFfx.a` calls those strings [V]); the shaders of the one permutation used (SDR, motion at render size, depth not inverted, no LUT, no FP16, wave 32; plus the accumulate with sharpening) |
| `backend.cpp` | the SDK's `FfxInterface` over plume: resources are `gpu::Texture`s (a view per mip for the luma pyramid's UAVs), pipelines are plume compute pipelines with one descriptor set, jobs are recorded into the frame's command list with layout transitions and a barrier per pass; constant buffers per pass instance and frame in flight (3) |
| `spirv_reflect.cpp` | reads each shader's names, kinds and bindings from its SPIR-V (`ArkeyanUpscaleTest` checks it) |
| `msl_table.cmake` | reads the argument buffer the Metal shader expects from its MSL (plume binds set 0 at `[[buffer(0)]]`, binding n at `[[id(n)]]`) |
| `fsr.cpp` | FSR's context (made again when the sizes change, after the GPU is idle), the inputs pass, the dispatch, and the three textures FSR 3.1 wants from the application (`ffxFsr3UpscalerGetSharedResourceDescriptions`: dilated depth, dilated motion, reconstructed previous depth; without them its passes write nowhere and the picture smears: the first runs' bug) |
| `fsr_inputs.hlsl` / `.metal` | the inputs pass and the clears of FSR's textures (a uint target takes the value's bits); `ObjectMaskPS`, the object mask's pixel shader (`ps_6_0`; Metal: a fragment function in the same library) |

**Shaders per backend:** the SDK's HLSL (`sdk/src/backends/dx12/shaders/fsr3upscaler`) compiled by our dxc
(`thirdparty/XenosRecomp/thirdparty/dxc-bin`): DXIL for D3D12, SPIR-V for Vulkan (`-fvk-*-shift`: b0, s8, t16, u48, one
number space per set as plume wants). Metal: the SPIR-V converted to MSL by MoltenVK's converter
(`MoltenVKShaderConverter -mab`, Homebrew's `molten-vk`, on the build machine only), compiled to metallibs (Metal 3,
macOS 13). D3D12 binds each register class in its own numbers (the register itself), Vulkan the shifted binding, Metal
the argument buffer id.

**Vulkan's storage image formats** (2026-11-08): dxc gives each storage image the format of its HLSL type (`float2`:
`Rg32f`), not the view's (FSR's motion and dilated motion are RG16F, its locks R8): the validation layer reported every
pass's loads and stores as undefined (`Undefined-Value-StorageImage-FormatMismatch-ImageView`, 93 000 in a 130 s SSA
run). Our dxc has no `-fspv-use-unknown-image-format`, and the SDK's HLSL stays unmodified, so on Vulkan the backend
gives each FSR and frame generation module to plume with its storage images' formats `Unknown`
(`gpu::spirv::WithoutStorageFormats`: the two `*WithoutFormat` capabilities added; an image an atomic uses keeps its
format, as Vulkan requires; two image types made the same are merged, since SPIR-V forbids declaring one twice), when
the device has `shaderStorageImageReadWithoutFormat` and `WriteWithoutFormat` (desktop drivers and MoltenVK do; plume
enables every feature the device has; the log says `[fsr] storage images without a format: yes`). Our inputs pass
declares its views' formats (`[[vk::image_format]]`). Metal (MSL from the unchanged SPIR-V) and D3D12 are unchanged.
[V] on MoltenVK with the validation layer: none of these warnings left, no SPIR-V error, in the six games (below);
`ArkeyanUpscaleTest` checks the rewrite on a hand-made module.

**FSR has a command list of its own** (`gpu::Flush` before and after the dispatch): a list starts on the GPU after the
one before, so FSR runs after the frame's work so far and before the rest. Recorded in the frame's list, FSR raced Trap
Team's frame on Metal although every pass has its barrier: 8x8 tiles of garbage kept and spread by the history, whole
frames in red and yellow, in 2 to 10 of each run's 15 shots. A GPU wait after FSR removed it (0 of 15); a list ending
after FSR only, 1 of 32 shots; its own list, 0 of 15, then 0 of 32 in a second run. Which access raced is not known [?].
The extra lists cost about 0.3 ms of GPU time (`--fsr-bench` 1040x620 -> 2560x1440: 5.97 ms).

**Metal texture atomics:** the converter emulates `InterlockedMin`/`Max` on a texture with a buffer of the same rows
(`<name>_atomic`). FSR's two uint textures written with atomics (the reconstructed previous depth, the SPD counter) are
made on Metal as a linear R32Uint texture over a buffer (`CreateAtomicTexture`), the row alignment given to the shaders
as function constant 65535.

## How to verify

- Unit: `ArkeyanUpscaleTest` (jitter, camera vote and its objects, camera fit, presets and scales, setting names,
  SPIR-V reflection).
- Per-object motion: the measure of "Per-object motion" above (`SKY_FSR_TRAIL=3`, a run with `SKY_FSR_OBJECTS=camera`
  against one without); the log's `[fsr] last 900 frames: N moving objects a frame, N drawn into the object mask`
  (drawn below numbered: replay pipelines still being built, or objects drawn into another depth target).
- GPU, no game: `ArkeyanRecompGpuTest` (with the isolation block of testing.md) runs FSR on the running backend: a still
  scene (mean channel error 1.15, max 5 on Metal) and a panned one with its camera motion (0.98) against the same pan
  without motion (6.45): a wrong binding, format or motion convention fails it. `ArkeyanRecompGpuTest --fsr-bench <w> <h>
  <output w> <output h>` gives FSR's cost per frame and fails when its still grey picture does not stay grey.
- One game: `SKY_UPSCALER=fsr3 SKY_WINDOW_SIZE=2560x1440` on an isolated run (testing.md); the log has
  `[upscale] fsr3 quality: the scene at scale 1, upscaled to the frame's scale 2`, `[fsr] FSR 3.1 upscaler 1000x600 ->
  2000x1200, SDR colour (formats 20 -> 20)` (plume's format numbers) and every 900 frames the camera vote's count, the
  depth's direction (and how often it changed) and the new histories (resets); never `[fsr] ... no longer upscaled`.
- Every game: `tools/regress/fsr_all.sh` (in `tools/test_all.sh`'s game sections).
- Live: the F3 menu's Upscaling row; the log has `[gamemenu] upscaler = fsr3`, then `[upscale] the render targets are
  made again at scale 1 (the frame at 2)`.

## Results (M1 Pro, macOS 26.6, offscreen 2560x1440, 2026-11-07) [V]

Shots and scripts: `private/fsr-runs/` (`NUMBERS.md`, `bin/`), runs `private/runs/fsr-*`.

| Game | Where | FSR (quality, 1440p) | Camera vote (of 900 frames) | Camera fit (near, far, fov) | Picture |
|---|---|---|---|---|---|
| SSA [Lux] | level_008; Level_021 | 1120x704 -> 2240x1408, SDR | 900 | 0.5, 600, 34 deg | clean |
| Giants | Chapter 1; level_203 (The Machine) | 1000x600 -> 2560x1440, SDR | 895-899 | 0.2, 610, 59 deg | clean; a fast-moving character trailed a little with the camera's motion alone: per-object motion since 0.9.6 |
| Swap Force | the canyon (tutorial); Boulder Brush | 1040x620 -> 2560x1440, SDR | 898 | 32, 1e5, 39-56 deg | clean |
| Trap Team | the tutorial (the opening, then play) | 1040x620 -> 2560x1440, SDR | 637-900 | 0.2, 250-760, 54-58 deg | clean since FSR has its own command list (above): 15 of 15 shots |
| SuperChargers | the tunnel ride, the Academy (route test) | 1040x620 -> 2560x1440, SDR | 853-884 | 32, 3.6e4-1e5, 43-59 deg | clean; route checks pass |
| Imaginators | to the hub (route test); level_508 | 1040x620 -> 2560x1440, SDR | 898-900 | 32, 3.6e4, 39-47 deg | clean; route checks pass |

**Sharpness, Giants** (mean |Laplacian| of the same view, a 440x520 crop of bark and leaves; the HUD corner): native
x1 1.08 (HUD 4.83), **FSR 2.55 (HUD 9.39)**, native x2 2.26 (HUD 9.13): the scene as sharp as x2 (with RCAS), the HUD
at full size.

**FSR's own GPU cost** (`--fsr-bench`, fp32): 1000x600 -> 2560x1440 4.4 ms, 1040x620 -> 2560x1440 5.3, 1120x704 ->
2240x1408 5.0, 2000x1200 -> 3840x2160 12.6, 1000x600 -> 3840x2160 9.6. FP16 shaders (FSR's own permutation) were
slower through the converted MSL (6.0 ms): not used.

**In game, Giants Chapter 1 at 1440p** (the performance log's `gpu_ms`, 508 frames of play, game slots shared with
other runs): native x1 6.53 ms (p90 9.12), **FSR quality 9.21 ms (p90 10.10)**, native x2 11.13 ms (p90 12.53). FSR
gives x2's sharpness about 2 ms cheaper than x2. At 2160p: native x3 13.56 ms (p90 14.28), FSR quality 19.25 ms
(p90 23.40): the scene at x2 plus FSR's 12.6 ms cost more than x3 on this M1 Pro, so at 4K FSR is for a GPU slower
than its native x3. At 30 fps every case holds 33.3 ms per frame.

**Backends:** `ArkeyanRecompGpuTest`'s FSR cases pass on Metal, on D3D12 (vkd3d-proton under Wine, Docker, lavapipe) and on
Vulkan (Windows build under Wine, lavapipe, validation layer on); Swap Force runs FSR on MoltenVK (`ARKEYAN_MACOS_VULKAN`).

**Off is unchanged:** Giants at scale 2, a frame every 20 presents of 32 s: 0.9.3 against 0.9.3 8 of 43 non-black
frames bit-identical, 0.9.3 against this build 8 of 38 (the rest differ by animation timing in both pairs), with the
shader cache retranslated (bias 0).

## Frame generation (FSR 3.1)

The owner (2026-11-07): frame generation in every game too, as demanding as the upscaling. One frame is made between two
real ones by the SDK's own **frame interpolation** and **optical flow** (`sdk/src/components/frameinterpolation`,
`opticalflow`, v1.1.4, unmodified) through our backend; the presentation, the HUD copy and the rules are ours
(`gpu/fsr/framegen.cpp`, `framegen_rules.h`). The SDK's DX12/Vulkan swap chain replacements are not used: they own the
present and its thread, which plume and the three backends already have; ours presents both frames through the existing
present pass, the same on Metal, D3D12 and Vulkan.

| Where | Key | Values |
|---|---|---|
| launcher.ini `[<game>.video]` (or `[video]`) | `fsr_frame_generation` | `off` (default), `on` |
| environment (tests) | `SKY_FSR_FRAME_GENERATION` | `off`, `on` (or `0`, `1`) |
| environment (tests) | `SKY_FSR_FRAME_GENERATION_PREVIEW=1` | runs it at all (0.9.4: held back for players, below) |
| F3 menu, Video | **Frame generation** (On, Off) | applied at the next frame, written to the game's section; greyed with the reason when FSR 3 does not upscale ("Needs Upscaling on FSR 3"), when the backend cannot ("Not available on this graphics card") or the game is not clean yet (`game::Adapter::frameGeneration`, "Not available in this game yet") |

**Held back for players (0.9.4, the owner's call: it does not render as it should yet).** Without
`SKY_FSR_FRAME_GENERATION_PREVIEW=1` it is off in every game whatever the setting (`framegen::HeldBack`,
`framegen_rules.h`, unit-tested in `ArkeyanFrameGenTest`; the log says `[framegen] asked, but it is coming later`): the F3 row
is greyed, shows Off and says "Coming later." / "Bientôt disponible.", the launcher's switch is greyed the same way. The
code is unchanged; `tools/regress/fsr_framegen.sh` sets the variable, so its runs are as before. To verify: a run with
`SKY_FSR_FRAME_GENERATION=on` and no preview logs the line above and no `[framegen] FSR 3.1 frame generation`.

```
frame N: scene draws -> FSR 3.1 upscale (dilated depth, dilated motion, reconstructed previous depth: shared)
         -> composite -> copy of the frame target (the picture without the HUD) -> HUD draws -> resolve -> Present(N)
PresentNow(N): optical flow (the HUD-less copy) -> frame interpolation (HUD-less N-1 and N, the upscaler's textures,
         the frame as presented for the HUD) -> present the generated frame; wait half a real frame; present frame N
```

- **Inputs**: the upscaler's own dilated depth and motion and its reconstructed previous depth (the textures FSR 3.1
  shares; motion at the render size, depth 0 near), its camera planes and field of view, the frame without its HUD (a
  copy of the frame target right after the composite draw, `gpu::framegen::CaptureHudless` from `d3d/render.cpp`), and
  the frame as presented (`Present`'s image). Frame interpolation needs the upscaler's inputs: it runs with
  `upscaler = fsr3` only (FSR's native 1:1 anti-aliasing at a 720p window included).
- **HUD and menus** (the SDK's HUD-less path): interpolation reads the HUD-less copies of the two real frames; where
  the presented frame differs from its HUD-less copy (the HUD, text, menus), the generated frame takes the presented
  frame's pixels: the HUD is drawn sharp on both frames, never interpolated into ghosts. The F3 menu (an overlay) is
  drawn on both.
- **Presentation and pacing** (`gpu.cpp`, `PresentGenerated`): the generated frame goes out when the real frame is
  ready, through the present pass with its own descriptor sets and acquire semaphore (one per frame in flight); the real
  frame goes out half the measured real interval later (`framegen::Pacing`, a moving average of about ten frames, held
  between 4 and 100 ms; the GPU thread's lock is free meanwhile). Shown frames: twice the real ones, evenly spaced, in
  order. The FPS counter counts the frames shown.
- **When no frame is made** (`framegen::Decide`): no scene this frame (movies, loading screens, the game's own front-end
  menus: `game::Adapter::menuLevels`), no HUD-less copy that matches the presented frame, the first frame after any of
  these or after frame generation was off (the interpolation's history starts again: the real frame alone), a camera
  cut (`framegen::CameraCut`: the camera's reprojection sends a point of the picture behind the camera or more than a
  quarter of the picture away; `game::Adapter::frameGenerationCutShift`, an eighth in Swap Force, whose in-game
  cutscenes orbit at 0.3-0.5 a frame; or the draws agreed on no camera motion: a cut changes the draws), and with VSync on a display that cannot show twice the real rate
  (`framegen::DisplayHasRoom`: a 60 FPS game on a 60 Hz display; 30 on 60 and 60 on 120 have room). The SDK's own
  scene-change test (optical flow) catches the cuts the camera does not show. The upscaler's reset flag is not used: it
  follows the swaps (a swap that presents nothing breaks its history), not the frames shown, and the interpolation's
  inputs hold on its reset too. Pause menus inside a level: the games keep drawing their scene behind them; the menu is
  HUD (taken from the real frame), so nothing smears.
- **Frame rate**: the games' own 30/60 pacing and `fps_limit` are unchanged (the real rate); the shown rate is twice
  it. VSync off: the real frame waits half the measured real interval after the generated one. VSync on: no wait, each
  present waits for its blank (the generated frame on one, the real on the next); frame generation runs only while the
  display has room for twice the real rate. When the GPU cannot interpolate in time (the frames shown bunch up: two 10 s
  windows with a spacing deviation above 5 ms, `framegen::UnevenGuard`), frame generation stops until the setting is
  switched on again, with `[framegen] the frames shown are uneven` in the log.
- **Latency**: the real frame is held half a real frame (16.7 ms at 30 FPS, 8.3 at 60) plus the interpolation's GPU
  work with VSync off, one blank more with VSync on (the generated frame takes the first); the log's `the real frame
  held N ms` measures it from the frame ready to the frame presented.

- **Scenes with motion layers** (`game::Adapter::sceneMotionLayers`, Swap Force): parts of the scene move with the
  camera while the world passes by (the tutorial's ship, Outlands' pirate ship, the Swap Zones' vehicles); the camera
  vote's winner changed from frame to frame between the ship and the world, and the generated frames smeared both. The
  vote keeps up to two more motions that a tenth of the draws' weight agrees on (`upscale::CameraVote::Others`); the
  inputs pass (`InputsLayersCS`: `fsr_layers.hlsl`, `fsr_inputs.metal`) picks, per 8x8 tile, the motion that finds the
  tile's colour in the previous frame (five taps per pixel, luminance; the camera's unless another is a quarter better),
  a pixel the camera's motion explains clearly better keeps it, and the sky (the far plane: an animated texture) keeps
  the camera's; a depth outside the scene's range (the HUD's portrait and its mask) gets no motion and the nearest
  depth. Per pixel, the choice made shards; the previous colour is a copy of the scene's (render size). Its push
  constants are 224 bytes (within D3D12's 256, above Vulkan's guaranteed 128): a device that cannot make the pipeline
  keeps the camera's motion alone, logged. `SKY_FSR_LAYERS=0` (test aid): the camera's motion alone.

**The backend's additions** (`gpu/fsr/backend.cpp`, `framegen.cmake`): the 11 frame interpolation passes and 7 optical
flow passes in one permutation each (motion at the render size, not jittered, depth not inverted, SDR, no FP16, wave
32), built like the upscaler's (DXIL, SPIR-V, MSL); structured buffers (interpolation's counters: SRV and UAV); pyramid
UAVs past a texture's last mip take its last mip (interpolation binds 12 on a smaller pyramid; a storage view of every mip
is invalid on Vulkan). The HUD-less copy and the optical flow and interpolation go in command lists of their own, as
FSR's do (Metal raced FSR's passes recorded with the frame's draws, Trap Team). Metal: a pipeline's descriptor ranges are added in the shader's argument buffer order, samplers
in place: plume's Metal set layout sorts its ranges by binding and takes a descriptor's kind from that sorted list by
the order they were added (the upscaler's samplers moved last only worked because textures and samplers share Metal's
encoding; a buffer after them was taken for a texture).

**Logs**: `[framegen] FSR 3.1 frame generation: render 1000x600, frame 2560x1440, optical flow 320x180` once; every 10
s `[framegen] last 10 s: 30.0 real fps, 60.0 shown; 300 generated; without one: ... (no scene) ... (reset) ... (no
HUD copy) ... (display: VSync at N Hz); shown frame spacing 16.67 ms mean, 0.11 ms deviation; the real frame held N
ms`. `SKY_FSR_FG_DEBUG=1`: the SDK's debug view in the generated frame; `2`: its tear and reset markers; `3`: each real
frame's decision in the log. `SKY_FSR_FG_SNAPSHOT=<prefix>` (`_EVERY`, `_FROM`, `_COUNT`): generated frames, the
real frames after them and the HUD-less copies as PNG (offscreen). Test knobs: `SKY_FSR_FG_FROM_SECONDS=<s>` (off, then
on: one run measures both), `SKY_FSR_FRAME_GENERATION_ANY_GAME=1` (also in a game that keeps it off),
`SKY_FSR_FRAME_GENERATION_D3D12=1` (D3D12), `SKY_FSR_FG_NO_PACING=1` (the bench).

### Results per game and backend (M1 Pro, macOS 26.6, offscreen 2560x1440, FSR quality, fps limit 30, VSync off, 2026-11-07)

Measured alone on the Mac (exclusive game lock), one run per game: frame generation off for the first part
(`SKY_FSR_FG_FROM_SECONDS`), then on; "frame" is the real frames' time (`[perf]`), "spacing" the frames shown
(`[framegen]`, 10 s windows after the first). Shots: `private/fg-runs/view/` (each a generated frame over the real one
after it). Runs: `private/fg-runs/keep/`.

| Game | Where | Off: real FPS, frame median / p95 | On: real FPS, frame p95 | Shown | Shown spacing | Real frame held | Generated frames |
|---|---|---|---|---|---|---|---|
| SSA [Lux] | level_027 | 30.0, 33.3 / 34.2 ms | 30.0, 33.5 ms | 59.9 | 16.7 ms ± 1.0-3.0 (the game's own uneven pacing) | 18.2 ms | clean; a dialog over the scene stays sharp [V] |
| Giants | Chapter 1 | 29.9, 33.3 / 33.4 | 30.0, 33.6 | 60.0 | 16.67 ± 0.11-0.39 | 17.2 | clean, HUD sharp; the hero's trail is the upscaler's (camera motion only) [V] |
| Swap Force | the tutorial (`fsr_framegen.sh`); six zones with the hero moving (`tutorial_b`'s Digger Swap Zone, `ww1`, `fg1`, `outlands01`, `finale01`, the Sheep Wreck pack) | - | 30.0, 33.3 | 60.0 (59.4-60.0) | 16.67-16.84 ± 0.7-1.7 | 17.2-17.3 | **on** since 2026-11-08 evening (`frameGeneration`, `sceneMotionLayers`, `frameGenerationCutShift` 0.25). With the camera's motion alone the tutorial's ship, Outlands' pirate ship and the Digger's Swap Zone smeared (two motions in the scene: the ship with the camera, the world passing by; the vote's winner changed between them): the motion layers above fixed them; the in-game cutscenes (the camera orbits at 0.3-0.5 a frame) and the camera's jumps (a collision) are cuts. Looked at: the ship's deck, masts and cargo clean, the HUD sharp, the portrait's mask no longer leaks; a character or enemy close to the camera in a fast fight leaves ghosts, with the motion layers on or off (the camera's motion is not theirs), worse under load; a little stepping where the ship's edge meets the passing world [V] |
| Trap Team | level_314_tutorial_2 | 30.0, 33.3 / 34.6 | 30.0, 33.5 | 60.0 | 16.67 ± 0.12-0.16 | 17.2 | clean [V] |
| SuperChargers | the intro's tunnel ride (to the Academy) | 30.0, 33.3 / 33.4 | 30.0, 33.4 | 60.0 | 16.67 ± 0.09 | 17.1 | clean [V] |
| Imaginators | hub_2016 | 30.0, 33.3 / 33.4 | 30.0, 33.5 | 60.0 | 16.67 ± 0.11-0.15 | 17.1 | clean [V] |

**Frame generation's GPU cost** (`ArkeyanRecompGpuTest --framegen-bench`, `SKY_FSR_FG_NO_PACING=1`, alone in the game
lock but with a compile running): optical flow, interpolation and the extra present, over FSR's upscale alone: 1000x600
-> 2560x1440 9.0 ms (FSR 4.0), 1040x620 -> 2560x1440 9.0, 1120x704 -> 2560x1440 9.1, 2000x1200 -> 3840x2160 20.0,
1280x720 -> 1280x720 3.6. On this M1 Pro a 30 FPS game has the room (33 ms a frame); a 60 FPS game at 1440p may lose
real frames to it (below).

Two more cases, measured alone: **VSync on** (offscreen, its 60 Hz stand-in; Giants): 60.0 shown, spacing 16.67 ms ±
0.17-0.33, real frame held 17.5 ms. **A 60 FPS game** (SuperChargers, fps limit 60, VSync off): with frame generation
the real rate fell to 56.6 and the frames shown bunched (8.8 ms ± 6.8): the uneven guard stopped it after two windows
and the game went back to 60.0 real. On this Mac frame generation is for the 30 FPS games (or 60 on a faster GPU).

The real rate does not drop with frame generation (30.0 either way); frame generation off shows the real frames 33.3
ms apart, on 16.67 ms apart with a spread of 0.1-0.4 ms (SSA's frames themselves vary by up to 1 ms). The real frame
goes out 17.1-18.2 ms after it is ready: half a real frame (16.7) plus the interpolation's work. The FPS counter shows
60.

| Backend | Status |
|---|---|
| Metal | [V] five games above; `ArkeyanRecompGpuTest`'s `framegen` case passes |
| Vulkan (MoltenVK, `ARKEYAN_MACOS_VULKAN`) | [V] SSA, Giants, Trap Team, SuperChargers, Imaginators (below); Swap Force off as on Metal |
| Vulkan (Windows build under Wine, Mesa lavapipe) | [?] the upscaler's cases pass; frame interpolation's main pass makes lavapipe (Mesa 25.0.7, under Rosetta) fault, even on the reset frame, which reads four plain descriptors: not provable here |
| D3D12 (vkd3d-proton on lavapipe) | [?] the same lavapipe fault; **off for players** (`gpu::framegen::Available()` false on DXIL) until a Windows PC proves it: `SKY_FSR_FRAME_GENERATION_D3D12=1` lets a tester try |

**Vulkan, game by game** (2026-11-08, MoltenVK, `-DARKEYAN_MACOS_VULKAN=ON -DARKEYAN_VULKAN_VALIDATION=ON`, one game at a time,
the Metal table's scenes, 2560x1440, FSR quality, fps limit 30, VSync off; shared game slot, not exclusive). Each game
twice: with the Khronos validation layer (errors, faults), then a second start without it
(`VK_LOADER_LAYERS_DISABLE=~all~`: the loader finds Homebrew's layer by itself otherwise) for the rates. The layer costs
so much CPU that the real rate falls to 8-20 fps and the uneven guard stops frame generation in SSA and Imaginators:
those runs prove the errors only.

| Game | Validation (frame generation on) | Without the layer: real fps, shown, spacing, real frame held | Generated frame |
|---|---|---|---|
| SSA | no error, no fault, no `[guard]` | 28.6, 60.0, 16.67 ms ± 1.3, 18.5 ms | dialog sharp, no ghost [V] |
| Giants | the same | 29.1, 59.7, 16.76 ± 0.8, 17.4 | HUD sharp [V] |
| Swap Force | the same (2026-11-08 evening, the motion layers on: `InputsLayersCS` from `fsr_layers.hlsl`) | tutorial 29.9-30.0, 58.9-60.0, 16.67-16.95 ± 0.4-3.2, 17.4-17.6 | as on Metal [V] |
| Trap Team | the same, and 60 shown / 30 real with the layer on | 28.5, 58.9, 16.67 ± 0.4, 17.5 | HUD sharp; the hero's blur as on Metal [V] |
| SuperChargers | the same | 28.7, 59.9, 16.70 ± 0.7, 17.4 | HUD and banner sharp [V] |
| Imaginators | the same | 28.9, 58.4, 16.71 ± 1.3, 17.6 | HUD sharp [V] |

"No error" leaves the two MoltenVK reports this page's Vulkan runs always show (`plume_backends.md`): the 16384-descriptor
heaps' `VUID-vkCreateDescriptorSetLayout-support-09582` and the depth-only passes' `ShaderOutputNotConsumed` warning.
Some 10 s windows run at 25 real fps (the game's own hitches, and Imaginators' GPU thread 90 % busy on MoltenVK): the
shown rate stays twice the real one there. **A first start on MoltenVK** builds each pipeline when first drawn (50-220 ms
each, `plume_backends.md`): in SuperChargers' cold run the frames shown bunched (± 7-9 ms) and the uneven guard turned
frame generation off as designed; the second start (`pipelines.bin` loaded) never did. A real Vulkan driver builds them
far faster. One SuperChargers start (the first of a new build, with the layer) ended 1.5 s in without a word; three later
starts, cold and warm, did not [?]. **SSA on Vulkan at 2560x1440 shows yellow-green blocks on the grass** in the real
frames, with the upscaler off too: not frame generation's, nor the scale's: the sun tile classes came late (solved
2026-11-11, [plume_backends.md](plume_backends.md)).

**Off is unchanged**: `ArkeyanRecompGpuTest`'s written frames with frame generation off (the present cases and the four
FSR cases) are byte-identical between 52c60e7 (lane F) and this branch; with the setting off, no frame generation code
runs in a frame (no copy after the composite, no extra present, `PresentNow` as before).

### How to verify frame generation

- Unit: `ArkeyanFrameGenTest` (the rules: off, no scene, no HUD copy, reset, camera cuts, the display's room; the pacing:
  30 and 60 real FPS, a hitch, a pause, the shown frames' spacing), `ArkeyanSettingsTest` (`fsr_frame_generation`).
- GPU, no game: `ArkeyanRecompGpuTest`'s `framegen` case: a panned scene upscaled 480x270 -> 960x540 with its motion and a
  white box drawn after the HUD-less copy; the generated frame must be nearer the halfway scene than either real frame
  and the box exact. `ArkeyanRecompGpuTest --framegen-bench <w> <h> <output w> <output h>` (`SKY_FSR_FG_NO_PACING=1`):
  frame generation's cost per frame.
- Every game: `tools/regress/fsr_framegen.sh` (in `tools/test_all.sh`'s game sections): its contexts made, no failure,
  no `[guard]`, frames written, the shown rate twice the real one in the busiest 10 s (a game
  whose adapter keeps it off, none since 0.9.4: it says so and never runs); Giants also live: off at the start, the F3 row turns it on at 45 s and off at 75 s (both logged and
  written, frames generated in between). With `SKY_FSR_QUALITY=native` it also checks the `[upscale] fsr3 native` line
  has the scene at the frame's scale. [V] 2026-11-08, Metal, 2560x1440: Giants scene x2, 30.0 real / 60.0 shown; Swap
  Force scene x2, 24.1 real / 48.1 shown (a second game ran at the same time); the four other presets unchanged
  (`ArkeyanUpscaleTest`: their ratios, values and scales).
- Vulkan on this Mac: a MoltenVK build of the game (`-DARKEYAN_MACOS_VULKAN=ON -DARKEYAN_VULKAN_VALIDATION=ON`, its own build
  folder, about 100-140 MB), run like `tools/regress/vulkan_validation.sh` (Homebrew's loader, layer and MoltenVK paths,
  no `nice`) with `SKY_UPSCALER=fsr3 SKY_FSR_FRAME_GENERATION=on`: the log has `[fsr] storage images without a format:
  yes`, no `Validation Error` but `-support-09582`, no `[guard]`; then once more with `VK_LOADER_LAYERS_DISABLE=~all~`
  for the `[framegen] last 10 s` rates (a second start: the first one builds the pipelines).
- A player's check on a real Windows PC and a Linux PC (not provable on this Mac): `upscaler = fsr3`,
  `fsr_frame_generation = on`, a 30 FPS game on a 60 Hz display (or 60 on 120): the FPS counter shows twice the
  rate; turning the camera, no tearing, no doubled HUD, no stutter; the F3 row switches it at the next frame.

## Open

- Per-object motion (0.9.6): a skinned character moves by its draw's matrix, not its bones (the limbs rely on the
  reactive mask); SSA's draws carry the camera alone, so SSA gets the reactive mask only; particles and other draws
  that write no depth keep the camera's motion; more than 127 objects in a frame: the rest keep it too. The 1440p
  measure is Metal only. Vulkan (MoltenVK, SSA level_027 at 1440p): 54 objects a frame, 52 drawn into the mask, the
  picture clean, no fault, no `[guard]` [V]; D3D12 not run (the mask's shader is built as DXIL `ps_6_0`) [?].
  Other games (Metal, `fsr_all.sh`): Giants and Swap Force pass; Imaginators reached play with 21.9 objects a frame,
  20.9 drawn, the camera found in 896 of 900 frames, no `[guard]` (its route was cut by the session's time limit);
  Trap Team and SuperChargers not run (their routes waited for a game slot) [?].
  `ArkeyanRecompGpuTest`'s FSR cases (which run the new inputs pass with an empty mask) pass on Metal with the same
  numbers as before (still 1.15, pan 0.98, no motion 6.45, HDR 1.04, frame generation 0.49); beside a game's run it
  stalled three times (once in the HDR case, twice before any FSR case), alone it passed 5 of 5 and in SSA's and
  Giants' unit tests; on MoltenVK it faults at start in `SDL_Vulkan_GetInstanceExtensions` (no window offscreen),
  before any case [?]. Frame generation interpolates
  with the same motion; its own ghosts in a close fight (below) were not measured again.
- Trap Team: the race FSR's own command list avoids is not understood (Metal; D3D12 not tried in Trap Team; on
  MoltenVK, with its own list, two runs and the one shot looked at clean); its camera fit jumps for a few frames to
  fov 130-140 deg (another perspective draw outweighs the scene's).
- Frame generation: results per game and backend in "Frame generation" above.
- Per-object motion for characters: a fight close to the camera (Swap Force's `fg1`) left ghosts in the generated
  frames, the motion layers on or off; the object mask (0.9.6) now gives those pixels their draws' own motion; not
  looked at again in `fg1` [?].
