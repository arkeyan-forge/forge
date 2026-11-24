# The Wii host: address space, executable, boot information, first boot

> **Purpose:** how does the Wii host lay out the guest's memory, load the game and start it, and how far does it get? · **Status:** current (2026-11-24) · **Tags:** [V]

## The address space
`wii::MapAddressSpace` (`runtime/wii/memory_map.{h,cpp}`) reserves the whole 32-bit space (guest address A at
base + A, as for the Xbox 360 games) and maps into it, everything else inaccessible:

| Range | What |
|---|---|
| `0x80000000`, `0xC0000000` (24 MB each) | MEM1, cached and uncached: **the same bytes** at both |
| `0x90000000`, `0xD0000000` (64 MB each) | MEM2, likewise |
| `0xCC000000`, `0xCD000000`, `0xCD800000` (64 KB each) | the register blocks: the GameCube's (VI, PI, MI, DSP, DI, SI, EXI, AI), the IPC registers to IOS, the Hollywood's; plain memory for now |
| `0xE0000000` (16 KB) | the locked cache (`dcbz_l`) |
| after the image | the translated code's function table (`PPC_IMAGE_BASE + PPC_IMAGE_SIZE`: past MEM1's end for SuperChargers Racing, `0x81EE1E60`) |

The two addresses of one memory are one host memory mapped twice: `PlatformReserveAddressSpace`,
`PlatformCreateSharedMemory`, `PlatformMapShared`, `PlatformMapPrivate` (`runtime/platform/platform.h`). POSIX: a
`PROT_NONE` reservation, then `MAP_FIXED` maps over it (`memfd_create` on Linux, an unlinked `shm_open` object on
macOS). Windows: a placeholder reservation (`VirtualAlloc2`, Windows 10 1803 and later), split at each range and
replaced by a view of a section (`MapViewOfFile3`) or by committed memory; both are looked up at run time, and without
them the reservation is released and each range mapped at its address. Not in the UWP build yet. Every range is
rounded out to 64 KB, Windows' granularity.

## The executable
`wii::LoadExecutable` (`runtime/wii/executable.{h,cpp}`) copies the sections into memory and clears the zeroed ones,
from either format, refusing a section outside MEM1 and MEM2, one past the file's end, or an entry outside the code:
- the **DOL** (`sys/main.dol`, what the console runs): 7 text and 11 data sections (file offsets at 0x00, addresses at
  0x48, sizes at 0x90), the zeroed range at 0xD8/0xDC, the entry at 0xE0. The zeroed range spans the small-data
  sections inside it (`.sdata`, `.sdata2`): only the gaps between them are cleared.
- an **ELF** (32-bit, big-endian, PowerPC): its `SHF_ALLOC` sections, `SHT_NOBITS` ones cleared.
SuperChargers Racing's DOL and ELF hold the same sections at the same addresses ([status](../../../games/racing/docs/status.md)):
the host loads the DOL, the translation reads the ELF for its symbols.

## The boot information
What the console's loader leaves in MEM1's first 16 KB before it jumps to the game (`wii::WriteBootInfo`,
`runtime/wii/boot_info.{h,cpp}`, from the public description of the Wii's low memory): the disc header's first 0x20
bytes (the ID, the maker, the Wii disc's magic `0x5D1C9EA3`) at `0x80000000`, the loader's mark `0x0D15EA5E` at 0x20,
MEM1's size (0x28, 0xF0, 0x3100, 0x3104), MEM2's (0x3118, 0x311C) and its arena (`0x90000800`-`0x933E0000`, IOS's IPC
buffers above it to `0x93400000`), the bus and processor clocks (243 and 729 MHz at 0xF8, 0xFC; the timebase ticks at
a quarter of the bus), the IOS version (0x3140: the title metadata's, `disc::WiiDisc::Ios`, 56 for SuperChargers
Racing) and the game ID again at 0x3180; the system menu's `setting.txt`, the console's region encrypted, at 0x3800
([ios.md](ios.md#the-consoles-settings-runtimewiisettingshcpp)).

## The first boot [V] (2026-11-24)
`ArkeyanWiiRuntime <disc.rvz> <NAND folder>` (`runtime/wii/main.cpp`; the NAND folder holds the console's NAND, made on
the first run: [ios.md](ios.md); built with `-DARKEYAN_WII_PPC_DIR=private/racing/ppc`, not by
default nor in CI) maps the space, loads `sys/main.dol`, writes the boot information, fills the function table and
calls the entry on a host thread with a 64 MB stack. The operating system's functions it answers
(`runtime/wii/os.{h,cpp}`, bound to the game's addresses in `games/racing/hooks.cpp`):
- `OSReport`, `OSPanic`, `OSFatal`: log lines, formatted by the Xbox host's guest printf (`kernel/guest_format.cpp`:
  only literal formats reach the host's), its words from r4-r10 and its doubles from f1-f8;
- `OSDisableInterrupts`, `OSEnableInterrupts`, `OSRestoreInterrupts`: the interrupt switch, which the translation
  does not keep (`mtmsr` is supervisor code);
- `EXIInit`, `SIInit`, `__OSInitSram`, `__OSInitAudioSystem`: skipped for now (each polls its hardware);
- `IOS_Open`, `IOS_OpenAsync`: the first request to IOS, where the boot stopped then (IOS is answered since:
  [ios.md](ios.md)).

SuperChargers Racing, Europe disc, in half a second: the SDK's banner (`Revolution OS`, kernel built Aug 23 2010,
`Firmware : 56.0.0` from the boot information's IOS, MEM1 arena `0x80800ec0`-`0x81730000`, MEM2 arena
`0x90000800`-`0x933e0000`, `RVL_SDK - OS release build`), then the first IOS request: `open "/dev/stm/immediate"`
(the state module: power, the reset button, the screen), exit code 0.

## Next
The boot goes on past the first IOS request: [threads.md](threads.md), [ios.md](ios.md).
