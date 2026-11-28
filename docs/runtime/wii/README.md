# Wii runtime: what a Wii game needs from the host

> **Purpose:** what does the forge already have for a Wii game (SuperChargers Racing), and where is each piece? · **Status:** current (2026-11-25) · **Tags:** none

## What it is
The seventh game, Skylanders SuperChargers Racing, is a Wii game (issue #82). A Wii game links its SDK statically, so
there is no import table: the console is answered by hooks on the SDK's functions, found by symbol in the game's
executable. This area grows one piece per pull request, in the order of #82: the disc, the paired-single
instructions, the game's executable, the operating system, the GX renderer, the audio, the input and the portal.

| Piece | State |
|---|---|
| Disc: RVZ images, partitions, file system, hash check | done: [disc_rvz.md](disc_rvz.md) |
| CPU: the 750CL's paired singles, quantized loads and stores, GQRs, in XenonRecomp | done: [xenonrecomp.md](../../tools/toolchain/xenonrecomp.md#the-750cl-wii) |
| The game's code: its ELF translated (functions from its symbols, the jump tables, the CodeWarrior helpers) | done: [games/racing/](../../../games/racing/README.md) |
| The host: address space (MEM1 and MEM2 with their mirrors), executable, boot information, the first boot to the first IOS request | done: [memory_boot.md](memory_boot.md) |
| Threads, events (the interrupts), the decrementer and the video retrace | done: [threads.md](threads.md) |
| IOS: the calls, the state module, the disc drive | done: [ios.md](ios.md) |
| The NAND (`/dev/fs`, in a host folder), ES, the console's settings (SYSCONF, `setting.txt`) | done: [ios.md](ios.md) |
| Audio: the audio interface's DMA, the DSP running AX's task, its command lists (the voices advanced, silent) | started: [audio.md](audio.md) |
| GX textures: tiled console formats decoded to linear RGBA8 | done: [gx_textures.md](gx_textures.md) |
| A module loaded at run time (an RSO): linked offline where the host places it, translated with the game | done: [modules.md](modules.md) |
| GX: the write-gather pipe, the FIFO, the command processor, the vertices, a software renderer, the window | started: [gx.md](gx.md) |
| The remotes: remote 1 answered by the host, scripted presses for tests | started: [remotes.md](remotes.md) |
| Everything else | not started (#82) |

## Where the code is
[`runtime/disc/`](../../../runtime/disc/): `rvz.{h,cpp}` (the image), `wii_disc.{h,cpp}` (partitions, files, hashes),
`sha1.{h,cpp}`. [`vfs.cpp`](../../../runtime/vfs.cpp) opens an RVZ like any other disc. The CPU: the decoder's
750CL mode in [`tools/xenonrecomp/ppc_decoder.cpp`](../../../tools/xenonrecomp/ppc_decoder.cpp), the instructions in
`XenonRecomp/recompiler_750cl.inl` ([`patches/xenonrecomp.patch`](../../../patches/xenonrecomp.patch)), on with the
config's `paired_singles`. The host: [`runtime/wii/`](../../../runtime/wii/) (`memory_map`, `executable`, `boot_info`,
`os`, `processor`, `threads`, `timers`, `ios`, `ios_calls`, `ios_devices`, `ios_disc`, `ios_nand`, `nand`, `settings`,
`audio`, `ax`, `module`, `gx_fifo`, `gather_pipe.h`, `gx_vertex`, `gx_texture`, `gx_efb`, `gx_raster`, `video`, `remote`, `main`); the module's link: [`tools/rso_link.py`](../../../tools/rso_link.py), the game's bindings in `games/racing/` (`addresses.h`, `hooks.cpp`).

## How to test it
`ArkeyanRvzTest` (in `ctest` and `tools/test_all.sh`) builds synthetic images; `ArkeyanRvzTest --disc <image.rvz>`
lists a real disc's files and checks its hashes. `ArkeyanXenonRecompTest` and `ArkeyanPpcDecoderTest`: one sample per
750CL instruction, translated and run on the host. `ArkeyanWiiTest [scratch dir]`: the address space's mirrors, synthetic DOLs and ELFs,
the boot information, the processor, IOS, the NAND in a scratch folder, SYSCONF and `setting.txt`, AX's command lists, the module's area and link hash.
`ArkeyanGxFifoTest`: the write-gather pipe's lines and GX's commands ([gx.md](gx.md)). `ArkeyanWiiRemoteTest`: remote 1's
samples and the test script ([remotes.md](remotes.md)). `ArkeyanGxEfbTest`, `ArkeyanGxRasterTest`: the EFB's copies and
the software renderer ([gx.md](gx.md#software-renderer)).
`ArkeyanWiiRuntime <disc.rvz> <NAND folder>`: the boot, on the real disc (a scratch NAND folder for tests).

## Gotchas
- Only RVZ images, compressed with Zstandard (Dolphin's default) or not at all: no ISO or WBFS, which would need the
  console's key, and no bzip2 or LZMA.

## Pages
- [disc_rvz.md](disc_rvz.md): the RVZ image, the Wii disc and its hash tree.
- [audio.md](audio.md): the audio interface, the DSP and AX's command lists.
- [gx_textures.md](gx_textures.md): GX texture tiles, palettes and CMPR decoded to RGBA8.
- [modules.md](modules.md): an RSO module, linked and translated where the host loads it.
- [gx.md](gx.md): the write-gather pipe, GX's FIFO and the command processor.
- [remotes.md](remotes.md): remote 1 answered by the host, scripted presses.
- [memory_boot.md](memory_boot.md): the host's address space, the executable, the boot information, the first boot.
- [threads.md](threads.md): one processor between the game's threads, events, the timers.
- [ios.md](ios.md): IOS answered by the host, its devices.
- [The 750CL](../../tools/toolchain/xenonrecomp.md#the-750cl-wii) (in the recompiler's page): paired singles, GQRs,
  what the game's executable still needs.
