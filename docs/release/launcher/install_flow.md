# Arkeyan Launcher's INSTALL flow

> **Purpose:** what happens in Arkeyan Launcher between a verified disc and PLAY: the kit download, Arkeyan-Builder's progress card, resume, and where things live? · **Status:** current (2026-10-27) · **Tags:** none

Detail in Arkeyan Launcher's own `docs/install.md`. The disc check itself (`CheckGameSource`: the table of checks and
messages, the startup order of command line, saved choice and dialog) is the runtime's code, shared by the launcher:
[../../runtime/host/runtime_launcher.md](../../runtime/host/runtime_launcher.md). What Arkeyan-Builder prints:
[kit_contract.md](kit_contract.md).

## The INSTALL flow
1. The ISO is dropped and verified; the SHA-256 of its `default.xex` is kept.
2. With no game built for that disc (and no runtime set up by hand), **INSTALL replaces PLAY**: "Install builds the game on your PC from your disc: about N minutes and 6 GB, internet needed once for the compiler" (`InstallMinutesFor`: 10 for SSA, 15 for Giants, the generic figure for Swap Force; `InstallGigabytes`).
3. With no kit, INSTALL first **downloads it** ("Downloading the game tools", a bar in MB, **Retry**; failure: "The game tools couldn't be downloaded"; every platform has a kit since 0.3.1, Linux included). Then `Arkeyan-Builder --iso <iso> --out <game folder> --kit <kit>`. Progress card:
   - a fixed **checklist** (`install_checklist.cpp`): Downloading the game tools (only when the launcher fetched the kit), Preparing the compiler, Reading the disc, Translating the game code, Translating the shaders, Compiling the game, Linking the game, Adding the movie shaders, Finishing (an unknown `@step` gets its own row);
   - an **overall bar** (compile is half of the weight), elapsed time and a **time left** that appears once the current step has progress;
   - **one line** with the step's last `@log`; **Show details** (a scrollable log); on **failure** a human headline, Arkeyan-Builder's `@error`, "What was already built is kept", **Copy log**, **Open logs folder**, **Retry**;
   - **across a restart**: `[install.<sha256>] state=running|failed` + `pid=` in `launcher.ini`; next start the card comes back from `install.log` ("Installation interrupted", **Resume**, which waits for an Arkeyan-Builder still running).
4. `@done <path>` + exit 0: saved per disc, PLAY appears (on Windows SDL cannot report the exit code of a process started without a console window: `@done` alone means success).
5. When the release publishes a newer version of the game than the one built (the games line of its notes, [updates_release.md](updates_release.md)) the Play page offers **UPDATE** (and a link "Play without updating"); the game's own settings (Game settings, Game tab) have **Reinstall the game** and **Open the game folder**. Arkeyan-Builder is incremental, so these are quick when little changed.
**Cancel** stops only the process the launcher started: SIGTERM then SIGKILL after 3 s (macOS/Linux); on Windows Arkeyan-Builder runs in a **job object** so the compilers stop with it (also when the launcher closes). Arkeyan-Builder's output also goes to `install.log`. `SKY_LAUNCHER_BUILD_JOBS=N` passes `--jobs N`.

Three games share the flow (SSA, Giants, Swap Force; one kit builds them when its `kit.txt` `games=` lists them, else the launcher fetches the right kit first). Swap Force's disc has English and French; its built game is `Skylanders Swap Force`.

## Where things are

| What | Where |
|---|---|
| the kit | `SKY_LAUNCHER_KIT`, else `[launcher] kit=`, else next to the launcher (next to `ArkeyanLauncher.app` on macOS): `ssa-kit/`, `kit/`, or a folder named `ssa-kit*`, else the newest downloaded `kit/<release tag>/`; must hold `Arkeyan-Builder` (`Arkeyan-Builder.exe`) |
| the kit's version | `build=` (else `version=`) in `kit.txt`, else Arkeyan-Builder's size and date |
| the built game | one folder per game, whatever the disc's release: `<Skylanders root>/SSA/`, `<Skylanders root>/Giants/`, `<Skylanders root>/SwapForce/` (`GameInstallFolder`; [data_folders.md](../../getting-started/platforms/data_folders.md)); never next to the ISO. Before 0.3.1: `Skylauncher/games/<sha256>` in the OS data folder |
| what was built | `[install.<sha256>]` `runtime=` (the `@done` path), `kit=` |
PLAY starts: `SKY_LAUNCHER_RUNTIME`, else the game built for this disc, else `[launcher] runtime=`, else the game's runtime (`Skylanders Spyro's Adventure`, `Skylanders Giants`, `Skylanders Swap Force`) next to the launcher (a hand-set runtime also hides INSTALL). `SKY_LAUNCHER_DATA` replaces the data folder (tests).

## Testing without the kit
`tests/fake_arkeyan_builder.c` prints Arkeyan-Builder's real step ids, titles and order with delays (target `FakeArkeyanBuilder`, built as `<build>/fake-kit/Arkeyan-Builder` with a `kit.txt`); `FAKE_SKYBUILD_FAIL=<step id>` fails half way through that step; `FAKE_SKYBUILD_DELAY` (0.05 s). `ArkeyanLauncherTest` parses events, runs the fake kit (failure, success, cancel), checks the checklist model; `SKY_LAUNCHER_TEST_INSTALL=real` with the real kit takes a shot every `SKY_LAUNCHER_TEST_EVERY` s (default 120). By hand: `SKY_LAUNCHER_KIT=$PWD/build/fake-kit`.

## Uninstall everything (2026-10-27)
Settings (gear) > Launcher > **Remove Arkeyan Launcher** > **Uninstall everything...**: a list of what goes with sizes (each built game, the game-data copies, the
tools `kit/` and `toolchains/`, caches and logs, `launcher.ini`, the desktop shortcuts the launcher made, Arkeyan Launcher itself), a toggle **Also delete my saves
and my figures** (off by default; adds the saves, the games' `figures/` and `textures/`, and the collection) with **Export first** beside it (the collection's
export), a **held** button (the existing hold-to-confirm), then the result ("Removed, except": what stayed and why) and **Quit**. Details, the safety rules and
the tests: Arkeyan Launcher's `docs/install.md`, "Uninstall everything"; the window: its launcher UI page. Code: `src/launcher/uninstall.{h,cpp}` (a **plan** that only
measures, then the run; no UI), `ui/uninstall_page.cpp`.

- **Never removed**: a disc image (a remembered path, any `.iso` / `.xiso`, even in a game's folder), a game the player moved out of its folder (told), other
  files in the data folder, and the older-layout folders of before 0.3.1.
- **Safe by construction**: targets come only from the environment (the data root, `SKY_LAUNCHER_DATA`, each game's folder, the figure folder, the settings file,
  the shortcuts, the launcher); absolute, no `..`, never `/`, a folder under it, home or a folder holding it, no link at the top, strictly inside the data
  folders; checked again when it runs. The figure folder is cleaned by name (`.sky`, `.bin`, `.dmp`, `.lock`, `collection.json`, `backups/`, `trash/`).
  A game running (or an install, or a figure in use) refuses the whole run.
- **The launcher itself** goes last, by a helper started before the window quits and waiting for its process: macOS moves the `.app` (and `.old`) to `~/.Trash`
  (`/bin/sh`), Windows deletes the `.exe` (hidden PowerShell, `-EncodedCommand`, retries 20 s), Linux deletes the AppImage. Refused for a path that is not that
  system's kind, a link, in the data folder or a system folder; a launcher with no path of its own (a build folder, a disk image) is told to be deleted by hand.
  After it ran the window writes no settings (`FreezeLauncherConfig`).
- **Tested** (`tests/uninstall_test.cpp`, `SKY_LAUNCHER_TEST_UNINSTALL_ONLY=1`): scratch trees for the macOS, Windows and Linux layouts as pure path logic; the plan
  (exact sizes, nothing touched); the run with and without the tick; every refusal; the macOS and Linux helper scripts **run on a dummy bundle or file** in scratch
  with a scratch Trash. **The Windows helper is only built and decoded in tests, never run** (no Windows here); neither is a real self-delete or Trash move of
  the built launcher. A scratch run (`SKYLANDERS_DATA`, `SKY_LAUNCHER_DATA`, `SKY_LAUNCHER_INI` or `SKY_COLLECTION` set) never targets the real launcher: it takes
  a dummy from `SKY_LAUNCHER_UNINSTALL_SELF` (macOS: with `SKY_LAUNCHER_UNINSTALL_TRASH`).

## Export for Xbox (design only, not built; Xbox paused)
The design (a button after verification, `tools/xbox/package.sh`, a per-player certificate) is in
[../../runtime/host/runtime_launcher.md](../../runtime/host/runtime_launcher.md); Arkeyan Launcher has no Xbox entry today.
