# The D3D9LTCG device: layout, resources, functions (XDK 2.0.20500)

> **Purpose:** what is in the statically linked D3D9LTCG device (layout, dirty masks, resources), where are its functions in SSA, and which do we hook? · **Status:** current (2026-10-26) · **Tags:** [XDK] [Lux] [SSA] [Giants] [SwapForce]

The title statically links the Xbox 360 **Direct3D 9 with link-time code generation** ("LTCG"): D3D functions are inlined into game
code, so **there is no single setter to hook** for most state. Our rule: leave the game's own D3D running (its setters only write
the device structure, plus the ring), **replace only the functions that emit work** (draws, resolve, clear, swap, command buffers,
shader creation) and read the state from the device at draw time. Detail: [engine_notes.md](../../engines/lux/engine_notes.md) §4.1-4.7;
symbols: `games/ssa/config/SSA_symbols.txt`; code: `runtime/d3d/device.h`. Class **[XDK]** for the layout *if the XDK
build matches*; every address is **[SSA]** (register names follow Xenia's Xenos register table).

**Next game, day 1:** compare the XEX static-library versions. If D3D9LTCG is not 2.0.20500, re-derive every offset from the setters. Find
`Direct3D_CreateDevice` as "the caller of an allocation of 0x5F80 bytes" (the size changes with the XDK), then `SetStateToDefaults` (it walks two
tables of function pointers: every render-state setter), then the setters give the shadow offsets. The layout matches Unleashed
Recompiled's `GuestDevice` (GPL: read-only reference) up to ~+0x2780; later fields differ (newer XDK there).

## Device structure (0x5F80 bytes, 128-aligned) [SSA] (addr 0x822A09F0 creates it; the NGL global pointer is 0x82BB4F94)

| Offset | Content |
|---|---|
| +0x00 u64 | dirty: **VS float constants** (bit 63 = c0-c3; 1 bit = 4 vec4 = 16 registers) |
| +0x08 u64 | dirty: PS float constants |
| +0x10 u64 | dirty: register groups + shader link ([masks](#dirty-masks)) |
| +0x18 u64 | dirty: fetch constants (bit 31 = slot 0) and registers 0x2280-0x2294 (bits 54..34) |
| +0x20 u64 | dirty: 0x2300-0x2325 (bits 37..0), 0x2380-0x2387 (45..38), bit 56 = bool+loop constants; bits 49-54 -> `0x822A4D10` [?], 62-63 -> `0x822A0940` [?] |
| +0x28 u64 | mask ANDed with +0x10 to trigger `0x822A48B8` (HiZ flush?) [?] |
| +0x30 / +0x34 / +0x38 | **ring write cursor** (pre-increment `stwu x,4(p)`: points at the last word written), end, kick-off threshold. **Not +0x35E8.** |
| +0x40 / +0x224 | SetRenderState / GetRenderState function pointers (101 each) |
| +0x1D4 / +0x3B8 | SetSamplerState / GetSamplerState pointers (20 each) |
| +0x480 | fetch constants: 32 slots x 6 dwords (textures 0-25; vertex fetch for stream s at `+0x778 - 8s`, i.e. slot 95-s in 2-dword units) |
| +0x780 / +0x1780 | VS / PS float constants, 256 vec4 each |
| +0x2780 / +0x2790 / +0x27A0 | VS bool (128 bits), PS bool, loop constants (registers 0x4900-0x4927) |
| +0x2880.. | **shadow registers**: 0x2000-0x200F at +0x2880, 0x2080-0x2082 at +0x28C0 (direct-to-ring, not dirty-tracked), 0x2100-0x2114 at +0x28CC (0x2104 colour mask at +0x28DC, 0x210E alpha ref at +0x2904, viewport 0x210F-0x2114 at +0x2908..+0x291C), 0x2180-0x2184 at +0x2920, 0x2200-0x220B at +0x2934 (0x2200 depth control +0x2934, 0x2201 +0x2938, 0x2202 colour control +0x293C, 0x2205 mode +0x2948), 0x2280-0x2294 at +0x2964, 0x2300-0x2325 at +0x29B8, 0x2380.. at +0x2A50 |
| +0x2A18..+0x2A44 | RB_COPY_* shadows (**reset by Resolve before it returns**) |
| +0x2B08 | owner thread id (NGL writes it, 21 functions) |
| +0x2B10 / +0x2B14 / +0x2B1C | retired-fence pointer / interrupt info block / current fence |
| +0x2B3C..+0x2B40 | flag bytes (bit 0 of +0x2B3C = Z-pass / predicated draw path) |
| +0x2F58 | current vertex declaration (dirty +0x10 bit 19); **SetVertexDeclaration is fully inlined** |
| +0x2F78..+0x3004 | render-state value cache for the getters (+0x2F80 = SCISSORTESTENABLE; +0x2F98 written by 60 inlined sites) |
| +0x3188 / +0x318C | FVF / index buffer |
| +0x3190 | render targets [4] (surface objects) |
| +0x31A0 | depth-stencil surface |
| +0x31A4 / +0x31E8 | streams [16] (vertex buffers) / stream strides (bytes, x4) |
| +0x31F8 | textures [26] |
| +0x3260..+0x3274 | viewport floats X, Y, W, H, MinZ, MaxZ |
| +0x327C..+0x3288 | scissor rect (left, top, right, bottom): **the window scissor is not shadowed elsewhere** |
| +0x328C / +0x3290 | **pixel shader / vertex shader** (the first symbol file had these swapped, D11) |
| +0x329C/+0x32A0/+0x32A4, +0x32C4.. | tiling: bin masks, tile count (+0x32C4), tile rects (+0x32CC..) |
| +0x34B0.. | Z-pass fixup list |
| +0x3594 | BeginVertices packet pointer (inline EndVertices sets +0x30 = +0x3594) |
| +0x35E0 | **command buffer being recorded** (0 = none) |
| +0x35E8 / +0x35EC (..+0x35F8) | resources referenced while recording |
| +0x3630 | D3DRS_PRESENTINTERVAL |
| +0x425C, +0x5EAC | gamma ramp? [?] |

`D3DDevice_SetStateToDefaults` (`0x822B1AB8`) walks the **RS table `0x82A426C0`** ({Get, Set, Default} x 101, value = 4*index, first 10 unused) and the
**SS table `0x82A42B80`** (x 20) and copies the Set/Get pointers into the device. All 101 + 20 setters were decoded (with register and dirty bit, in a private
`rsdump.txt`, not in the tree); they only write shadows and dirty bits.

## Later XDKs: 21076 (Giants) and 21250 (Swap Force) [XDK] [Giants] [SwapForce] [V]

What the runtime reads, per XDK build (the `namespace game` constants of `games/<id>/addresses.h`):

| | 20500 (SSA, D3D9LTCG) | 21076 (Giants, D3D9LTCG) | 21250 (Swap Force, **D3D9**) |
|---|---|---|---|
| Fields from the render-state cache on (`DeviceStateShift`) | +0 | +0x80 | +0x80 |
| Vertex declaration (`DeviceVertexDeclaration`) | 0x2F58 | 0x2FD0 | 0x2FD0 |
| Constants, register shadows, fences (0x480-0x2B1C), ring cursor +0x30 | | unchanged | unchanged |
| `DrawIndexedVertices` base vertex (`DrawIndexedBaseVertex`) | r5 | none | r5 again |
| `Resolve` DestLevel (`ResolveHasLevel`) | r8 | none (slice r8, color r9, stencil sp+0x54) | r8 again; the public function wraps a worker with two more arguments |
| `BlockOnFence` | as Giants [?] | flag byte r6 | flag byte r7 (one more argument) |
| Device size | 0x5F80 | 0x6080 (cleared by CreateDevice) | 0x6080 (allocated, 128-aligned) |

- **D3D9 without LTCG** (Swap Force): the setters are ordinary functions (`SetVertexDeclaration` is
  `stw r4,0x2FD0(r3)` plus dirty bit 19 of +0x10), so they can be found by shape against SSA's symbols (0.97-1.00) and
  give the offsets directly. The renderer does not hook them: the state is still read at draw time.
- Found with `tools/xmatch.py` (`.:giants` takes Giants as the reference). Addresses and evidence:
  [games/giants/docs/boot.md](../../../games/giants/docs/boot.md) "Direct3D",
  [games/swapforce/docs/boot.md](../../../games/swapforce/docs/boot.md) "Direct3D".

## Dirty masks

Each group lists its **first register in its highest bit** (MSB-first). `d3d/device.h` `RegisterGroups` encodes them:

| Mask (device +) | Bits -> registers |
|---|---|
| +0x00 / +0x08 | VS / PS float constants, bit 63 = c0-c3 ... bit 0 = c252-c255 |
| +0x10 | 0x2200-0x220B -> bits 11..0; 0x2180-0x2184 -> 16..12; **shader link 17-20** (FlushShaders); 0x2100-0x2114 -> 41..21; 0x2000-0x200F -> 57..42 |
| +0x18 | fetch slots 0-31 -> bits 31..0; 0x2280-0x2294 -> 54..34 |
| +0x20 | 0x2300-0x2325 -> 37..0; 0x2380-0x2387 -> 45..38; bit 56 bool + loop constants |

Example: RB_STENCILREFMASK `0x210D` is bit `41 - 0xD` = **bit 28 of +0x10**; RB_COLOR_INFO is `57 - 1`, RB_DEPTH_INFO `57 - 2` of +0x10.
`WriteDirtyRegisters` (`0x822A4C50`: `(dev, mask<<, firstReg, shadowPtr)`) writes type-0 packets for set bits; `WriteDirtyAluConstants` (`0x822A4FE8`, one bit = 4
vec4), `WriteDirtyFetchConstants` (`0x822A4E90`), `FlushShaders` (`0x822A5E08`: links VS/PS, `IM_LOAD` microcode type 0 VS / 1 PS, `SQ_PROGRAM_CNTL`),
`MakeSpaceForRegisters` (`0x822A4608`, ring overflow). **FlushShaders sends SQ_PROGRAM_CNTL/SQ_CONTEXT_MISC into the ring without updating the shadow (+0x2920)**:
read param_gen from the pixel shader header ([shaders.md](shaders.md)).

## Resources

Resource header (0x18 bytes): `+0` Common (low 4 bits type: **1 VB, 2 IB, 3 texture, 4 surface, 5 declaration, 6 vertex shader, 7 pixel shader**; bits 8-11 lock count;
bit 30 surface-of-texture; `0x100000` D3D-created; **bit 31 = 32-bit indices** [?]), `+4` refcount, `+8` Fence, `+0xC` ReadFence, `+0x10` Identifier, `+0x14` BaseFlush.
- Texture: `+0x18` MipFlush, **`+0x1C..+0x30` the GPU texture fetch constant** (6 dwords). VB: vertex fetch constant at `+0x18` (address dword, size dword).
  IB: `+0x18` address. Surface: `+0x18` parent texture, `+0x1C` RB_COLOR_INFO / RB_DEPTH_INFO word (EDRAM base tile, format),
  **`+0x24` size = (w-1) << 18 | (h-1) << 3**.
- `D3DResource_GetType` (`0x82296A40`): 3 texture, 0x12 cube, 0x13 array, 0x14 line; 0x10 and 0x11 both noted "volume" [?].
  `LockInternal` (`0x822963B8`: blocks on fence, `++lock count`, cache coherency packets), `UnlockInternal` (`0x822966B8`: `dcbst` BaseFlush/MipFlush ranges).
  The Create functions for textures/surfaces/buffers were **never identified** (not needed: everything is read from fetch constants; costs a first-frame
  decode: [renderer_threads.md](renderer_threads.md)).
- Shader objects: `CreateVertexShader` (`0x82290508`: type 6 object, 0x368-byte header with 2 x 0x1A0 bind slots, microcode pointer +0x20), `CreatePixelShader`
  (`0x82290160`: type 7, 0x28 header, microcode in physical memory, pointer +0x18), `XGGetMicrocodeShaderParts` (`0x822900D8`). **Create*Shader rewrites parts of the
  container in place: hash the container before calling the original** (D14).
- Vertex declaration object: header 0x34 bytes, **element count at +0x18**, elements of 12 bytes from +0x34 (`u16 stream, u16 offset, u32 type, u8 method?, u8 usage (+9), u8 usageIndex (+10)`), end marker 0xFF; Common `0x00100005`.
  `CreateVertexDeclaration` `0x82290908` (object 0x38 + 12n), `SetFVF` `0x822907C8` derives a declaration into +0x2F58.

## Function address table [SSA]

Format: address, name, notes. `V` verified. Names ending `?` are uncertain.

| Address | Function | Notes |
|---|---|---|
| 0x822A09F0 | Direct3D_CreateDevice | allocates 0x5F80 bytes; NGL stores the pointer at 0x82BB4F94 |
| 0x822B1AB8 | SetStateToDefaults | walks RS/SS tables |
| 0x822B21B0 | D3D_SetDisplayMode | only caller of `VdSetDisplayMode` |
| 0x822B2348 | D3D_InitializeEngines | `VdInitializeEngines`, `VdSetGraphicsInterruptCallback` |
| 0x822A4210 | InitializeRingBuffer | `VdInitializeRingBuffer`, `VdEnableRingBufferRPtrWriteBack`, INDIRECT_BUFFER |
| 0x822A3BB0 / 0x822A3CB8 | KickOff / BlockOnFence | **hooked** (null GPU) |
| 0x822A3DB0 | InsertCallback | **hooked** |
| 0x822A4000 | D3D_GraphicsInterrupt | registered via `VdSetGraphicsInterruptCallback` |
| 0x822A2D78 | ring segment switch | **hooked** (`ResyncRing`) |
| 0x8229AD58 / 0x8229B170 | DrawVertices / DrawIndexedVertices | **hooked**; `(dev, prim, start, count)`, `(dev, prim, baseVertex, startIndex, indexCount)` |
| 0x8229A850 / 0x8229AD10 | BeginVertices / DrawVerticesUP | BeginVertices **hooked**; `(dev, prim, count, stride) -> ptr` (allocates via 0x822A2BD8, writes vfetch constant 95 + DRAW_INDX; EndVertices inlined) |
| 0x8229B5E8 / 0x8229BA88 | Draw(Indexed)TessellatedVertices? | callers 0x8262B3D0 / 0x8262B430; unused in level_027 |
| 0x822ADDE0 | ClearF worker (also BeginTiling / BeginZPass) | **hooked**: clears via an inline rectangle draw |
| 0x822AD840 | builds the clear's depth-control values (0x76 / 0x8701) | |
| 0x822A1560 | Resolve | **hooked**; 10th argument on the stack |
| 0x822A2550 | resets RB_COPY_* shadows | |
| 0x822A17AC | `copy_dest_swap` from the destination swizzle | |
| 0x822A95C0 | Swap | **hooked**: `VdGetSystemCommandBuffer`, `VdSwap`, EVENT_WRITE_SHD |
| 0x822A9088 | FrameEnd? | maps the present interval |
| 0x822A6BC8 / 0x822A6FB8 / 0x822A73D8 | Begin / End? / RunCommandBuffer | **hooked** ([command_buffers.md](command_buffers.md)) |
| 0x8228FDB8 / 0x8228FE90 | SetVertex/PixelShaderConstantF | `(dev, start, data, vec4Count, dirtyMask)` copies to dev+0x780+16*start (VS), +0x1780 (PS) |
| 0x8228FF68 / 0x8228FFC8 | Set VS / PS ConstantB | bits at +0x2780 / +0x2790; +0x20 |= 1<<56 |
| 0x82290028 / 0x82290080 | Set VS ConstantI / PS ConstantI? | loop constants +0x27A0 + 4i |
| 0x82290250 / 0x822905F8 | SetPixelShader (+0x328C) / SetVertexShader (+0x3290) | **were swapped in the first symbol file** |
| 0x82290160 / 0x82290508 | CreatePixelShader / CreateVertexShader | **hooked** (hash first) |
| 0x82290410 | D3DVertexShader_Init | resets VS bind slots |
| 0x82290988 | SetShaderGPRAllocation | `(dev, flags, vsGPRs, psGPRs)`, runs 3 indirect buffers; ListSendModule sets 32 VS / 96 PS |
| 0x8229E100 | SetTexture | `(dev, sampler, tex, dirtyMask)`; fetch const merged into dev+0x480+0x18*s, ptr at +0x31F8+4s |
| 0x82299E98 | SetStreamSource | `(dev, stream, vb, offset, stride, dirtyMask)` |
| 0x82299FB8 | SetIndices | ptr +0x318C |
| 0x8229A048 / 0x8229A3D8 | SetRenderTarget / SetDepthStencilSurface | `(dev, idx, surf)`, copy surf+0x1C to the RB_COLOR_INFO shadow |
| 0x82297430 | UpdateSurfaceInfo? | RB_SURFACE_INFO / PA_SC_AA_CONFIG from the surface, resets scissor + viewport |
| 0x82299E18 / 0x822977B0 | SetViewport / SetViewportF | PA_CL_VPORT_* shadows +0x2908..+0x291C |
| 0x82299D20 / 0x822974C0 | SetScissorRect / WriteWindowScissor | **direct ring write** of 0x2080-0x2082, per tile when tiling |
| 0x822A5E08 | FlushShaders | |
| 0x822A4C50 / 0x822A4FE8 / 0x822A4E90 | WriteDirtyRegisters / AluConstants / FetchConstants | |
| 0x822A48B8 | FlushHiZ? | called when dirty +0x10 & +0x28 |
| 0x822A4608 | MakeSpaceForRegisters | |
| 0x822A25E8 | ReserveCommandSpace? | 19 callers |
| 0x822AE608 / 0x822AE778 / 0x822AEAD0 | SetPredication / BeginTiling / EndTiling | callers 0x8262D3E8 / 0x8262D608 |
| 0x822AEF88 / 0x822AF218 | BeginZPass? / EndZPass? | fixup list +0x34B0 |
| 0x8229E290..0x8229E5E8 | Begin/EndConditionalSurvey / Rendering? | VIZ_QUERY, PA_SC_VIZ_QUERY (+0x29B0) |
| 0x82296948 / 0x822968D0 / 0x822969C8 / 0x82296A40 | Resource Release / AddRef / GetDevice / GetType | |
| 0x822963B8 / 0x822966B8 | Resource Lock / UnlockInternal | |
| 0x8229CCB0, 0x8229CDB0, 0x8229D3F0, 0x8229D480, 0x8229DFC8, 0x8229E060, 0x8229E080, 0x8229DA90 | texture/volume/surface GetLevelDesc, LockRect/LockBox, GetDesc, Lock/UnlockRect, GetLevelCount | |
| 0x82297A10 / 0x82297A70 / 0x82297AA8 / 0x82297BC8 / 0x82297C58 / 0x82297ED0 / 0x82298148 / 0x82298520 / 0x82298700 / 0x822992C0 | SetRenderState CULLMODE / ALPHATESTENABLE / ALPHABLENDENABLE / SRCBLEND / DESTBLEND / ALPHAREF / ZENABLE / SCISSORTESTENABLE / COLORWRITEENABLE / PRESENTINTERVAL | shadow offsets in the symbol file |
| 0x822A0C50 | UnbindAll? | sets RT/DS/shaders/streams/textures/indices to NULL |
| 0x822A86C8 / 0x822A8A40 | CaptureStateBlock? / StateBlock_Apply? | uncertain (caller 0x82850698) |
| 0x8229A6B0 | SetGammaRamp? | +0x425C, +0x5EAC |
| 0x82291448-0x82296028 | XGraphics shader assembler? | context stride 0x26D0; the D3DX HLSL compiler "2.0.20500.0" is at 0x822F..0x823F |
| 0x822E5B88 | D3DXGetShaderConstantTable | |

## What we hook in the renderer (SSA) [V]

DrawVertices, DrawIndexedVertices, BeginVertices (draw snapshotted at the call, sent at the next D3D call once the game filled the buffer), Resolve, the ClearF
worker `0x822ADDE0`, Swap, Begin/End/RunCommandBuffer, KickOff/BlockOnFence/InsertCallback (null GPU), the ring-segment switch `0x822A2D78`,
Create{Vertex,Pixel}Shader, `BinkDoFrame`, `Movie_Update` (test aid), and the game's depth-reading CPU job `sub_821404D8`. Plus the two NGL
"empty frame" guards and the heap-free guard ([lux/ngl_renderer.md](../../engines/lux/ngl_renderer.md)). The engine's direct ring writers
(`ngl_WriteStencilRefDirect` `0x8213BBB8`, the material state packet `sub_828FCBA8`) are **not hooked any more**: the ring mirror parses what they write
([ring_mirror.md](ring_mirror.md)). Every hook is a `PPC_FUNC(GAME_HOOK(Name))` that exists only when the game's `games/<id>/addresses.h` defines
`GAME_HOOK_Name`: **SSA defines 39 names, Giants 26, 47 distinct** (counted 2026-10-26; the table and the recount
command: [architecture/hooks.md](../../architecture/hooks.md)). Finding them in a new binary:
[toolchain/sigmatch.md](../../tools/toolchain/sigmatch.md).

## Draws, the frame, and code that bypasses the device API [Lux] [SSA] [V]

- **Draws.** No DRAW packet is built outside the D3D library. Draw functions inline the whole dirty flush (ALU
  constants, FlushShaders, register groups, fetch constants), then emit `VGT_INDX_OFFSET` + a predicated `DRAW_INDX`. In
  Z-pass mode they also emit `SET_BIN_MASK_LO` + `EVENT_WRITE_EXT` and record a fixup (+0x34B0).
- **Inlined setters** write shadows and dirty bits directly: RB_DEPTHCONTROL (80 functions), PA_SU_SC_MODE_CNTL (82),
  RB_COLORCONTROL (40), the render-state cache +0x2F98 (60), VS/PS constants (indexed), fetch constants +0x480 (8), the
  vertex declaration (4). Harmless, since the state is read at draw time.
- **Direct ring writers** (read by the ring mirror, [ring_mirror.md](ring_mirror.md)): `0x8213BBB8`
  (RB_STENCILREFMASK), `0x82893F10` (EVENT_WRITE + HiZ), `0x82629010` (bin mask), `0x822891F0` (InsertCallback),
  ListSendModule. A full scan was never done [?].
- **The frame** (ListSendModule `0x826320C8`): GPR allocation -> invalidate the NGL state cache (`0x82BA4F70`) ->
  InsertCallback -> nglSceneTraverse (Begin/EndTiling, Z-pass, draws) -> InsertCallback -> fence -> `0x822A9088` ->
  nglPresent: Swap(dev, front-buffer texture). Predicated tiling **is** used (EndTiling resolves per tile).
