# games/imaginators/: Skylanders Imaginators (Alchemy engine)

> **Purpose:** what is in Imaginators' game folder, how to build it, and where its docs are · **Status:** current (2026-11-04) · **Tags:** [Xbox] [XDK] [Alchemy] [Imaginators]

Status: day 1 ([status](docs/status.md)). Not in the kit.

| File | What |
|---|---|
| `addresses.h` | the guest functions to hook (`GAME_HOOK_<Name>`) and the guest data it reads |
| `adapter.cpp` | the `game::Adapter` (`runtime/game.h`): the minimum |
| `imports.cpp` | the imports no game before SuperChargers had: SuperChargers' ten (included) and seven of its own |
| `art.json` | where the launcher's logo, background, portraits and menu theme are on the disc ([docs/art.md](docs/art.md)) |
| `info.cpp` | id, name and the supported disc release (title ID, media ID, SHA-256 of `default.xex`) |
| `game.txt` | what the player kit's Arkeyan-Builder knows about the game |
| `config/` | XenonRecomp input: `Imaginators.toml`, `Imaginators_switch_tables.toml` |

Build: `cmake -S . -B build-imaginators -G Ninja -DARKEYAN_GAME=imaginators`. The generated code is `private/imaginators/ppc/` and
the game's derived data `private/imaginators/`. The engine's hooks are in `engines/alchemy/`.

## Docs
| Page | What |
|---|---|
| [docs/status.md](docs/status.md) | disc identity, status, where to start, open problems |
| [docs/boot.md](docs/boot.md) | the first boots: how far they got |
| [docs/art.md](docs/art.md) | the launcher's pictures and menu theme on the disc |
| [docs/toybox.md](docs/toybox.md) | the Toy Box: creation crystals, Senseis, badges, the proof in the game |
| [docs/research.md](docs/research.md) | day 1's numbers, the engine, Creation Crystals (leads) |
