# games/ssa/: Skylanders: Spyro's Adventure (Lux engine)

> **Purpose:** what is in SSA's game folder, how to build it, and where its docs are · **Status:** current (2026-10-26) · **Tags:** [SSA] [Lux]

Status: the reference port; plays well on macOS, works on Windows and Linux, installs through Arkeyan Launcher
([docs/status.md](docs/status.md)).

| File | What |
|---|---|
| `addresses.h` | the guest functions to hook (`GAME_HOOK_<Name>`) and the guest data it reads (`namespace game`) |
| `adapter.cpp` | the `game::Adapter` (`runtime/game.h`): the portal prompt reader, the ultrawide camera hook |
| `info.cpp` | id, name and the supported disc release (title ID, media ID, SHA-256 of `default.xex`) |
| `game.txt` | what the player kit's Arkeyan-Builder knows about the game (runtime name, config, `default.xex` hash, shader source) |
| `art.json` | the art manifest: which pictures of the disc the launcher and the Toy Box read |
| `language.cpp`, `ssa.h` | voices in another language than the text |
| `ssa_portal_prompt.*` | where the title keeps its portal prompt and how it is classified (also built into `ArkeyanPortalTest`) |
| `config/` | XenonRecomp input: `SSA.toml`, `SSA_switch_tables.toml`, `SSA_symbols.txt` |
| `pipelines/baseline.zst` | the built-in pipeline list (render states and shader hashes, no game content) |

Build: `cmake -S . -B build -G Ninja` (SSA is the default game). The generated code is `private/ssa/ppc/`
(`tools/prepare_game.sh <iso>`) and the game's derived data `private/ssa/`. The engine's hooks are in
[`engines/lux/`](../../engines/lux/README.md).

## Docs
| Page | What |
|---|---|
| [docs/status.md](docs/status.md) | disc identity, status, where to start, open problems |
| [docs/boot.md](docs/boot.md) | what broke on the way to the title and to a level |
| [docs/research.md](docs/research.md) | public research and its confidence |
| [docs/parity.md](docs/parity.md) | SSA is the reference; the parity tables are in the other games' pages |
| [docs/100.md](docs/100.md) | what "100 %" means and what is left |
| [docs/playtest.md](docs/playtest.md) | the human playtest checklist |
| [docs/art.md](docs/art.md) | the pictures on the disc (logo, background, portraits) |
| [docs/shaders.md](docs/shaders.md) | the shaders on the disc and the cache |

The engine: [docs/engines/lux/README.md](../../docs/engines/lux/README.md).
