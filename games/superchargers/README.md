# games/superchargers/: Skylanders: SuperChargers (Alchemy engine)

> **Purpose:** what is in SuperChargers' game folder, how to build it, and where its docs are · **Status:** current (2026-11-04) · **Tags:** [Xbox] [XDK] [Alchemy] [SuperChargers]

Status: day 2; the first level plays with a figure and a vehicle ([status](docs/status.md)). Not in the kit.

| File | What |
|---|---|
| `addresses.h` | the guest functions to hook (`GAME_HOOK_<Name>`) and the guest data it reads |
| `adapter.cpp` | the `game::Adapter` (`runtime/game.h`): the minimum |
| `imports.cpp` | the ten imports no earlier game had |
| `info.cpp` | id, name and the supported disc release (title ID, media ID, SHA-256 of `default.xex`) |
| `game.txt` | what the player kit's Arkeyan-Builder knows about the game |
| `config/` | XenonRecomp input: `SuperChargers.toml`, `SuperChargers_switch_tables.toml` |

Build: `cmake -S . -B build-superchargers -G Ninja -DARKEYAN_GAME=superchargers`. The generated code is `private/superchargers/ppc/` and
the game's derived data `private/superchargers/`. The engine's hooks are in `engines/alchemy/`.

## Docs
| Page | What |
|---|---|
| [docs/status.md](docs/status.md) | disc identity, status, where to start, open problems |
| [docs/boot.md](docs/boot.md) | what broke on the way to the first level |
| [docs/art.md](docs/art.md) | the logo, background, portraits and menu theme the launcher reads from the disc |
| [docs/toybox.md](docs/toybox.md) | the Toy Box: vehicle seats, the vehicles' gearbits, the badges of every toy |
| [docs/shaders.md](docs/shaders.md) | the shaders on the disc (IGA v11 big-endian, IGZ v9) and the cache |
| [docs/research.md](docs/research.md) | day 1's numbers, vehicles and the portal (leads) |
