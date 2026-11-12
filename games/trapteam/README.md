# games/trapteam/: Skylanders: Trap Team (Alchemy engine)

> **Purpose:** what is in Trap Team's game folder, how to build it, and where its docs are · **Status:** current (2026-11-01) · **Tags:** [Xbox] [XDK] [Alchemy] [TrapTeam]

Status: in the kit and off by default; the tutorial is playable ([status](docs/status.md)).

| File | What |
|---|---|
| `addresses.h` | the guest functions to hook (`GAME_HOOK_<Name>`) and the guest data it reads |
| `adapter.cpp` | the `game::Adapter` (`runtime/game.h`) |
| `info.cpp` | id, name and the supported disc release (title ID, media ID, SHA-256 of `default.xex`) |
| `game.txt` | what the player kit's Arkeyan-Builder knows about the game |
| `config/` | XenonRecomp input: `TrapTeam.toml`, `TrapTeam_switch_tables.toml` |
| `swap_wait.cpp` | the render thread's inlined spin on its last swap (a mid-instruction hook in `TrapTeam.toml`) made a wait for the next graphics interrupt (2026-11-03, [renderer_threads.md](../../docs/runtime/gpu/renderer_threads.md), "Trap Team's swap wait") |

Build: `cmake -S . -B build-trapteam -G Ninja -DARKEYAN_GAME=trapteam`. The generated code is `private/trapteam/ppc/` and the game's
derived data `private/trapteam/`. The engine's hooks are in `engines/alchemy/`.

## Docs
| Page | What |
|---|---|
| [docs/100.md](docs/100.md) | what stable means for Trap Team, and what is left |
| [docs/010.md](docs/010.md) | the 0.1.0 checklist: the first chapters, item by item |
| [docs/status.md](docs/status.md) | disc identity, status, where to start, open problems |
| [docs/boot.md](docs/boot.md) | what broke on the way to the title and to a level |
| [docs/research.md](docs/research.md) | public research and day 1's numbers |
| [docs/shaders.md](docs/shaders.md) | the shaders on the disc and the cache recipe |
| [docs/glyphs.md](docs/glyphs.md) | the button pictures, their cells, what stays the 360's |
