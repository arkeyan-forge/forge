# games/racing/: Skylanders SuperChargers Racing (Wii)

> **Purpose:** what is in SuperChargers Racing's game folder, how its code is translated, and where its docs are · **Status:** current (2026-11-23) · **Tags:** none

Status: the game's code translates, compiles and boots on the Wii host to its first IOS request ([status](docs/status.md)). Not in the kit, not
a build target (`ARKEYAN_GAME` does not take it yet). The Wii's other pieces: [docs/runtime/wii/](../../docs/runtime/wii/README.md).

| File | What |
|---|---|
| `config/Racing.toml` | XenonRecomp's input: the disc's ELF, `paired_singles`, the CodeWarrior register-save helpers |
| `config/Racing_switch_tables.toml` | the 200 jump tables, from [`tools/elf_switch_tables.py`](../../tools/elf_switch_tables.py) |
| `addresses.h` | the SDK functions the Wii host answers (`GAME_HOOK_<Name>`, found by name in the ELF) |
| `hooks.cpp` | each bound to the host's answer ([`runtime/wii/os.h`](../../runtime/wii/os.h)) |

Translate: put the disc's ELF (`files/goliath.engine.wii.final.elf`, read with `ArkeyanRvzTest --disc` or any Wii disc
tool) at `private/racing/private/main.elf`, copy `config/` to `private/racing/config/`, then from `private/racing/`
run `XenonRecomp config/Racing.toml <thirdparty/XenonRecomp/XenonUtils/ppc_context.h>`: `ppc/` (90 files, 81 MB). A
new disc version: `python3 -I tools/elf_switch_tables.py <elf> > games/racing/config/Racing_switch_tables.toml`.
Boot: `cmake -S . -B build-wii -G Ninja -DSKY_TESTS_ONLY=ON -DARKEYAN_WII_PPC_DIR=$PWD/private/racing/ppc`, `ninja -C
build-wii ArkeyanWiiRuntime`, then `build-wii/ArkeyanWiiRuntime <disc.rvz>` ([memory_boot.md](../../docs/runtime/wii/memory_boot.md)).

## Docs
| Page | What |
|---|---|
| [docs/status.md](docs/status.md) | the disc, the translation, the first boot, what is next |
