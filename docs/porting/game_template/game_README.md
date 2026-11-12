# games/<id>/: Skylanders: <Game> (<Engine> engine)

> **Purpose:** what is in <Game>'s game folder, how to build it, and where its docs are · **Status:** current (YYYY-MM-DD) · **Tags:** <the tags that apply: [Xbox] [XDK], the engine's, the game's>

Status: one line (`games/<id>/docs/status.md`).

| File | What |
|---|---|
| `addresses.h` | the guest functions to hook (`GAME_HOOK_<Name>`) and the guest data it reads |
| `adapter.cpp` | the `game::Adapter` (`runtime/game.h`) |
| `info.cpp` | id, name and the supported disc release (title ID, media ID, SHA-256 of `default.xex`) |
| `game.txt` | what the player kit's Arkeyan-Builder knows about the game |
| `art.json` | the art manifest |
| `config/` | XenonRecomp input: `<Game>.toml`, `<Game>_switch_tables.toml` |
| `pipelines/baseline.zst` | the built-in pipeline list |

Build: `cmake -S . -B build-<id> -G Ninja -DARKEYAN_GAME=<id>`. The generated code is `private/<id>/ppc/` and the game's
derived data `private/<id>/`. The engine's hooks are in `engines/<engine>/`.

## Docs
| Page (make each a link) | What |
|---|---|
| `games/<id>/docs/status.md` | disc identity, status, where to start, open problems |
| `games/<id>/docs/boot.md` | what broke on the way to the title and to a level |
| `games/<id>/docs/research.md` | public research and its confidence |
| `games/<id>/docs/parity.md` | feature parity against SSA |
| `games/<id>/docs/100.md` | what "100 %" means and what is left |
| `games/<id>/docs/playtest.md` | the human playtest checklist |
| `games/<id>/docs/art.md` | the pictures on the disc |
| `games/<id>/docs/shaders.md` | the shaders on the disc and the cache |
