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

**Not yet**: the draws drawn (the vertices decoded and transformed, TEV, the textures, the EFB copies to the frame
buffer), the draw-done and token interrupts (`GXDrawDone`, `GXWaitDrawDone` skipped; `GXSetDrawSync`'s token written
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
