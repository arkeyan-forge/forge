# games/swapforce/: Skylanders: Swap Force (Alchemy engine)

> **Purpose:** what is in Swap Force's game folder, how to build it, and where its docs are · **Status:** current (2026-11-01) · **Tags:** [Xbox] [XDK] [Alchemy] [SwapForce]

Status: ported and in the kit; 158 of 195 zones play in the tour ([status](docs/status.md)).

| File | What |
|---|---|
| `addresses.h` | the guest functions to hook (`GAME_HOOK_<Name>`) and the guest data it reads |
| `adapter.cpp` | the `game::Adapter` (`runtime/game.h`) |
| `info.cpp` | id, name and the supported disc release (title ID, media ID, SHA-256 of `default.xex`) |
| `swap_wait.cpp` | the render thread's spin on its last swap made a wait for the next graphics interrupt (2026-10-28, [renderer_threads.md](../../docs/runtime/gpu/renderer_threads.md), "Swap Force's swap wait") |
| `game.txt` | what the player kit's Arkeyan-Builder knows about the game |
| `config/` | XenonRecomp input: `SwapForce.toml`, `SwapForce_switch_tables.toml` |

Build: `cmake -S . -B build-swapforce -G Ninja -DARKEYAN_GAME=swapforce`. The generated code is `private/swapforce/ppc/` and the game's
derived data `private/swapforce/`. The engine's hooks are in `engines/alchemy/`.

## Docs
| Page | What |
|---|---|
| [docs/status.md](docs/status.md) | disc identity, status, where to start, open problems |
| [docs/boot.md](docs/boot.md) | what broke on the way to the title and to a level |
| [docs/research.md](docs/research.md) | public research and its confidence |
| [docs/parity.md](docs/parity.md) | feature parity against SSA |
| [docs/100.md](docs/100.md) | what "100 %" means and what is left |
| [docs/playtest.md](docs/playtest.md) | the human playtest checklist |
| [docs/art.md](docs/art.md) | the pictures on the disc |
| [docs/shaders.md](docs/shaders.md) | the shaders on the disc and the cache |
