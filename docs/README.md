# Documentation index

> **Purpose:** the public documentation, its subject indexes, page headers and tags, and retired-path mappings · **Status:** current (2026-11-12) · **Tags:** none

## The tree

```
README.md         the project: what, why, status of each game, quick start
docs/             getting-started, architecture, runtime, engines, tools, release, porting
games/<id>/       one game: code, config, docs/ pages for status, boot, research and game-specific systems
engines/<engine>/ one engine's hooks (docs in docs/engines/<engine>/)
```

## Conventions: one page per subject, one header, eight tags

- **One page per subject.** Add to the canonical page (this index says which); never open a second page for a subject
  that has one. A moved or merged page gets a row in the old-path table of [old_paths.md](old_paths.md) (`python3 -I tools/docs_move.py <old> <new>`
  moves a page and rewrites every reference to it).
- **Every page** (`docs/`, `games/*/docs/`) starts with its title and this one line, checked by `tools/check_docs.py`:

  `> **Purpose:** <the question it answers> · **Status:** current (<date>) | design (not built yet) | archive (history, not maintained) · **Tags:** <tags or none>`
- **An area README** (e.g. `docs/runtime/gpu/`, `docs/engines/lux/`, `docs/release/launcher/`) has the same sections: What it is,
  Where the code is, How to test it, Gotchas, Pages.
- **Tags**, the only ones (`tools/check_docs.py` refuses other spellings):

  | Tag | Means |
  |---|---|
  | **[Xbox]** | console, kernel or hardware facts, and host facts, true for every game |
  | **[XDK]** | the Xbox development kit's libraries linked into the titles (D3D9LTCG, XAudio2, XMA, XAM by XDK build) |
  | **[Lux]** / **[Alchemy]** | an engine (SSA is Lux; Giants is Alchemy) |
  | **[SSA]** / **[Giants]** / **[SwapForce]** | one game's binary: addresses, data, behaviour |
  | **[V]** / **[?]** | verified (a run, a test, the disassembly) / a lead (inferred, not proved) |

  Evidence goes after the tag, in parentheses: `[V] (perf2 run)`, `[?] (one public source)`.

## The areas (one line each; every page of an area is listed in that area's README)

| Area | Answers |
|---|---|
| [getting-started/](getting-started/README.md) | How to build, run, configure and test the games ([platforms/](getting-started/platforms/README.md): what runs where) |
| [architecture/](architecture/README.md) | How the code is organised (runtime, engines, games) |
| [runtime/](runtime/README.md) | What replaces the Xbox 360: [xbox360/](runtime/xbox360/README.md) console, [gpu/](runtime/gpu/README.md) graphics, [host/](runtime/host/README.md) portal, figures, collection |
| [runtime/host/game_music.md](runtime/host/game_music.md) | Each game’s disc theme, decoding and launcher cache |
| [engines/](engines/README.md) | Which engine each game runs on: [lux/](engines/lux/README.md), [alchemy/](engines/alchemy/README.md) |
| [tools/](tools/README.md) | The tools that turn a disc into a port ([toolchain/](tools/toolchain/README.md)), build it and ship the kit |
| [release/](release/README.md) | Shipping: the kit, Arkeyan Launcher ([launcher/](release/launcher/README.md)), licences, identity, hygiene |
| [releases/v0.9.5.md](releases/v0.9.5.md) | What 0.9.5 changes for players |
| [releases/v0.9.6.md](releases/v0.9.6.md) | What 0.9.6 changes for players |
| [porting/](porting/README.md) | How to port a game and what porting SSA and Giants taught ([game_template/](porting/game_template/README.md)); [perfect_port.md](porting/perfect_port.md) (every fix before a game is perfect); [perf_audit_090.md](porting/perf_audit_090.md) (where the time goes after 0.9.0); per-game research is indexed below |
| [status/STATUS.md](status/STATUS.md), [COVERAGE.md](status/COVERAGE.md) | Every game's catalogue (levels, challenges, arenas, modes, credits, figures) and how its automatic run went; what is not covered, and why |
| [CONVENTIONS.md](CONVENTIONS.md) | The rules for our own code: format, naming, placement, memory, errors, tests, commits |
| [old_paths.md](old_paths.md) | Where retired public pages were moved or merged |
| [naming.md](naming.md) | Current product names, their history and compatibility identifiers |

## Per game: `games/<id>/docs/`

The same pages for every game ([template](porting/game_template/README.md)); `status.md` is the current state.

| Game | Pages |
|---|---|
| SSA | [status](../games/ssa/docs/status.md), [100](../games/ssa/docs/100.md), [boot](../games/ssa/docs/boot.md), [parity](../games/ssa/docs/parity.md), [playtest](../games/ssa/docs/playtest.md), [research](../games/ssa/docs/research.md), [shaders](../games/ssa/docs/shaders.md), [art](../games/ssa/docs/art.md) |
| Giants | [status](../games/giants/docs/status.md), [100](../games/giants/docs/100.md), [boot](../games/giants/docs/boot.md), [parity](../games/giants/docs/parity.md), [playtest](../games/giants/docs/playtest.md), [research](../games/giants/docs/research.md), [shaders](../games/giants/docs/shaders.md), [art](../games/giants/docs/art.md) |
| Swap Force | [status](../games/swapforce/docs/status.md), [100](../games/swapforce/docs/100.md), [boot](../games/swapforce/docs/boot.md), [parity](../games/swapforce/docs/parity.md), [playtest](../games/swapforce/docs/playtest.md), [research](../games/swapforce/docs/research.md), [shaders](../games/swapforce/docs/shaders.md), [art](../games/swapforce/docs/art.md) |
| Trap Team | [status](../games/trapteam/docs/status.md), [100](../games/trapteam/docs/100.md), [010](../games/trapteam/docs/010.md), [boot](../games/trapteam/docs/boot.md), [research](../games/trapteam/docs/research.md), [shaders](../games/trapteam/docs/shaders.md), [glyphs](../games/trapteam/docs/glyphs.md), [art](../games/trapteam/docs/art.md), [toybox](../games/trapteam/docs/toybox.md) |
| SuperChargers | [status](../games/superchargers/docs/status.md), [boot](../games/superchargers/docs/boot.md), [research](../games/superchargers/docs/research.md), [shaders](../games/superchargers/docs/shaders.md), [art](../games/superchargers/docs/art.md), [toybox](../games/superchargers/docs/toybox.md) |
| Imaginators | [status](../games/imaginators/docs/status.md), [boot](../games/imaginators/docs/boot.md), [research](../games/imaginators/docs/research.md), [art](../games/imaginators/docs/art.md), [toybox](../games/imaginators/docs/toybox.md) |

Port records: [SSA](../games/ssa/docs/history.md), [Giants](../games/giants/docs/history.md),
[Swap Force](../games/swapforce/docs/history.md).
