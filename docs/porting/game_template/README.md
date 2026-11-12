# The game template: the folder every game's docs start from

> **Purpose:** how to start a new game's folder with the same files and headings as SSA and Giants · **Status:** current (2026-10-26) · **Tags:** none

Every game has the same `games/<id>/` layout ([layout.md](../../architecture/layout.md)):
`README.md`, `game.txt`, `art.json`, `info.cpp`, `adapter.cpp`, `addresses.h`, `config/`, `pipelines/`, and
`docs/` with eight pages. This folder holds the skeletons of the README and the eight pages.

## How to use it
1. Copy the pages: `mkdir -p games/<id>/docs && cp docs/porting/game_template/{status,boot,research,parity,100,playtest,art,shaders}.md games/<id>/docs/`.
2. Copy `game_README.md` to `games/<id>/README.md`.
3. Replace `<Game>` (the full name), `<id>` (the short id, as in `-DARKEYAN_GAME=<id>`), `<Engine>` and the date; keep
   every heading, even when a section only says "not yet".
4. Fill the pages as the port goes ([the Giants day-1 example](../day1.md)); the worked examples are
   [SSA](../../../games/ssa/README.md) (the reference) and [Giants](../../../games/giants/README.md).

## Pages
| Page | Answers |
|---|---|
| [game_README.md](game_README.md) | `games/<id>/README.md`: the files, the build line, the generated code, the docs |
| [status.md](status.md) | disc identity, status, where to start, open problems, dated news |
| [boot.md](boot.md) | what broke on the way to the title and to a level |
| [research.md](research.md) | public research: sources, facts, confidence |
| [parity.md](parity.md) | feature parity against SSA |
| [100.md](100.md) | what "100 %" means, what is done and what is left |
| [playtest.md](playtest.md) | the human playtest checklist |
| [art.md](art.md) | the pictures on the disc (logo, background, portraits) |
| [shaders.md](shaders.md) | the shaders on the disc and the cache |
