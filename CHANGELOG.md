# Changelog

## 0.1.0 (unreleased)

First public release, source only: each player builds the game from their own disc.

- Static recompilation of *Skylanders: Spyro's Adventure* (Xbox 360): XenonRecomp translates the game's code and
  XenosRecomp its shaders, on the player's machine (`tools/prepare_game.sh`).
- Runtime: Xbox 360 kernel and XAM services, the Xenos GPU on Metal (macOS), Vulkan (Windows, Linux) and D3D12
  (Windows fallback, Xbox), XMA audio through a minimal LGPL FFmpeg, Bink movies, saves, ten text languages and the
  voice language.
- Saves that cannot be lost: every save file is written to a temporary, synced and renamed over the old one, and the
  previous version is kept in `SSA/saves-backup/<time>/` (the last 10, plus one per day for 7 days); Arkeyan Launcher's
  Settings > Game data lists them and restores one, refused while the game runs (`docs/getting-started/settings.md`).
- Game data copy: Arkeyan Launcher can copy the disc's files (verified, resumable) into the game's folder; the game then reads that copy and
  the disc image is no longer needed (`docs/runtime/host/game_data.md`).
- Ultrawide: a window wider than 16:9 shows more of the world (Hor+ camera, scene and frame as wide as the window, HUD
  and menus centred at their own 16:9 layout, movies with bars); `[video] aspect=auto|16:9` (`docs/runtime/gpu/ultrawide.md`).
- Platforms: macOS (Apple silicon, tested daily); Windows 10/11 x64 and Linux x86_64/arm64 (built and smoke-tested
  under Wine and in containers, not yet on real hardware); Xbox One and Series X|S in Dev Mode (UWP package, the game
  shows, performance work under way).
- Portal of Power: real portals (USB HID; Xbox 360 portal through libusb; Xbox One portal on the console, untested)
  and an emulated portal that reads figure dumps.
- Achievements and play time: the achievements the games unlock are kept (with their time) in the game's data folder and shown as a
  notice, the games' own checks see them, and the list (names, descriptions, gamerscore, pictures) is read from the player's disc; the play
  time is counted by the game (`docs/runtime/xbox360/xam.md`).
- Toy Box: an in-game menu to put figures on the emulated portal and edit their progress, with portraits read from
  the disc.
- Launcher and settings: disc image picker and verification, `launcher.ini` and command-line options (resolution
  scale, fullscreen, vsync, anisotropy, experimental 60 FPS, keyboard bindings, button glyphs per controller family,
  languages), `--version`.
- HD texture packs: `texture_dump=1` writes each distinct game texture once as a DDS file named by a hash of its content,
  and DDS files of the same name in the game's `textures/` folder replace it, at any size (`docs/runtime/gpu/texture_packs.md`).
- One data folder, `Skylanders` (`%APPDATA%`, `~/Library/Application Support`, `$XDG_DATA_HOME`), for the collection,
  `launcher.ini`, the kits, the compiler and one folder per game (`SSA/`: the built game, saves, log); the older folders
  are moved into it at the first start, nothing overwritten or deleted (`docs/getting-started/settings.md`); `SKYLANDERS_DATA` moves it.
- A game log on every desktop platform (`SSARecomp.log` in the game's folder, the previous one kept), with the
  system, GPU and settings first and the `[perf]` lines every 10 s; `resolution_scale=auto` by default (x1 on
  integrated GPUs); `--portal-probe` for the launcher's portal test; HID portals found on Windows.
- A built-in pipeline list (`games/ssa/pipelines/baseline.zst`: shader hashes and render states only) so pipelines are
  built while loading instead of during play.
