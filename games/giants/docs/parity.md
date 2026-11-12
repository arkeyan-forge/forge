# Giants: parity with SSA (runtime and Arkeyan Launcher)

> **Purpose:** feature by feature, how SSA does it and where Giants stands · **Status:** current (2026-10-26) · **Tags:** [Giants] [Alchemy]

SSA is the reference ([../../ssa/docs/parity.md](../../ssa/docs/parity.md)). Everything SSA has, Giants must have, done
the same way. Legend (every game): ✅ done, 🔧 in progress, ⏸ paused, ❌ not done.

## Arkeyan Launcher
| Feature | SSA | Giants | Where documented |
|---|---|---|---|
| Disc check | `GameRelease` rows (the `default.xex` SHA-256) | ✅ `games/giants/info.cpp` and `game.txt`; Arkeyan-Builder picks the game from the disc | [kit_contract.md](../../../docs/release/launcher/kit_contract.md) ("Several games in one kit") |
| INSTALL, update, PLAY through the kit | the kit + Arkeyan-Builder | ✅ macOS (kit 0.5.0, 563 s install); ✅ the Windows and Linux kits built, tested under Wine and in Docker; ❌ a real PC | [history.md](history.md) ("Installing Giants") |
| Game page art from the player's disc | `art::Extract`: the logo and the painted sky of `uiimage.str`, the XDBF title icon, cached in `<data>/<Game>/launcher-cache/` | ✅ the logo composed from `Title.bld`'s sprite sheet, the main menu's painting from `global.arc`, the title icon | [art.md](art.md), [game_art.md](../../../docs/runtime/host/game_art.md) |
| The game's look in the launcher | `GameStyle` in Arkeyan Launcher's theme: the logo's sky blue, soft stone bevels | ✅ the menus' glossy gel buttons in the logo's gold and orange | Arkeyan Launcher's `launcher_ui.md` ("Each game's look") |
| The collection: catalogue, figure page, create, delete, trash, backups | the shared collection library, `catalogue.json` ("accepts" per game) | ✅ the Giants toys, their seats and accepts lists | [collection.md](../../../docs/runtime/host/collection.md), [figures.md](../../../docs/runtime/host/figures.md) |
| Toy portraits from the disc | `uigameimage.str`, 78 PNG in `<data>/SSA/portraits/` | ✅ the collection screen's 123 portraits in `<data>/Giants/portraits/`; `ArkeyanArtTest` with `GIANTS_ISO` | [art.md](art.md) |
| Portal: test, live status, model | `--portal-probe`, the status line, the identity table | ✅ generic; Giants' `R` IDs are in the table; ❌ a check with a real portal | [portal.md](../../../docs/runtime/host/portal.md) |
| Smart portal (the prompt opens the Toy Box) | the SSA adapter (prompt object, text hash) | ✅ the "Portal Control" script variables, found in the physical heap | [boot.md](boot.md) ("Portal prompt") |
| Settings: game text language and voices | per-game keys, the SSA voice-language hook | ✅ text language (`--language fr`); ✅ voices (`voice_language`, 12 languages on the disc: `en fr de es es-419 it nl pt sv no da fi`): the voice language's pack is loaded before the text's, the movies' Bink voice track follows (`Language_LoadPack`, `Movie_Open`); proven by the streams read and the audio dump | [boot.md](boot.md#voice-language) |
| Settings: video (scale, widescreen, HD textures, intro movies) | generic keys and the SSA hooks | ✅ intro skip (`BinkOpen`), widescreen and ultrawide, HD textures | [bink.md](../../../docs/engines/lux/bink.md), [ultrawide.md](../../../docs/runtime/gpu/ultrawide.md) |
| Save backups and restore | generic, per game folder | ✅ generic; ❌ verified on Giants' saves | [settings.md](../../../docs/getting-started/settings.md) ("Saves and their backups") |
| Report a problem | generic | ✅ | Arkeyan Launcher's `support.md` |
| The app's icon | the neutral disc (`ArkeyanRuntime.icns`), replaced at install by the game's XDBF icon | ✅ macOS: the game's own icon from the disc at install; ❌ Windows and Linux have no icon | [game_art.md](../../../docs/runtime/host/game_art.md) ("The app icon") |

## Runtime (in game)
| Feature | SSA | Giants | Where documented |
|---|---|---|---|
| Toy Box (My Skylanders, catalogue, seats, quick-swap wheel) | generic ImGui UI | ✅ the 72 toys Giants reads; creates blank ones (LightCore through "versions"), edits up to level 15, shows the portal's colour | [history.md](history.md) ("Figures"), [figures.md](../../../docs/runtime/host/figures.md) |
| Toy Box portraits | the cached disc portraits | ✅ its own portraits first, else SSA's (`[toybox] 123 portraits`) | [art.md](art.md) |
| Pipelines compiled **during the loading screens** | the baseline from a 70-level tour, compiled when each `mission*.str` opens | ✅ 2363-pipeline baseline (the 91-level tour, then tours with Tree Rex, Hot Dog and the other Giants characters), waited for at every `level\*.arc` open; draw-time builds 215 -> 2, 135 -> 2; a figure no tour used 23-102 -> 1-6 per level | [history.md](history.md) ("Level tour"), [pipeline_cache.md](../../../docs/runtime/gpu/pipeline_cache.md) |
| Fast loads: the game's decompression as host code | zlib's `inflate` (`runtime/inflate.cpp`): `mission.str` 3.4 -> 0.3-0.5 s | ✅ LZMA (the LZMA SDK's decoder loop, 2026-10-28): the level from its `.arc` 1.7-2.3 -> 1.0-1.2 s, the decoders' time halved; zlib is linked but unused | [lzma.md](../../../docs/engines/alchemy/lzma.md), [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md) ("Optimizations per game") |
| Movies | Bink 1 on the GPU, the intro skip, 16:9 bars | ✅ Bink 2 drawn on the GPU; 30.00 fps against the header's 30, sound in real time; 16:9 with black bars; 🔧 a person's lip-sync check | [bink.md](../../../docs/engines/lux/bink.md) ("Bink 2 in Giants") |
| Ultrawide (Hor+) | the camera-aspect global and wide targets (SSA hooks) | ✅ the display-aspect global `0x826565C0`, the scene size (same pixel count), the tiled frame, movies with bars; 2560x1080 and 3440x1440 shots, 55.7 fps; limit: the HUD's edge shading at the 16:9 edges | [ultrawide.md](../../../docs/runtime/gpu/ultrawide.md) ("Giants") |
| HD texture packs | generic dump and replace | ✅ 444 textures dumped in level_203, one replaced and seen; the pack folder is the game's (was always `SSA/`) | [texture_packs.md](../../../docs/runtime/gpu/texture_packs.md) |
| Button glyphs (Xbox, PlayStation, Nintendo, keyboard) | the game's glyph textures replaced | ✅ the font atlases' button cells, the title's A, the tutorial rock's small gold Y coin and the world prompts' large Y and A coins (the "Mash!" prompt when a Giant lifts a rock: the Y still shown on the keyboard in 0.6.0, fixed 2026-10-28, keycap shot), for each style (shots of the coin in the 5 styles); `auto` follows the device; harness checks (PS5 pad, then a Switch, an Xbox One, a PS3 and a 360 pad, then the keyboard; shots of the coin in each, 2026-10-28); the keycaps show Giants' own bindings (`[giants.input]`: Y bound to G for Giants only reads G); [?] the giant robot's RT keycap, not seen; ❌ a real PlayStation/Switch pad by a person | [input.md](../../../docs/runtime/xbox360/input.md) ("Glyphs") |
| 60 fps option | the engine's variable step (`--fps-limit 60`) | ✅ Giants presents at every blank (`alchemy.xml` presentationInterval=1): limit 30 shows every second frame (29.9 fps in level_203), limit 60 runs free; speed unchanged | [settings.md](../../../docs/getting-started/settings.md) |
| Saves: atomic, backups | generic | ✅ generic; ❌ verified on Giants | [settings.md](../../../docs/getting-started/settings.md) ("Saves and their backups") |
| Audio | XMA, the driver cadence fix | ✅ | [boot.md](boot.md) ("Choppy, metallic sound") |
| Player 2 (a second controller) | the same runtime path (XAM user 1) | ✅ Player 2 joins, a second figure is placed, both play (scripted presses; ❌ a real second pad) | [figures.md](../../../docs/runtime/host/figures.md), [input.md](../../../docs/runtime/xbox360/input.md) |

## How to use this page
- Keep this page up to date: update its rows with each change.
- A row is ✅ only with a test or a run that proves it.
- The same table drives the later games: copy it from [the game template](../../../docs/porting/game_template/parity.md).
