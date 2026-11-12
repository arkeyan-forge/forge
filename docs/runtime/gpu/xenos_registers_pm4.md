# Xenos registers and PM4 packets (what we read, what we parse)

> **Purpose:** which Xenos registers do we read and which PM4 packets do we parse? · **Status:** current (2026-10-26) · **Tags:** [Xbox]

Register indices are dword indices of the Xenos register file (Xenia's `register_table.inc` names). Layouts and enum values follow Xenia
(BSD): `d3d/xenos.h`, `d3d/device.h`, `d3d/ring.cpp`. Class **[Xbox]** throughout (D3D9LTCG 2.0.20500; other XDK builds may move
*device offsets*, not the registers).

## The two sources of GPU state

Option B means **no command processor is emulated**: the game's own D3D keeps writing a PM4 ring nobody executes. The renderer reads
the GPU state for each draw from two places and must merge them correctly:

1. **Shadows**: copies of register groups in the device structure (setters only write the shadow and set a dirty bit;
   [d3d9ltcg_device.md](d3d9ltcg_device.md)). Cheap, but they **lie** for anything the engine writes straight into the ring.
2. **The ring mirror** (`d3d/ring.cpp`): the PM4 stream the game wrote, parsed for register writes ([ring_mirror.md](ring_mirror.md)).

## Registers the renderer uses

| Register | Index | Used for | Source |
|---|---|---|---|
| RB_SURFACE_INFO | 0x2000 | surface pitch/MSAA (shadow +0x2880) | shadow |
| RB_COLOR_INFO | 0x2001 | EDRAM base tile (low 12 bits), color format (bits 16-19) of render target 0; targets 1-3 follow | surface object `+0x1C` |
| RB_DEPTH_INFO | 0x2002 | depth EDRAM base, format | surface object `+0x1C` |
| PA_SC_SCREEN_SCISSOR | 0x200E/0x200F | | shadow |
| PA_SC_WINDOW_OFFSET / SCISSOR TL,BR | 0x2080-0x2082 | per-tile window; **written directly to the ring by D3D, not dirty-tracked** (shadow +0x28C0) | device scissor rect at +0x327C |
| RB_COLOR_MASK | 0x2104 | per-target write mask, 4 bits each | **ring** |
| RB_BLEND_RED..ALPHA | 0x2105-0x2108 | blend constant (BLEND_FACTOR) | **ring** |
| RB_STENCILREFMASK | 0x210D | stencil ref (bits 0-7), read mask (8-15), write mask (16-23) | **ring** (written by `ngl_WriteStencilRefDirect` and state blocks) |
| RB_ALPHA_REF | 0x210E | alpha-test reference (float) | **ring** |
| PA_CL_VPORT_XSCALE, XOFFSET, YSCALE, YOFFSET, ZSCALE, ZOFFSET | 0x210F-0x2114 | viewport: `screen = ndc * scale + offset`; (shadow +0x2908..+0x291C) | shadow |
| SQ_PROGRAM_CNTL | 0x2180 | bit 18 `param_gen` (point coordinate / pixel position into a PS register) | **PS header, not shadow** |
| SQ_CONTEXT_MISC | 0x2181 | bits 8-15 `param_gen_pos` register | **PS header, not shadow** |
| RB_DEPTHCONTROL | 0x2200 | bit 0 stencil enable, bit 1 depth enable, bit 2 depth write, bits 4-6 depth func, bit 7 separate back-face stencil, bits 8-10 front stencil func, bits 11-13 / 14-16 / 17-19 front stencil fail / pass / depth-fail ops (Xenia order), back face the same fields from bit 20 (func 20-22, ops 23-25 / 26-28 / 29-31) | **ring** |
| RB_BLENDCONTROL0..3 | 0x2201, 0x2209-0x220B | bits 0-4 src color, 5-7 color op, 8-12 dst color, 16-20 src alpha, 21-23 alpha op, 24-28 dst alpha | **ring** (0) / shadow |
| RB_COLORCONTROL | 0x2202 | bit 3 alpha test enable (the function bits are **ignored**: always GEQUAL) | **ring** |
| PA_CL_CLIP_CNTL | 0x2204 | | shadow (resolve rewrites it) |
| PA_SU_SC_MODE_CNTL | 0x2205 | bit 0 cull front, bit 1 cull back, bit 2 front face CW; bits 11, 12, 13 polygon offset for front faces, back faces, points and lines (values in PA_SU_POLY_OFFSET_FRONT_SCALE, _FRONT_OFFSET, _BACK_SCALE, _BACK_OFFSET, 0x2380-0x2383; the host's depth bias, `PrepareDraw`) | **ring** |
| PA_CL_VTE_CNTL | 0x2206 | bit 0 viewport transform X/Y scale... **0 = screen-space draw** (shaders output pixels), bits 4/5 z scale/offset | shadow |
| RB_MODECONTROL | 0x2208 | | shadow |
| PA_SC_AA_CONFIG | 0x2301 | | shadow |
| RB_COPY_CONTROL .. DEST_INFO | 0x2318-0x231B | resolve parameters; **reset by `Resolve` before it returns**: never read | call arguments |
| RB_DEPTH_CLEAR / RB_COLOR_CLEAR | 0x231D / 0x231E | | call arguments |
| ALU constants | 0x4000-0x43FF VS (256 float4), 0x4400-0x47FF PS | float constants | **ring** per 16-register group |
| Fetch constants | 0x4800-0x48BF (32 slots x 6 dwords) | texture slots 0-25, vertex slots (stream s = 95-s) | **ring** (slots 0-25) |
| Bool / loop constants | 0x4900-0x4907 / 0x4908-0x4927 | shader booleans: VS bits 0-15 from 0x4900, PS bits 16-31 from 0x4904 | ring |

Blend factors (`xenos::BlendFactor`): 0 ZERO, 1 ONE, 4 SRC_COLOR, 5 INV_SRC_COLOR, 6 SRC_ALPHA, 7 INV_SRC_ALPHA, 8 DEST_COLOR, 9 INV_DEST_COLOR,
10 DEST_ALPHA, 11 INV_DEST_ALPHA, 12/14 CONSTANT, 13/15 INV_CONSTANT, 16 SRC_ALPHA_SATURATE. Ops: 0 ADD, 1 SUBTRACT, 2 MIN, 3 MAX, 4 REV_SUBTRACT.
**Xenos has no blend-enable bit**: ONE/ZERO/ADD for colour and alpha (`control & 0x1FFF1FFF == 0x00010001`) means **no blending**.
Compare functions 0-7: NEVER, LESS, EQUAL, LESS_EQUAL, GREATER, NOT_EQUAL, GREATER_EQUAL, ALWAYS. Stencil ops 0-7: KEEP, ZERO, REPLACE,
INCR_SAT, DECR_SAT, INVERT, INCR_WRAP, DECR_WRAP. Address modes: 0 WRAP, 1 MIRROR, 3/5/7 MIRROR_ONCE, 6 BORDER, else CLAMP.

## PM4 packets we parse (`d3d/ring.cpp` `Parse`)

Header word, big-endian in guest memory:

| Type (header >> 30) | Layout | Handling |
|---|---|---|
| 0 | `reg = header & 0x7FFF`, `count = ((header >> 16) & 0x3FFF) + 1`; bit 15 = write `count` times to the same register | write registers |
| 1 | writes two registers | skipped (12 bytes) |
| 2 | one-word filler | skipped |
| 3 | opcode `(header >> 8) & 0x7F`, `count` data words | see below |

Type-3 opcodes we act on:

| Opcode | Name | Payload | Effect |
|---|---|---|---|
| `0x2D` | SET_CONSTANT | `offsetType` (type in bits 16-23, offset 11 bits), values | `ConstantBase(type)` = 0 ALU `0x4000`, 1 fetch `0x4800`, 2 bool `0x4900`, 3 loop `0x4908`, 4 context regs `0x2000` |
| `0x55` | SET_CONSTANT2 | register index, values | write from `reg` |
| `0x3F` / `0x37` | INDIRECT_BUFFER (_PFD) | physical address, size in dwords | recurse (depth < 2) into prebuilt packets: **a shader's literal constants** |
| `0x2F` | LOAD_ALU_CONSTANT | physical source, type/offset, size | copy from memory: marked `shaderOwned` (a shader's literals, c255 downward, e.g. PS c251-c253) |

Not parsed but seen: `DRAW_INDX` (`0xC0012201` auto-indexed, `0xC0032201` with indices; preceded by `VGT_INDX_OFFSET`), `EVENT_WRITE` (0xF + HiZ
control), `EVENT_WRITE_EXT`, `EVENT_WRITE_SHD` (swap), `SET_BIN_MASK_LO` / `SELECT` (tiling and Z-pass), `INDIRECT_BUFFER` header `0xC0013F00`,
`IM_LOAD` microcode loads. **No DRAW packet is built outside the D3D library**: the draws are all inside D3D functions we hook.

## Primitive types (D3DPRIMITIVETYPE as the title passes them)

1 point list, 2 line list, 3 line strip, 4 triangle list, 5 triangle fan (unsupported on Metal/D3D12), 6 triangle strip, **8 rectangle list**
(3 corners per rectangle, the 4th = v1 + v2 - v0 attribute by attribute, expanded to 2 triangles), **13 quad list** (4 vertices, 2 triangles).
Rectangle/quad lists are used for UI and full-screen passes. Tessellated draws exist in the XDK (`0x8229B5E8`/`0x8229BA88`) but SSA's level frames use none.

## Vertex fetch constants

Stream `s` fetches through vertex fetch constant **`95 - s`** (2 dwords: address dword `& ~3` physical, size dword `(>> 2) & 0xFFFFFF` in dwords x 4 bytes);
device offset `+0x778 - 8*s`. Stride bytes: `device + 0x31E8 + s` (x 4). `BeginVertices` writes fetch constant 95 itself.
Index buffers: `device+0x318C` points at an IB object, `+0x18` = physical address; **16-bit indices** only so far (D42: Common bit 31 may mark 32-bit).

## Depth format facts

Depth is **D24FS8**: 20e4 floating-point depth in bits 31-8 (4-bit exponent, bias 15, 20-bit mantissa, range [0, 2)), stencil in bits 7-0.
**Guest depth is reversed** (GEQUAL passes, far = 0; the scene depth is cleared to 0, shadow maps to 1). `Float32To20e4` converts the host float depth back
when the CPU needs it ([lux/level_frame_lightprepass.md](../../engines/lux/level_frame_lightprepass.md)).
