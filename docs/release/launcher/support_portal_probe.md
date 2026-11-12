# Arkeyan Launcher's support: logs, the crash dialog, the portal card

> **Purpose:** where are the logs a player sends, what happens when the game stops, and how does Arkeyan Launcher test the portal? · **Status:** current (2026-10-26) · **Tags:** none

Detail in Arkeyan Launcher's own `docs/support.md`; the game log itself: [settings.md](../../getting-started/settings.md)
("The game log").

## Where the logs are (0.3.1 layout; before: the folder of `launcher.ini`)
| File | Written by | What |
|---|---|---|
| `<Skylanders root>/<SSA or Giants>/Arkeyan.log` (+ `.prev.log`; the same name for Giants) | the game runtime | its own log, flushed per line (stderr is the file; a crash loses nothing); first lines: `[log]`, `[version] ArkeyanRuntime <PROJECT_VERSION>, build <rev>[-dirty]` (0.1.0 today, root `CMakeLists.txt`; `runtime/host/log.cpp`), `[system]` (OS, CPU, AVX/AVX2, RAM; Wine adds its version), `[settings]`, `[portal] mode`, `[gpu]` adapter/window/scale lines; then `[perf]` every 10 s (`SKY_PERF_LOG=0` off) |
| `<root>/logs/skylauncher-game.log` (+ `.prev`) | the launcher | the runtime's stdout/stderr from its first byte: early crashes, a missing DLL's message |
| `<game folder>/install.log` | the launcher | what Arkeyan-Builder printed (install-logs/ for older ones) |
| `<root>/logs/migration.log` | the migration | every action |
Settings > Support (`ui/support_page.cpp`): **Report a problem** (one text file with the last logs, the settings and the versions, shown before it is saved or copied; nothing is uploaded), **Copy game log** (0.9.5: the last run once, the newest of `skylauncher-game.log` and every game's `Arkeyan.log`, under `===== Game log: <game>, last written <time>, <file> =====`; the row says "Last run: <game>, <when>"; the launcher's `docs/support.md`, "The last run"; the runtime's first line carries the run's start, `[log] <path>, started <date> <time> (local time)`) and **Open logs folder**. `SKY_LAUNCHER_LOG_DIR` replaces the folder (tests). `SKY_LOG=<file>` writes that file instead (never the player's); an empty value writes none; `--help`, `--version`, `--list-gamepads`, `--portal-probe` write no log. The Xbox build logs to `LocalState\Arkeyan.log`.

## The launcher stays while the game runs
`RunGame` starts the runtime with output piped to `skylauncher-game.log`; the launcher (no window) waits. On Windows the process starts without a console window and its exit code is read from a handle of our own (SDL reports none). A non-zero exit shows **"The game stopped unexpectedly"** with the exit code and the logs folder: **Report** (copies the log, opens a new GitHub issue with a template; paste the log), **Copy log**, **Close**. Nothing is sent by the launcher.

## Test the portal (Settings > Controls, and the Support page)
Runs `<runtime> --portal-probe` (at most 15 s) and reads stdout. The card lists each portal ("Found" / "Found, not usable" with the reason) and says what the game will do given the mode: real portal, virtual portal, or waiting for one. No portal: "No portal found: the game will use the virtual portal". A runtime without `--portal-probe` exits 2: "This version of the game doesn't have the portal test yet". Mode = `[portal] mode` (`auto`, `emulated` (Virtual), `real`; the launcher passes `SKY_PORTAL`; old `hid`/`usb` show as "Real portal").

The probe's output (the stderr block, the JSON lines on stdout, the fields, how to parse them) is a stable contract of the
runtime: [portal_probe.md](../../runtime/host/portal_probe.md). The portal's steps in play are in the game log
([portal.md](../../runtime/host/portal.md)).
