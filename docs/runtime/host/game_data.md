# Copying the game data (no disc image needed afterwards)

> **Purpose:** how "Copy the game data" copies and verifies the disc's files, and how the runtime, Arkeyan-Builder and the launcher use the copy · **Status:** current (2026-10-26) · **Tags:** [Xbox] [V]

Code: [`host/collection/game_data.{h,cpp}`](../../../runtime/host/collection/game_data.cpp) (the copy, its manifest, its checks),
[`vfs.{h,cpp}`](../../../runtime/vfs.cpp) (the reader's folder mode), `CopiedGameData` in
[`host/launcher.cpp`](../../../runtime/host/launcher.cpp) (the runtime's choice), tests
[`game_data_test.cpp`](../../../runtime/host/collection/game_data_test.cpp) (`ArkeyanGameDataTest`). Class: **[Xbox]** and
engine-generic (both games, any future one). The launcher's side is in the Arkeyan Launcher repo: its launcher_ui doc, "Game data".

## What the player gets

By default the game reads the disc image (~8 GB) while it runs, and nothing is duplicated. In Arkeyan Launcher's game settings > Disc >
**Game data**, "Copy the game data" copies the disc's files (about the image's size) into the game's folder, checking each one.
Once the copy is complete the game, its updates and its reinstall use the copy: the ISO can be moved, deleted or left on a USB
disk. "Remove the copy" gives the space back and the disc image is read again. A copy that is damaged, or unfinished, is never
used: the game falls back to the disc image.

## The folder

`<data>/<Game>/gamedata/` (`gamedata::Folder(id)`, [datadir.h](collection.md)):

- the disc's files under their disc paths, **lowercase** (the disc is case-insensitive; the reader lowercases every lookup, so a
  case-sensitive file system works too), e.g. `gamedata/default.xex`, `gamedata/media/...`;
- `gamedata.manifest`: `gamedata 1`, `xex=<SHA-256 of the disc's default.xex>`, `complete=0|1`, then one `file<TAB><size><TAB><path>`
  per file. It is written (atomically) before the first file with `complete=0`, and again with `complete=1` after the last one;
- `<name>.part`: the file being copied. Neither of the last two is part of the game: the reader skips them.

## The copy (`gamedata::Copy`)

1. The files already there with their exact size are kept (they were verified when they were put in place).
2. The room is checked **first**: the bytes still to write plus a 64 MB reserve against `fs::space`. If it is short nothing is written
   (`NotEnoughSpace`, with the two numbers).
3. Each missing file is written to `<name>.part` in 4 MiB chunks, then **read back and compared chunk by chunk with the disc** (a
   second read of the disc, not a hash: no stream hash is needed, and what is verified is the copy against the source), and only then
   renamed. A mismatch deletes the `.part` and stops with `Failed`.
4. A cancel flag is looked at between two chunks: the `.part` goes, the finished files stay (`Cancelled`). Copying again resumes: only
   the missing files are read. A power cut leaves at worst one `.part`, which the next run overwrites.
5. `complete=1` is written last.

`gamedata::Inspect(folder)` reads the manifest and stats every file (no content read; thousands of files in a few ms), so it is cheap
enough for every start. It answers `None` (no manifest), `Partial` (`complete=0`), `Complete`, or `Damaged` (it was complete and a file
is missing or has another size, or the manifest can't be read). A damaged copy is repaired by copying again (only the damaged files are
read), or removed (`gamedata::Remove`, which refuses a folder with no manifest).

Names that could leave the folder (`..`, absolute, `\`, `:`) are refused: a disc can't write elsewhere.

## The runtime prefers the copy

`DiscFileSystem::Open(path)` takes an image **or a folder**: a folder is indexed by walking it (paths lowercased, `gamedata.manifest`
and `*.part` skipped, a `default.xex` required); a file is opened for each read and closed after it, so no handle is kept (the
game opens a few hundred files and a Mac allows 256 descriptors by default). Everything else (the Nt file API, the art readers, the
title metadata) reads through the same `Find` / `Read` / `List` and does not know the difference.

A portable game's own pack comes before everything, the copy included ([portable.md](../../getting-started/portable.md); `Open` also takes a file that ends with a pack).
`ResolveGameSource` (the runtime's start) then calls `CopiedGameData(game)`: when `Inspect` says `Complete` and `CheckGameSource` accepts
the folder (its `default.xex` is a release this game was recompiled from), the copy is used **whatever disc image path was given, or
none**. `--launcher` / `--select-iso` still open the chooser. A `Damaged` copy is said on stderr ("the copy in ... is damaged (...):
reading the disc image instead") and the disc image is used; a `Partial` one is ignored silently. `CheckGameSource` accepts a folder only
when it holds `gamedata.manifest` (any other folder is still "Extracted game folders aren't supported yet").

`Arkeyan-Builder --iso <disc image or copied folder>`: the build's disc steps read the folder the same way (its stamp is the manifest's size and
time, so copying again re-runs them once).

## Arkeyan Launcher

Arkeyan Launcher has its own copy of `vfs.{h,cpp}` (the same file, two include paths) and `game_data.{h,cpp}` (`tools/sync_skylauncher.sh`
copies the latter). It makes the copy on a worker thread (`GameCopyJob`, `launcher/game_copy.h`), picks the source at start
(`PickGameSource`: the complete copy, else the remembered image), and shows the states: the Arkeyan Launcher repo's launcher_ui doc, "Game data".

## Isolation of the runs

The copy lives in `<data>/<Game>/gamedata/`, so a test must move the data root (`SKYLANDERS_DATA`) or it reads and writes the player's. Arkeyan-Builder's
headless run of the game it just built (`--iso <copy>`) sets `SKYLANDERS_DATA`, `SKY_LOG` and `SKY_LAUNCHER_INI` to its work folder like the
other scratch variables ([testing.md](../../getting-started/testing.md), "Isolation"; `tools/regress/kit_isolation.sh` checks it).

## Checks

- `ArkeyanGameDataTest <scratch>` (in `tools/test_all.sh`): a synthetic disc of five files (one of 9 MiB, an empty one, nested folders):
  happy path and the folder reader (case-insensitive lookups, `List`, a read at the end of a big file), removal, not enough space (and
  just enough), interrupted inside a big file then resumed (finished files not read again), a copy that differs from the disc (refused,
  no file left), unsafe names, damaged (shortened file, missing file, garbage manifest), repaired by copying again.
- Arkeyan Launcher's `ArkeyanLauncherTest` runs the same on the real disc's small files and drives its window (`SKY_LAUNCHER_TEST_COPY`).
- Verified with real discs, 2026-10-26: both games copied whole (8.1 GB each, files checked against the disc) and run isolated from
  the copy with **no disc image argument** (see `games/<id>/docs/status.md`).
