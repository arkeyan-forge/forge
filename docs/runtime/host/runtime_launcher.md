# The runtime's own launcher window (`--launcher`)

> **Purpose:** how does a runtime executable (`Skylanders Spyro's Adventure`, `Skylanders Giants`) find, check and remember the player's disc image, and what is its own `--launcher` window? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [SSA] [Giants]

**This is not Arkeyan Launcher.** It is the runtime's built-in first-run window (`runtime/host/launcher*.cpp`, drawn with
SDL_Renderer, no ImGui), the ancestor of Arkeyan Launcher. **Players use Arkeyan Launcher** (a separate public repo, its own
ImGui UI, the kit that builds the game: [release/launcher/README.md](../../release/launcher/README.md)), which starts
the runtime with the disc path on the command line, so this window does not show. It stays for development runs and
for a runtime started on its own. The "Export for Xbox" section is design (not built; the Xbox port is paused). Its look: [runtime_launcher_ui.md](runtime_launcher_ui.md).

The player brings their own disc image. The runtime checks that it is the game it was recompiled from, remembers it,
and starts the game. Nothing from the game ships with the project.

## Startup flow

`main()` (`runtime/main.cpp`) calls `ResolveGameSource(argc, argv, *FindGame(ARKEYAN_GAME_ID), interactive)`
(`runtime/host/launcher.cpp`): the game is the one the runtime was built for (`-DARKEYAN_GAME`), and `interactive` is false
when `SKY_HEADLESS` is set. The disc path is resolved in this order:

0. **The copied game data**, when the player made it ("Copy the game data", [game_data.md](game_data.md)) and it is
   complete and intact (its `default.xex` is a known release): used first, whatever the arguments, with no disc image
   needed. A damaged copy is said in the log and ignored; `--launcher` / `--select-iso` skip this step.
1. **Command line**: the first argument that is not a flag. It is verified; on failure the reason is printed (and shown
   in a message box when interactive) and the program exits with code 1, with no fallback. A command-line path is used
   for that run only and is **not saved**, so test runs and Arkeyan Launcher never change the saved choice.
2. **Saved choice**: `[<game id>] iso=` in `launcher.ini`. Skipped when `--launcher` or `--select-iso` is passed or
   when Shift (or Option on macOS) is held at startup (`SelectionKeyHeld`: `CGEventSourceFlagsState` on macOS,
   `GetAsyncKeyState(VK_SHIFT)` on Windows, SDL's modifier state elsewhere). If the saved image no longer verifies,
   the window opens so the player sees why.
3. **The launcher window** (`RunLauncherWindow`, [runtime_launcher_ui.md](runtime_launcher_ui.md)): choose or drop an
   image, verification, PLAY. An accepted image is saved to `launcher.ini`; closing the window quits.

Non-interactive mode (`SKY_HEADLESS=1`) never initialises SDL video and never shows a dialog. Without a path it prints
`usage: <runtime> [--launcher] [--select-iso] [<disc image .iso>]` (`<runtime>` is `Skylanders Spyro's Adventure` or `Skylanders Giants`) and
exits with code 1.

## Verification (`CheckGameSource`)

| Check | Status | What the player is told |
|---|---|---|
| path exists (`SDL_GetPathInfo`) | `NotFound` | doesn't exist; moved, renamed, or drive disconnected? |
| it's a file, not a folder | `Folder` | extracted folders aren't supported yet; choose the .iso |
| it opens (`SDL_IOFromFile`) | `Unreadable` | can't be read (OS error); on macOS, a hint about Privacy & Security > Files and Folders |
| XDVDFS volume at one of the known Xbox 360 layouts (`DiscFileSystem::Open`) | `NotXboxDisc` | not an Xbox 360 disc image, or the file is incomplete |
| `default.xex` exists | `NotXbox360Game` | not an Xbox 360 game (original Xbox, data disc), or incomplete |
| `default.xex` reads completely | `Damaged` | incomplete: default.xex is cut short; dump again |
| valid XEX2 execution info header (bounds-checked) | `Damaged` | default.xex isn't a valid Xbox 360 executable |
| title ID belongs to this game (game table) | `WrongGame` | "This disc image is <other game / title ID X>, not <game>" |
| SHA-256 of `default.xex` matches a known release, else its loaded image's (`GameRelease::imageSha256`: a header-only edition, [edition_finder.md](../../tools/edition_finder.md)) | `WrongVersion` | other region or revision: found media ID/version, plus the supported list |
| (same media ID and version but a different hash) | `Damaged` | right disc, but default.xex was damaged or modified |

The reference SHA-256 comes from the player's own image. It is a fingerprint, not game content, and lives in each
game's `games/<id>/info.cpp` (`GameRelease` entries of `g_gameInfo`; `runtime/host/launcher_games.cpp` only exposes
the built game as `g_games`):

| Release | Title ID | Media ID | Version | default.xex SHA-256 |
|---|---|---|---|---|
| SSA, USA, Europe (Redump, XGD3, 8 738 846 720 bytes) | 415608AB | 53CE7B97 | 0.0.0.2 | `e195f513…46ef7b` (12 595 200 bytes) |
| Giants, USA, Europe (Redump, XGD3, 8 738 846 720 bytes) | 415608DA | 52FFE94B | 0.0.0.2 | `12d1e29e…f9fc09e` (7 020 544 bytes) |

Verification reads only the directory tree and `default.xex`: about 12.6 MB for SSA, 0.1 to 0.7 s on an M1 Pro (the range depends on how busy the machine is). The first implementation used XenonRecomp's TinySHA1, which hashes byte by byte and took 0.6 to 1.8 s, so a block-based SHA-256 replaced it.

**Limitation:** an image that is complete up to `default.xex` but cut off later still passes (a per-file manifest or a
whole-image hash would catch it; not built).

## Settings file

- Location: `launcher.ini` in the data root (`datadir::LauncherIni()`, [settings.md](../../getting-started/settings.md)). On macOS that is `~/Library/Application Support/Skylanders/launcher.ini`, on Windows `%APPDATA%\Skylanders\launcher.ini`, on Linux `~/.local/share/Skylanders/launcher.ini`.
- Format: `[section]` lines, `key=value` lines, and comment lines starting with `#` or `;`. Keys and values are trimmed. There is one section per game, keyed by the game id. Keys outside a section belong to the global section `""`. Reads and writes go through `SDL_LoadFile`/`SDL_SaveFile`, so paths are UTF-8 on every platform.

```ini
# Launcher settings: one [section] per game.

[ssa]
iso=<home>/Games/Skylanders - Spyro's Adventure (USA, Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da,Fi).iso
```

- Rewriting the file drops hand-written comments and keeps every key.

## Choosing another disc image later

Hold **Shift** (or **Option** on macOS) while the app starts, or pass `--launcher` or `--select-iso` (macOS:
`open "Skylanders Spyro's Adventure.app" --args --launcher`); or delete or edit `launcher.ini`. On Wayland the modifier state may not be
known at startup [?].

## The app bundle (macOS)

- `cmake` + `ninja ArkeyanRuntime` build `build/ArkeyanRuntime/<runtime>.app` (`MACOSX_BUNDLE`, `<runtime>` = `ARKEYAN_RUNTIME_NAME`: `Skylanders Spyro's Adventure` or `Skylanders Giants`). The template is `resources/macos/Info.plist.in` (`runtime/CMakeLists.txt`):
  - bundle id `org.ssarecomp.<ARKEYAN_BUNDLE_ID>` in a kit (the player's game), `org.ssarecomp.<ARKEYAN_BUNDLE_ID>.dev` in a local build;
    the version and the bundle version are the game's (`games/<id>/game.txt`). macOS keys the Dock's icon on both: before
    0.9.2 every bundle had version 1 and a local build shared the player's identifier, so a test build's neutral icon could
    stand in the Dock for the player's SuperChargers
  - `NSHighResolutionCapable`, the games category
  - `LSMinimumSystemVersion` = `ARKEYAN_MACOS_MINIMUM`: `CMAKE_OSX_DEPLOYMENT_TARGET`, or the build machine's major release when that is unset
  - `NS*FolderUsageDescription` / `NSRemovableVolumesUsageDescription` / `NSNetworkVolumesUsageDescription`: the text macOS shows when the remembered image sits in Documents, Desktop or Downloads, or on another drive
- No game-controller key is needed on macOS. SDL3 reaches gamepads through IOKit/GameController without a usage string, and CoreBluetooth is only built for iOS/tvOS.
- Icon: the local disc-derived family from `ARKEYAN_GAME_ICON_DIR`, or the neutral `resources/macos/ArkeyanRuntime.icns` fallback. It is generated by `resources/macos/make_icon.py <out.icns>` (pure Python plus `sips`/`iconutil`) and contains no game or publisher artwork.
- Post-build steps: `codesign --force --sign - --timestamp=none "Skylanders Spyro's Adventure.app"` (ad-hoc signature that seals Info.plist and resources), then `build/ArkeyanRuntime/<runtime>` is replaced by a symlink to `<runtime>.app/Contents/MacOS/<runtime>`, so scripts keep working.
- Distribution builds should set `-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0` (or 13.0). Both `ArkeyanRecompLib` and SDL must be rebuilt with the same target.

## Testing

```sh
ninja -C build ArkeyanRuntimeLauncherTest           # EXCLUDE_FROM_ALL; tools/test_all.sh builds and runs it
./build/ArkeyanRuntime/ArkeyanRuntimeLauncherTest "$ISO" <scratch dir> [<damaged copy>...]
```

The damaged copies (cut-short images) are optional; the script that made them (`private/launcher/make_truncated.py`)
is not in the tree any more.

`ArkeyanRuntimeLauncherTest` checks:

- SHA-256 test vectors (one-block and two-block padding)
- the real image passes, and its XDBF title name and icon can be read
- each failure status, using synthetic images it writes itself: a missing file, a folder, an unreadable file, empty and random files, an XDVDFS image holding `default.xex` with no XEX2 header, a cut-short file, another title ID, another media ID, and a modified `default.xex`
- damaged copies of the real image
- the command line without UI (path, `--select-iso`, no argument, bad path)
- the settings file: a round trip with spaces, `=`, `#`, `[]`, apostrophes and UTF-8 in paths, a hand-edited file with CRLF and comments, and a missing file

The saved-choice path can be checked end to end without touching the real home folder:
`SKYLANDERS_DATA=<dir> SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk "Skylanders Spyro's Adventure.app/Contents/MacOS/Skylanders Spyro's Adventure"`, with `<dir>/launcher.ini` written beforehand (`CFFIXED_USER_HOME` does not move the data folder).

## Not verified (needs a person at the Mac)

- The window and the error boxes by hand (the screenshot test covers the window's states).
- Launching from Finder. That needs a real home, so the first run shows UI. Expect a one-time macOS prompt for Documents access when the remembered image is in `~/Documents`. Ad-hoc signatures change with every rebuild, so macOS may ask again after a rebuild.
- That holding Option is detected without any permission prompt.
- Gatekeeper on a downloaded copy (the test Mac had assessment disabled).

## Export for Xbox One / Series X|S (design, not built; Xbox paused)

An "Export for Xbox" button appears once the ISO is verified. It produces an installable `.msix` on the player's PC or Mac (decided: a plain `.msix`, no bundle; one x64 package for Xbox One and Series X|S, built without AVX2/FMA). The console has no launcher.

### Where the x64 UWP binary comes from

- **Prebuilt** (runtime + recompiled x64 code, packaged by us): the same derivative-code exposure as above. Rejected for the same reason.
- **Local build** (recommended): run XenonRecomp on the player's `default.xex` (shared with the desktop pipeline), then cross-compile for `x86_64-w64-mingw32` UWP with a bundled **llvm-mingw**. VLC's UWP/Xbox builds use the same toolchain family, and it supports `-mwindowsapp`/UCRT app builds.
  - Download: llvm-mingw 20261006 is 124 MB (`ucrt-macos-universal.tar.xz`), 84 MB (Ubuntu x86_64) or 191 MB (Windows zip). Unpacked it is around 1 GB (estimate); trimmed to the x86_64 target only, about half. It is downloaded on demand, the first time the player exports.
  - Build time: the same generated code as the desktop build (928 s of CPU on a 10-core M1 Pro), plus a prebuilt x64 runtime library. Expect about 2–8 min depending on cores.
  - Disk: about 1 GB for the toolchain, 160 MB of generated sources, a few hundred MB of objects, and a package of about 80–150 MB without game data. Plan for 2–3 GB free (plus the ISO if it goes into the package).

### Packaging and signing on the player's machine

- The launcher **calls `tools/xbox/package.sh`**, (it exists) and does not duplicate it. That script owns the manifest (`AppxManifest.xml.in`), the logos (generated placeholders, no game art), the `makemsix` pack step (Microsoft's open-source msix-packaging, MIT) and signing (for example osslsigncode, which signs APPX/MSIX since 2.6).
- Certificate: a self-signed code-signing certificate created once per player (subject = the manifest `Publisher`) and stored in the launcher's data directory. The private key never leaves the machine. The `.cer` is exported next to the package so it can be installed on the console with the package.
- The launcher passes the inputs (binary, version, certificate path, output path) and shows the script's progress and log.

### Game data on the console

| Option | Package size | Pros | Cons |
|---|---|---|---|
| ISO inside the package (**chosen**) | ~8.8 GB | Simplest runtime (read-only package folder, fixed path) | Uploading 8.7 GB through Device Portal is slow and fragile, every runtime update re-uploads it, and the result is a ready-to-run copy of the game that must never be shared (hard to prevent) |
| ISO on a USB drive (exFAT/NTFS), picked on the console | ~100–150 MB | Small, fast updates, and the game data never enters the package | Needs the `removableStorage` capability, an `.iso` file-type declaration, a first-run picker on the console with the choice kept in `FutureAccessList`, and a StorageFile/handle backend for `vfs.cpp`. Verify on hardware |
| Copied into the app's LocalState via Device Portal | ~100–150 MB | No USB drive | 8.7 GB upload through the Device Portal file explorer is slow; uninstalling deletes it |

Extracting only the game files saves little: they total 8.1 GB (1,142 files, measured) against 8.7 GB for the whole image. The raw ISO goes into the package as is.

### UX

- **Steps with progress:** Check toolchain (download if missing) → Recompile → Compile (n/123 objects) → Package → Sign → Done ("Open folder"). Each step can be cancelled. The cache is reused when nothing changed.
- **Errors** in plain words, with a "Copy log" button. Disk space is checked up front.
- **Instructions shown at the end** (the console steps can't be automated):
  1. Activate Developer Mode: a Microsoft Partner Center developer account (one-time fee), the "Xbox Dev Mode" app on the console, then restart into Dev Mode.
  2. In Dev Home, enable Remote Access / Device Portal and set a username and password.
  3. From the PC or Mac, open `https://<console-ip>:11443`, choose Add, upload the `.msix` (and the `.cer` if asked), then install. The upload is about 8.8 GB because the ISO is inside.
  4. In Dev Home, select the app, then View details, and set **App type: Game**. Without this, a UWP app gets the small "App" memory and GPU budget.
  5. Start the app.
- Later: the launcher could upload the package itself through Device Portal's REST API (`/api/app/packagemanager/package`).
- Xbox One versus Series X|S: Xbox One has a smaller Game budget in Dev Mode and a weaker GPU. Whether it is viable is still to be measured.

## What became of the old plans

The "cross-platform launcher" and "multi-game launcher" plans of this page are done, in other forms: local
recompilation from the player's own `default.xex` is the kit that Arkeyan Launcher downloads
([kit_contract.md](../../release/launcher/kit_contract.md)), one runtime executable per game is the build switch
(`ARKEYAN_GAME`, [layout.md](../../architecture/layout.md)), and the game list, art and per-game settings are
Arkeyan Launcher's. `ReadTitleMetadata()` (`launcher.h`) still reads the title's name and 64x64 icon from the XEX's XDBF
resource.
