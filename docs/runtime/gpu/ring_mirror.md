# The PM4 ring mirror (`d3d/ring.{h,cpp}`)

> **Purpose:** why do the device shadows lie, and how does the PM4 ring mirror recover the real GPU state? · **Status:** current (2026-10-26) · **Tags:** [XDK] [Lux] [SSA]

**The most important renderer mechanism, and the one to build first on a new Lux game.** Cost of building it late: the level frame was dark for ~3 work sessions
(we chased "stencil EQUAL 2 never passes" and EDRAM aliasing; the cause was NGL writing state straight into the ring).
Class: mechanism **[Xbox] [Lux]**, register list **[Lux]**, addresses **[SSA]**.

## Why

1. **Setters are inlined into game code**: `RB_DEPTHCONTROL` is written by 80 functions, `PA_SU_SC_MODE_CNTL` 82, `RB_COLORCONTROL` 40, the render-state cache `+0x2F98` 60, VS/PS
   constants (indexed), fetch constants `+0x480` (8), the vertex declaration `+0x2F58` (4). Harmless: read the shadows at draw time.
2. **Direct PM4 writes bypass the shadows** (the harmful part):
   - `ngl_WriteStencilRefDirect` (`0x8213BBB8`, light rendering): type-0 write of RB_STENCILREFMASK (0x210D): ref 0, read mask = `material[3]`, write mask 0xFF.
   - the **material state blocks** built by `sub_828FC670` (small-block pool) and emitted by `sub_828FCBA8` (called from the mesh run-state setter `sub_8285F700`): a prepared
     packet of RB_DEPTHCONTROL, RB_BLENDCONTROL0, RB_COLORCONTROL, PA_SU_SC_MODE_CNTL (0x2205), RB_COLOR_MASK (0x2104), RB_BLEND_ALPHA (0x2108), RB_STENCILREFMASK, RB_ALPHA_REF (0x210E).
   - D3D itself writes **shader literal constants** (VS c253-c255, PS c254-c255 in the first analysis; in fact from c255 down, e.g. PS c251-c253) only into the ring, and **FlushShaders** sends
     SQ_PROGRAM_CNTL / SQ_CONTEXT_MISC without updating their shadows.
   - others: `0x82893F10` (`ngl_FlushHiZDirect?`: EVENT_WRITE 0xF + RB_HIZCONTROL), `0x82629010` (bin mask), `0x822891F0` (InsertCallback), ListSendModule, D3D's `WriteWindowScissor` (`0x822974C0`).
     A full scan for game functions that advance `device+0x30` themselves was never finished (false positives).
   - the engine also **loads constant ranges it owns** (VS c20-c27, c46-c47, ...) straight into the ring: a level run found ~1.2 M ring/shadow mismatches on non-dirty registers.
3. Level_027 truth from the ring: Z-prepass `RB_DEPTHCONTROL 0x8767` (stencil REPLACE, ref 1), main pass `0x766/0x762` (**no stencil test**), G-buffer `0x726`. The shadows said
   "stencil EQUAL 2" which the GPU never runs.

## Mechanism

- `ScanRing(device)`: parse the packets between the last cursor and `device+0x30` (the last word written) ([xenos_registers_pm4.md](xenos_registers_pm4.md) for the packet types).
  While a command buffer is recorded (`device+0x35E0 != 0`) the writes go to `g_recorded` for the recorded draw instead.
- `ResyncRing(device)`: continue from the cursor after the write position moved elsewhere (another ring segment `0x822A2D78`, KickOff, Begin/EndCommandBuffer, Swap).
  `TakeRingJumps()` counts scans that found the cursor moved without a resync, detours the cursor recovers included (below): 0 per 60 frames on the title.
- **A jump is often a detour** (`d3d/ring_cursor.h`, issue #39, 2026-11-11) [SSA] [V]: around each small command buffer the write position reads
  `0xE0010000` (before `BeginCommandBuffer` and after `EndCommandBuffer`, until the next write reserves ring space) and `4` (the KickOff inside it);
  the ring then goes on from where the recording began. Reading from the new position dropped the packets the next draw wrote in the ring (its
  blend, colour control, depth and stencil blocks), so that draw used the one before's: alpha-blended clouds and sprites drawn opaque (black or white
  shapes), the sky black, a lit frame without its textures, for one frame. SSA records ~65 such buffers a frame while a level fades in (~195 jumps a
  frame on `level_027`'s start, ~1550 on `level_hub`'s load), hence "only at the first load". `RingCursor` keeps the ring position left at a jump and,
  when the cursor comes back ahead of it (within 4 MB), reads from there; following the ring outside a recording forgets it. Proof (Mac, Metal,
  scale 2, 60 FPS, [the new game recipe](../../getting-started/testing.md#the-fast-recipe-ssa-to-level_027-in-75-s-ssa), every frame from the title read back): before, a one-frame flash at `level_027`'s fade-in in 7 of 7 runs
  (also at 30 FPS, scale 1, `SKY_RESOLVE_DEFER=0`, `SKY_WRITE_WATCH=0`, `SKY_GPU_THREAD=0`); after, 0 in 7 runs on Metal and 2 on MoltenVK, the fade-in rising without a dip.
  `ArkeyanRingCursorTest` replays the logged sequence. Giants (title to `level_213_tutotial`, ~5900 frames) makes no jump: the cursor changes nothing there. Not reproduced on the hub through `SKY_LEVEL=level_hub` (with or without the fix): the player's
  hub flicker (arriving from `level_027`) is the same mechanism by its jumps [?].
  - **Not only at a load** (a second player's report, NVIDIA, D3D12 and Vulkan, 2026-11-11) [SSA] [V]: any moment that makes many objects at once
    records command buffers. On `level_027`'s bridge, the rock that breaks it makes 112-156 jumps in one second; before the fix the bridge, or a
    falling plank, is black for one frame (2 of 2 Mac runs, Metal, scale 2, the player's videos the same); after, 0 of 3 (Metal 2, MoltenVK 1).
    Walking and attacking on the grass makes 0-4 jumps per 60 frames and no flash. A temporary count of the reads that skip packets, with the fix:
    0 from the title through `level_027`'s fade-in and the bridge (~1300 detours recovered), 0 through the hub's load (1338).
    `ArkeyanRingCursorTest` also replays a longer detour logged there: placeholder, `4`, a position outside the ring, back to the place left.
- **`GpuRegister(device, reg)`** = the value the GPU uses for the draw the guest just sent. **The hooks scan *after* the guest's draw call** (the ring then holds exactly what D3D sent;
  guessing from the dirty bits beforehand caused flicker).
  - `FromRing(reg)` registers (the stream is right): `0x2200-0x2202`, `0x2205`, `0x2104-0x2108`, `0x210D`, `0x210E`, **every float constant `0x4000-0x47FF`**, and **texture fetch slots 0-25
    (`0x4800 + 6*slot`)**. All other registers come from the shadow (D3D keeps those right, and its own writes into the stream, such as a **Resolve's copy draw**, would go stale:
    the first mirror kept resolve values `RB_SURFACE_INFO 0A020280`, `PA_CL_VTE_CNTL 0x300`, `0x2301 C003` and the title logo disappeared).
  - `UseStream`: constants only use the stream value if written this draw epoch, or shader-owned, or during a replay, or the shadow is not dirty (D3D sends only the constants the shaders use;
    translated shaders may read others: a value written before this draw yields to a shadow still pending).
- `GpuFloatConstants(device, &version, &groupVersions)`: the 512 float constants in the device layout (big-endian), rebuilt only for groups of 16 registers that can have changed
  (a version per group lets `gpu::Draw` queue only changed groups: 6.9e6 draws checked, 0 mismatches).
- `SendInheritedShadows(device, cb)`: before a run, send the inherited dirty float/fetch groups from the shadows (walk dirty-bit groups, **not** all 10 536 registers: the first version cost 22 -> 30 fps).
- `DrawConsumed()` advances the epoch after each draw; `shaderOwned` marks literals written from an indirect buffer or loaded by `LOAD_ALU_CONSTANT`.

## Traps and rules (read before changing it)

1. **Forget D3D-owned registers after a Resolve** (or never take them from the ring): done by restricting the ring to the list above.
2. Take **param_gen enable and register from the pixel shader's own header**, never from SQ_PROGRAM_CNTL/SQ_CONTEXT_MISC (shadow *or* mirror): stale values gave saturated yellow
   ground (register 0x406 instead of 0x506, `COLOR0` replaced by the pixel position) and a missing/flickering sky (stale enable bit). See [shaders.md](shaders.md).
3. **RB_COLORCONTROL's alpha function is still ignored** (always GEQUAL): per level frame, 8 HUD draws use ALWAYS (`0x8700000F`) and 5 particle draws use GREATER (`0x8700000C`).
   Honouring the function from the shadows made the logo a rectangle (the shadow holds ALWAYS left by the text renderer): it is right only because the ring gives the real packet.
4. `RB_DEPTHCONTROL` of the G-buffer draws alternates (0x00700722 / 0x00000726): ring value of the previous material block; no visible effect.
5. Run `tools/perf/level_run.sh` after any change here (a regression 30 -> 21 fps went unnoticed for a day).
6. A **debug capture prints both**: `SKY_D3D_CAPTURE=<s>` shows the shadow registers and a `gpu` line (mirror values) per draw: diff them before chasing any "wrong blend/depth/stencil" bug.
