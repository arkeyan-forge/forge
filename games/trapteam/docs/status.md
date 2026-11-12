# Skylanders: Trap Team (trapteam)

> **Purpose:** which disc Trap Team needs, where the port stands, and where to start · **Status:** current (2026-11-10) · **Tags:** [Xbox] [XDK] [Alchemy] [TrapTeam] [V]

## At a glance

- **Played on:** macOS (the test suite, the owner); Windows on NVIDIA and AMD (the owner, issue #27).
- **Known limits:** Linux not tried yet; Spy Rise dark on macOS (#50); separate voice language and LB fixed in 0.9.6, to confirm (#51).

(The source of the README's game table: `python3 -I tools/game_status.py` writes it from this section and `game.txt`.)

## Disc
| | |
|---|---|
| Release | 2014; Redump "Skylanders - Trap Team (Europe) (En,De)", 8738846720 bytes (the only disc tried) |
| Title ID, media ID, version | `41560911`, `7A74A451`, `0x00000014` [V] (read from the XEX execution info) |
| `default.xex` SHA-256 | `f1d2373e...` (full: `f1d2373e4507e21cc371e6f22d02e909602a1b5bc4704896c3c099cc64002a35`), 9252864 bytes; the image is 0x00980000 bytes at `0x82000000`, entry `0x8258A820`, `.text` `0x821CCA00` + `0x615EE0` |
| Engine | **Alchemy** (Giants' hooks 35 of 38, SSA's Lux hooks none), XDK **2.0.21256** (Swap Force 21250, Giants 21076) [V] |

The disc stays outside git; everything derived from it is in `private/trapteam/` (day 1: `private/trapteam/day1/`).

## Status
Two discs: Europe (En,De) and Europe (Fr,Nl), the same loaded image byte for byte; only the XEX header differs (media id, version) [V].
Day 1 (2026-10-30): codegen, build and boot work; the title ("Press A to begin"), the autosave notice, the opening movie and **the first level
(`level_313_tutorial`)** render and run for the 130 s tried (its intro stream opens, about 850 draws at 30 fps) after the vertex-buffer fix
([boot.md](boot.md): the video pool was full, `enableVramCopy` is forced off, commit `24060f0`: a workaround **[?]**, why the console has the room is unproven). The main menu **was seen once** (a slow run, 2026-10-30: "CHOOSE PLAY", Story 0/100, fully drawn, before the shader fix below); in the faster runs the presses went
from the title to the movie `fmv1` and the tutorial, which the game starts on a fresh profile.
**With a Trap Master on the emulated portal (`SKY_PORTAL_PLACE=new:450`) the game goes past "Place a Skylander" and the tutorial is playable**: Gusto, the HUD, 30 fps,
movement and a jump seen over 190 s, no fault ([boot.md](boot.md) "Past the prompt").
**`SKY_LEVEL` works** (2026-10-30): the adapter's `firstLevel` is `\level\level_313_tutorial` (the three-line change in `adapter.cpp`, as Giants' tutorial): the game's first
level's two files are read from the level asked for. One tour on three levels (`tools/regress/coverage.sh trapteam <disc> private/runs/tt-tour --levels
level_300_birdcage,level_301_the_machine,level_302_chompywurm --no-credits`): the three `coverage.tsv` lines are `ok`, each opened, 0 faults, lowest fps 11.0, 30.0 and 29.0 [V].
**A villain captured in play** (2026-11-02, [boot.md](boot.md#the-first-capture-2026-11-02-v)): Sheep Creep, the tutorial's first villain, into an empty Life Trap on the emulated portal; the game writes copy A of the trap (count 1, villain 20 at `figures::TrapVillainOffset`), the villain plays (LT), the trap comes off and back through the Toy Box, and a restart loads it again. Repeat: `tools/regress/trapteam_capture.sh`. Earlier: blind play met no villain, a crafted trap works ([figures.md](../../../docs/runtime/host/figures.md#acceptance-in-the-game-2026-10-31-v)).
**Pipeline baseline** (2026-11-02): `games/trapteam/pipelines/baseline.zst`, 658 pipelines from the 56 levels and the figure tours; draw-time builds after a level opens 125-142 -> 0-1 ([pipeline_cache.md](../../../docs/runtime/gpu/pipeline_cache.md#trap-team-alchemy-trapteam)).
**The 0.1.0 path** (2026-11-03, [010.md](010.md)): the opening movie ends with Start only; a relaunch shows "Select a save slot" and continues; chapters 2 and 3 and the Academy open and play through `SKY_LEVEL` with no fault (the Academy's sky is grey there: its lighting files are not loaded by the redirect [?]); the chapter ends are not reached (the capture route drifts, 1 pass of 4). Repeat: `tools/regress/trapteam_chapters.sh`.
Numbers and how to repeat them: [research.md](research.md) "Day 1".

### Arenas in the tour (2026-10-31) [V]
**Verdict: the faults of `arena_challenge_309_000`, `_311_000` and `_361_000` are artefacts of the `SKY_LEVEL` redirect, not
bugs a player hits**, as Giants' challenge levels ([history.md](../../giants/docs/history.md), "Challenge levels in the tour").
- The fault: `sub_8264B3F0` (`lwz r11,140(r3)`) reads `0x8C` of the global at `0x8284EB50`, null: the Havok world (the
  level's `tfbPhysicsWorld`). Called from `sub_8229C920` <- `sub_822AC078` <- `sub_822ABC90` <- `sub_822434E8`, under 0.4 s
  after the arena's `.bld` opens (reproduced on `_361_000`: `.bld` at 28.9 s, last frame 29.3 s, `r3` 0). The world is made when the level's physics object is activated (`sub_822486C0` ->
  `sub_8229F808` -> `sub_8229EC98` stores it) and released by `sub_8229FA88`.
- On the disc, `tfbPhysicsWorld` (with its `hkpWorldCinfo`) is in each of the six `level\Arena_Mesh_{300,302,303,309,311,361}.bld`
  and in none of the `Arena_Challenge_*_000.bld`. The redirect swaps the tutorial's two files for the arena's only: the
  log opens no `Arena_Mesh`, so the world is never made. 300, 302 and 303 have no world either; they did not reach that
  call in the 130 s.
- The game's own path loads the mesh: `permanent\global.bld` has `Arena_Mesh_BaseString` ("base mesh load string") and
  `misc\UI_ArenaSelect.bld` its "mesh load" with `Arena Challenge Def` and `System - Set Game Mode` [V]; which mesh goes
  with which arena is decided there [?]. A run through the arena select menu is not done.
- The coverage report (`tools/regress/coverage.sh`) keeps the verdict `crash` and prefixes such a row with
  "SKY_LEVEL artefact, not a player bug".

**The Toy Box is complete with the game's own pictures (2026-10-30, lane L):** logo, background and 441 pictures (toys, traps, villains) read from the disc ([art.md](art.md)), the catalogue from the game's own tables (46 villains, 61 trap shapes), 5 seats with a Trap seat, a trap's sheet listing its element's villains, Add a figure by category ([toybox.md](toybox.md)). A crafted villain trap works (`d448b32`); a real capture remains unproved.

**The window was black in the owner's run, the logo movies at least (2026-10-30, lane H):** the cause found [V] is the shader cache: it lacked Bink's two movie
shaders (the day-1 dump skipped the movies), so every movie draw was skipped and the presented texture had 0 lit pixels during the movies, offscreen as well
(`SKY_PRESENT_PROBE`). With the two added (880 shaders, [shaders.md](shaders.md)) the movies draw and a windowed run shows the picture. Not the cause, by forced
runs: the gamma ramp, the front buffer's format. **Not reproduced:** a window black *after* the movies (my windowed runs showed the title); if the owner's
build lacks the movie shaders, rebuild its cache as in shaders.md. Offscreen frames never proved a window ([render_bugs.md](../../../docs/runtime/gpu/render_bugs.md)).

**The portal change is merged** (`85ccfc2`). Commit `8347ed7` (`EmulatedPortal::SetStatusInterval`, 8 ms, `runtime/host/portal*`) makes Trap Team boot; it touches every game's emulated portal.

**Day-1 blockers resolved:** `TexCoord 8` has a location (`046227a`), and the IGA v11 reader was added (`a9cd75b`).

**Lead for the pool question (log only, nothing changed; 2026-10-30 [V] for the values, [?] for what a console gives):** the 160 MB block of the video pool
might be sized from a free-memory query. `SKY_ALLOC_LOG=1` over a run to the first level, for every memory query the game imports
([boot.md](boot.md) "Memory queries"): **it calls no free-memory query**: `MmQueryStatistics` is not in its import table, `NtQueryVirtualMemory` is
imported but never called. So the block does not follow a value we report; its size comes from the game ([boot.md](boot.md)).

## The picture: scale, anisotropy, ultrawide (2026-10-30, lane J) [V]
Same tutorial frame (`level_313_tutorial`, Gusto idle, `SKY_DUMP_FRAME` at 150-160 s, offscreen), captures under `private/runs/` (`s1b`, `s2b`, `s2k`, `s2n`, `a16`, `bc`, `uw8`).
- **Scale 2 applies as in the other Alchemy games.** The scene chain is 1040x620 (half 520x310, quarter 260x155, reductions 104x62, 520x620, 104x620; the
  log's "3D renders at 1120x704" is the generic line) and every resolve is x2 (2080x1240, 1040x620, 520x310...), the frame 2560x1440.
  **Colour tables are render targets here too:** the 64x2 (4 resolves a frame) and the 256x16 (1) stay at their guest size (`IsTableSize`).
  Proof: with the rule off (binary `s2n`) the green histogram of the upper frame piles one bin to **16.1 x** its neighbours (5 bins above 3 x), the lava's
  posterization of Swap Force; with the rule 1.16 (`s2b`) and 1.21 (`s2k`), scale 1 gives 1.22 (`analyze.py banding`'s measure).
- **FXAA is in the tone-map pass:** pixel shader `6BD91E6572C6581C` (vertex `A6F60BE72C1BA743`), one draw into a 1040x620 target that reads 16 textures
  including the 64x2 ramps; c0 = +-0.5 / size, c1 = (8, 8, -4, -4) / size, sharpness 8, threshold 0.125, minimum 0.05: FXAA 3.11 as Swap Force's, but not a pass of its own.
  `game::HostKernelPixelShader` is that hash (`games/trapteam/addresses.h`): its kernel stays in guest pixels at scale N. Four static ground regions at scale 2,
  Laplacian before and after: 3.62 -> 4.13, 3.02 -> 3.28, 5.78 -> 6.70, 5.31 -> 6.16 (+9 to +16 %; two runs of an idle scene, so [?] to a point: clouds and goo move).
- **Tiles:** at the console's 1040x620 there is no split, at either scale, in the two levels tried (every resolve is a full rectangle, `rect 0 0 w h`; tutorial at scale 1 and 2, `level_300_birdcage` at 2);
  tiling is the guest's and the host scale does not change it. **At the frame's 1280x720 (the default since 2026-10-31, below) the game splits the G-buffer into 2 tiles** for the console's EDRAM
  (`[scene] 1280x720 targets kept in one piece (the game split them into 2 tiles ...)`): `Scene_Tiles` (`0x823EBCD0`) and `Scene_SurfaceTiles` (`0x823EF930`) are hooked as Swap Force's (same offsets
  +32, +36, +40, +44, +88, +108, +112, +132, byte +125; the plan is `sub_82443DF8`, return `0x82443E44`, targets are set by `sub_823EC8E0`).
- **Cliff textures:** soft at both scales, not blocky or posterized; at scale 2 more of their strata show. The softness is the texture's own resolution [?] (no console frame to compare).
- **Anisotropy reaches the samplers:** scale 2, `--anisotropy 16` against 1 (`a16`, `s2k`): the same static ground regions' Laplacian 4.13 -> 4.93, 3.28 -> 4.79, 6.70 -> 7.85, 6.16 -> 7.38
  (+17 to +46 %); the far grass keeps its blades instead of smearing. Only textures with mipmaps get it (`SamplerFor`); the guest never asks.
- **Ultrawide works** (`games/trapteam/ultrawide.cpp`, `SKY_WINDOW_SIZE=2560x1080`, scale 1; `uw8` with `original_resolution`: scene 1232x512, the 1040x620's pixel count at the window's shape; `uw9` with the default full scene: **1696x720**, 47 resolves, 1 tile, no fault), frame 1707x720,
  the HUD (Gusto's portrait and bars) in the centred 16:9 area, more world on both sides, round things round, the camera where it is at 16:9. Hooks: `sub_8243F650` (the pass size,
  result at +92/+96) and the projection builder `sub_823B1AF0` (a horizontal field of view in radians and an aspect: 84.3 -> 100.7 degrees, the vertical one stays). The same camera is built by two
  callers: `sub_8243C700` (the scene target's shape, already wide) and `sub_8230F3D8` (a global 16:9 at `0x82826308`, two calls; their aspect is replaced in the hook).
  **Do not put the window's aspect in that global:** the camera rig reads it and with 2.37 it pulled the camera back and away from Gusto (`uw6`, `uw7`) [V], why is [?].
  Limits: the HUD's edge shading stays at the 16:9 borders (a step in the sky, as Giants); not tried: scale 2 at 21:9, 32:9, the title and menus, movies (the Bink hooks exist), other levels.

## The constant blur (2026-10-31, lane J) [V]
The owner: the whole picture is soft, all the time. Same tutorial frame, scale 2 (the 3D renders at 2080x1240, the frame is 2560x1440, the window 2560x1504 on a Retina screen), Laplacian of the
same static regions (ground, path, near grass, far ground, cliff: 5 regions) of the presented frame; offscreen dumps and **real window captures** (`screencapture -l <window id>`, `SDL_MAC_BACKGROUND_APP=1`, `win0`, `win1`).
Causes, biggest first:
1. **The scene is drawn at 1040x620 and stretched to 1280x720** (x1.23, x1.16, bilinear), then x2 at scale 2. Fixed by rendering the scene at the frame's size, as Swap Force does (`fullScene`
   = 1280x720, `games/trapteam/ultrawide.cpp`, commit `aaf5c85`): **+52 to +70 % detail** in the five regions (offscreen, `os1` against `fs1`), **+53 to +65 %** in the real window (`win0` against `win1`).
   Same frame structure as before (47 resolves, about 1410 draws, 30 fps, no halo, no fault); `--original-resolution 1` keeps the console's 1040x620. About 43 % more scene pixels.
2. **FXAA's kernel was in guest pixels at scale 2** (`HostKernelPixelShader` is 0 on the release branch, the real hash is in `f5a59cb`): the same frame with the kernel in host pixels, **+5 to +17 %**, one run (three frames 6 s apart, idle scene).
3. **FXAA itself (the game's look on a console), still on, now a setting** ("Edge smoothing", `edge_smoothing`, [settings.md](../../../docs/getting-started/settings.md)): one run, four consecutive frames with the pass's constants changed per frame
   (a temporary patch), full scene, scale 2. Detail against the game's setting (threshold 0.125, minimum 0.05): threshold 0.25 / 0.1 **+0 to +17 %**, 0.5 / 0.2 **+1 to +34 %**, FXAA off **+1 to +41 %**
   (near grass +41 %, far ground +34 %, cliff +30 %, ground +22 %, path +1 %). At scale 2 the silhouettes show little stair-stepping without it (the thin rope edge a little), at scale 1 more.
   **The setting (2026-10-31):** Game (the game's constants), Light (0.5 / 0.2), Off (1000, a plain copy); the launcher and the F3 menu call them Game, Gentle, None. Default (not chosen): Light from resolution scale 2, Game at 1;
   a choice stays. `analyze.py sharpness` (the 3D area's detail against Game), one frame each: **Light +30.3 %, Off +34.3 %**; in a real window (`edge-win-*`, five regions) **Light +1 to +37 %, Off +1 to +42 %**.
Not the cause: **the present** (`gpu/gpu.cpp` `Letterbox`: bilinear, exactly 1:1 when the window is as large as the frame, 2560x1440 in 2560x1440 of content: the window capture's numbers equal the offscreen's within 2 %);
**depth of field and bloom** (the near-field ground is bit for bit the same Laplacian from the lit scene's resolve to the one before the tone map, 16.43 at file 273 to 286; the far islands' blur is the game's depth of field);
**half-size targets** (every non-table target of the frame is x2: 2560x1440, 1280x720, 640x360, 256x144, 1280x1440, 512x512, 1024x4096; the 64x2 and 256x16 tables stay guest-sized).
Not measured: Swap Force at the same scale (no Swap Force disc in these folders); the tone-map curve's own contrast (the Laplacian also falls with contrast).

## The title logo's black outline on a Windows PC (2026-11-10) [?]
**Reported** (0.9.4, Windows 11, RTX 3060 Laptop, Vulkan, scale 2, anisotropy 4, edge smoothing Game, HD textures on and off):
the logo's letters have thick black sides and no white glow around the logo; "the 3D models are not clean", the grass poor.
**Not reproduced on the Mac** [V]: the same title at scale 2 (2240x1408 scene to a 1280x720 window) and the same settings
on Metal and on Vulkan (MoltenVK build, also scale 1, also with `MVK_CONFIG_FAST_MATH_ENABLED=0`) shows the white glow and
dark-teal letter sides; Chapter 2's grass and models are sharp. The logo is a 3D model drawn into three targets (colour,
normal, a mask whose red marks the glowing rim) and lit by a full-screen deferred pass (`DABCC7B24007F225`: normal, ramps,
a cube map, specular through `log2`/`exp2`); our scale, downscale, FXAA and anisotropy paths all run there.
**The shaders are not the cause** [V]: the run-time containers of the title (`SKY_DUMP_SHADERS`) translated by the
Windows kit's XenosRecomp (llvm-mingw, under Wine) give the same HLSL as the Mac's for all 82 pixel shaders, and the
SPIR-V of all 225 shaders is byte-identical (`tools/shaders/compare_cache.py`; the DXIL differs, as each DXC build signs
its own). So the PC ran the very SPIR-V that renders correctly here: the difference is in how the NVIDIA Vulkan driver
runs it (a value the specification leaves undefined, such as `clamp` of a NaN or a denormal, is the likely kind) [?].
**On the PC** (0.9.5, D3D12 by default): the title with D3D12, then F3 > Graphics API > Vulkan (next start), a shot of
each; if D3D12 is clean, the fault is the Vulkan driver's path. If both show it: Texture filtering Off, then Edge
smoothing None, then Resolution scale x1, one at a time, a shot each. Mac reference shots: `private/shots095c/`.
**0.9.5 on the PC** (D3D12, 60 fps): the same, in D3D12 and in Vulkan: no white glow, a grey-teal band where the Mac has
the glow, the frame darker. **Not the x86-64 CPU** [V] (2026-11-10): an x86_64 build of the title with the Windows kit's
CPU flags (`-march=btver2 -mno-sse4a`: no FMA, so `fmadd` and `vmaddfp` are not fused as on arm64), run under Rosetta
on the Mac's GPU, shows the white glow; so does the Windows executable itself (llvm-mingw, `-O2`) under Wine on D3D12
(vkd3d-proton) on lavapipe (`tools/windows/d3d12_test.sh`, `GAME=trapteam`, `SKY_PRESS=` stays on the title). One frame's
`[dump]` lines (`SKY_DUMP_FRAME=f900 SKY_DUMP_VS_CONSTANTS=12`), arm64 against x86_64: the same 2118 lines, draws,
targets, blend states and vertex streams; only the values that follow the animation clock differ (the frame came at
36.5 s and 34.9 s). The glow is also there at 60 fps on the Mac, and the Khronos layer's synchronization validation
(MoltenVK build, `-DARKEYAN_MACOS_VULKAN=ON -DARKEYAN_VULKAN_VALIDATION=ON`) reports no hazard on the title. What is
left is the NVIDIA driver, whose D3D12 and Vulkan share one shader compiler [?]: a value the specifications leave
undefined (NaN through `log2`/`exp2`, a flushed denormal, a target read while written). The next step needs the PC: one
frame's targets, pass by pass, against the Mac's ([test_095_windows.md](../../../docs/release/test_095_windows.md),
"The title logo"). Note for an x86_64 macOS build (never shipped): Apple clang called `_tlv_get_addr` for
`t_insideKickOff` (`d3d/null_gpu.cpp`, in the `BlockOnFence` hook, whose frame is realigned for a 64-byte `PPCContext`
copy) with the descriptor in `r9`, not `rdi`, so the first fence wait crashed; reading it in a `noinline` helper avoids it.
**Where the glow comes from** [V] (2026-11-10, the title frame, `SKY_DUMP_FRAME=f900 SKY_DUMP_DRAWS=1`): the deferred
lighting pass `DABCC7B24007F225` writes, in the scene's alpha, `0.5 + 0.5 x` the rim mask of the logo's third G-buffer
target (S4.z) or the brightness over a threshold; the bright pass `E7841B53A63F9CF3` (1280x720) keeps `|2a - 1|` of four
taps as the glow's weight; `86540393DB1FCA1F` and `BEA6F55F99140665` blur it at 1280x720 and 640x360; `B9C585B73738EC5B`
adds both blurs to the scene (and brightens it where `a > 0.5`). The PC's picture is our frame **before** `B9C585`
(the pale rim, no white halo): on NVIDIA that chain adds nothing, or the alpha it starts from is 0.5 at the rim.
**Not NaN or infinities** [V]: `SKY_DUMP_FLOATS` reads every target of every draw of the frame back as floats: 211
targets, no NaN, no infinity (the only negatives are the linear depth's view-space Z); all but the depth targets and one
cleared RGBA16F light buffer are RGBA8 UNORM, which turn a NaN into 0 on every GPU. The clamped log, reciprocal and
reciprocal square root (`clamp(op(x), -FLT_MAX, FLT_MAX)`) do lean on infinities that DXC's `fast` arithmetic and SPIR-V
without `SignedZeroInfNanPreserve` let a driver ignore, so NVIDIA may drop that clamp; but the title translated with the
clamps removed (IEEE infinities, Metal) keeps its glow, so that is not the cause. An infinity-free form of the three ops
(`xeLog`, `xeRcp`, `xeRsq` in `shader_common.h`, with a CPU test) waits on a local branch: it changes
every shader of the six games, so it ships only if the PC proves it matters. Not a target read before it is written
either: the RGBA16F buffer the lighting reads is cleared in the frame (`[dump] clear` lines).
**The six games, one frame each** [V] (`SKY_DUMP_FLOATS`, resolves and textures): SSA level_027 389 targets, Giants'
tutorial 196, Trap Team's level_313_tutorial 312, SuperChargers' intro at 235 s 167, Imaginators at 65 s (before its
first level) 98: no NaN, no infinity. Swap Force's tutorial_a 107: one, its deferred light buffer (below).
**Left for the PC** [?], what its log settles ([test_095_windows.md](../../../docs/release/test_095_windows.md), "The
title logo"; `tools/regress/compare_dump_floats.py`): which of the chain's targets first leaves the Mac's numbers (the
Mac: the lighting's alpha mean 0.509, the bright pass's 0.018). If the logo's G-buffer already differs, the cause is
upstream (the vertex normals' DEC3N unpacking, `spec 1`; the three-target draw); if only the lighting's alpha, a value
NVIDIA's compiler takes another way there (`r3.w > r2.w`, `saturate`, the cube fetch); if only the blurs or `B9C585`, their
samplers or constants. Also noted: plume's D3D12 pipelines leave `IndependentBlendEnable` off, so every target of a
draw takes target 0's blend and write mask on D3D12 (Vulkan and Metal use each target's own); fixed with the cause below.
**The unwritten targets were not the cause** (2026-11-10, build 6f2ef6e7 on the PC) [V]: with them masked the PC's
`#170` and `#176` stay 0 and the glow stays black. Nothing ever writes those two targets (a run that clears every new
target to a known colour finds it there at frame 2947): they hold what a new texture holds, 1, 0, 1, 1 on Apple and 0
on NVIDIA, and a Mac run with them at 0 still draws the white glow. The mask is right for Xenos and stays.
**An NVIDIA cause, fixed** [V] (Mac; the PC: [?]): the screen-space passes fetch at the pixel position with
unnormalized coordinates (`tfetch2DDenorm`), which land exactly on texel h's left and top edge. Apple and AMD read texel
h; NVIDIA reads h - 1. The PC's 8-bit dump (40 s, `private/nvdump/frames/ttdump/`) shows it: each of `aa563ab8` and
`de43d527` (the edge-aware blurs before the lighting) moves its picture one host pixel right and down against the
G-buffer (2 after both), and the lit scene `#66` is off by one (`private/g095/shift.py`; the Mac's are at 0, 0). A Mac
shader cache that puts the coordinate 1/1024 of a texel before the edge reproduces exactly these shifts and the thin
dark line along the logo's outer edge; the fix puts it 1/256 of a texel inside the texel (`DENORM_TEXEL`,
[shaders.md](../../../docs/runtime/gpu/shaders.md)), and with the same emulation the shifts are 0 and the edge is clean
(`private/g095/shots/m14.png`). Without the emulation the Mac's frame looks the same; every target's mean stays within 0.012 of an unfixed
run, where two unfixed runs already differ by up to 0.007 in the same glow targets (`#109` moves 0.02 between any runs).
**The rim: NVIDIA read every cube map as 0, fixed** [V] (Mac; the PC: [?]) (2026-11-10, build 80b0488d's dump at 40 s, every
draw, against a Mac dump at the same settings, `private/h095/`): the logo sits 9 host pixels lower on the PC (its bobbing),
so the two are compared after that shift, pixel by pixel. Every input of the lighting `DABCC7B24007F225` is the same on the
rim (colour, normals, the mask, depth, the blurred `#116`) and so are its constants; its output is not, and only where the
mask is (255, 95, 60): there 0.51 x the Mac's (77 against 154), where the classes (0, 0, 0) and (0, 255, 0) match (0.98).
The rim's mask makes it a metal: its specular colour is the albedo and its cube-map reflection (`tfetchCubeLod`, LOD
6 - 6 x mask.g = 3.8) carries most of its light. A Mac shader cache whose cube fetch returns 0 gives the PC's numbers on
every class (rim 61, 77, 78 against the PC's 61, 77, 79; (0, 255, 0) 94, 180, 209 against 94, 180, 209). The cause: a cube
map bound with no swizzle went into the cube heap with plume's default view, which follows the resource, a 2D array of
six slices on D3D12 (`CreateShaderResourceView(nullptr)`) and Vulkan (`VK_IMAGE_VIEW_TYPE_2D_ARRAY`), read by a shader that
declares `TextureCube`: undefined, 0 on NVIDIA, the faces on AMD and Apple. Every cube map and stacked array now gets a
view of the type its heap declares (`gpu/heap_view.h`, [plume_backends.md](../../../docs/runtime/gpu/plume_backends.md)), in
every game. Metal changes too: the rim is lit at 134 instead of 154 (the classes at LOD 0 move by 1 %), and the white
glow stays; on lavapipe (the Windows executable under Wine and vkd3d-proton, a conformant driver) the same change moves
the rim from 174 to 153, so the old read was off on Apple and lavapipe as well, and the cube view is the right one
(lavapipe's frame is brighter everywhere, another difference, open). SSA, Swap Force, Trap Team, SuperChargers and
Imaginators bind cube maps (Giants' `level_203` none): a level of each, before and after, shows no visible change
(`private/h095/shots/six.png`). The Khronos layer, with and without GPU-assisted validation, does
not report the old view on MoltenVK (bindless heaps). The owner's check: [test_095_windows.md](../../../docs/release/test_095_windows.md),
"The title logo".

## Nightmare (2026-11-05) [TrapTeam] [V]
**Hidden until the story is beaten; `nightmare_from_start` is not hooked yet** (the runtime ignores it with a line in the log, so the
launcher shows no toggle until it is).
- A new save's "Select a difficulty" (after the slot's New Game) has three choices, Easy, **Medium** (highlighted) and Hard; Right at
  Hard stays on Hard, Left at Easy on Easy: no fourth, no padlock (run `nm-tt-explore3`, the frames under `private/runs/`).
- The fourth exists: the title level's scripts (`level/Title.bld`, read with the IGA v11 reader; names only) hold `Menu Diff Button
  Name 003 Nightmare`, `Menu - 3d - Diff - Nightmare Button Action`, the save slot's `allow nightmare difficulty` (in `Title - Save
  Slot Load/Save`) and the difficulty menu's parameter `disallow nightmare setting` (in `Menu - 3d - Diff - Create`); `global.bld`
  has `adventure mode complete - nightmare`, and the achievements "Complete Story Mode on the Nightmare difficulty setting".
- Why no hook yet: unlike Swap Force and Imaginators, the executable has no `nightmare_difficulty_unlocked` and no native check (no
  shape match of Swap Force's `sub_829CC788` or of its menu's `sub_82C12BC0`); the decision is a script variable of the title
  level. The lead: script variables are `ScriptReference` objects, as Giants' portal prompt reads them ([Giants' boot.md](../../giants/docs/boot.md),
  "Portal prompt"); answering `allow nightmare difficulty` (or clearing `disallow nightmare setting`) where the script reads it
  needs the script VM's variable read, not found yet.

## Where to start
- Build: `cmake -S . -B build-trapteam -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_GAME=trapteam`, then
  `nice ninja -j4 -C build-trapteam ArkeyanRuntime` (the executable is `Skylanders Trap Team`). `private/trapteam/ppc/` is made by
  XenonRecomp from `config/` ([day1.md](../../../docs/porting/day1.md) step 2) and `shader_cache.cpp` by
  `SKY_GAME=trapteam tools/shaders/build_cache.sh <disc> <dump dir>`.
- The shaders come from run-time dumps (`SKY_DUMP_SHADERS`): the disc's archives are IGA **v11**, which no extractor reads yet.
- Run it isolated ([testing.md](../../../docs/getting-started/testing.md)) with `SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated`; the title
  is up at about 15-40 s.

## Open problems
0. **LB and RB** (tutorial, Gusto, 2026-10-31 [V]): X, B and RB each start an attack, Y (held 1 s) and LB (held 1 s) nothing; RB looks like B's dash. Static inspection (2026-11-01) proves that the controller update merges LB into Y and RB into B; the host passes both. Issue #51 adds the missing LB row to the runtime's Commands page for keyboard (`key_lb`, default Q) and pad (`pad_lb`). The third attack and contextual behavior still need a real run ([evidence and checks](boot.md#lb-and-independent-voices)). The standalone launcher's `launcher/game_keys.cpp` is in a separate checkout, absent here; its matching LB row still needs updating there.
1. The video pool has too little room for the vertex-buffer copy (worked around **[?]**: why the console has the room is unproven) [boot.md](boot.md).
2. **36 of 225 vertex shaders are not translated** (`TexCoord 8`, a usage XenosRecomp has no location for).
3. The 20 s wait on the legal screen in some runs (portal `V`, `L`, `C` unanswered?) is unexplained.
4. No portal prompt or save-task hook. Independent `voice_language` is wired for en/de and fr/nl: issue #51 replaces the global-pack-only availability check with the launcher's detection of actual speech entries, so valid overrides are no longer disabled when `global.bld` has no language packs. Missing speech falls back to following text. Synthetic availability and input regressions pass; audible speech and movie language, pack precedence and transitions still need a real run ([checks](boot.md#lb-and-independent-voices)). Glyphs: the pause menu's hints and the title's "Press A" follow the device ([glyphs.md](glyphs.md)); in-sentence prompts not seen yet. Ultrawide and the full scene are in the adapter.
