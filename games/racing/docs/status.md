# Skylanders SuperChargers Racing (racing)

> **Purpose:** which disc SuperChargers Racing needs, how far its port is, and what is next · **Status:** current (2026-11-23) · **Tags:** [V]

## At a glance
| | |
|---|---|
| Disc | the Europe disc, `SKNP52` (Skylanders Superchargers Racing), as an RVZ image ([disc_rvz.md](../../../docs/runtime/wii/disc_rvz.md)) |
| Executable | `sys/main.dol`, SHA-256 `862c9de4102cd2e69820673fa17d3378c7319d219249e8543273b888c95c54e5`; the disc also ships `files/goliath.engine.wii.final.elf`, SHA-256 `a85732a7a297c4974bbd597e0ce838f085231bac244172b60db8fb1cefc167e9`, the same 11 sections at the same addresses (entry `0x80006310`) with 44,937 symbols: the translation reads the ELF |
| Code loaded at run time | `files/Data/gamelogic.rso`, a relocatable module: not translated yet |
| Engine | its own (not Alchemy): nothing in `engines/` applies |
| Compiler | CodeWarrior for the Wii: paired singles, `lmw`/`stmw` prologues, `_savegpr_14` and its kin, `__ptmf_scall` |

## The translation [V] (2026-11-23)
`config/Racing.toml` with `paired_singles = true`: **21,847 functions** (21,733 from the ELF's symbols, covering 98 %
of `.init` and `.text`; the rest are the register-save helpers' entries and the gaps XenonRecomp's analysis splits),
**200 jump tables** (`tools/elf_switch_tables.py`: every `bctr`; 158 others are calls through a pointer), 90 files,
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

## Next
1. The Wii host: the operating system's threads, alarms and interrupts; IOS (`/dev/stm`, the file system, the disc,
   the settings); EXI and SI; then VI and the first GX frame.
2. `gamelogic.rso`: load and translate the module.
3. The `*_as_local` options, once it boots.
