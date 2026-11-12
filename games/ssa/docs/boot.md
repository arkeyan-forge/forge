# SSA boot: from the disc to the title and to a level

> **Purpose:** what stood between SSA's disc and a playable level, in short, with links to the detail · **Status:** current (2026-10-26; a summary of the October 2026 work) · **Tags:** [Lux] [SSA] [Xbox]

SSA was the first port: its boot findings were written as they came, in the Lux pages, the gotcha list and the
port notes. This page is the map; the detail stays where it is.

## What broke on the way to the title
- **Kernel and XAM stubs that "succeed" without outputs** (country, music mute, XNADDR, enumerators, save names,
  TIME_FIELDS): implement the outputs, grep `[stub]` (D5, D9) ([gotchas.md](../../../docs/porting/gotchas.md)).
- **Floats byte-swapped numerically** (garbage viewports) and the physical window's +0x1000 offset (Bink planes) (D7,
  D8): [memory.md](../../../docs/runtime/xbox360/memory.md).
- **Indirect calls to unrecompiled code** (packed leaf functions, jump tables) (D13):
  [xenonrecomp.md](../../../docs/tools/toolchain/xenonrecomp.md).
- **The startup state machine, intro movies, the save manager**: [boot_saves.md](../../../docs/engines/lux/boot_saves.md)
  ([engine_notes §3.2](../../../docs/engines/lux/engine_notes.md)).
- **Menus without backgrounds, a white scene** (command buffers not replayed; clears that are draws) (D3, D4):
  [command_buffers.md](../../../docs/runtime/gpu/command_buffers.md),
  [edram_resolve_tiling.md](../../../docs/runtime/gpu/edram_resolve_tiling.md).
- Frontend glitches and their fixes: [render_bugs.md](../../../docs/runtime/gpu/render_bugs.md) ("Frontend").

## What broke on the way to a level
- **WideLoad hung the main thread** (`WaitForMultipleObjects` on zero handles) (D6):
  [str_levels_loading.md](../../../docs/engines/lux/str_levels_loading.md).
- **The game's zlib is slow when recompiled**: replaced by the host's ([zlib.md](../../../docs/engines/lux/zlib.md)).
- **Pink 3D, a stencil that never passes**: NGL writes state straight into the ring (D2):
  [ring_mirror.md](../../../docs/runtime/gpu/ring_mirror.md); the level frame pass by pass:
  [level_frame_lightprepass.md](../../../docs/engines/lux/level_frame_lightprepass.md).
- **A rare heap fault** (missing memory fences in the recompiled spin locks) (D1):
  [gotchas.md](../../../docs/porting/gotchas.md).
- Performance to 30 fps: [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md).
- Every trap of the port, numbered: [gotchas.md](../../../docs/porting/gotchas.md) (D1-D44, from
  [engine_notes §6](../../../docs/engines/lux/engine_notes.md)).

## Still open
- The heap crash of October (`sub_828A2360`) has not been seen since the fences: a long soak would confirm it
  ([100.md](100.md)).
- One unexplained fault once in about 25 runs during `tfb.bik` (D40) [?].
