# Skylanders SuperChargers Racing (racing)

> **Purpose:** which disc SuperChargers Racing needs, how far its port is, and what is next · **Status:** current (2026-11-23) · **Tags:** [V]

## At a glance
| | |
|---|---|
| Disc | the Europe disc, `SKNP52` (Skylanders Superchargers Racing), as an RVZ image ([disc_rvz.md](../../../docs/runtime/wii/disc_rvz.md)) |
| Executable | `sys/main.dol`, SHA-256 `862c9de4102cd2e69820673fa17d3378c7319d219249e8543273b888c95c54e5`; the disc also ships `files/goliath.engine.wii.final.elf`, SHA-256 `a85732a7a297c4974bbd597e0ce838f085231bac244172b60db8fb1cefc167e9`, the same 11 sections at the same addresses (entry `0x80006310`) with 44,937 symbols: the translation reads the ELF |
| Code loaded at run time | `files/Data/gamelogic.rso`, a relocatable module (the game's logic, 3.1 MB of code): linked at `0x81800000` and translated with the ELF ([modules.md](../../../docs/runtime/wii/modules.md)) |
| Engine | its own (not Alchemy): nothing in `engines/` applies |
| Compiler | CodeWarrior for the Wii: paired singles, `lmw`/`stmw` prologues, `_savegpr_14` and its kin, `__ptmf_scall` |

## The translation [V] (2026-11-23)
`config/Racing.toml` with `paired_singles = true`: **21,847 functions** (21,733 from the ELF's symbols, covering 98 %
of `.init` and `.text`; the rest are the register-save helpers' entries and the gaps XenonRecomp's analysis splits),
**201 jump tables** (`tools/elf_switch_tables.py`: every `bctr`; 157 others are calls through a pointer), 90 files,
81 MB, under a second. The generated C++ compiles (clang, C++20, 0 errors). The log lists only:
- 610 sites of the operating system's supervisor instructions, left out of the translation (`rfi`, `mtmsr`, `mtsr`,
  `mtsrr0/1`, `mtsprg`, `mtspr`/`mfspr` on the BAT, HID, cache and debug SPRs, `sc`): the exception vectors and the
  kernel, which the Wii host replaces;
- 15 data words in `.init` (its first word and the copy and clear tables `__init_data` reads), decoded as code.

## The first boot [V] (2026-11-24)
On the Wii host (`ArkeyanWiiRuntime`, [memory_boot.md](../../../docs/runtime/wii/memory_boot.md)): `sys/main.dol`
loaded (11 sections and the zeroed range), the boot information written (IOS 56, from the title metadata), the entry
called. `OSInit` prints the SDK's banner (`Revolution OS`, kernel built Aug 23 2010, `Firmware : 56.0.0`, MEM1 arena
`0x80800ec0`-`0x81730000`, MEM2 arena `0x90000800`-`0x933e0000`), with EXI, SI, the SRAM and the DSP skipped, then
asks IOS for `/dev/stm/immediate`: the first request to IOS, where the host stops (exit code 0), in half a second.
Before the host answered `OSDisableInterrupts` and the rest, the same boot outside the runtime waited in `EXISync`.

## Further [V] (2026-11-24)
With the threads, the timers and IOS ([threads.md](../../../docs/runtime/wii/threads.md),
[ios.md](../../../docs/runtime/wii/ios.md)): past OSInit's firmware check, the disc drive opened, the first GX "draw
done", the engine's first thread (priority 17) started and handed back; then the game stops in its network library
(`bdSingleton.inl` line 29: Demonware's allocator not set), on a path that needs the NAND (`/dev/fs`) or ES.

## The engine's start [V] (2026-11-24)
With the NAND, ES and the console's settings ([ios.md](../../../docs/runtime/wii/ios.md)): the engine's start goes
through its saves, streams, resources, input, packages and graphics, hands VI its first frame buffer (`0x90572060`),
and waits in the audio interface's start (`__AI_SRC_INIT`). Three answers on the way, in `hooks.cpp`:
- the Bluetooth stack (`WUDInit`) is not started (its HCI over USB is not served) and reports itself ready
  (`WUDGetStatus` 3), so the remotes' library starts with no remote connected; Wwise waits for it at its start;
- `GXSetDrawSync`'s token is written back at once (the pixel engine's token register): `Flip` waits for it, and
  restarts the console (`OSRestart`) when it takes too long;
- `OSGetTime` reads the timebase's halves apart (`mftbu`, then `mftb`, then `mftbu`, until both high halves agree): the
  translation read the whole timebase for the high half, so the two rarely agreed and each alarm took about 1.6 ms
  (KPAD's start never got past them). Fixed in XenonRecomp ([xenonrecomp.md](../../../docs/tools/toolchain/xenonrecomp.md#the-patch-hunks-what-each-fixes-and-why)).

## Wwise and the game module [V] (2026-11-24)
With the audio hardware ([audio.md](../../../docs/runtime/wii/audio.md)): Wwise's start goes through (AX's frames, 96
voices, silent for now), the engine's streaming threads read the disc through the DVD library, and the engine loads
`gamelogic.rso` into the heap (`InitGL`); the call to its prolog (`0x80F07320` on this run) is the first to code that
was not translated. Two fixes on the way:
- the remotes' library started twice (the input's `WPADInit`, then Wwise's): `WUDInit` answers "started" only the
  first time, as the stack's own does, or `WPADInit` set its periodic alarm again while it was queued, and the alarm
  queue looped;
- a loop's switch in `RSOStaticLocateObject` (its offset in a register of its own, the counter bounds-checked) was
  taken for a call through a pointer: `tools/elf_switch_tables.py` now matches it (201 tables).

## The main loop [V] (2026-11-25)
With `gamelogic.rso` linked at `0x81800000` and translated with the ELF ([modules.md](../../../docs/runtime/wii/modules.md);
37,296 functions, 231 jump tables, 147 files, 139 MB): the game's link matches the translation's
(`[rso] the module at 0x81800000 is linked as its translation`), the module's prolog runs, the sockets library starts,
the engine's loading threads read the disc, and the main loop runs: 600 frames handed to VI in a minute (nothing
drawn yet: GX's commands are not). Two thread fixes on the way ([threads.md](../../../docs/runtime/wii/threads.md)): the
interrupt switch is each thread's (a thread resumed after a preemption spun in `Flip` with interrupts off), and an
OSThread made again gets a new host thread (the ended one returned from `OSExitThread`).

## Next
1. GX: the commands drawn, the frames shown.
2. The audio's mix and the host's output; EXI, SI (the GameCube controllers), the remotes.
3. The `*_as_local` options; the frame rate (600 frames in a minute, the retrace's 59.94 a second the target).
