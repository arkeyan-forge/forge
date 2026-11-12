# A portable game: one file, no disc image, no launcher

> **Purpose:** what Arkeyan Launcher's "Export for <system>" makes, how the game finds its own data in it, what it does at its first start, and how to check it · **Status:** current (2026-11-01) · **Tags:** [Xbox]

Code: [`host/collection/portable_pack.{h,cpp}`](../../runtime/host/collection/portable_pack.h) (the pack: its format, the
writer, the reader, the first-start import of the figures; shared with Arkeyan Launcher by `tools/sync_skylauncher.sh`),
[`host/portable.{h,cpp}`](../../runtime/host/portable.h) (the game's side: its own pack, the first start),
[`vfs.{h,cpp}`](../../runtime/vfs.cpp) (pack mode of the disc reader), `ResolveGameSource` in
[`host/launcher.cpp`](../../runtime/host/launcher.cpp), `main.cpp` (the first start, the one-copy lock, the window icon).
Test: `ArkeyanPortablePackTest <scratch>` (in `tools/test_all.sh`). The launcher's side (the button, the message, the export
job) is the portable page in Arkeyan Launcher's documentation.

## What the player gets

On an installed game's settings page (Game tab), **Export for macOS / Windows / Linux** writes the game for the system the
launcher runs on, as one file: `Skylanders Spyro's Adventure.app` (macOS: a bundle, as every app), `.exe` (Windows),
`.AppImage` (Linux). It holds the built game (its shaders are in the executable), the disc's files as they are (no
padding, no compression: start-up stays as fast as from the copied game data), the game's icon, the player's keys and
figures. It plays with no disc image and no launcher: started with no argument, the game finds its data in itself.
A short message says first that the copy holds the player's game, for personal use, not to be shared.

- **Saves and settings** stay in the player's data folder (`<data root>/<Game>/`, `launcher.ini`): the same ones as the
  installed game, so the two share the saves. The running game's marker (`save_backup.h`, `<saves>-backup/game.lock`)
  now also stops a second copy at start: "<game> is already running. Close it before starting it again." (a message box,
  exit 1), whichever of the two started first. The launcher's PLAY already refused while a marker is live.
- **Figures**: the collection's files (the dumps and `collection.json`; not the locks or `backups/`) are in the file. At
  the first start of that exported copy on a machine, each dump is added to the shared collection when no file of that
  name is there; one that exists is never overwritten. The added dumps' entries of the file's `collection.json` (label,
  edition, a swapper's other half) join the machine's `collection.json`; its own entries stay (`portable::ImportCollection`).
- **Keys**: the game's `[<game>.input]` section as the launcher would apply it (seeded from `[input]`) is in the file; at
  that first start it goes into `launcher.ini` when the machine has no `[<game>.input]` yet. Changing keys later: the F3
  menu's Commands page ([gamemenu.md](../runtime/host/gamemenu.md)), which writes the same lines.
- **Updates**: none. The portable never looks at the network; the F3 menu shows its version. To update it, export it again
  (the new file replaces the old one of the same name).
- The first start is done once per exported copy: `<data root>/<Game>/portable-<export stamp>.done`, written only when
  every figure was added or already there (a full disk or a read-only collection: tried again at the next start), and one
  log line, `portable: first start of the copy exported <stamp> (v<version>): <n> figures added, <m> already there, <k>
  not written; keys added|kept`.

## The file

A pack ([`portable_pack.h`](../../runtime/host/collection/portable_pack.h)): the files back to back, then a text index, then
a 32-byte trailer at the very end (`skypack1`, the index's offset and size as little-endian u64, `SKYPACK!`). Index lines are
`<offset>\t<size>\t<name>`, offsets from the start of the whole file. Names: `game/<disc path, lowercase>`, `portable.txt`
(game id, version, kit, the disc's `default.xex` SHA-256, the export's UTC stamp), `keys.ini`, `collection/<file>`,
`icon.png`, and on Windows and Linux `THIRD_PARTY_NOTICES.md` (the .app keeps its own in `Resources`).

| System | Where the pack is | Why |
|---|---|---|
| Windows | after the end of `<name>.exe` | the loader maps the PE image its headers describe and never reads the bytes after it: the 4 GB limit is the image's, the pack is read through a file handle |
| Linux | after the end of the ELF executable (named `.AppImage`; it is not a squashfs AppImage, it needs no FUSE) | the same: the ELF loader maps its segments only |
| macOS | `<name>.app/Contents/Resources/game.skypack` | the executable stays as Arkeyan-Builder signed it; the bundle is signed again (ad hoc) so its seal covers the pack |

The reader refuses a trailer that points outside the file, an index that does not parse, a name that leaves the tree
(`..`, absolute, `\`, `:`), a name twice, and two files that overlap. The game opens its pack once and reads it like a
disc image (`DiscFileSystem::Open` tries a folder, then a pack, then an XDVDFS image); `CheckGameSource` then checks its
`default.xex` against the releases the game was recompiled from, as for a disc.

## How the game starts

1. `portable::OwnPack()`: the program's own file (`datadir::ProgramPath()`), or `../Resources/game.skypack` when it runs from
   `Contents/MacOS`; a pack with a `game/default.xex`. A normal build reads only the last 32 bytes of its executable.
2. `portable::FirstLaunch` (before the settings are read, so the keys apply at once), then the settings, the log (its line).
3. The one-copy check (`savebackup::GameRunning()`).
4. `ResolveGameSource`: a portable's own pack wins over everything (arguments, the copied game data, the remembered disc);
   a damaged one says so and stops ("This portable copy of the game is damaged: ... Export it again.").

## Checks

- `ArkeyanPortablePackTest <scratch>`: a pack after a fake executable (its bytes kept) and one alone; every file read back,
  across chunks and at the end; the folders a path implies, case-insensitive lookups; not a pack, too short, a trailer past
  the end, a cut index, unsafe names, a name twice; a cancelled write; the import (added, never over an existing figure,
  nothing again the second time, no `.part` left).
- Arkeyan Launcher's `ArkeyanLauncherTest --no-disc` runs the export on stand-ins (`PortableChecks`), and
  `ArkeyanLauncherTest --export-portable <disc> <installed game's executable> <folder> [<collection>] [<launcher.ini>]` exports a
  real installed game the way the button does.
- The end-to-end runs and their numbers: below.

## Proof
2026-11-01, on the Apple silicon Mac, scratch folders only (`private/runs/portable/{mac,win,linux}-proof.sh`), one portable on disk at a
time, each deleted after its check [V]:

| | macOS (SSA) | Windows (Giants, under Wine) | Linux (SSA, arm64 container) |
|---|---|---|---|
| exported by | `ArkeyanLauncherTest --export-portable` (macOS build) | the same, Windows build under Wine | the same, Linux build in `ssarecomp-linux-arm64` |
| file | 8,166,821,125 bytes (`.app`), 53 s; `codesign --verify --deep --strict` ok | 8,406,542,295 bytes (`.exe`, above 4 GiB), 17 s; its first 51 MB are the game's exe unchanged | 8,278,720,020 bytes (`.AppImage`), 17 s; executable, the game's bytes first |
| game | the build of this tree | `SGRecomp.exe` linked by Arkeyan-Builder with this tree's Windows kit (`KIT_GAMES=giants tools/make_kit.sh windows`) | `build-linux-arm64` of this tree |
| started | moved to another folder, no argument, no `launcher.ini`, no disc path | moved, no argument | moved, no argument |
| reached | `frontend`, then `level_027` | `title`, then `level_213_tutotial` (Vulkan through MoltenVK) | `frontend`, then `level_027` (lavapipe) |

On macOS: run A (the installed build with the disc) made the saves (`[content] create ... -> 3`, then written); the portable on the
same user folder opened them (`-> 0 (disposition 2)`); on a new user folder it wrote its own there. Its first start added the figure
the folder lacked and kept the one it had (that Gill kept its 500 XP; the exported one had 0), added the keys, and a second start
imported nothing. The installed game started while the portable ran stopped at once (exit 1, "... is already running").
Not tried: a real Windows PC (Defender and SmartScreen on an 8 GB executable), x64 Linux, a portable moved to another Mac (quarantine:
ad hoc signed, so macOS asks once, as for any app from outside the App Store).
