# A level frame (level_027): pass list, light pre-pass, sun tiles, ambient cube

> **Purpose:** what a level frame of SSA draws, pass by pass · **Status:** current (2026-11-11) · **Tags:** [Lux] [SSA]

Class **[Lux]** pipeline (expected the same in later Lux games: **capture one level frame first and write this list again before touching the renderer**), **[SSA]** names and numbers.
Detail: [engine_notes.md](engine_notes.md) §3.6. Capture: `SKY_D3D_CAPTURE` + `SKY_DUMP_FRAME` ([debugging.md](../../porting/debugging.md)).

EDRAM at 1120x704: **depth D24FS8 at tile 0x000, RT0 at 0x268, RT1 at 0x4D0** (0x268 tiles each). ~1780-2900 host draws per frame (3700-4000 in the busy part; ~84 command-buffer replays in the Z-prepass).

## Pass list

1. 256² / 512² k_2_10_10_10 passes (water ripples), resolved.
2. **Z-prepass** (~836 host draws): depth only (no pixel shader), `RB_DEPTHCONTROL 0x8767` (stencil REPLACE, ref 1); depth resolved (flags 4) into a **k_24_8_FLOAT** texture at phys `0x06A60000` (`DepthMap`).
3. Two **1024² shadow maps** (D24S8) cleared to 1, resolved (flags `0x14`) into k_24_8 (`DynamicShadowMap`).
4. **G-buffer**: MRT RT0 k_2_10_10_10 (normals) + RT1 k_8_8_8_8 (resolved as k_8 = `GBuffer2`); depth GEQUAL/EQUAL (`0x726`); the terrain uses depth EQUAL against the Z-prepass (needs bit-identical positions, D16).
5. Half-res depth + SSAO (560x352), resolves to k_8_8.
6. **14 rect draws** rewriting the depth buffer's stencil byte through an aliased colour target (write mask R, copy shader `xex_82092DB8.ps` sampling `0x06A60000` as k_8_8_8_8): **not needed** once the ring state is right.
7. 560x352 **4xMSAA** depth-only draws on the same EDRAM base (Hi-Stencil): single-sample on the host.
8. **Light pre-pass**: MRT of two k_2_10_10_10 lighting buffers, additive (ONE, ONE), stencil from `ngl_WriteStencilRefDirect` (ref 0, read mask `material[3]`, write mask 0xFF; **NOTEQUAL 0** for lights); then an **ambient full-screen pass**.
9. **Main colour pass**: ~670 draws with the world shader `0BA301663D16E692` (DiffuseLightingBuffer, SpecularLightingBuffer, DynamicShadowMap PCF, fog, IrisParams; main pass `0x766/0x762`: **no stencil test**), ~550 more; k_8_8_8_8.
10. **Bloom / luminance chain 288x176 -> ... -> 4x4**, a **colour-grading LUT 1024x32 generated in the frame**, final composite `LUT(saturate(scene + bloom * k))` at 1280x720, then the UI.

Shader constant names (from the translated HLSL): `DepthMap`, `DynamicShadowMap`, `GBuffer`, `GBuffer2`, `DiffuseLightingBuffer`, `SpecularLightingBuffer`, `VposParams`, `IrisParams`, `AmbientMap`, `ShadowBlendFilterParams`.

## Light pre-pass details [Lux]

- Accumulation into the k_2_10_10_10 light buffers (diffuse, specular; **stored x0.25 and x0.125**, the material shaders multiply by 4 and 8): **2 omni icospheres** (`effects_0034`), then the **sun through 4 tile passes**
  (primitive 1 = points, **770 points = 35x22 tiles of 32x32 screen pixels**, one variant per tile by shadow case: `effects_0044` two-map blend/16-tap PCF, `0046`, `0048`, `0050` = **no shadow**), then **one full-screen ambient pass**
  (PS `CA4F0B671FEB3477`, `effects_0030`: `ssao * (AmbientMap(normal) * 2 + AmbientColor - 1)`). **No material shader has an ambient term of its own.**
- **`AmbientMap` is a cube texture** (32x32 k_8_8_8_8) the game renders and resolves **face by face every frame** (`D3DDevice_Resolve` `slice` = face, r9). Resolves must therefore support cube destinations.
- **Sun tile classes** (the "blocky shading" bug): each tile's VS fetches a class byte (computed index, `GuestShader::pulled`) and keeps the tile only in its own pass: class 1 -> `effects_0046`, 2 -> `effects_0044` (16-tap PCF),
  4 -> `effects_0048`, 8 -> `effects_0050` (no shadow). The class bytes (VB at physical `0x045AEA80`, light manager singleton `0x82BC07D0`) are written by the **CPU job `sub_821404D8`** (found by write-protecting the page; runs on a worker thread through
  `sub_82891B80` / `sub_82891A40`, 35 x 11 tiles per job): it reads **4 KB per tile from `0xA6A60000` = the scene depth resolve**, decodes 20e4 depth from bits 31-8 (`vperm` rotate by 8, mask `0x00FFFFFF`, `<< 3`, `+ 0x38000000`; constants at `0x82BC09D0..`)
  and picks the class from the depth range. See [../gpu/edram_resolve_tiling.md](../../runtime/gpu/edram_resolve_tiling.md) for the write-back. Two copies of this job exist in SSA (`LightTile_ClassifyJob`): **check which is the worker's** in another game.
  Proof by forcing every class: all 8 = sun without shadows, all 2 = correct soft shadows.
- **The job's timing** (2026-11-11) [V]: two jobs (rows 0-10 and 11-21) start on their worker threads right after the
  depth resolve and call `Sleep(0)` after each tile; the frame's light passes are drawn before they end, so a frame
  uses the classes of the job before. They must not back off on those yields (`kernel::WorkingThreadScope`,
  [threads_sync.md](../../runtime/xbox360/threads_sync.md#yielding-and-idle-loops-lux)): backed off, a job took
  ~115 ms and the classes lagged 3-7 frames, which showed as lit tiles along moving grass edges
  ([render_bugs.md](../../runtime/gpu/render_bugs.md)).
- The sun passes take their PCF offsets in UV from `ShadowBlendFilterParams` (not scale-related: classes are identical at x1 and x2).

## Level-load facts

~2857 textures (25.6 MB) are uploaded in the first frame (~334 ms with a warm pipeline list); the first frame is also the game's own main-thread update ([../gpu/renderer_threads.md](../../runtime/gpu/renderer_threads.md)).
**The sky of level_027 is violet on a real 360 too** (dome PS `tex.rgb * IrisParams.x (1.1) * vertex colour`; BC1 texture decodes blue, D3DCOLOR `0xFFF07DFE` ARGB = (240,125,254): (140,184,210)*1.1*(.94,.49,1) = measured (145,99,230)).

## Hub / other levels

`SKY_LEVEL=<level>` starts the new game in another level: `level_000..046`, `level_hub` (first stage), `challenge_level_000..031`, `pvp_level_001..010`, `dnftest` (all 70 run without fault). The frontend `mission.str` + `screenmask.apk`.
