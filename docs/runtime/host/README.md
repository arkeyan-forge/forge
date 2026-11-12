# Host: portal, figures, collection, Toy Box, art, the runtime's launcher window

> **Purpose:** what does the host side of the runtime add on top of the console (portal, figures, the shared collection, the Toy Box, the disc's pictures, the `--launcher` window), and which page answers what? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants]

## What it is

Everything the player touches that the Xbox 360 did not provide by itself: a Portal of Power (real through HID or USB,
or emulated from figure dumps), the figure tag library, one figure collection shared by every game and Arkeyan Launcher,
the in-game Toy Box (figure manager and quick-swap wheel), each game's pictures read from the player's disc, and the
runtime's own first-run window. The window, input and audio output are covered in [xbox360/](../xbox360/README.md).

## Where the code is

| Code | What |
|---|---|
| `runtime/kernel/portal.cpp`, `runtime/host/portal*.cpp`, `runtime/host/uwp/portal_backends.cpp` | the portal: xam ordinals, backends (emulated, HID, USB, async, stall), identity, prompt watcher, probe |
| `games/<id>/<id>_portal_prompt.*`, `games/<id>/adapter.cpp` | each game's prompt reader |
| `runtime/host/figures/` | the tag library: crypto, tag, data, catalogue, `figure_test.cpp` |
| `runtime/host/collection/` | the shared collection: catalogue, collection, icons, json, datadir, save backups; the achievements' XDBF reader and cache (`achievements`), the unlocks and play time (`progress`) |
| `runtime/host/toybox.cpp`, `runtime/host/toybox/{model,draw}.*`, `runtime/host/imgui_software.cpp` | the Toy Box |
| `runtime/host/toybox_link.*`, `toybox_socket.*`, `toybox_remote.*` | the link to the launcher's Toy Box window ([toybox_link.md](toybox_link.md)) |
| (design, 0.10.0) | online play in input lockstep, the deterministic netplay mode, rooms ([netplay.md](netplay.md)) |
| `runtime/host/art/` | the disc's pictures (Lux `strm`/APKF, Alchemy IGA/IGZ, Xenos texture decoding, the PNG cache) |
| `runtime/host/profiles.*`, `runtime/kernel/xam_user.cpp` | the player profiles (name, picture, XUID) served as the console's gamer profiles |
| `runtime/host/launcher*.cpp`, `launcher_ui*`, `launcher_assets.*`, `widgets/` | the runtime's `--launcher` window and the disc check |
| `resources/collection/catalogue.json` | every toy of the six games (271), seats and accepts per game |

## How to test it

- Unit tests, all in `tools/test_all.sh` ([testing.md](../../getting-started/testing.md)): `ArkeyanPortalTest`,
  `ArkeyanPortalAsyncTest`, `ArkeyanFigureTest`, `ArkeyanCollectionTest`, `ArkeyanDatadirTest`, `ArkeyanSavesTest`, `ArkeyanProgressTest`, `ArkeyanAchievementsKernelTest`, `ArkeyanArtTest`,
  `ArkeyanRuntimeLauncherTest`, `ArkeyanProfilesTest`.
- A game run with a figure: `SKY_PORTAL=emulated`, a scratch `SKY_COLLECTION`, `SKY_PORTAL_PLACE=<dump>.sky`,
  `SKY_PORTAL_LOG=1`; `SKY_TOYBOX_SCRIPT` drives the Toy Box ([figures.md](figures.md#tests)).
- `--portal-probe` for real hardware ([portal_probe.md](portal_probe.md)).

## Gotchas

- The default portal mode is `auto`: a real portal plugged in at launch wins, else the emulated one
  ([portal.md](portal.md#backends-xbox)).
- Real portal I/O never runs on the title's threads (`AsyncPortal`): a blocking HID write froze Windows games.
- Never point a test at the user's real collection: always a scratch `SKY_COLLECTION`.
- A figure keeps every byte a game does not know; Spyro's Adventure never touches Giants' extra areas
  ([figures.md](figures.md#data-rules-one-figure-every-game-lux-alchemy)).

## Pages

| Page | Answers |
|---|---|
| [portal.md](portal.md) | The portal protocol, the backends and the default mode, the portal light, the portal's identity, the prompt watcher, the overview |
| [portal_probe.md](portal_probe.md) | `--portal-probe`: is a real portal reachable (the canonical JSON contract)? |
| [figures.md](figures.md) | The tag format, crypto, progress fields, levels, Giants' areas, the toys each game accepts, portraits, the Toy Box controls, tests |
| [collection.md](collection.md) | The shared collection: one folder for every game (format, locks, backups, migration) |
| [collection_api.md](collection_api.md) | The collection library's API for a UI (launcher, Toy Box) |
| [toybox_ux.md](toybox_ux.md) | The Toy Box and figure management UX design (screens 1 to 12, what is built, what is design) |
| [ingame_ui.md](ingame_ui.md) | The in-game interface's one design: type, spacing, palette, motion, focus, TV safe area; the toast and achievement card, the F3 sheet |
| [profiles.md](profiles.md) | The player profiles a game shows: `SKY_PLAYER_*` / `SKY_PLAYER2_*` (name, picture, XUID), the names the fonts draw, the gamer picture, where each game shows them |
| [gamemenu.md](gamemenu.md) | The F3 menu (volume, FPS counter, live settings), the problem report file, and how a crash names the guest function |
| [game_music.md](game_music.md) | Each game's main menu theme from the disc (Wwise, FMOD) decoded by Arkeyan-Builder for Arkeyan Launcher's game page |
| [game_art.md](game_art.md) | Each game's logo, background and portraits from the disc: the provider, the cache, the rule for a toy's picture |
| [game_data.md](game_data.md) | Copying the disc's files into the game folder (no disc image needed afterwards): the copy, its manifest, the reader's folder mode |
| [runtime_launcher.md](runtime_launcher.md) | The runtime's own `--launcher` flow: finding, checking and remembering the disc image (not Arkeyan Launcher) |
| [runtime_launcher_ui.md](runtime_launcher_ui.md) | The look and code of that window, and its screenshot test |

Related: [engines/lux/portal_title_side.md](../../engines/lux/portal_title_side.md) (what the title's portal driver
expects), [release/launcher/](../../release/launcher/README.md) (Arkeyan Launcher).
