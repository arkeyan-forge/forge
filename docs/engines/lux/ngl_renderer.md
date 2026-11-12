# NGL: the engine's renderer on top of D3D

> **Purpose:** how NGL, the Lux renderer, drives D3D and what that means for our renderer · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

Class **[Lux]** behaviour, **[SSA]** addresses. Detail: [engine_notes.md](engine_notes.md) §3.4-3.5, [d3d9ltcg_device.md](../../runtime/gpu/d3d9ltcg_device.md), [command_buffers.md](../../runtime/gpu/command_buffers.md).

## The frame (`nglListSend`, `ListSendModule`)

- The game side builds scenes with `nglListBeginScene` (`0x82633FD8`: allocates a scene, links it under the current one `0x82BB4F44` or as the next root `0x82BB4F48`), `nglInitScene` (`0x82633E78`: clears it; **render list at `scene+0x348` = 0**),
  `nglListEndScene`, then **`nglListSend`** (`0x829075F8`): waits for the previous ListSendModule, root = next root, queues the job.
- **`ListSendModule`** (`0x826320C8`), a job: `SetShaderGPRAllocation` (**32 VS / 96 PS GPRs**) -> invalidate the NGL state cache (`0x82BA4F70`) -> `InsertCallback` -> **`nglSceneTraverse`** (`0x82631098`, recursive, current scene in `0x82BB4F4C`: Begin/EndTiling,
  ZPass, draws) -> `InsertCallback` (callback `0x826319B8`) -> fence -> `0x822A9088` (FrameEnd?, maps the present interval) -> **`nglPresent`** (`0x8262D690`): `Swap(dev, frontBufferTex + 0x14)` with double-buffered front-buffer textures `0x82C54698` / `0x82C54720`.
- `nglRenderListSubmit` (`0x822895E0`) submits `scene+0x348`; sentinel `0x82B5FAAC`. `nglResolveTiles` (`0x8262D540`): per tile `SetPredication(3 << 2i)` + Resolve (frontend path). `nglInit` (`0x826324D0`): `Direct3D_CreateDevice(1, 1, &pp 0x82B94E98, &0x82BB4F94)`.
- The title asks for **present interval 2** (`sub_82631EA8`): 30 fps. NGL's frame statistics: struct `0x82BA8258` (frame time `+0x5C`, fps `+72`; copy `0x82BA71E0`), only the debug overlay (`sub_8298B880`, "%.2f FPS\n%.2fms") reads them.
- **Two engine assumptions break when a frame has nothing to draw** (movie start / a movie with no picture ready): a frame submitted with no scene makes the traversal dereference `0x82BB4F4C = 0`; a scene whose render list is still null crashes
  `nglRenderListSubmit`. **Both are skipped in `patches.cpp`** (the only two NGL game-code patches, plus the heap guard). Next Lux game: the same two guards at re-found addresses.
- One fault seen once in ~25 runs during `tfb.bik`: guest `0x24` in `sub_822AE460` <- `sub_82630B78` <- `sub_82631098` (scene traversal). Never reproduced (D40).

## NGL habits that make a "read the D3D shadows at draw time" renderer insufficient

1. **Inlined setters** (harmless; see [../gpu/ring_mirror.md](../../runtime/gpu/ring_mirror.md)).
2. **Direct PM4 ring writes** bypass the shadows (cause of "pink/violet 3D", of the "stencil == 2" blocker and of the glyph/panel glitches): `ngl_WriteStencilRefDirect` `0x8213BBB8`, the material state blocks `sub_828FC670` -> `sub_828FCBA8` (its only caller is `0x8285F700`; `FC670`'s is `0x8285D6A8`), `ngl_FlushHiZDirect?` `0x82893F10`, the bin mask `0x82629010`, `InsertCallback` `0x822891F0`.
3. **Command buffers per mesh/material** with **inherit tags** ([../gpu/command_buffers.md](../../runtime/gpu/command_buffers.md)); after a run the inherited groups the buffer wrote stay on the GPU with the buffer's values.
4. **Vertex streaming ring**: dynamic vertices through a **2.56 MB ring at `0xAA5E0000`**; each draw's fetch constant points at the write position with size = *the rest of the ring* (cache on the range read).
5. **Reversed depth** (GEQUAL, far = 0; scene depth cleared to 0, shadow maps to 1).
6. **Depth-only passes have no pixel shader** (must not be skipped).
7. **EDRAM aliasing tricks** (colour target over the depth buffer with write mask R + copy shader `xex_82092DB8.ps` rewrites the stencil byte; 560x352 4xMSAA depth-only draws on the same base) - not emulated, not needed.
8. **Point sprites** (particles): 4 vertex shaders export point size (export 63); the PS gets the point coordinate when `SQ_PROGRAM_CNTL` bit 18 (`param_gen`) is set, in register `SQ_CONTEXT_MISC.param_gen_pos`.
9. **16-bit texcoord pairs** swapped by the 32-bit vertex swap; **normals DEC3N**; **viewport transform disabled** (`PA_CL_VTE_CNTL` bit 0 = 0) for screen-space draws; **rectangle lists and quad lists** for UI; `ngl_DrawQuadUP` (`0x8228B3E8`: inline SetVertexDeclaration `+0x2F58` + BeginVertices(QUADLIST)), `ngl_UnbindStream` (`0x82289248`); **BeginVertices draws must be snapshotted at call time** (state may change before the game fills the buffer).
10. **Clears are draws** in `0x822ADDE0`.

## The command-buffer cache [Lux]

`sub_828519B0` -> 64-entry cache at `0x82AA7BC0`; recorded by `sub_82850F90`; mesh draw `sub_8285C540`; run-state setter `sub_8285F700`; direct draw `sub_8214AE58`; inherit tag setters `sub_82850C48` (OR), `sub_82850C90` (ANDC), `sub_82850CD0`/`CE8`, defaults `0x82BB46D0`.
Menus run 5-14 buffers/frame; a level's Z-prepass ~84 replays.

## Billboards and particles (the SIGBUS lead) [Lux]

`sub_829B71D0` is the render callback of the **`apsBillboardNode`** node type (type record `0x820BB0B0`: name, then callbacks `0x822694A0`, `0x824CBE80`, `0x829B71D0`, ...), called by the scene traversal (`sub_82631098`, lr `0x826314C0`) on the JQ Worker thread. It calls
`(*(this+240))->vtable[6]` (a bool: `sub_822610B0` returns `this+72 == 3`). `this+240` is a **particle-system Action** (vtable `0x820BA99C`, next to the `EnvCollideAction` / `RandomSpawnAction` / `TrajectoryAction` names); its scalar deleting destructor `sub_82261138`
resets the vtable to the base `0x820ABE20` and frees through the small-block pool (`sub_82132CC0`, pool `0x82BB5488`). The billboard nodes are per-frame copies in two alternating arenas (`0x4121xxxx` / `0x4162xxxx`) pointing to live Actions
(`0x44xxxxxx-0x46xxxxxx`): a particle effect destroyed by the game thread while the job thread still renders the previous frame's list would give exactly the crash (freed Action: vtable overwritten by the free list -> garbage target).
Not proven; ~2 h of soaks in 5 runs never reproduced it. The indirect-call guard logs and skips such calls.
