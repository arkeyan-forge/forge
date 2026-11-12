# ArkeyanRuntime: native Skylanders on PC, Mac and Linux

> **Purpose:** what this repository is, the status of each game, a quick start and the map of the tree · **Status:** current (2026-11-12) · **Tags:** none

Native ports of the **Skylanders** games for the Xbox 360, made by **static recompilation**: the game's PowerPC code and
Xenos shaders are translated into C++ and host shaders on the player's own machine, from the player's own disc, and a
hand-written runtime stands in for the Xbox (kernel and XAM, graphics through Metal, Vulkan or D3D12, audio, input, the
Portal of Power, the Toy Box and a shared figure collection).

**This repository contains no game data and never will:** no disc image, game file, translated code, shader, artwork or
figure dump. The player brings a disc image they dumped themselves.

## Why
Play the games natively on current machines, keep the real Portal of Power and the figures working, and do it in a way
that distributes nothing from the games: the **player kit** (tools plus the prebuilt runtime, no game code) builds the
game on the player's machine from their ISO. Why local recompilation: [docs/release/release.md](docs/release/release.md).

## Status of each game (2026-11-12)

Versions are those prepared for the 0.9.6 kits. Played platforms reflect real machines; testing is still incomplete.

| Game | Engine | Version | Played on | Known limits |
|---|---|---|---|---|
| [Spyro's Adventure (SSA, 2011)](games/ssa/docs/status.md) | Lux | 0.9.12 | macOS, Windows, Linux | Windows grass fix awaiting confirmation; some bosses and Heroic Challenges unconfirmed |
| [Giants (2012)](games/giants/docs/status.md) | Alchemy | 0.8.12 | macOS, Linux with a real portal | Windows tested through Wine only; challenge-level crashes |
| [Swap Force (2013)](games/swapforce/docs/status.md) | Alchemy | 0.5.11 | macOS, Windows, Linux | Green-screen reports; some modes untested |
| [Trap Team (2014)](games/trapteam/docs/status.md) | Alchemy | 0.1.5 | macOS, Windows (NVIDIA and AMD) | D3D12 black screen on one PC; new Vulkan fallback; dark Spy Rise; voice-language and LB fixes awaiting confirmation |
| [SuperChargers (2015)](games/superchargers/docs/status.md) | Alchemy | 0.1.5 | macOS, Windows, Steam Deck Desktop Mode | Figure-owner fix awaiting confirmation with a real portal |
| [Imaginators (2016)](games/imaginators/docs/status.md) | Alchemy | 0.1.5 | macOS, Windows | Linux untested; a created character's HUD portrait fixed in 0.9.6 |

Per platform: [docs/getting-started/platforms/](docs/getting-started/platforms/README.md). Live status: the per-game status pages above.

## Quick start
1. `git clone --recurse-submodules https://github.com/arkeyan-forge/forge.git forge && cd forge`
2. `tools/prepare_game.sh "<your SSA disc>.iso"` (translates the code and shaders into `private/ssa/ppc/`)
3. `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && ninja -C build ArkeyanRuntime` (Giants: `-DARKEYAN_GAME=giants` in `build-giants/`)
4. `"build/ArkeyanRuntime/Skylanders Spyro's Adventure" "<disc>.iso"`

Build instructions, controls, the portal and the Toy Box:
[docs/getting-started/building-and-playing.md](docs/getting-started/building-and-playing.md). Players who do not build
from source use **Arkeyan Launcher**, which downloads the kit and builds the game for them
([docs/release/launcher/README.md](docs/release/launcher/README.md)).

## Map of the repository
| Path | What |
|---|---|
| `runtime/` | the shared runtime that replaces the Xbox 360, game-neutral ([README](runtime/README.md)) |
| `engines/lux/`, `engines/alchemy/` | hooks of one engine each ([lux](engines/lux/README.md), [alchemy](engines/alchemy/README.md)) |
| `games/<id>/` | six ports: addresses, adapter, disc identity, recompiler config, pipeline baseline and `docs/` notes; see the game table above |
| `tools/` | scripts and small programs: `prepare_game.sh`, `make_kit.sh`, `arkeyan-builder/`, `sigmatch/`, `ppcinfo.py`, `check_docs.py`, Xbox packaging |
| `docs/` | **the documentation, one tree**: start at [docs/README.md](docs/README.md) |
| `patches/`, `thirdparty/` | our changes to the submodules; the submodules (SDL, imgui, plume, XenonRecomp, XenosRecomp) |
| `resources/`, `cmake/`, `ArkeyanRecompLib/` | non-code assets and licences; CMake helpers; the target that compiles a game's generated code |
| `private/` | everything derived from a disc (git-ignored): `private/<game>/ppc/`, shader caches, dumps, toolchains |

## The forge repositories
This repository is the entry point of the **Arkeyan forge**; the rest are git submodules at their usual paths, so a
recursive clone has the whole tree.

| Repository | Mounted at | What |
|---|---|---|
| `runtime` | `runtime/` | the shared runtime that replaces the Xbox 360 |
| `spyros-adventure`, `giants`, `swap-force` | `games/ssa/`, `games/giants/`, `games/swapforce/` | one game each |
| `trap-team`, `superchargers`, `imaginators` | `games/trapteam/`, `games/superchargers/`, `games/imaginators/` | one game each |
| `arkeyan-builder` | `tools/arkeyan-builder/` | the player kit's builder |
| `xenonrecomp`, `xenosrecomp` | (kept in sync; the forge still uses the upstream submodules and `patches/`) | the code translators with our patches applied |

## Documentation at a glance
[docs/README.md](docs/README.md) indexes the public documentation; [old paths](docs/old_paths.md) maps retired pages.
Sections: [getting-started](docs/getting-started/README.md), [architecture](docs/architecture/README.md),
[runtime](docs/runtime/README.md), [engines](docs/engines/README.md), [tools](docs/tools/README.md),
[release](docs/release/README.md), [porting](docs/porting/README.md). Checked by
`python3 -I tools/check_docs.py` (broken links and paths).

## Licence
MIT ([LICENSE](LICENSE)); third-party components and their obligations: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md),
[docs/release/licensing.md](docs/release/licensing.md). *Skylanders* and its characters belong to their owners; this
project is not affiliated with them.
