# The Wii host's GX: the write-gather pipe, the FIFO and the command processor

> **Purpose:** how do GX's commands get from the game's code to the host, and what does the host read from them? · **Status:** current (2026-11-25) · **Tags:** [V]

## How GX feeds the hardware
GX, the SDK's graphics library, writes every command to one address, the **write-gather pipe** (`0xCC008000`): the
processor gathers the stores (bytes, halves, words, floats, quantized pairs) into 32-byte lines and writes each full
line to the **CPU's FIFO** in memory, at the processor interface's write pointer (`0xCC003014`: the address in bits
5-28, the wrap bit `0x20000000`; the FIFO's base and end at `0xCC00300C` and `0xCC003010`), which moves on by 32 and
back to the base past the end. `GXFlush` writes 32 zero bytes (no-ops) to push out the last line. In GX's immediate
mode the graphics processor's FIFO is the same one (**linked**: the command processor's control register,
`0xCC000002`, has its read enable `0x1` and its link `0x10`) and the GP reads each line as it comes. A display list is
built with the CPU's FIFO pointed at the list's buffer, unlinked (`GXBeginDisplayList`), its length the write
pointer's progress (`GXEndDisplayList`); the FIFO then calls it by address and size.

## In the translation
In the code, GX's writes are stores through a register `lis` set to the pipe's upper half (`0xCC01`, the store's
displacement `-0x8000`), or one copied from it (`mr`, `addi`): XenonRecomp's `write_gather_pipe` option
([xenonrecomp.md](../../tools/toolchain/xenonrecomp.md#the-750cl-wii)) translates every store through such a register
as `PPC_MM_STORE_*` (`PPC_MM_STORE_PAIRED` for `psq_st`), and the host's
[`runtime/wii/gather_pipe.h`](../../../runtime/wii/gather_pipe.h), included before the translation, sends those whose
address is the pipe's to `wii::gx::GatherWrite` and the others to memory. The registers are found over the whole
function, not along its paths, so a register that also points elsewhere only adds a check. SuperChargers Racing
(2026-11-25): 4,644 such stores and 29 quantized ones; of the engine's 3,706 stores at displacement `-0x8000`, all but
4 go through such a register, and those 4 are ordinary memory (`addis` from a pointer). Its game module has none: it
draws through the engine's GX.

## On the host (`runtime/wii/gx_fifo.{h,cpp}`)
- **The pipe**: `GatherWrite` fills a 32-byte line, byte by byte in the CPU's order; a full line goes to the CPU's
  FIFO at the write pointer, which moves on (or wraps, the wrap bit set). Linked, the line goes to the command
  processor at once, and the GP's read and write pointers (`0xCC000038`, `0xCC000034`) are set to the CPU's, its
  distance to 0: the GP is never behind, so its watermarks never interrupt. The pointer wraps only on reaching the
  end's line: `GXRedirectWriteGatherPipe` points the FIFO at a buffer of its own (often in MEM2) with a 64 MB end
  below it, and the lines must go on there, not back to the base (low memory).
- **The pipe's address register** (WPAR, SPR 921, which the translation does not read or write): `PPCMfwpar` reads
  it empty (each line is gone at its 32nd byte), so the SDK's wait for the pipe to drain ends; `PPCMtwpar`
  (redirecting, restoring) empties the line begun, the zero padding the SDK writes to push the last line out. Both
  are hooks (`games/<id>/addresses.h`), `wii::gx::GatherReset` the second's.
- **The command processor** (`wii::gx::CommandProcessor`, one: `wii::gx::Processor()`): the FIFO's bytes in any pieces
  (a command cut by a line's end waits for the rest), each command run whole: `0x08` CP registers (the vertex
  descriptor `0x50`/`0x60`, the vertex formats `0x70`-`0x97`, the arrays' bases `0xA0`-`0xAF` and strides `0xB0`-`0xBF`),
  `0x10` the transform unit's memory and registers (`0x0000`-`0x1057`), `0x20`-`0x38` the indexed loads (XF words
  from arrays 12-15), `0x40` a display list (run from memory, not from a list), `0x61` BP registers (with `0xFE`, the
  next write's mask), `0x80`-`0xBF` draws (the primitive in bits 3-5, the vertex format in 0-2, a 16-bit count, then the
  vertices). Each draw's vertex size comes from the descriptor and its format: per attribute, nothing, direct (its
  components' count and format), or an 8- or 16-bit index (an NBT normal by three when the format says so).
  `onDraw`, `onCopy` (BP `0x52`, an EFB copy) and `onBp` hand them on. An opcode not known stops the stream until
  `Reset` (none in SuperChargers Racing's boot).
- **The counters**: `[vi] frame buffer N` logs the lines, commands, draws, vertices, lists and copies since the last
  frame buffer (frames 1-3, 60, 600).

## Vertices
[`runtime/wii/gx_vertex.h`](../../../runtime/wii/gx_vertex.h) exposes `Vertex` and `DecodeVertices`: a draw's raw
bytes become positions, normal/binormal/tangent vectors, two RGBA byte colours, eight texture coordinates and their
matrix indices, without allocation or a GPU. The descriptor (CP `0x50`/`0x60`) selects absent, direct, 8-bit indexed
or 16-bit indexed attributes; the draw's VAT slot selects CP `0x70`/`0x80`/`0x90` plus its format. Sizes agree with
`CommandProcessor::VertexSize`. Absent components are zero, including z for xy positions and t for s-only texture
coordinates. Absent matrix indices use CP `0x30` (position, then textures 0-3 in six-bit fields) and `0x40`
(textures 4-7); present indices retain the vertex's byte.

Position and texture components are big-endian u8, s8, u16, s16 divided by `2^shift`, or f32 unchanged. The position's
shift is VAT A bits 4-8; texture 0's is A bits 25-29; textures 1-3 use B bits 4-8, 13-17, 22-26; textures 4-7 use C
bits 0-4, 9-13, 18-22, 27-31. Normals use s8/64, s16/16384 or f32; NBT contains nine components in normal, binormal,
tangent order. Colours decode RGB565, RGB888 and RGB888x as opaque (x ignored), and RGBA4444, RGBA6666 and RGBA8888
with alpha. Packed channels expand by bit replication; RGBA6666 takes R, G, B, A as six bits each, high first.

An indexed attribute reads array 0 (position), 1 (normal), 2-3 (colours), or 4-11 (textures), at physical base
CP `0xA0 + array` plus index times stride CP `0xB0 + array`. The cached address is `0x80000000 | (physical & 0x1FFFFFFF)`;
the entire attribute must pass `InMemory` before it is read. Failure returns false and may leave partial output.
NBT uses one index to nine consecutive components, or, with VAT A bit 31 set, three indices to three components each.

`TriangleListSize` gives the output index count and `TriangleList` writes indices offset by `first`. Both quad
opcodes produce `(0,1,2), (0,2,3)` for each four vertices. Triangles retain their order. Strip triangle i uses
`(i,i+1,i+2)` when even and `(i+1,i,i+2)` when odd, preserving winding. Fans use `(0,i,i+1)` for i starting at 1.
Incomplete quads or triangles are omitted; strips and fans below three vertices, lines, line strips and points
produce no indices. Lines and points will use a separate drawing path.

`ArkeyanGxVertexTest` (in `ctest`) uses synthetic vertices: every numeric and colour format, xy positions, normal
and NBT encodings, all eight texture shifts, explicit and default matrices, both index widths in mapped MEM1 and
MEM2, and rejected out-of-memory attributes. Each decode case checks its byte size against `VertexSize`; primitive
tests check the lists, their winding, offsets and short counts. No game data or renderer is needed.

## Software renderer
The first renderer draws on the CPU, for correctness before speed (`runtime/wii/gx_raster.{h,cpp}`, `gx_efb.{h,cpp}`,
`video.{h,cpp}`). The command processor hands it each draw (`onDraw`) and each BP write (`onBp`: the TEV's colour and
konst registers share BP `0xE0`-`0xE7`, bit 23 choosing the bank).

- **The EFB** (`wii::gx::Efb`): 640 x 528 colours (RGBA8) and 24-bit depths, as the console's embedded frame buffer.
- **A draw**: its vertices decoded ([Vertices](#vertices)), transformed by the position matrix (XF `pmi * 4`), the
  projection (XF `0x1020`-`0x1026`) and the viewport (`0x101A`-`0x101F`, its 342 offset), the colour channels (XF
  `0x100C`, `0x100E`: the material from the vertex or the register, no lighting yet) and the texture coordinates
  (XF `0x103F`, `0x1040`); each triangle culled (BP `0x00` bits 14-15), clipped to the scissor (BP `0x20`, `0x21`) and
  rasterised; per pixel the TEV stages (BP `0xC0`/`0xC1`, their order `0x28`, konst selections `0xF6`), the alpha test
  (`0xF3`), the depth test and update (`0x40`) and the blend (`0x41`). Textures (BP `0x80`, `0x88`, `0x94`, `+0xA0` for
  maps 4-7) are decoded once a draw ([gx_textures.md](gx_textures.md)) and sampled nearest.
- **The EFB copy** (BP `0x52` with its "to the XFB" bit, `0x4000`): the source (`0x49`, `0x4A`) converted to YUYV
  (BT.601, video range) at the destination (`0x4B`, in 32-byte units; the stride `0x4D`), scaled in height by `0x4E`
  (256 / scale) when bit `0x400` is set, then the EFB cleared to `0x4F`-`0x51` when bit `0x800` is. A copy to a texture
  is not made yet.
- **The window** (`wii::video`): `VISetNextFrameBuffer`'s frame buffer, its size the last display copy's to it,
  converted back to RGBA and shown through the host's GPU layer (Metal on the Mac); `video::Pump` reads the window's
  events and remote 1's buttons ([remotes.md](remotes.md)) on the main thread. `SKY_WII_FRAMES=<folder>` writes frames
  1-3 and every 60th there as PPM pictures, for the tests (they hold the game's pictures: keep them in `private/`).

The position matrix and projection retain each vertex's clip-space `(x, y, z, w)` before the perspective divide.
Perspective triangles are clipped with Sutherland-Hodgman against GX's near plane (`z + w >= 0`) and a positive-w
guard (`w > 1e-6`); colours and texture coordinates are interpolated linearly in clip space at intersections.
Fixed arrays hold the polygon without allocating per triangle. The polygon is divided by w, mapped to the viewport,
culled and drawn as a triangle fan, with perspective-correct colour and texture interpolation. Orthographic
projections (`w = 1`) keep their existing path without near clipping. The triangle and cull counters count input
triangles, including those split into a fan; a triangle removed by clipping produces no pixels.

GX's clip-space depth from `-w` to `0` maps to the viewport's depth range. Both transformed vertex depths and the
interpolated pixel depth are clamped to `0..0xFFFFFF`, the latter before comparison and writing to the EFB. This
keeps far-plane roundoff from rejecting pixels against a buffer cleared to `0xFFFFFF` with a less-or-equal test.

`ArkeyanGxRasterTest` (in `ctest`) feeds synthetic GX commands and vertices in mapped memory to the software
rasteriser. It checks identity position matrices with perspective and orthographic projection, the viewport's 342
offset and depth mapping, flat and interpolated vertex colours, exact triangle and additive quad coverage, both
windings under every cull mode, inclusive scissor bounds, depth comparison and disabled depth updates, alpha
rejection, source-alpha blending, all four TEV konst colours through BP's shared register banks, and raster counters.
Clipping cases cover a vertex behind or on the eye plane, a triangle wholly behind the eye or outside the near
plane, the positive-w guard, both windings and every cull mode after clipping, and colour and texture interpolation
across new vertices. A far-plane triangle checks every pixel for holes with less-or-equal comparison, with depth
updates both enabled and disabled.

**Not yet**: lighting, the indirect texture stages, filtering and mipmaps, the EFB copies to textures, the GPU renderer;
the draw-done and token interrupts (`GXDrawDone`, `GXWaitDrawDone` skipped; `GXSetDrawSync`'s token written
back by its hook), an unlinked GP FIFO (a game that fills one FIFO while the GP reads another).

## How far it boots [V] (2026-11-25)
SuperChargers Racing, its boot to the main loop: the first frame buffer after 115 lines (1,011 commands, a draw of 4
vertices, 2 copies); each frame of the loop 22 lines, 90 commands, 3 draws of 4 vertices and one copy; no opcode not
known, no display list yet.

## How to test it
`ArkeyanGxFifoTest` (in `ctest`): the pipe's lines in an unlinked FIFO, the write pointer's wrap, a byte and a word
sharing a line, a redirected buffer past the end, the line emptied by WPAR's write; linked, CP and XF loads, a draw's vertices, a masked BP write, an EFB copy, the GP's pointers; a
display list called whole and one cut in a draw; an indexed load; vertex sizes from the descriptor and formats; a
command cut between two pieces; an opcode not known. `ArkeyanXenonRecompTest`: the stores `write_gather_pipe` marks
(`stw`, `sth`, `stb` through `mr`, `stfs`, `psq_st` through `addi`) and one it leaves.
