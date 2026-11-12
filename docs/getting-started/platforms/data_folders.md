# The `Skylanders` data folder: where it is on each OS, and the 0.3.1 migration

> **Purpose:** where the player's data root lives on each OS, how the older folders (before 0.3.1) were laid out, and how `MigrateOnce()` moves them · **Status:** current (2026-10-26) · **Tags:** none (host platform)

Code: [`runtime/host/collection/datadir.{h,cpp}`](../../../runtime/host/collection/datadir.cpp) (shared with Arkeyan-Builder
and, through `tools/sync_skylauncher.sh`, with the launcher); test `ArkeyanDatadirTest`. The layout inside the root (what
each file and folder is, and which variable replaces which) is canonical in
[../settings.md](../settings.md#where-everything-is-one-folder-skylanders). Landed with 0.3.1 (2026-10-25): older notes
and logs show the folders of the table below.

## The root
`<root>` = `SKYLANDERS_DATA` if set (tests, portable use), else Windows `%APPDATA%\Skylanders`, macOS
`~/Library/Application Support/Skylanders`, Linux `$XDG_DATA_HOME/Skylanders` (default `~/.local/share/Skylanders`).
Nothing in `datadir` creates a folder. Each game has its folder `<root>/<Game>/`: `GameFolderName(id)` upper-cases ids
of 3 letters or fewer and capitalises the others (`ssa` -> `SSA/`, `giants` -> `Giants/`), except `swapforce`, which is `SwapForce/` (two words; nothing migrates, no player had the automatic `Swapforce/`). The Xbox (UWP) build keeps
its data in the app's `LocalState` (figures in the publisher cache folder `Skylanders\Collection`); the Xbox signing
certificate (`xbox-signing`, a developer's key) stays in `<application data>/SSARecomp/xbox-signing`.

## Portable mode
A file `portable.txt` (any content, even empty) in the folder that holds the launcher makes `<that folder>/data` the root: the settings,
saves, figures, logs, kits and built games of the launcher and of every game it starts are under it, and nothing is written in
the user's folders. On macOS the folder is the one that holds `ArkeyanLauncher.app`, not `Contents/MacOS`. `ApplyPortable()` (first
thing in the launcher's `main`) sets `SKYLANDERS_DATA` to it, so `Root()` and the games (which inherit the variable) follow, and no
older folder is migrated; a `SKYLANDERS_DATA` already set wins. Move the folder, move the player. [V] `ArkeyanDatadirTest` copies its
own program to a scratch folder and checks the root with and without `portable.txt`; the launcher started from a fresh copy with
the file beside it, with writes to the real data folders denied (`sandbox-exec`), wrote nothing there.

## Before 0.3.1

| What | macOS (`~/Library/Application Support/`) | Windows |
|---|---|---|
| launcher settings, logs, figures, portraits, cache | `SSARecomp/` (`launcher.ini`, `Arkeyan.log`/`.prev.log`, `skylauncher-game.log`, `figures/`, `portraits/`, `launcher-cache/`, `xbox-signing/`) | `%APPDATA%\SSARecomp\` |
| game settings and saves | `SSARecomp/SSARecomp/` (`pipelines.bin`, `portal.txt`, `saves/`) | `%APPDATA%\SSARecomp\SSARecomp\` |
| collection | `Skylanders/Collection/` | `%APPDATA%\Skylanders\Collection\` |
| built games, kits | Arkeyan Launcher's data folder (`games/<hash>/`, `kit/`) | `%LOCALAPPDATA%\Skylauncher\` |
| llvm-mingw cache | - | `%LOCALAPPDATA%\SSARecomp\toolchains\` |

## `MigrateOnce()` (first start of the new game **or** the new launcher; called first thing in `main`)

| From | To |
|---|---|
| `SSARecomp/launcher.ini` | `launcher.ini` (absolute paths in it that pointed into a moved folder are rewritten; the ISO's stays) |
| `SSARecomp/{Arkeyan.log, .prev.log, figures, portraits, launcher-cache}` | `SSA/` |
| `SSARecomp/skylauncher-game{,.prev}.log` | `logs/` |
| every entry of `SSARecomp/SSARecomp/` | `SSA/` |
| every entry of `Skylauncher/games/<first hash>/` (the built game) | `SSA/` (a second game folder for another disc stays where it is) |
| `Skylauncher/kit/*` | `kit/` |
| `%LOCALAPPDATA%\SSARecomp\toolchains/*` (Windows) | `toolchains/` |

Rules (tested by `ArkeyanDatadirTest` on fake trees only):
- entries are **renamed one by one**; a missing source is skipped, so an interrupted run is finished by the next;
- **nothing is overwritten or deleted**: if the target exists, the moved entry is kept beside it as
  `<name>.old-<yyyymmdd-hhmmss>`, unless the target already holds all its bytes (then the leftover source is dropped);
- if a rename fails (another volume, a file in use), the entry is copied to `<name>.migrating`, compared byte for
  byte, renamed into place, and only then the original is deleted; if that fails too it stays (retried next start);
- an old folder is removed only once empty (`SSARecomp/` stays while it holds `xbox-signing`);
- every action is logged in `logs/migration.log`; a `launcher-cache` made again by an older build is dropped, not kept
  as `.old-`.

**Never runs in a test run**: skipped when `SKYLANDERS_DATA`, `SKY_LAUNCHER_DATA`, `SKY_SAVE_DIR`, `SKY_SETTINGS`,
`SKY_LAUNCHER_INI` or `SKY_COLLECTION` is set (the old folders would be the player's own).

**Removing it all**: Arkeyan Launcher's Settings > Launcher > "Uninstall everything" removes the built games, the game-data copies, `kit/`, `toolchains/`, `logs/`, the
caches, `launcher.ini`, the shortcuts and the launcher itself, and the saves and the collection only when its box is ticked; it never touches a disc image,
the older-layout folders above, or anything outside these folders ([../../release/launcher/install_flow.md](../../release/launcher/install_flow.md)).

The figure collection has its own one-time import: `LegacyFolder()` = `SKY_MIGRATE_FROM`, else `<root>/SSA/figures`
only when neither `SKY_COLLECTION` nor `SKY_FIGURES` is set ([../../runtime/host/collection.md](../../runtime/host/collection.md)).
