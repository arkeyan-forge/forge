# Recorded command buffers (Begin / End / RunCommandBuffer)

> **Purpose:** how does the title record and run D3D command buffers, and how does our replay reproduce the GPU state they leave? · **Status:** current (2026-10-26) · **Tags:** [XDK] [Lux] [SSA]

Code: [`d3d/command_buffers.cpp`](../../../runtime/d3d/command_buffers.cpp), [`d3d/ring.cpp`](../../../runtime/d3d/ring.cpp).
Class: the D3D mechanism **[XDK]**; the engine's use of it **[Lux]**; addresses **[SSA]**. [V] = static analysis and
runtime logging, [?] = unverified. The dirty mask bits: [d3d9ltcg_device.md](d3d9ltcg_device.md#dirty-masks).

**Symptom if wrong: menus without backgrounds, 3D meshes missing while the UI draws.** Before the replay existed, `RunCommandBuffer` was a no-op
(D3). Check it first in any Lux game.

## Who uses them [Lux]

The engine **caches one command buffer per mesh/material combination**:
- `sub_828519B0` looks the combination up in a **64-entry cache at `0x82AA7BC0`** (buffers at +0x30; refuses to evict an entry used in the last 10 frames [?]),
  and on a miss records it with `sub_82850F90`: `BeginCommandBuffer`, `SetDepthStencilSurface`, the material's VS/PS bool and float constants, vertex and
  pixel shader, sampler states, textures, stream source, indices, **one** `DrawIndexedVertices`, then `EndCommandBuffer`.
- `sub_8285C540` (virtual, the mesh draw) sets the run state with `sub_8285F700` (shaders, render states, transforms) and calls `RunCommandBuffer`;
  without a cached buffer it draws directly (`sub_8214AE58`). Menus run **5-14** buffers a frame (backgrounds, logo, button frames); a level Z-prepass ~84.

## D3D side [XDK]

- **`BeginCommandBuffer(device, cb)`** (`0x822A6BC8`): `device+0x35E0 = cb`; copies the **inherit tags** (5 x u64, globals `0x82BB46D0`) to `cb+0x18` and the
  persist tags to `cb+0x38`; **clears the fetch-constant shadows** (`device+0x480`, 0x300 bytes); writes the window scissor into the buffer; sets the
  dirty masks `device+0x00..0x20` to **`~inherit`** (so the first recorded draw writes every group the buffer does not inherit).
- Recorded draws flush the dirty groups into the buffer as usual: **a buffer holds exactly the groups that were dirty at some recorded draw**. Menu buffers
  record: VS c16-c23 and c248-c255, PS c16-c19 and c248-c255, RB_DEPTHCONTROL, 0x2203, PA_SU_SC_MODE_CNTL, RB_MODECONTROL, the shaders, the viewport,
  RB_SURFACE_INFO, RB_DEPTH_INFO, texture slot 0, vertex fetch slots 26-31.
- **`EndCommandBuffer(device)`** (`0x822A6FB8`): stores the dirty masks into the buffer and finishes it [?] (name).
- **`RunCommandBuffer(device, cb)`** (`0x822A73D8`): sends the dirty groups the buffer inherits (`dirty & cb+0x18..`) for VS/PS constants, emits
  `INDIRECT_BUFFER` (`0xC0013F00`) per segment, then sets the device dirty masks to the **complement of the buffer's persist tags (`cb+0x40..+0x60`,
  copied from +0x18 by Begin)**. Device shadows are not changed: **after a run, the inherited groups the buffer wrote keep the buffer's values on the GPU
  while the shadows keep older ones** (an older doc said "ORs the masks"; the disassembly says otherwise).
- Inherit tag defaults at **`0x82BB46D0`** (5 x u64, set by `sub_82850C48` OR / `sub_82850C90` ANDC / `sub_82850CD0`/`CE8`): at run time VS
  `FFFFFFFFFFFFFFFC`, PS `FFFFFFFFFFFFFFFC`, regs `03FFFFFFFFE1FFF7`, fetch `007FFFFCFFFFFFC0`, misc `01FFFFFFFFFFFFFF`: **nearly everything is inherited
  except c248-c255, the shaders and vertex fetch slots 26-31.**

## Our replay (`d3d/command_buffers.cpp`)

A draw made **while recording** is not drawn: `RecordDraw` keeps `{primitive, first, count, indexed, baseVertex}` plus **the device bytes of every group dirty at
some recorded draw so far** (`RangesFor`: float constants 64 bytes per bit, registers 4 bytes, `RenderTargets` 16, `DepthStencil` 4, the declaration + both
shader pointers when any shader-link bit is set, fetch slots 24 bytes with the strides of the streams they hold, bool/loop constants 0xA0, **the index buffer
pointer always**) and the register writes the ring shows for the draw (`TakeRecordedWrites`). A **run** (`sub_822A73D8` replacement):

1. `ScanRing`; look the buffer up (an unrecorded buffer logs `[render] run of command buffer 0x.., which was not recorded`).
2. `SendInheritedShadows(device, cb)`: emulates the guest's own send of inherited dirty float/fetch constants from the shadows.
3. For each recorded draw: save the device bytes of its ranges, write the recorded bytes over the device, apply the recorded register writes, `Draw()`, `DrawConsumed()`,
   restore the device bytes. A trailing "writes only" entry (`WritesOnly = ~0u`, made at `EndCommandBuffer`) applies the register writes made after the last draw.
4. Finally set the device dirty masks to `~cb[0x40..]` like the guest.

Register writes recorded into a buffer (including those after its last draw, scanned at `EndCommandBuffer`) are applied
to the ring mirror when the buffer runs; the renderer takes texture fetch constants (slots 0-25), float constants and
NGL's registers from the mirror unless the shadow is dirty, so the inherited state a run wrote stays in effect until
the game sets it again. (The mirror replaced an earlier targeted "persistent state" and a `sub_828FCBA8` hook when
the branches were merged.)

Re-recording a buffer **replaces** its draws (runs in flight keep the old list through a `shared_ptr`). Known open: `BeginVertices` draws made while recording are drawn
immediately [not seen]; the window scissor written by Begin is not replayed (the renderer reads `device+0x327C`).

## Why the replay is not enough: the "GPU state" is a union

`GPU state = shadows U last replayed buffer U direct ring writes.` Examples that each cost a long debugging round:
- The **autosave dialog's panel** is drawn directly after two buffer runs *without setting its texture*; on the console it reuses the dialog texture the previous run
  left in fetch slot 0 (the inherited group). Our replay restored the shadows, so it drew with the title's START glyph (huge, opaque). Fix: texture fetch
  constants (slots 0-25) come from the ring mirror ([ring_mirror.md](ring_mirror.md)).
- The engine's **cached material path `sub_828FCBA8`** (from `sub_8285F700`, before a mesh's buffer run) writes a prepared PM4 packet straight into the ring:
  `00022200 762 070E0706 0000000E | 2205 | 2104 FFFF | 2108 3EFEFF00 | 0001210D 00FFFF01 3B808081` = RB_DEPTHCONTROL, RB_BLENDCONTROL0 (SRC_ALPHA/INV_SRC_ALPHA),
  RB_COLORCONTROL (alpha test GEQUAL), PA_SU_SC_MODE_CNTL, RB_COLOR_MASK, RB_BLEND_ALPHA, RB_STENCILREFMASK, RB_ALPHA_REF. The shadows held CONSTANT_ALPHA blending
  (left by post-processing) and alpha function ALWAYS (left by the text renderer): the title's cloud panel (256x128 DXT5, left edge alpha 0..20) drew opaque (flat blue
  staircase = our alpha test cutting DXT5 blocks), the logo had white patches, the menu header was black, the dialog's background split. Fix: mirror the ring.
