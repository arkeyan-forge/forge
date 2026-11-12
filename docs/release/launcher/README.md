# Arkeyan Launcher: the public launcher and its relation to this repo

> **Purpose:** what is Arkeyan Launcher, how does it relate to this repo, who changes what, and where is each of its parts documented? · **Status:** current (2026-11-12) · **Tags:** none (host product)

## What it is
Arkeyan Launcher is the **public launcher** (`Arkeyan-Launcher`, MIT, Dear ImGui on SDL3); this repository is the
**public runtime and game adapters**. Arkeyan Launcher contains **no game code and no game art**: it verifies the player's own disc, downloads
the **player kit** and runs its `Arkeyan-Builder` to build the game on the player's machine, starts the built game
(the full game title from `games/<id>/game.txt` for all six games), shows the shared collection and updates itself. Its look (colours, fonts,
background, layout) is **the designer's design**. It is not the runtime's own `--launcher` window
([../../runtime/host/runtime_launcher.md](../../runtime/host/runtime_launcher.md)).

| | This repository (the Arkeyan forge) | Arkeyan Launcher (`Arkeyan-Launcher`) |
|---|---|---|
| Visibility | **public** source, MIT; no game data | **public**, MIT |
| Holds | the runtime, the engines, the games' addresses, the tools, the docs | the launcher UI, the updater, the install flow, its own `README.md` and `docs/` |
| Never holds | game data | game code, game art, the kit's binaries |
| Releases | no tags; `tools/make_kit.sh` makes the **kits** | tags `vX.Y.Z`; CI attaches the launchers and `SHA256SUMS`; `tools/release_kit.sh` attaches the kits |

Version 0.9.5 is released; the 0.9.6 kits are prepared with all six games. See the
[0.9.5 notes](../../releases/v0.9.5.md), [0.9.6 notes](../../releases/v0.9.6.md) and
[game table](../../../README.md#status-of-each-game-2026-11-12) for current versions and tested platforms.

## What passes between the two repos
1. **The kit** (this repo -> Arkeyan Launcher's release): `tools/make_kit.sh` builds `skylanders-kit-<platform>.zip`, Arkeyan Launcher's
   `tools/release_kit.sh` attaches it to a launcher release. The contract: [kit_contract.md](kit_contract.md).
2. **The shared library** (this repo -> an Arkeyan Launcher checkout): `tools/sync_skylauncher.sh [--check] <skylauncher dir>`
   copies, from `runtime/host/`:
   - `figures/` `figure_catalog`, `figure_crypto`, `figure_data`, `figure_tag` (`.h`, `.cpp`) -> `src/shared/figures/`;
   - `collection/` `json`, `catalogue`, `collection`, `icons`, `datadir`, `save_backup`, `game_data`, `upgrades`, `search` -> `src/shared/collection/`;
   - `art/` `xenos_texture`, `alchemy_archive`, `lux_images`, `game_art` -> `src/shared/art/`;
   - `resources/collection/catalogue.json`, `cmake/embed_file.cmake`, `cmake/join_json.cmake`, and each
     `games/<id>/art.json` (all six games) -> the same name under Arkeyan Launcher's
     resources folder (one subfolder per game). Swap Force's data folder is `SwapForce/` (`datadir::GameFolderName`).

   Arkeyan Launcher's `tools/sync_collection.sh [--check] <this repo>` is the same copy from its side. **This repo's copies
   are the reference** (they have the tests): change them here, then sync.
3. **Names and data the launcher depends on**: the runtime names (`GameInfo::runtime` in Arkeyan Launcher's
   `src/launcher/launcher_games.cpp` = `runtime=` in `games/<id>/game.txt`), the disc table (title ID, media ID, SHA-256
   of `default.xex`; here `games/<id>/info.cpp`), `launcher.ini` (shared: the runtime's keys in
   [../../getting-started/settings.md](../../getting-started/settings.md)), the `Skylanders` data folder
   ([../../getting-started/platforms/data_folders.md](../../getting-started/platforms/data_folders.md)), the collection
   ([../../runtime/host/collection.md](../../runtime/host/collection.md)), the log and `--portal-probe`
   ([support_portal_probe.md](support_portal_probe.md)).

## Who changes what
| Change | Where | Then |
|---|---|---|
| Runtime or game behaviour | this repo | rebuild the kits (`tools/make_kit.sh`), attach them with `release_kit.sh` |
| The figure, collection or art library | this repo (`runtime/host/`, with its tests) | `tools/sync_skylauncher.sh` into Arkeyan Launcher, commit there |
| A new supported game | `games/<id>/` here (`info.cpp`, `game.txt`); a `GameRelease` + `GameInfo` row in Arkeyan Launcher | [../../porting/day1.md](../../porting/day1.md) |
| The launcher's look or flow | Arkeyan Launcher | a launcher release; the kit is untouched |
| The kit contract | **both** | bump the kit `version`; document it in [kit_contract.md](kit_contract.md) |

## Where the code is (Arkeyan Launcher `main`, 2026-10-26)
| Path | Role |
|---|---|
| `src/main.cpp` | picks the game (`[launcher] game`) and disc, shows the launcher, starts the runtime (`RunGame`); the window closes while the game runs, the launcher stays (no window) to capture output and show the crash dialog |
| `src/launcher/launcher.{h,cpp}`, `launcher_games.cpp` | `CheckGameSource`, `ReadTitleMetadata`, the settings file, `ResolveGameSource`; the game table |
| `src/launcher/game_install.*`, `install_checklist.*`, `ui/install_page.cpp` | INSTALL: find the kit, run Arkeyan-Builder, parse events, the checklist, resume |
| `src/launcher/game_run.*`, `portal_status.*`, `report.*`, `ui/support_page.cpp` | run the game piped to `skylauncher-game.log`, the crash dialog, the portal status and probe, "Report a problem" |
| `src/launcher/game_settings.*`, `game_copy.*`, `shortcut.*`, `ui/game_settings_page.cpp`, `ui/game_data_page.cpp` | per-game settings, copying the game data off the disc, desktop shortcuts |
| `src/launcher/updater.*`, `ui/update_page.cpp` | GitHub release updates, the kit download and update |
| `src/launcher/uninstall.*`, `ui/uninstall_page.cpp` | "Uninstall everything": the plan (a dry run), the run, the launcher's own removal ([install_flow.md](install_flow.md)) |
| `src/launcher/ui/launcher_ui.cpp`, `game_look.cpp`, `launcher_app.h`, `theme/launcher_theme.h` | the window: sidebar, Home / Play / Settings, each game's look from its disc pictures |
| `src/launcher/ui/collection_*` | the Collection page ([collection_toybox.md](collection_toybox.md)) |
| `src/assets/launcher_assets.*` | the disc-art cache |
| `src/shared/` | `vfs` (disc reader), `platform/`, `mini_json`, `stb_impl.cpp`, embedded fonts; `collection/`, `figures/`, `art/` = this repo's library (synced) |
| `tests/` | `launcher_test.cpp`, `settings_test.cpp`, `shell_test.cpp`, `copy_test.cpp`, `fake_arkeyan_builder.c` |
| `tools/release_kit.sh`, `tools/sync_collection.sh` | release and sync helpers |
| `CMakeLists.txt` | `SKYLAUNCHER_VERSION`; `SkylauncherCore` (library), `ArkeyanLauncher` (WIN32 MACOSX_BUNDLE), `ArkeyanLauncherTest`, `FakeArkeyanBuilder` (both EXCLUDE_FROM_ALL) |

Game table today: SSA (`0x415608AB`, `Skylanders Spyro's Adventure`), Giants (`0x415608DA`, `Skylanders Giants`) and Swap Force (`Skylanders Swap Force`); the sidebar shows the later
games as "Coming later". v0.3.1 lists SSA only. **Adding a game = a `GameRelease` row (hash from the player's own
dump) + the runtime name.** `g_launcherName = "SSARecomp"` named the settings folder before 0.3.1 (now the `Skylanders`
root).

`launcher.ini` sections only the launcher reads (the game sections and `[video]`, `[input]`, `[portal]` are the
runtime's: [settings.md](../../getting-started/settings.md)):

| Section | Keys |
|---|---|
| `launcher` | `game` (the selected game), `fullscreen`, `ui_scale` (`auto` or 0.75-3), `reduce_motion`, `runtime` (path), `kit` (folder) |
| `update` | `auto_check`, `last_check` (epoch), `repo` (default `Arkeyan-Launcher`), `kit_repo`, `kit_tag` |
| `install.<sha256>` | `runtime`, `kit`, `state` (`running` / `failed`), `pid`: the game built for the disc whose `default.xex` has this SHA-256 |
| `collection` | `favourites`, `moved` (legacy; moved once into `collection.json`) |

## How to test it
Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build ArkeyanLauncher` (CMake 3.20+, Ninja,
Clang C++20; `--recursive` clone). `ninja -C build ArkeyanLauncherTest FakeArkeyanBuilder`, then run `ArkeyanLauncherTest`. Every
launcher run sets `SKY_LAUNCHER_INI` and `SKY_COLLECTION` to scratch paths and `SKY_LAUNCHER_NO_UPDATE_CHECK=1`
([../../getting-started/testing.md](../../getting-started/testing.md)). The fake kit and the screenshot test:
[install_flow.md](install_flow.md). After a library change here: `tools/sync_skylauncher.sh --check <checkout>` lists
what differs.

## Gotchas
- One launcher process, **one runtime process per game** (decided): a crash does not take down the launcher, each game
  keeps its single-title globals (4 GB guest reservation, kernel objects). Rejected: one process loading per-game
  modules. On the Xbox there is no launcher: one package per game.
- XenonUtils' headers rely on Clang (anonymous structs with non-trivial members): GCC cannot build the launcher.
- macOS distribution needs notarization (an ad-hoc-signed download is blocked by quarantine: right-click Open, or
  Privacy & Security > Open Anyway on macOS 15+); the locally built game is never quarantined. Windows: SmartScreen
  warns. Linux: xdg-desktop-portal or zenity for the file dialog.
- **Local recompilation is the default** because it is the only option that keeps "nothing derived from the game is
  distributed" ([../release.md](../release.md)).
- A force-push or a release deletion on Arkeyan Launcher is done by the user only; its history was
  rewritten on 2026-10-24: a clone made before holds old SHAs, never push from it.

## Pages
| Page | Answers |
|---|---|
| [kit_contract.md](kit_contract.md) | The kit contract: `kit.txt`, `config/<id>/game.txt`, Arkeyan-Builder's CLI and events, the layout, licences. |
| [install_flow.md](install_flow.md) | Arkeyan Launcher's INSTALL: the kit download, Arkeyan-Builder's progress card, resume, where things live, the fake kit. |
| [updates_release.md](updates_release.md) | The updater, the kit download and update, the release flow, CI. |
| [collection_toybox.md](collection_toybox.md) | Arkeyan Launcher's Collection page (the library and the in-game Toy Box are runtime pages). |
| [support_portal_probe.md](support_portal_probe.md) | Logs, the crash dialog, the portal card. |
| Arkeyan Launcher's own `docs/` | `install.md`, `updates.md`, `collection_ui.md`, `launcher_ui.md`, `settings.md`, `support.md`, `figures_overlap.md` (in its repo). |
