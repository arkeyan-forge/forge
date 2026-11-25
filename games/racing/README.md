# games/racing/: Skylanders SuperChargers Racing (Wii)

> **Purpose:** what is in SuperChargers Racing's game folder, how its code is translated, and where its docs are · **Status:** current (2026-11-23) · **Tags:** none

Status: the game's code and its module translate, compile and boot on the Wii host into the main loop, its GX commands read, nothing drawn yet ([status](docs/status.md)). Not in the kit, not
a build target (`ARKEYAN_GAME` does not take it yet). The Wii's other pieces: [docs/runtime/wii/](../../docs/runtime/wii/README.md).

| File | What |
|---|---|
| `config/Racing.toml` | XenonRecomp's input: the disc's ELF with `gamelogic.rso` linked in, `paired_singles`, the CodeWarrior register-save helpers |
| `config/Racing_switch_tables.toml` | the 231 jump tables (30 the module's), from [`tools/elf_switch_tables.py`](../../tools/elf_switch_tables.py) |
| `addresses.h` | the SDK functions the Wii host answers (`GAME_HOOK_<Name>`, found by name in the ELF) |
| `hooks.cpp` | each bound to the host's answer ([`runtime/wii/os.h`](../../runtime/wii/os.h)) |

Translate: take the disc's ELF (`files/goliath.engine.wii.final.elf`) and module (`files/Data/gamelogic.rso`) with any
Wii disc tool; copy `config/` to `private/racing/config/`; link the module into the ELF where the host loads it
([modules.md](../../docs/runtime/wii/modules.md)): `python3 -I tools/rso_link.py <elf> <rso> 0x81800000 <0x81800000 +
the RSO's size, 32-byte aligned> private/racing/private/main.elf --exports 0x80510498 206 --hash
private/racing/ppc/ppc_module_link.h`; then from `private/racing/` run `XenonRecomp config/Racing.toml
<thirdparty/XenonRecomp/XenonUtils/ppc_context.h>`: `ppc/` (147 files, 139 MB). A new disc version: `python3 -I
tools/elf_switch_tables.py private/racing/private/main.elf > games/racing/config/Racing_switch_tables.toml`.
Boot: `cmake -S . -B build-wii -G Ninja -DSKY_TESTS_ONLY=ON -DARKEYAN_WII_PPC_DIR=$PWD/private/racing/ppc`, `ninja -C
build-wii ArkeyanWiiRuntime`, then `build-wii/ArkeyanWiiRuntime <disc.rvz> <NAND folder>` ([memory_boot.md](../../docs/runtime/wii/memory_boot.md)).

## Docs
| Page | What |
|---|---|
| [docs/status.md](docs/status.md) | the disc, the translation, the first boot, what is next |
