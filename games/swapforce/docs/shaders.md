# Swap Force's shaders: the disc and the cache

> **Purpose:** where Swap Force's shaders are on the disc and how the kit builds the cache · **Status:** current (2026-10-30) · **Tags:** [Xbox] [XDK] [Alchemy] [SwapForce]

## On the disc

### Where the shaders are [SwapForce] [V]
**All of them are in one file: `/archives/shaders_xenon.pak`** (592025 bytes, IGA v10 big-endian, 62 entries): 61 IGZ v7
files, one per shader family (`shaders\characteroutdoor.igz`, `ubershaderoutdoor.igz`, `water.igz`, `fxaa.igz`,
`tiledlight_<n>p.igz`...), plus one `shaders_xenon.pkgb` (a package list, no shader). They hold **1074 distinct
containers (580 vertex, 494 pixel)**. None of the other 2230 `.pak` files of the disc has any (all scanned,
2026-10-27). The game opens it at start, before `permanent_xenon.pak`; like Giants, Alchemy builds the shader objects
itself and calls `D3DVertexShader_Init`/`D3DPixelShader_Init` (both hooked: `games/swapforce/addresses.h`).

Not in the archive: **7 containers compiled into `default.xex`** (their physical parts are in the loaded image, their
virtual parts are assembled at run time, as Giants' Bink ones), named by the debug-database paths next to them:
- 5 from Alchemy's own movie module, `movie/binkMovie` (`binkVertexShader`, `bink1PixelShader`, `bink2PixelShader`,
  `bink1AlphaPixelShader`, `bink2AlphaPixelShader`): `2C7D434B8344C7F7.vs`, `4D79576E97BF424C.vs`,
  `2BE3ED3FB609BD71.ps`, `515F525572999F76.ps`, `811CCF44C1A9FC42.ps` (dump names);
- 2 from Alchemy's `gfx` defaults (`defaultVertexShader`, `defaultPixelShader`): `8D91FA8A6A2C9429.vs`,
  `73A2EEDD9631994F.ps`.

So Swap Force has 7 run-time shaders where Giants has 50 (Giants' come from the Bink 2 library itself) [V for the
dump; whether a movie of another kind adds more is [?]].

### Format facts [Alchemy] [SwapForce] [V]
IGA v10 and the IGZ v7 header: [art.md](art.md#formats-alchemy-swapforce-v). What the shaders add:
- IGZ v7 sections (at 0x14, `{u32 ?, offset, size, alignment}`): section 0 the tagged records; the object pools; the
  last one, 0x20-aligned, the **physical memory** (the microcode).
- The record `TMHN` (stored `NHMT`) lists the memory handles, 8 bytes each: `{u32 kind<<24 | size, u32 pool<<27 |
  offset}`, `pool` n being section n + 1 (v6 put the pool in the top 8 bits). Kind 0x18 = physical memory, as in v6.
- A container's virtual part (`102A1100` pixel / `102A1101` vertex, virtual size, physical size) sits in an object
  pool and its physical part in the physical pool: **not next to each other** (Giants' v6 test "within 0x40 bytes
  after" does not apply). The containers, in file order, and the 0x18 handles, in table order, go together: each
  container takes the next handle of its physical size. On the disc every one of the 1074 finds its handle with the
  same size, in step, and the 549 that a dump also has are byte-identical to it.
- What the game hashes is again `virtual part + physical part`, so the extractor's files are XenosRecomp's input as is.

## How we extract it

### From the disc (the kit's path)
- **Python** (developers): `python3 -I tools/shaders/swapforce_extract.py <iso> private/swapforce/shaders/disc
  [<dump dir>]`: reads `shaders_xenon.pak` (1 s; `ARCHIVES=all` scans all 2231 `.pak` files, 70 s with 4 processes),
  writes `<sha1 prefix>.vs|.ps`, `manifest.txt` and, with a dump dir, `missing.txt`.
- **Arkeyan-Builder** (C++, the player kit): `shaders=igz:/archives/shaders_xenon.pak` in `games/swapforce/game.txt`.
  `tools/arkeyan-builder/igz_shaders.cpp` reads IGZ v7 too (`ForEachContainerV7`), through the runtime's IGA reader, which
  reads v10 since 2026-10-27 (`runtime/host/art/alchemy_archive.cpp`). It writes the same 1074 containers as the Python
  tool (compared by content). Then, as for Giants, the short headless run dumps what the game builds at run time and
  the cache is rebuilt ([kit_contract.md](../../../docs/release/launcher/kit_contract.md)). The two `gfx` defaults come
  at +0 s, four of the five Bink ones with the start-up logos at +1-3 s (`2C7D434B8344C7F7.vs`, `4D79576E97BF424C.vs`,
  `2BE3ED3FB609BD71.ps`, `515F525572999F76.ps`), and **`811CCF44C1A9FC42.ps`** only on the tutorial's loading screen
  (`[render] no translated pixel shader 811CCF44C1A9FC42 (primitive 6)` right after `open \archives\tutorial_a.pak`
  and `fmv01`), after the menus.
- **The movie pass reaches it** [V] (2026-10-27): `games/swapforce/game.txt` gives Arkeyan-Builder `movie_portal=emulated`
  (an empty emulated portal), `movie_press=` (A every 2 s from 10 to 110 s: the title, Story Mode, the first NEW slot,
  the difficulty) and `movie_shaders=7` (the run ends once 7 new shaders are dumped, at the latest after 120 s), so
  the installed game has **1081** shaders. Until then the pass ran with `SKY_PORTAL=none` and stopped 8 s after the
  first new shader (1080 shaders): with no portal, Swap Force stops after the title on its own **"PORTAL
  DISCONNECTED"** screen (two `SL_LOAD_SLOTS Thread` lines, then nothing; [history.md](history.md), "Moved from the status page"),
  so presses alone could not reach the loading screen. Headless with the empty emulated portal, the installed game
  dumps `811CCF44C1A9FC42.ps` ~34 s in at a load of 4. If a slow machine does not get there in 120 s, the install
  still ends with the shaders found (a `warning: 7 expected` line) and only that screen's draws are skipped.
- The cache: `SKY_GAME=swapforce SKY_SHADER_OUT=<dir> tools/shaders/build_cache.sh <iso> <containers dir>` writes
  `<dir>/shader_cache.cpp` (input in `<dir>/input`) without touching `private/swapforce/ppc/`. From the 1074 disc
  containers and the 7 dump-only ones: **1081 of 1081 translated, 0 without DXIL** (5 min on an M1 Pro), and the 556
  hashes of the dump cache are all in it.

### Coverage
Against the 556 containers of the run-time dumps (title, menus, tutorial level):

| | |
|---|---|
| found on the disc | **1074** (580 vertex, 494 pixel) |
| of the dump, on the disc (byte-identical) | **549** |
| in the dump, not on the disc | **7** (compiled into the XEX, above) |
| on the disc, never dumped | 525 (levels and effects the runs did not reach) |

### From a run-time dump (the first cache, still useful to find run-time shaders)
As Giants' first cache [V]: `SKY_DUMP_SHADERS=<dir>` on a run through the title and the tutorial level
(`private/swapforce/run.sh`, A presses, `SKY_PORTAL_PLACE=new:9`), then `SKY_GAME=swapforce tools/shaders/build_cache.sh
<disc .iso> <dump dir>` writes `private/swapforce/ppc/shader_cache.cpp`: two runs gave **556 containers, 556 of 556
translated** (~1 min on an M1 Pro); a third run with that cache dumped no new one, and every draw of the title, menus
and tutorial has its shader (`skipped 0 (shader)`).

19 of the 556 use **unnormalized texture coordinates** (the deferred composite, the shadow-mask blur, FXAA): the cache must come from a XenosRecomp with `tfetch2DDenorm` (2026-10-27, [shaders.md](../../../docs/runtime/gpu/shaders.md)); an older cache draws the lit scene black.

## The post-processing passes and the blur [SwapForce] [Alchemy]

### The user's report and the cause (2026-10-27) [V]
"A kind of general blur over the whole game" and "rendering problems with the lava at the start". **Both came from the
`anisotropy` setting** (the launcher's "Texture filtering", `--anisotropy`) above 1: the runtime gave that anisotropy to
every sampler with linear filters, also to textures without mipmaps, i.e. the render targets the post passes read at
computed coordinates and the colour tables. The tutorial at `--anisotropy 16` showed the whole 3D view smeared, the lava
sky with green speckled contours and Eruptor green; with the fix (anisotropy only for textures with mipmaps,
`SamplerFor` in `runtime/d3d/render.cpp`, [textures_formats.md](../../../docs/runtime/gpu/textures_formats.md)) the same
moment is sharp and orange, and the ground keeps the anisotropic filtering (sand ripples stay sharp in the distance in
`barrens/brn1`). The game itself never asks for anisotropy: the aniso field of its fetch constants is 0 in every draw
of a tutorial frame. Which taps did the damage is inferred from the shaders [?]: the motion blur (`3DF3E533`) samples the
scene at 7 jittered points and the tone map looks up a 256x16 colour table, both without mips. [Xbox] (the same code
served SSA and Giants: any colour ramp they read is fixed the same way.)

Shots (untracked, `private/sfblur/shots/`): `tutorial_t60_aniso16_before.png` / `_after.png` (and `t80`),
`brn1_crop_aniso16_before.png` (Eruptor green) / `_after.png`, and at anisotropy 1 `tutorial_t60_aniso1_main.png`,
`brn1_crop_aniso1.png`. **Check it**: the tutorial recipe of [testing.md](../../../docs/getting-started/testing.md) with
`--anisotropy 16` and `SKY_GPU_SNAPSHOT_SECONDS=60,80`: no green, no smear.

### The chain at the end of a level frame (tutorial, `cap` of 2026-10-27, scale 2)
Passes 14-16 of [boot.md](boot.md#the-tutorials-level-frame-the-pass-list-2026-10-27-swapforce-alchemy-v), by pixel
shader (first 8 hex digits of the hash), with the constants the game sets:

| Pass | Pixel shader | What it does |
|---|---|---|
| Bloom | `20FCED23`, `9F067E57`, `EAC5877F` | bright pass and a separable blur, at 520x310 and again at 260x155 (mean value ~3/255 in the tutorial: no haze) |
| Bloom composite | `9B8EE4E4` | scene + 260x155 x 2.467 + 520x310 x 2.051 (c0); writes the **depth of field weight to alpha**: `saturate(abs(linear depth) x 1/3500 - 0.857)` (c1), 0 up to ~3000 units, 1 beyond ~6500 |
| Lava layers | `45500B15` | translucent lava sheets (a 512x128 DXT5 with mips), blended over the scene, depth tested |
| Depth of field | `786D32D7`, `90459BAF`, then `F6354FA3` | the scene halved to 520x620 then 520x310 (taps weighted by their alpha), then `scene + (blurred - scene) x scene.alpha`: only the far background is blurred, by design |
| Heat distortion | `B974E1A3` | particles that read the scene shifted by a DXN normal map |
| Motion blur and tone map | `3DF3E533` (+ `E441188B` under the character's stencil) | 7 taps along a per-object velocity, then the 256x16 colour table. The velocity is in the alpha of two G-buffer targets, packed by pass 6 (`1D4DD81F`: RT0 = normal.a, third.a, depth in z/w; the sky gets 0). Standing still in `brn1`, 98.4 % of the pixels have zero velocity; on the tutorial's flying ship the background streams radially, by design |
| FXAA | `123B15C5` | FXAA 3.11, Xbox 360 variant: c0 = +-0.5/size, c1 = (8, 8, -4, -4)/size, sharpness 8, threshold 0.125, minimum 0.05, luma in alpha (written by the tone map). It changes 1.7 % of the pixels (edges) |
| Frame | `124F67B6` | the 1040x620 scene stretched to 1280x720 with bilinear filtering, then the HUD |

**At anisotropy 1 every pass checked is faithful** [V]: the depth of field's composite copies the scene exactly where its
weight is 0 (error 0.00 over the deck); the unnormalized fetches land on texel centres at scale 2 (a shift of half a
guest texel, as on the console, not a blur); no half-resolution pass is stretched by mistake (no 2x2 blocks in the
G-buffer or the scene). What softness is left is the game's own: the 3D at 1040x620 stretched x1.23 / x1.16 to the
frame (at scale 2 on a 2560x1440 window: 2080 scene pixels over 2560), FXAA, the depth of field on the far background,
the motion blur while the ship flies. Since 2026-10-27 the scene is rendered at the frame's size by default (no stretch; the console's
1040x620 with `original_resolution=1`: [boot.md](boot.md#the-scene-at-full-resolution-2026-10-27-alchemy-swapforce-v)).
The table above is the 1040x620 frame; at full resolution every size is the same fraction of 1280x720 and the game
computes the constants from the passes.

### The lava's smear and the soft backgrounds (2026-10-27) [V]
The user's recording (tutorial, `main` at 1040x620): "the lava bugs a lot", smeared like a strong motion blur; the
Skylander menu's clouds and the backdrops look soft. Checked on frame captures (`SKY_DUMP_VS_CONSTANTS`,
[debugging.md](../../../docs/porting/debugging.md)); nothing of ours was found:
- **The velocity is the game's own, per vertex.** The G-buffer materials (vertex shaders `2EED9EE6`, skinned
  `359FC863`) project each vertex with this frame's matrices and with the previous frame's (static meshes: previous
  world-view c14-c17 and projection c18-c21; skinned: previous bones from c90, previous world-view c165-c168), scale
  the difference by c22 / c173 = (-224, 126), clamp it to 7 and write it to two G-buffer alphas; pass 6 (`1D4DD81F`)
  packs them, zero only where nothing was drawn. The depth range does not enter (positions only). In the captured
  frame the previous matrices are the current ones moved by the ship's motion (the deck: 0.1 unit; the bones: under
  1 unit), so the far canyon walls stream radially because the ship flies forward through them: the game's motion
  blur, by design. [?] Whether the console blurs as much cannot be told without a console capture.
- **The backgrounds are the disc's textures.** The pause and Skylander menus draw 256x512, 512x512, 256x144 and
  512x1080 textures without mipmaps, base level, linear filter, no LOD bias, stretched over the screen; the canyon
  walls use 256x256 and 512x512 DXT1 textures with mipmaps (no bias; at scale 2 we pick finer mips than the console),
  then the depth of field and the motion blur soften them. A higher-resolution look needs texture packs
  ([texture_packs.md](../../../docs/runtime/gpu/texture_packs.md)) or less post-processing (a product choice).

### The lava and the crystals flicker (2026-10-28): frame pacing, not a draw [SwapForce] [Alchemy] [V]
**Corrected 2026-10-29:** the pacing made it worse, but the shimmer itself is a draw state, the missing polygon offset
([below](#the-lava-shimmers-cause-the-polygon-offset-2026-10-29-swapforce-alchemy-xbox-v)). Every run compared here
lacked it, so "no excess of per-pixel flicker" and "at 30 fps no flicker beyond the scene's own motion" were wrong.
The user's recording (kit 0.6.0, macOS, game window 1392x840): "the lava is broken, the crystal-like things too, the
picture is epileptic, the lava sparkles at the start". The recording has 44-48 distinct frames a second, so the game ran
at `fps_limit=60` below 60: each frame was shown 1, 2 or 3 refreshes while the scene moved by the same step from one frame
to the next (correlation of step and display time 0.0 over 10 s). That judder, on the two fastest scenes of the
tutorial (the ship flies through the lava canyon, then through a tunnel of cyan shards at ~265 units a frame), is the
"epileptic" picture. The fix is in the runtime's pacing: at 60 the frames are shown every 2 blanks while they miss their
blank ([video_vblank.md](../../../docs/runtime/xbox360/video_vblank.md#pacing-waitforflippresentinterval-in-the-swap-hook)).
Headless at the user's settings the present intervals went from median 38.9 ms (std 7.2) to 33.3 ms (std 3.9).

What was checked and found right, frame by frame (`SKY_DUMP_FRAME` with `SKY_DUMP_FRAMES=<n>`, consecutive snapshots at
30 fps, where each present is one game step):
- **The canyon** (G-buffer, vertex shader `9183BAD7`, pixel shader `35F4F3BF`, 1024/2048 textures): its wobble is
  `sin(frequency x time + phase)` with the material time in c3.x, which advances by the measured frame time (one frame
  behind the present, as the render thread is); c3.y is the time two frames back. Its albedo changes little between
  frames; the flow overlays (`1A20887C` with `C3EFBD6E` / `614DDFBC`) have static texture matrices.
- **The lava sheets** (`45500B15`): the scroll is c8.x = time x 0.01, smooth. **The 256x256 ping-pong** (pass 2 of
  [boot.md](boot.md#the-tutorials-level-frame-the-pass-list-2026-10-27-swapforce-alchemy-v)) is a ripple simulation whose
  normal map feeds the overlays (texture 5); it is flat in the tutorial (127, 127, 255).
- **The tunnel and the swirl sky** are skinned meshes (`4292A997`, `E5509D27`, bones from c8 / c17) drawn in the forward
  pass, after the G-buffer: their bones change smoothly. They have **no motion blur by design**: the velocity the blur
  reads is packed in pass 6 from the G-buffer, where the sky is 0. So at 30 fps two frames in the tunnel differ by ~45 of
  255 on average, on the console as here [?] (no console capture).
- **The motion blur's inputs**: the previous-frame matrices of a frame are exactly the current ones of the frame before.
- No out-of-order or alternating frames (frame i+2 always further from i than i+1), no 2x2-quad pattern (the ratio of the
  luma step across quad edges to the step inside a quad is 0.94, as in a normal picture), no excess of per-pixel flicker
  over the lava (2.4 % of the pixels a frame, the same at 30 and 60 fps).
- One translation gap found on the way, without a visible effect: [`SetTextureLod`](../../../docs/runtime/gpu/shaders.md).

**At 30 fps** (2026-10-28, after the user said the game was set to 30) [V]: the same places at `--fps-limit 30`, scale 1,
every present saved (each one a game step of 1/30 s): the canyon and the tunnel draw as above, with no flicker beyond the
scene's own motion (`c30_lava_zoom.png`, four consecutive canyon frames; `seq_vtx_shards.png`, three consecutive tunnel
frames). Also checked: the literal constants of the canyon and tunnel vertex shaders (`c253` = (-pi, 1/14, 2 pi, 0) for
the sine range reduction, `c255.x` = 3 for the bone stride) are in the constant buffer at their draws, as the translated
code reads them (`g_C(253)`, `g_C(255)`); the post passes' constants follow the full-resolution scene (1280x720: bloom at
640x360, offsets 1/1280) as they follow 1040x620 with `original_resolution=1`. **The recording cannot come from a game
at 30**: at `fps_limit=30` Swap Force keeps its interval 2 (`[video] present interval 0x2 seen (fps limit 30)`), so at
most 30 different pictures a second (presents 33.3 ms apart headless), and the recording has 44-47 (`mpdecimate`), in
equal steps of about 1/60 s of game time. So the game the user saw ran at 60: the launcher's 30 did not reach it [?]
(the game reads `fps_limit` only from `launcher.ini`'s `[video]`, which Arkeyan Launcher fills from the game's
`[swapforce.video]` before a launch; the game log's `[settings] ... fps limit N` line says which value it used).

Shots and data (untracked, `private/sflava/`): `seq_vtx_shards.png` (three consecutive tunnel frames), `seq_f60s1_crop.png`
and `z_seq30_1100.png` (consecutive lava frames), `dump6_alb.png` / `dump6_fin.png` (albedo and final over 4 frames),
`capt_mb.png` (tunnel frame before and after the motion blur, the packed G-buffer), `runs/pace_before` and
`runs/pace_after3` (`frames.txt`: the present times before and after).

### The lava shimmer's cause: the polygon offset (2026-10-29) [SwapForce] [Alchemy] [Xbox] [V]
**The owner's report** (2026-10-28, macOS, scale 2, a steady 59-60 FPS, and the same at 30): the lava still glitches
and shimmers. **His recording** (95 s, the game window at 120 frames a second, the tutorial's canyon) shows, frame by
frame, patches of the canyon walls (a yellow lava flow, a dark rock band) drawn on one frame and missing on the next,
while the frames before and after agree: a one-frame "pop" of whole patches, not single pixels, not motion
(`owner_clip_24s_one_frame_pop.png`: frames 24 s + k-1, k, k+1 and the 8x8 blocks out of line with both neighbours).

**The cause.** The canyon walls (G-buffer pass, vertex shader `9183BAD7`) get **decals** drawn over them into the same
G-buffer: vertex shader `1A20887C` with pixel shaders `FFB558E4` (8 draws a frame), `614DDFBC` (3), `C3EFBD6E` (2), and
`001E4E4F` / `F4DF48F4` in the crystal tunnel. They test depth with GEQUAL (reversed depth, no depth write) and ask for
a **polygon offset**: `PA_SU_SC_MODE_CNTL` = `0x00019802` (bits 11 and 12: front and back offset on),
`PA_SU_POLY_OFFSET_FRONT_SCALE` / `BACK_SCALE` = 16 or 48, offset 0. The runtime never translated those registers, so
the decals depth-fought the wall under them: which patch won the test changed with the camera and the walls' wobble,
from one frame to the next. Seen with `SKY_DUMP_FRAME` (the `[dump]` lines now print `vs`, `ps`, `sumode` and the four
`polyoffset` registers; no other draw of the frame enables the offset).

**The fix** (runtime, Xbox-generic, `PrepareDraw` in `runtime/d3d/render.cpp`): the offset becomes the host pipeline's
depth bias, with Xenia's conversion (BSD): slope scale / 16 (the guest counts 1/16 subpixels) times the resolution scale
(the slope is per guest pixel), the constant offset x 2^19 (20e4 depth) or 2^23 (24-bit) rounded away from zero (0 in
every Swap Force draw seen, so its unit is [?]); front values for front faces and points or lines, back values for back
faces, front first when both apply. `SKY_POLY_OFFSET=0` turns it off ([debugging.md](../../../docs/porting/debugging.md)).

**Proof** (headless, the tutorial recipe, 40 consecutive presents from 50 s (canyon) or 110 s (tunnel);
`analyze.py shimmer`, below):
- **A paused, still scene** with the offset toggled every other frame: frames of the same parity are identical, frames
  of the other parity differ by whole decal patches on the walls (`paused_offset_toggled_crop.png`, `_diff.png`).
- **The flip map**: per pixel, the part of the luma that flips with the frame's parity (a lock-in at the Nyquist
  frequency: smooth motion averages out, a patch drawn on one frame and not the next stays); the share of pixels that
  flip by more than 16 levels on the walls (canyon: the upper 45 % of the frame; tunnel: the whole frame), and the
  one-frame pops (8x8 blocks out of line with both neighbours by more than 12 levels, whole frame):

  | Run | flip, without the offset | flip, with it | pops, without | pops, with |
  |---|---|---|---|---|
  | canyon, 60 FPS, scale 1 | 2.85 % | 0.00 % | 4.6 % | 0.6 % |
  | canyon, 60 FPS, scale 2 | 1.70 % | 0.05 % | 5.2 % | 1.9 % |
  | crystal tunnel, 60 FPS, scale 2 | 2.03 % | 0.04 % | 8.0 % | 2.1 % |
  | crystal tunnel, 60 FPS, scale 1 | | 0.00 % | | 1.3 % |
  | canyon, 30 FPS, scale 1 | 3.31 % | 0.29 % | 5.5 % | 2.8 % |
  | canyon, 30 FPS, scale 2 | 1.95 % | 0.74 % | 5.9 % | 3.7 % |
  | canyon, 30 FPS, scale 2, anisotropy 4 (the harness's run) | 3.24 % | 0.49 %, 0.59 %, 0.81 % | | |
  | crystal tunnel, 30 FPS, scale 1 | 1.77 % | 0.54 % | 7.9 % | 3.9 % |
  | crystal tunnel, 30 FPS, scale 2 | | 1.09 % | | 6.5 % |

  At 60 the snapshots use a 640x360 window and `SKY_CADENCE=0` (a 1280x720 readback of every present holds a 60 FPS run
  at 30 while it lasts; at 30 the window is 1280x720); what is left at 30 is the scene's own motion, twice the step: in
the tunnel at scale 2 (1.09 %) a fine pattern along the fast walls' texture edges, no patch
(`lockin_tunnel_30fps_scale2_after.png`).
  Pictures (frame and flip map, before on top): `lockin_canyon_60fps_scale2_before_after.png`,
  `lockin_tunnel_60fps_scale2_before_after.png`, and the `_30fps_` ones.

**The other leads, ruled out** with the same measure (whole frame), offset on, canyon, scale 1, 30 FPS: anisotropy 16
(0.18 % flip, 2.7 % pops) and the mip filter forced to linear (0.21 %, 2.8 %) change nothing against the baseline
(0.16 %, 2.75 %); the
canyon, decal, lava-sheet and motion-blur shaders use no derivative, gradient or LOD instruction; an animation constant
late on alternate frames, or a render target read while written, would flip with the parity and the flip map is flat
with the offset on. `SetTextureLod` was measured before ([shaders.md](../../../docs/runtime/gpu/shaders.md)).

**Elsewhere in the game**: the runtime logs the first draw that asks for an offset (`[render] polygon offset first
used: ...`). In the harness's runs (2026-10-29) Swap Force's `barrens/brn2` draws the same decals (`614DDFBC`, slope
16), so the fix covers the levels after the tutorial too; SSA (`level_027`) and Giants (Chapter 1, a tour level, the
save and coin runs) never ask for it. The zone tour would list every zone that does [?].

**On the recording's rate**: from 16 s on, the owner's clip holds frames for 2 and 4 refreshes of 120 Hz in turn (40
distinct frames a second, the even 40 FPS below 60 of
[video_vblank.md](../../../docs/runtime/xbox360/video_vblank.md#pacing-waitforflippresentinterval-in-the-swap-hook)); the
patches pop at either rate.

Shots (untracked, `private/lavafix/shots/`): the pictures named above. The check: `tools/test_all.sh` (Swap Force's
tutorial run, "the lava canyon does not shimmer"), [testing.md](../../../docs/getting-started/testing.md).

### The lava's posterized gradients (2026-10-30) [SwapForce] [SSA] [Xbox] [V]
**The owner's report** (0.6.1, the first level): the lava textures look wrong, a kind of blur. No clip; reproduced headless.
The picture: hard contours and flat areas in the lava's gradients, like a poster, at scale 2 only.

**The cause.** The tone map reads a colour table the game renders in the frame (pixel shader `7EE359C5`, a 256x16 target,
16 x 16 x 16 entries; two ramps come the same way, `F2BA0933`, 64x1 and 64x2). `TargetFor` scaled every target, so at
scale 2 the table was 512x32 with each entry doubled (the 2x2 box of the dump equals the scale 1 table exactly). A bilinear
lookup between doubled entries is flat for half of each step and twice as steep for the other half: the tone map's
output piled up on one level every 18.

**The four cases** (tutorial, 3 frames each, `private/runs/lava_{s1f30,s2f30,s1f60,s2f60}`; load 15-75 from the other
lanes, so the frame times are not comparable). Slope = the spread of the tone map's output-against-input curve, from the same
frame's resolves before and after it; spike = the worst bin of the output histogram against its neighbours:

| Case | Slope std | Spike | Shows it |
|---|---|---|---|
| scale 1, 30 FPS | 0.11-0.30 | x1.2 | no |
| scale 1, 60 FPS | 0.09-0.18 | x1.2-1.3 | no |
| scale 2, 30 FPS | 0.78-0.89 | x26-27 | **yes** |
| scale 2, 60 FPS | 0.76-0.77 | x22-24 | **yes** |
| scale 2, 60 FPS, the table at guest size | 0.08 | x1.2 | no |

**The bisect by pass** (`SKY_SKIP_PS`, the build before the fix, scale 2, 60 FPS, final frames, `analyze.py banding`):
nothing removed x39; lava sheets x27; heat x16; depth of field x15; FXAA x3.6; **motion blur and tone map x1.3**. Only the
tone map's removal clears it, and it is the pass that reads the table.

**The fix** (runtime, Xbox-generic): a target with a side of 32 guest pixels or less is a table and keeps its guest size
(`IsTableSize`, `TargetScale`; viewport, scissor, resolve and clear rectangles follow the target, `DrawDesc::resolutionScale`
replaces the global scale for vPos and point sizes, and a resolve is flagged scaled only if its host size differs). No picture
target is that small in the three games (the smallest side seen is 72). A draw into a table that read a scaled resolve
with fetch offsets or unnormalized coordinates would get scale 1 for them: none exists in the frames checked [?]
for other levels.

**SSA has the same bug, milder**: its 1024x32 grade table (pixel shader `EAE0BF4C`, 32 steps, so half as tall a stair) was
2048x64, doubled: x3.73, fixed x1.55 at `level_027`. **Giants** (`level_203_the_machine`, scale 2): no table target, x1.18.

**FXAA at scale 2** (lead from Core of Light's notes, verified): the game sets c0 and c1 from 1280x720 whatever the scale
(c0 = +-0.5/size, c1 = (4, 4, -2, -2)/size),
and `tfetch2D` scales the corner taps' fetch offsets (+-0.5 guest texel) for resolves: the taps land 2 host pixels apart.
Forced, the same run alternating frames: as the game 2.90 % of the pixels changed and 20 % of the Laplacian lost, in host
pixels 1.44 % and 13 %. The fix: for `game::HostKernelPixelShader` (`123B15C5`) c0-c1 are divided by the scale and the
draw has no scaled fetch (after it 1.38-1.42 %, 13 % on consecutive frames). Scale 1 is untouched. SSA and Giants have no such
pass (a scan of their post constants): the constant is 0.

**The leads that failed**, one line each:
- Fetch offsets (`1644cee`): right for the kernels that must follow the frame. The depth of field removes as much at scale 1
  and 2 (Laplacian x 0.49 and x 0.50). Only FXAA needed the exception above.
- Motion blur at 60: constants equal at 30 and 60 (c0 = 1280, 720; c1 = 1/1280, 1/720), the walls' velocity sits at the
  clamp in both (5 %-95 % of the velocity channel 13-234 and 12-232). So the blur at 60 is the blur at 30, not wrong: no
  `motion_blur` setting.
- LOD and mips: not the cause of the posterization (lava sheets removed: still x27). Whether a mip choice softens them is
  not shown either way [?]; `SetTextureLod` was already measured and changed nothing.
- Heat distortion: the scene's shift has a mean of about 0 (+0.3, +0.2 host pixels at scale 2; 0.0 at scale 1), so the DXN
  decode has no sign or bias error.
- Small render targets: the depth of field's chain keeps its ratio at both scales (above); the bloom chain was not measured
  apart, it does not change the bisect's result.

**What softness is left** is the game's: over the canyon the depth of field halves the Laplacian at either scale, then the
motion blur and FXAA. Whether a console blurs that much is [?] (no console capture).

**Check it**: `tools/test_all.sh` (Swap Force's tutorial, "the lava canyon's gradients are not posterized"; before the fix
6 to 39 on final frames, after it 1.2 to 2.3). Pairs (untracked, `private/lava/shots/`):
`pair_60fps_scale2_{before,after,crop}.png`, `pair_30fps_scale2_...`, the closest frames of two runs of the scripted scene:
the swap count does not line up between runs here (the menus take a variable time), so these are the same moment, not one
frame. `fx_game_vs_host.png`: FXAA on consecutive frames.

## Tests
The explicit-LOD/cube translator update (2026-11-01) affects 55 of 1081 local distinct containers (28 also use
noncanonical cube operands); their DXIL/SPIR-V compile, while a visual reflection check remains due. Players must
update/rebuild the game with the new kit; developers rerun the disc plus runtime/movie cache recipe, configure and
relink. Arkeyan-Builder's stamp hashes the new translator binary, forcing retranslation; compiled cache entries hash the full
translated text, so old affected entries cannot be reused. Before release, advance `games/swapforce/game.txt` from
0.5.4 and the kit from 0.8.0; this lane changes neither version. Keep the cache player-local. Check reflections while
turning the camera (roughness, face direction, sparkle), plus deferred lighting, FXAA, shadows and movies at scales 1
and 2 on Metal, D3D12 and Vulkan. [Full audit and proof](../../../docs/runtime/gpu/shaders.md).

- The extraction: `python3 -I tools/shaders/swapforce_extract.py <iso> <out> private/swapforce/shaders/all` prints
  `1074 distinct (580 vertex)` and `found by the extractor: 549; missing: 7`. Arkeyan-Builder's C++ extractor on the same
  archive writes the same 1074 files (by content), and on Giants' `global.bld` the same 4394 files as before the v10/v7
  change (byte-identical set, 2026-10-27). `ArkeyanArtTest` reads a synthetic IGA v10 fixture (big-endian, 16-byte entries,
  0x800-aligned chunks).
- Not yet: a game run on the disc-made cache (`private/swapforce/shaders/cache_disc/`, 1081 shaders).
- The frame snapshots of the runs (`SKY_GPU_SNAPSHOT`): the title screen, menus, loading screen, the HUD and, since 2026-10-27, the tutorial's 3D scene draw like the game ([boot.md](boot.md), "What broke on the way to a level").

## Is it engine-generic?
The dump path is: any game whose shader Init or Create functions are hooked. The disc path is per archive format:
Giants IGA v8 / IGZ v6 (`global.bld`), Swap Force IGA v10 / IGZ v7 (`shaders_xenon.pak`). The idea carries over
[Alchemy]: one archive of shader IGZ files, containers found by their D3D header, the physical part found through the
memory-handle table. The "in order, same size" pairing is checked on Swap Force only; Trap Team and later (IGA v11,
IGZ v8+) have to be checked again.

## Edge smoothing (2026-10-31) [V]
The setting `edge_smoothing` (game, light, off; [settings.md](../../../docs/getting-started/settings.md)) changes this pass's contrast thresholds (pixel constants c3 and c4, floats 12 and 16: 0.125 and 0.05 in the game, 0.5 and 0.2 in Light, 1000 in Off, a plain copy).
One static frame each at scale 2, `analyze.py sharpness` (the 3D area's detail against Game): **Light +23.9 %, Off +26.8 %**. Not chosen, the runtime uses Light from scale 2.
