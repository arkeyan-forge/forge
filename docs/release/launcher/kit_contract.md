# The player kit contract (what the launcher reads, what Arkeyan-Builder prints)

> **Purpose:** what must the kit hold and print so that Arkeyan Launcher can install any game from it? The one page of the contract. · **Status:** current (2026-11-12) · **Tags:** [Xbox]

Code: `tools/make_kit.sh`, `tools/arkeyan-builder/` (`main.cpp`, `kit_commands.py`), `games/<id>/game.txt`,
`resources/kit-licenses/COMPONENTS.md`; Arkeyan Launcher's `src/launcher/game_install.cpp`, `updater.cpp`. How Arkeyan-Builder and
`make_kit.sh` work inside: [../../tools/arkeyan-builder.md](../../tools/arkeyan-builder.md). **A change to anything on this page
changes both repos: bump the kit `version`.**

## Why a kit

The game is never distributed: a player builds it on their own machine from their own disc. The **kit**
(`skylanders-kit-<platform>.zip`) holds everything but the game: **Arkeyan-Builder** (the build driver, so the player needs no bash,
Python, CMake or git), **XenonRecomp** and **XenosRecomp** (separate programs), the **prebuilt runtime** of every game
(`lib/`, `include/`, `commands/`), `config/<id>/`, `res/`, `dxc` (next to XenosRecomp) and `licenses/`.

## The release assets

The kits are assets of the launcher's own GitHub release (`Arkeyan-Launcher`). `tools/make_kit.sh` writes
`dist/skylanders-kit-<platform>.zip`; the release step publishes each zip **under that name only**, in `SHA256SUMS`
([updates_release.md](updates_release.md)). From 0.6.1 on there are no `ssa-kit-<platform>.zip` copies: a 0.4.x or 0.5.x
launcher asks for that name, finds no update, and its player downloads Arkeyan Launcher 0.6.1 by hand.

| Platform | Asset |
|---|---|
| Windows x64 | `skylanders-kit-windows-x64.zip` |
| macOS Apple silicon | `skylanders-kit-macos-arm64.zip` |
| Linux x86_64 | `skylanders-kit-linux-x64.zip` |
| macOS Intel | not supported: the launcher is Apple Silicon only (from 0.7), macOS refuses it on Intel |

The zip holds one folder, `skylanders-kit-<platform>/` (the launcher takes the zip's single folder whatever its name).
`kit.txt` keys are unchanged in the default kit; a `KIT_LAYOUT_RECORD=1` kit (#72, not released) is format 2, below.

## What the launcher reads from it

- **`kit.txt`**, `key=value` lines (written by `make_kit.sh`):

| Field | Meaning |
|---|---|
| `platform` | `windows-x64`, `macos-arm64` or `linux-x64`; Arkeyan-Builder refuses a kit that is not for the machine (a Windows kit also runs under Wine / cross builds) |
| `version` | The kit version from `make_kit.sh` (currently `0.9.6`); shown in Arkeyan-Builder's first `@log` line and used as the fallback for `[update] kit_tag` |
| `build` | the git revision it was built from (`-dirty` with local changes): the launcher's "kit version" (another `build=` = "Update the game") |
| `games` | **(0.5.0)** the games the kit builds, comma separated: `ssa,giants,swapforce,trapteam,superchargers,imaginators` by default. `KIT_GAMES="ssa giants swapforce"` keeps the previous three-game set; `KIT_GAMES="ssa"` makes a one-game kit. The launcher needs a kit whose `games=` lists the requested game. One `config/<id>/game.txt` each; Arkeyan-Builder stops if it is missing |
| `game_version.<id>` | **(0.5.2)** the version of each game of `games=` (`0.8.0`), from `version=` in `games/<id>/game.txt` (`make_kit.sh` refuses a game without a `X.Y.Z`). The launcher records it when it builds the game and compares it with the release's `games.json`: a higher one there is an **UPDATE**. A kit before 0.5.2 has none |
| `llvm_mingw` | Windows and macOS kits: the llvm-mingw release the Windows compiler must be (`20260922`) |
| `zig` | Linux kit: the zig release the compiler must be (`0.17.0`); Arkeyan-Builder stops if it is not its own |
| `ppc_config_sha256.<id>`, `ppc_context_sha256` | SHA-256 of the headers the game's runtime was compiled against (`PPC_IMAGE_BASE/SIZE`, `PPC_CODE_BASE/SIZE`: one `ppc_config.h` per game, their memory layouts differ; XenonRecomp's context is the same for every game, hence one hash); **Arkeyan-Builder stops if the disc's translation differs (another disc version)**. Before 0.5.0 the first was a single `ppc_config_sha256` |
| `kit_format` | **(#72, opt-in, not in a released kit)** absent in the default kit; `2` in a kit built with `KIT_LAYOUT_RECORD=1`, from public sources only ([kits_without_disc.md](kits_without_disc.md#step-4-done-2026-11-12)): no `ppc_config_sha256.*`/`ppc_context_sha256`, the ABI ids below instead, a `layout_record`, no `fingerprints.txt`. Arkeyan-Builder refuses another value, a format 2 with a missing or malformed id, and a `layout_record` without `kit_format` ("update the game tools"). The launcher does not read it yet (step 7) |
| `abi.context`, `abi.policy.<id>` | **(format 2)** SHA-256 of the public `ppc_context.h` (`tools/abi/headers.py emit`, byte-identical to the one XenonRecomp writes) and, per game, of the `#define PPC_CONFIG_*` lines of its `ppc_config.h`, in order, each with its newline (`headers.py ids <folder>`). After the translation Arkeyan-Builder computes both from the player's headers: **another policy or context stops the install** ("this disc does not match the kit (ppc_config.h differs)"); the four bounds are the layout record's check |
| `layout_record` | **(#72, format 2)** the version of the code layout record (`runtime/guest_layout.h`, `GuestLayoutVersion`, `1`). Its runtimes read the four bounds from a record and ship none: Arkeyan-Builder writes, compiles and links the install's own ([kits_without_disc.md](kits_without_disc.md#step-3-done-2026-11-12)); it stops on another version. The launcher does not read it |

- **`Arkeyan-Builder`** (`Arkeyan-Builder.exe`) at the kit's root, executable (the zip keeps the Unix modes); from 0.8.0 to the 0.9.0 kit a copy named `SkyBuild` (`SkyBuild.exe`) beside it, for launchers up to 0.7.2 (the 0.9.2 kit has none: a launcher fetches the kit of its own tag, and from 0.8.0 looks for `Arkeyan-Builder` first);
  from 0.6.1 there is no `SSABuild` copy (0.4.x launchers ran the builder under that old name); the launcher accepts an
  older kit with either name; `README.txt`,
  `licenses/` (shown to the player; the licences must stay with the kit).

## The games line in the release notes

There is no manifest asset. The release says each game's latest version in **one hidden line at the end of its notes**, an HTML
comment that GitHub does not show: `<!-- games: ssa=0.9.1 giants=0.8.1 swapforce=0.5.0 -->`. `version` is from the kits'
`game_version.<id>`; the kit that builds it is the release's own (its tag). Arkeyan Launcher's `tools/release_kit.sh` writes it (from
the kits' `kit.txt`, which must agree): it reads the release's notes, replaces an earlier games line or appends one, and writes the
notes back; `DRY_RUN=1` only prints them. The launcher reads the line from the release JSON it already fetches and offers **UPDATE**
for a game whose version there is higher than the one it built ([updates_release.md](updates_release.md)); it strips HTML comments
wherever it shows the notes.
**To publish a fix of one game: bump `version=` in `games/<id>/game.txt`, rebuild the kits, release with `release_kit.sh`.**

## Several games in one kit (0.5.0) [Xbox]

A kit builds every game of `games=` (by default `ssa,giants,swapforce,trapteam,superchargers,imaginators`); the player drops a disc and **Arkeyan-Builder picks the game**:

1. It reads `default.xex` from the disc and takes its SHA-256 (`IdentifyGame`, before anything is downloaded).
2. It compares that with `xex_sha256=` of each `config/<id>/game.txt`. The file comes from `games/<id>/game.txt` in this repo; `make_kit.sh` checks that its hashes are the ones `games/<id>/info.cpp` (the runtime's own disc check) accepts. **An unknown disc stops at once** with `@error this disc is not one of the games these tools build (<names>): update the game tools (default.xex SHA-256 ...)`. The launcher never starts Arkeyan-Builder on a disc the kit's `games=` lacks (it fetches the right kit first), so the message is for a hand-run Arkeyan-Builder and an older launcher.
3. Everything else is that game's: its XenonRecomp config (`game.txt` `toml=`), its prebuilt runtime, the way its shaders are found (`shaders=`), the name of the built game (`runtime=`).

`config/<id>/game.txt`, `key=value`: `id`, `version` (the game's own version, `X.Y.Z`; 1.0 = the game at 100 %: each fix bumps the last number; Arkeyan-Builder ignores it), `name` (shown in logs and errors), `runtime` (the built game: `Skylanders Spyro's Adventure`, `Skylanders Giants`, `Skylanders Swap Force`, `Skylanders Trap Team`, `Skylanders SuperChargers`, `Skylanders Imaginators`; the macOS bundle is `<runtime>.app`, the Windows file `<runtime>.exe`; it must be `GameInfo::runtime` in Arkeyan Launcher's game table), `toml` (XenonRecomp's config in the same folder), `xex_sha256` (comma separated), `image_sha256` (each release's loaded image, same order: a disc with one of them is accepted), `releases` (their labels, `;` separated), `title_id` (hex), `shaders`. Older kits may also have `fingerprints.txt` beside it: optional edition-report reference addresses, lengths, structural metadata and hashes ([edition_finder.md](../../tools/edition_finder.md)). New kits omit it; identification and disc acceptance do not depend on it. Unknown-code reports without address diagnostics say "Address matching is unavailable for this report." Optional, for the movie pass (the headless run that dumps the run-time shaders; the legacy no-target recipe uses a timed pass): `movie_portal` (its `SKY_PORTAL`, default `none`; Swap Force: `emulated`, an empty emulated portal, without which it stops at "PORTAL DISCONNECTED" after the title), `movie_press` (its `SKY_PRESS`; Swap Force: A every 2 s, through the menus to a new game) and `movie_shaders` (the run ends once that many new shaders are dumped, or after 120 s; Swap Force: 7; Trap Team: 16; a required target).

`shaders=` says where the Xenos shaders come from:

| Value | Game | What Arkeyan-Builder does |
|---|---|---|
| `lsa` | SSA (Lux) | writes `shaders.lsa`, `effects.lsa` and the loaded `default.xex` image to `shaders/input/` |
| `igz:<path>` | Giants (Alchemy) | reads the IGA v8 archive `<path>` (`/permanent/global.bld`, 8.7 MB), decodes its LZMA chunks, scans the IGZ v6 files for Xenos containers and writes each as `<hash>.vs\|.ps` to `shaders/input/`: **C++ in the kit** (`tools/arkeyan-builder/igz_shaders.cpp`, a small LZMA decoder included), so the kit needs no Python. The Python tool `tools/shaders/giants_extract.py` stays as the reference and gives the same 4394 containers. Formats: [Giants' shaders](../../../games/giants/docs/shaders.md) |
| `igz:<path>` | Swap Force (Alchemy) | the same with the IGA v10 archive `/archives/shaders_xenon.pak` (592 KB) and its IGZ v7 files: 1074 containers, the same as `tools/shaders/swapforce_extract.py`. Formats: [Swap Force's shaders](../../../games/swapforce/docs/shaders.md) |
| `igz:all` | Trap Team (Alchemy) | walks every `.arc` and `.bld` path on the disc, reads IGA v11 / IGZ v8 with the same C++ reader, and deduplicates containers across archives by content. The measured disc has 862 distinct containers (412 vertex, 450 pixel). No archives or containers ship in the kit. Recipe and limits: [Trap Team's shaders](../../../games/trapteam/docs/shaders.md) |

Every game declares a `movie_shaders` target. The built game runs headless with isolated settings, saves and
collection to capture shaders constructed at run time, then the builder translates them, rebuilds the cache and relinks.
`movie_portal` and `movie_press` control the run; it ends at the target or after 120 s. Trap Team's recipe uses an empty
emulated portal, A every 4 s and a target of 16; a complete capture within that limit remains unverified.

A partial or empty capture fails with `@error` and emits no `@done`. Complete captured containers are retained for a
retry. `movies/done.txt` is committed only after translation, compilation and linking succeed, and required translations
are checked on cached installs. The older recipe with no target stops 8 s after its first new shader and cannot prove
completeness. Cross builds require a complete native capture. See [builder details](../../tools/arkeyan-builder.md)
and [Trap Team's shader notes](../../../games/trapteam/docs/shaders.md).

The metadata loop writes each game's current `game_version.<id>` and `ppc_config_sha256.<id>`, alongside the shared
`ppc_context_sha256`. All six games receive the same header, version and runtime-hash checks.

**Layout.** What differs between games is per game, the rest is shared:

| Path | Content |
|---|---|
| `config/<id>/` | `game.txt`, the XenonRecomp config and switch tables |
| `commands/<id>/` | `compile.args`, `shader_cache.args`, `link.args` for that game |
| `lib/<id>/runtime/` | the runtime's own objects, compiled against the game's `ppc_config.h` (and its pipeline baseline) |
| `lib/*.a` | SDL, plume, FFmpeg, zstd, the GPU library, ... one copy for all games (`kit_commands.py` keeps a per-game copy `lib/<id>/<name>.a` only if a library really differs between games, and prints it; the 0.9.2 Windows kit prints none). What the GPU library takes from its game (the game id and the pipeline baseline) is the object `game_inputs.cpp` (`runtime/gpu/`), linked into the executable and kept in `lib/<id>/runtime/` |
| `include/`, `share/` | the same for every game (simde, `indirect_call_guard.h`, `shader_cache.h`, `ppc_context.h`, `shader_common.h`; a `layout_record=` kit adds `share/guest_layout.h`, the record's declaration, and `lib/<id>/runtime/guest_layout.cpp.o`): no `include/<id>/` is needed, the game headers are generated in the player's work folder |
| `res/<id>/Info.plist` (macOS) | the bundle template of that game (its executable name); `res/ArkeyanRuntime.icns` (the neutral icon: Arkeyan-Builder replaces it in the player's bundle with the game's own, made from the disc's title logo) and `res/fonts` are shared |

`kit_commands.py` runs once per game on that game's runtime build (`build-kit-<platform>/<id>/runtime`); `make_kit.sh` builds FFmpeg once and passes it to every runtime. `KIT_GAMES="ssa" tools/make_kit.sh macos` builds a one-game kit.

**Names [Giants].** The Giants runtime is `Skylanders Giants` (`"Skylanders Giants.app"`, `"Skylanders Giants.exe"`), built from the same CMake target `ArkeyanRuntime` with `OUTPUT_NAME` (`runtime/CMakeLists.txt`, `ARKEYAN_RUNTIME_NAME`), so `-DARKEYAN_GAME=giants` builds keep `ninja ArkeyanRuntime` but produce `"build-giants/ArkeyanRuntime/Skylanders Giants"` (plus `.app` on macOS). The log stays `Arkeyan.log`, written in the game's own data folder (`Skylanders/SSA/` or `Skylanders/Giants/`, `host/log.cpp`), as Arkeyan Launcher's support page expects.

## Arkeyan-Builder

`Arkeyan-Builder --iso <disc .iso, or the folder of the copied game data ([game_data.md](../../runtime/host/game_data.md))> --out <game folder> [--kit <kit folder>] [--jobs N] [--toolchain <folder>]`. `--kit` defaults to Arkeyan-Builder's own folder; `--jobs` to what the CPU and memory allow (**2 GB per compiler**); `--toolchain` for a Windows kit run on another OS (an llvm-mingw folder), or an extracted compiler / download cache on Windows and Linux. Exit 0 done, 1 failed, 2 bad arguments. A second run, or an **update** (a new kit in a new folder, run on the installed game), redoes only what changed, decided by content (the translators' and libraries' hashes, the headers' digest), never by the kit's folder, `build=` or file times: a runtime-only update is the link and nothing else; the table is in [../../tools/arkeyan-builder.md](../../tools/arkeyan-builder.md). One event per line on stdout, UTF-8:

| Line | Meaning |
|---|---|
| `@step <id> <title>` | a stage starts: `toolchain`, `disc`, `recompile`, `shaders`, `compile`, `link`, then `movies` ("Adding the movie shaders": the movie pass repeats `shaders`, `compile`, `link`) |
| `@progress <done> <total>` | progress of the current stage (megabytes for the download, files for the rest; shaders for the `shaders` step, one line per 10 shaders) |
| `@log <text>` | information, incl. the last lines of a failed tool's log and `warning: ...`; **`timing <step id> <seconds> s`** is logged when each step ends (0.5.0) |
| `@error <text>` | the build failed; exit 1 (2 for bad arguments) |
| `@done <path>` | built; `<path>` is `<runtime>.exe`, `<runtime>.app/Contents/MacOS/<runtime>` or `<runtime>` (Linux), `runtime` = `Skylanders Spyro's Adventure`, `Skylanders Giants`, `Skylanders Swap Force` or `Skylanders Trap Team`; exit 0 |
The built game's folder also holds `game_icon.png` (512 px, the disc's title logo fitted in a transparent square, made by Arkeyan-Builder for every game: the game's window icon and Arkeyan Launcher's desktop shortcuts load it; [game_art.md](../../runtime/host/game_art.md#the-app-icon)). Work files in `<game folder>/arkeyan-work` (an existing `ssabuild-work` from an older kit is used in place) (2-3 GB; deletable once the game runs: the next build starts over). A Windows kit run on macOS/Linux (a cross build for Wine tests) reuses the `ppc/` and shader cache of a native run and stops with an `@error` saying so when missing. The movie pass **feeds only new shaders and cannot lose the built game** (if the second pass fails the game built above is kept; a rerun retries).

## Kit contents (made by `tools/make_kit.sh [windows] [macos] [linux]`)

`bin/{XenonRecomp,XenosRecomp}`, `Arkeyan-Builder`, `config/<id>/` (see "Several games"), `share/{ppc_context.h, shader_common.h}`, `include/{indirect_call_guard.h, shader_cache.h, simde/}`, `res/{fonts, THIRD_PARTY_NOTICES.md}`, `lib/` (static libraries and objects: SDL, plume, FFmpeg, ...; each game's runtime objects in `lib/<id>/runtime/`), `commands/<id>/` (compile and link command templates from `tools/arkeyan-builder/kit_commands.py`: `ninja -t commands ArkeyanRuntime` with the build machine's paths replaced by `${KIT} ${CXX} ${SDK} ${PPC} ${SRC} ${OBJ} ${OUT} ${OBJECTS}`; a line that is exactly `${OBJECTS}` becomes the object list; the game code library and the shader-cache library are left out: Arkeyan-Builder compiles them from the disc),
`licenses/` (LICENSE, THIRD_PARTY_NOTICES, `COMPONENTS.md` table, one file per component under `components/`, the corresponding source of the LGPL parts: `ffmpeg-8.1.2-without-tests.tar.xz` + `ffmpeg-build.sh`, `XenonRecomp-source.tar.gz` + `XenonRecomp-ssarecomp.patch`). A missing licence file stops the kit build: no component without its licence.
**Reproducible (Windows, 0.9.2).** Two clean builds of one commit give the same `files.sha256`: objects are compiled with `-mno-incremental-linker-compatible` and linked with `-Wl,--no-insert-timestamp` (`cmake/toolchains/x86_64-w64-mingw32-clang.cmake`, and `make_kit.sh` for the tools, whose `-DCMAKE_CXX_FLAGS` replaces the toolchain file's), because clang writes the build time into every COFF object header and lld into every PE header. That time was also why `lib/` held six copies of SDL, plume, ... that differed in bytes 5 to 8 only. The only per-commit bytes are `kit.txt`'s `build=` and `host_log.cpp.obj` (the revision string). [V] Windows kit, 6 games, M1: 188,580 KB and 67.6 MB zipped (0.9.0) -> 130,932 KB and 47.7 MB zipped (`lib/` 115,828 -> 64,160 KB; `SkyBuild.exe`, 6.3 MB, gone). Check: build twice (`rm -rf build-kit-windows dist/skylanders-kit-windows-x64`; `tools/make_kit.sh windows`) and `diff` the two `files.sha256`. The macOS and Linux kits were not checked for it.
**Hygiene**: every compile maps the checkout path to `.` (`-ffile-prefix-map`, `-fmacro-prefix-map`, `-fdebug-prefix-map`), binaries stripped, `-UXENON_RECOMP_USE_ALIAS` for XenonRecomp (every kit emits wrapper functions, the form the macOS game code always had), **`tools/kit_scan.sh <kit>` must pass** before zipping; `make_kit.sh` refuses any file that could hold game data (`.xex`, `.iso`, `ppc_recomp*`, `shader_cache.cpp`, `.lsa`, `image.bin`).

## File list (0.8.1)

Every kit has `files.sha256` at its root: each other file of the kit with its SHA-256, one `<64 hex>  <path>` line, as
`shasum -a 256` prints it (relative paths with `/` on every platform, sorted in C order). `make_kit.sh` writes it last, after
the macOS metadata is removed, and `tools/kit_scan.sh` refuses a kit whose list does not name exactly
its other files, or whose files differ from it (`shasum -a 256 -c --strict`). By hand: `cd <kit> && shasum -a 256 -c files.sha256`.

Why: Windows Defender quarantined `Arkeyan-Builder.exe` of the 0.8.0 kit on a tester's PC (an ML false positive). The zip is
checked against `SHA256SUMS` when it is downloaded, but nothing looked at the extracted files, and the install then failed in
an unclear way. Arkeyan Launcher 0.8.1 checks the kit in use and every installed game at each start, on a thread, names a
missing or changed file with what an antivirus does, and offers Repair (Arkeyan Launcher's `docs/install.md`, "File check"). An
installed game's list is the launcher's (written when the install ends, in the game folder); the format and the checker are
`runtime/host/collection/file_list.{h,cpp}`, shared with Arkeyan Launcher (`tools/sync_skylauncher.sh`), tested by
`ArkeyanFileListTest <scratch>` (FIPS 180-4 vectors at the block edges, the list format and unsafe paths, a missing file, a changed
one, an extra one ignored, the record that spares a re-read, a damaged list, a cancel). Files that legitimately change
(settings, saves, caches, logs) are in no list. [V]

**Speed** [V] (M1, the 0.8.0 macOS kit and the four games' `.app`, 919 files, 379 MB, `private/runs/bench`): hashing everything
takes 2.05 s (about 185 MB/s), over the 2 s target, so a check leaves `files.sha256.verified` next to the list (each file's hash,
size and modification time once it matched) and reads again only a file whose size or time changed: 7 ms for the same 919
files. Writing a game's list at the end of its install takes 0.3-0.5 s, on a thread. A file changed with its size and time put
back is not seen; an antivirus that removes or rewrites a file is.

## Licences in the kit

Runtime and tools MIT; **LGPL**: FFmpeg 8.1.2 (WMA Pro only, linked statically on the player's machine from `lib/`: the objects and libraries are in the kit so the player can relink, the source is provided) and libmspack (`lzxd.c` inside XenonUtils); no GPL code (XenonRecomp decodes with our own PowerPC decoder); dxc (NCSA / LLVM Release License; the text copied from Microsoft's repo because dxc-bin has none: **still an open question**); llvm-mingw runtime (Apache-2.0 with LLVM exception) and MinGW-w64 (ZPL/MIT), downloaded by Arkeyan-Builder on Windows, not shipped; on Linux zig (MIT) is downloaded and its libc++ (Apache-2.0 with LLVM exception) linked statically, zlib (Ubuntu's `libz.a`) linked in: their licences are in `licenses/components/`; fonts OFL. Xenia code (BSD-3) adapted in named files. Never distribute: ISO, `private/<id>/ppc/`, the shader cache, saves, dumps, logs, a built game.
