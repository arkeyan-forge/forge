# games/giants/: Skylanders: Giants (Alchemy engine)

> **Purpose:** what is in Giants' game folder, how to build it, and where its docs are · **Status:** current (2026-10-26) · **Tags:** [Giants] [Alchemy]

Status: plays from the title to Chapter 1 and through the 91-level tour (78 levels play), with every toy type, Bink 2
movies and the smart portal; installs through Arkeyan Launcher with kit 0.5.0 (macOS) ([docs/status.md](docs/status.md)).

| File | What |
|---|---|
| `addresses.h` | the guest functions to hook (`GAME_HOOK_<Name>`: D3D, XMA, ultrawide, Bink) and the guest data it reads; a missing line is "unknown" |
| `adapter.cpp` | the `game::Adapter` (`runtime/game.h`): the portal prompt (a watcher thread finds the game script's variables), the camera aspect, the intro movies |
| `info.cpp` | id, name and the supported disc release (SHA-256 of `default.xex`) |
| `game.txt` | what the player kit's Arkeyan-Builder knows about the game (runtime `Skylanders Giants`, config, `default.xex` hash, shaders from `/permanent/global.bld`) |
| `art.json` | the art manifest: which pictures of the disc the launcher and the Toy Box read |
| `giants_portal_prompt.*` | where Giants keeps its portal screens (script variables) and how a message and a state classify (also built into `ArkeyanPortalTest`) |
| `config/` | XenonRecomp input: `Giants.toml`, `Giants_switch_tables.toml` |
| `pipelines/baseline.zst` | the built-in pipeline list (2363 pipelines from tours with several figures) |

Build: `cmake -S . -B build-giants -G Ninja -DARKEYAN_GAME=giants`. The generated code is `private/giants/ppc/` and the
game's derived data `private/giants/`. The engine's hooks are in [`engines/alchemy/`](../../engines/alchemy/README.md).

## Docs
| Page | What |
|---|---|
| [docs/status.md](docs/status.md) | disc identity, status, where to start, open problems, dated news |
| [docs/boot.md](docs/boot.md) | what broke on the way to the title and to a level |
| [docs/research.md](docs/research.md) | public research and its confidence |
| [docs/parity.md](docs/parity.md) | feature parity against SSA |
| [docs/100.md](docs/100.md) | what "100 %" means and what is left |
| [docs/playtest.md](docs/playtest.md) | the human playtest checklist |
| [docs/art.md](docs/art.md) | the pictures on the disc; the IGA name table, IGZ objects, picture formats |
| [docs/shaders.md](docs/shaders.md) | the shaders on the disc (IGA v8, IGZ v6), the extractor, the cache |

The engine: [docs/engines/alchemy/README.md](../../docs/engines/alchemy/README.md).
