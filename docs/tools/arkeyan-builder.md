# Arkeyan-Builder and the player kit

> **Purpose:** how does Arkeyan-Builder build a game on the player's machine (Linux: zig), and how is a kit made? · **Status:** current (2026-11-09) · **Tags:** [Xbox]

The game is never distributed: a player builds it on their own machine from their own disc. The **kit**
(`skylanders-kit-<platform>.zip`, made by `tools/make_kit.sh`) holds everything but the game: Arkeyan-Builder (the build driver, so
the player needs no bash, Python, CMake or git), XenonRecomp and XenosRecomp (the code and shader translators,
separate programs), the prebuilt runtime of every game (`lib/`, `include/`, `commands/`), `config/<id>/`, `res/`, dxc
and `licenses/`. Arkeyan-Builder picks the game by the SHA-256 of the disc's `default.xex`, translates the game code and
shaders, compiles them against that game's runtime and links it. Nothing of the disc is in a kit (`make_kit.sh` refuses
`.xex`, `.iso`, `ppc_recomp*`, `shader_cache.cpp`, `.lsa`, `image.bin`), and `tools/kit_scan.sh` refuses anything
personal (user names, build paths, e-mail addresses) in any file, binaries included.

**The contract** with Arkeyan Launcher (asset names, `kit.txt` fields, `config/<id>/game.txt`, the event lines, the layout)
is [../release/launcher/kit_contract.md](../release/launcher/kit_contract.md); this page is how Arkeyan-Builder and
`make_kit.sh` work. Code: `tools/arkeyan-builder/` (`main.cpp`, `igz_shaders.cpp` for Giants', Swap Force's and Trap Team's shaders, `kit_commands.py`),
`tools/make_kit.sh`.

## Running Arkeyan-Builder

    Arkeyan-Builder --iso <disc .iso or copied game data folder> --out <game folder> [--kit <kit folder>] [--jobs N] [--toolchain <folder>] [--icon-only] [--hd-textures off|2x|auto]

`--kit` defaults to Arkeyan-Builder's own folder; `--jobs` to what the CPU and memory allow (2 GB per compiler); `--toolchain`
is needed only for a Windows kit run on another OS (an llvm-mingw folder); on Windows and Linux it may also name an
extracted compiler (llvm-mingw, zig) or the folder to cache the download in. A second run redoes only what changed. `--icon-only` makes the game's icon alone and stops (a check, no build). `Arkeyan-Builder --iso <disc> --menu-music <file.wav> [--kit <kit folder>]` decodes the game's main menu theme into a looping WAV and stops (Arkeyan Launcher's game page; [game_music.md](../runtime/host/game_music.md); the kit's Arkeyan-Builder links the static FFmpeg for it).
`--hd-textures off|2x|auto` (optional, off when absent) doubles the game's textures from the player's disc into the pack
folder `<game data>/textures-hd/` after the build, or alone when there is no `--out`; a rerun with the same disc and
mode is a no-op, `off` removes the folder ([hd_textures.md](hd_textures.md)).
The first `@log` line names Arkeyan-Builder's kit `version` and `build` from `kit.txt`; a disc whose `default.xex` is in no
`config/<id>/game.txt`, and whose loaded image is no release's `image_sha256` either (a header-only edition is accepted),
stops at once with an `@error` listing the supported games. `Arkeyan-Builder --edition-report <disc> [--kit <kit folder>]`
prints what the disc is, as plain text, and stops ([edition_finder.md](edition_finder.md)).

**The game's title and filenames.** The `name` and `runtime` fields of each game's metadata are
`Skylanders Spyro's Adventure`, `Skylanders Giants`, `Skylanders Swap Force`, `Skylanders Trap Team`,
`Skylanders SuperChargers` and `Skylanders Imaginators`.
The executable uses that name (plus `.exe` on Windows); the macOS bundle uses it plus `.app`, and both
`CFBundleName` and `CFBundleDisplayName` show it. Bundle identifiers stay stable. Process launches use argument
arrays and response files, so spaces, accented folders and SSA's apostrophe need no shell escaping. **zig (Linux) gets
every argument on its command line, never a response file** (`WithResponseFile`, 2026-11-09, issue #20): zig's own
response-file reader opens a quote at any apostrophe and never closes it, even inside double quotes (`\'` turns into
`"`), so the link of `.../Skylanders Spyro's Adventure` failed with `: unrecognized file extension`, and a home folder
like `O'Brien` would break every compile and link. Linux's command line holds the whole link. [V] the Linux kit's
Arkeyan-Builder from this change, in `ssa-player-ubuntu24`: SSA to `@done /game/out/Skylanders Spyro's Adventure`, then
90 s of play to `level_027` (`tools/linux/kit_test.sh`); and with the kit at `/opt/o'kit` and the output at
`/game/o'brien out`, the whole install to `@done`. Test: `ArkeyanVersionResourceTest` ("zig keeps apostrophes on argv").
Windows (llvm-mingw's clang) and macOS (Xcode's clang) keep their response files: clang reads an apostrophe inside
double quotes as itself.
For a shell, quote the entire path, for example `"./Skylanders Spyro's Adventure"`. On Linux, Arkeyan-Builder also
writes `game.desktop` beside the executable, with the title and quoted executable path.
The launcher accepts the old `SSARecomp`, `SGRecomp`, `SSFRecomp` and `TTRecomp` names alongside the new ones;
a valid recorded install path is kept, including installs made by the 0.8.0 candidate (once 0.7.3), 0.6.1 and beta kits. Changing the title requires no reinstall.
After a successful rebuild and signing, Arkeyan-Builder removes the old executable or bundle of that game. Failed links
keep the old install; settings, saves and launcher shortcuts stay in place. Recorded old paths resolve to the new
filename beside them until the launcher records the rebuild.

**The game's icon.** The same recipe applies to all six games: the disc's title logo (`art::ReadLogo`),
cropped at alpha greater than 16, centred on transparency with a margin of 1/24 of the square's size.
Each size is rendered from the original with area averaging in premultiplied colour.
Arkeyan-Builder writes the family beside the built game: `game_icon.png` (512 px), `game_icon.ico`
(16, 24, 32, 48, 64, 128 and 256 px PNG entries), and `game_icon.icns`
(16 through 1024 px, including Retina entries). Generation works on every host without `sips` or `iconutil`.
Windows embeds every ICO size in the executable; the window loads the same PNG. macOS copies the ICNS into
Resources before signing the bundle. Linux uses the PNG for the window and desktop entry.
Shortcuts reuse the installed ICO/ICNS bytes and the PNG; older installs without the family keep their PNG fallback.
If no logo is readable, the disc's title icon is decoded and fitted with exactly the same rule.
If neither can be read, Arkeyan-Builder logs the reason and uses platform defaults (the kit's neutral icon on macOS).
No disc art is tracked or shipped in a kit. More: [game_art.md](../runtime/host/game_art.md#the-app-icon).

Local CMake builds use the same names and can take this locally generated family through `ARKEYAN_GAME_ICON_DIR`
(default `private/<game>/icon`). Without those files they clearly report platform defaults.
Kit builds deliberately omit local art; Arkeyan-Builder adds the player's own family when linking.
Synthetic icon decoding, sizes, centring, margins and transparency are checked by `ArkeyanBuilderTest`;
launcher tests cover current and legacy runtime names across all three layouts. Desktop appearance, pinned taskbar
icons, Dock caching and actual disc logo readers still require a manual run on each system.

**The executables' details** (Windows). Arkeyan-Builder writes a VERSIONINFO for the built game (name and version from `game.txt`,
`tools/arkeyan-builder/version_resource.h`) in the `game_icon.rc` of the icon, so a game whose icon failed has one too;
`Arkeyan-Builder.exe` carries the kit's version (`ARKEYAN_KIT_VERSION`, set by `make_kit.sh`, `arkeyan-builder.rc.in`). Check:
`llvm-readobj --coff-resources <exe>` lists VERSIONINFO, or the exe's Details tab; `ArkeyanVersionResourceTest`.

**Names and compatibility identifiers:** [naming.md](../naming.md). The builder sets `SKY_*` environment
variables; older `SSA_*` aliases remain supported.

**The movie pass** (`movies` step): the built game runs headless with scratch settings, saves and collection and
`SKY_DUMP_SHADERS`; the shaders it builds at run time that the cache lacks are translated and the game relinked. By
default no portal or presses. A game's `game.txt` may
set `movie_portal` (`SKY_PORTAL`), `movie_press` (`SKY_PRESS`) and `movie_shaders` (end once that many are dumped; at
most 120 s either way): Swap Force needs an empty emulated portal and presses through the menus to reach its 7th
shader ([kit_contract.md](../release/launcher/kit_contract.md), [Swap Force's shaders](../../games/swapforce/docs/shaders.md)).

All current games declare a target. A short or empty capture fails with `@error` and keeps complete captured containers
for the next install attempt. The legacy recipe without a target stops 8 s after its first new shader; that heuristic
cannot prove completeness, and it left SuperChargers' and Imaginators' 0.9.0 installs on a slow start without Bink's two shaders (movies black,
[render_bugs.md](../runtime/gpu/render_bugs.md)): every game now declares a target (`tools/regress/movie_targets.py`). The completion marker is written only after translation, compilation and linking succeed;
every retained run-time shader must exist in the translated cache. A cross build needs a complete native capture.

**Shader translation** (`shaders` step): XenosRecomp runs one worker per logical core (each worker translates a shader, compiles its DXIL and SPIR-V in process and starts the Metal compiler as a child on macOS), then compresses the DXIL, AIR and SPIR-V caches side by side (zstd level 12 with a 128 MB long-distance window: Giants' AIR in 4 s instead of 59 s at level 22, for about a fifth more bytes). **Only the streams the game reads are compiled**: Arkeyan-Builder names them in a fifth argument, `air` on macOS, `spirv` on Linux, `dxil,spirv` on Windows; the others are written empty. Without that argument (the developer tools, `tools/shaders/build_cache.sh`) every stream of the build is compiled, so one cache serves every build. `SKY_WINDOWS_SHADERS=1` adds Windows' two on macOS or Linux, for the cross build below; a player's install never sets it. On macOS the Metal compiler is given a precompiled header of `shader_common.h` (made once per run; each shader's own copy of the header is skipped by its include guard): its 0.37 s per shader were nearly all spent reading that header (Metal's texture templates), 0.07 s are left. The Metal code is the same (all 399 shaders of a Giants sample compared, `metal-objdump -d` with the metadata numbers set aside); only the AIR's metadata differs, about 40 bytes per shader. It prints `Progress <done> <total>` (flushed) and Arkeyan-Builder turns it into `@progress` lines while it runs. Arkeyan-Builder gives it a fourth argument, `shaders/cache`: every compiled shader (DXIL, SPIR-V, AIR, errors) is kept there, keyed by the hash of its translated text, so the movie pass compiles only the shaders the game built at run time and a game update with a new translator compiles only the shaders whose text changed. The folder is emptied when the kit's DXC libraries change, and it stays in the work folder (200 MB for Giants; it may be deleted with the rest). Where the time went (Giants, 399 of its shaders, 8-core Apple silicon Mac, 2026-11-02): 215 CPU s with every stream at level 22 (the Metal compiler 83 %, DXIL 10 %), 84 s with every stream now, 47 s with the AIR alone (a macOS install); a cache of an older kit is reused whole (0.6 s). To check that two caches hold the same shaders: `python3 -I tools/shaders/compare_cache.py a.cpp b.cpp` (DXIL and SPIR-V byte for byte, AIR by size: the Metal compiler writes a random temporary name into every AIR, so no two runs match byte for byte; a cache of a kit before the precompiled header differs by its AIR sizes too).

**What each step's freshness depends on** (a rerun, or an update that unpacks a new kit in `kit/<tag>/`, redoes a step only
when one of these changed; **content decides, never a file time or the kit's folder or `build=`**):

| Step | Redone when |
|---|---|
| `disc` | the disc image's stamp (path, size, time) changed, or any extracted input differs from `disc/shaders.txt` (names and SHA-256). Older work folders without this inventory reread the disc once. A reread clears the old inputs and movie capture so another disc cannot reuse them. Trap Team's 1916 archives are decoded one per job at once and written in disc order (the same files and log as one by one) |
| `recompile` | the disc, the SHA-256 of **XenonRecomp**, of every file of `config/<id>/` but `game.txt`, of `share/ppc_context.h`, or the kit's `ppc_config_sha256.<id>` / `ppc_context_sha256` changed (a `kit_format=2` kit: the SHA-256s of the translated `ppc_config.h` and `ppc_context.h` once they carry its `abi.policy.<id>` and `abi.context`, the same values, so only a change of `config/<id>/` translates again; a policy or context that differs stops the install). The translated files are written only where they differ, so unchanged functions keep their object. A runtime-only update keeps `ppc/` |
| `recompile`: the layout record | only with a kit whose `kit.txt` has `layout_record=`, at the end of `recompile` (no `@step` of its own, so the launcher's checklist is unchanged; #72, [kits_without_disc.md](../release/launcher/kits_without_disc.md#step-3-done-2026-11-12)): `layout/stamp.txt` differs (the disc, the SHA-256 of `ppc/ppc_config.h` and of the kit's `share/guest_layout.h`, the record version). It parses `disc/default.xex`, refuses generated bounds that are not the executable's (code range: the envelope of its code sections) or that `CheckGuestLayout` rejects, and writes `layout/guest_layout_record.cpp` (version, bounds, the image's SHA-256) beside a copy of `guest_layout.h`, only where its text changed. Another `layout_record` version or no header: the install stops ("update the game tools") |
| `shaders` | the disc, the run-time shader list (movie pass), the SHA-256 of **XenosRecomp** or of `share/shader_common.h`, or the streams asked for (`SKY_WINDOWS_SHADERS`) changed. XenosRecomp then translates every shader again (seconds) but **compiles only the ones whose translated HLSL is new**: `shaders/cache/` holds the compiled DXIL/SPIR-V/AIR keyed by the hash of the translated text (plus pixel/vertex, point size and specialisation mask), and is emptied only when the kit's DXC libraries (`bin/`, all but the two translators) change; the translated text starts with `shader_common.h`, so a new one is covered. An entry records its streams: one with all of them (every cache of an older kit) serves any request, one made for the AIR alone is compiled again when DXIL or SPIR-V are asked for |
| `compile` | per object: it is missing, older than its `ppc/*.cpp`, or its `obj/<name>.key` differs. The key is the SHA-256 of the compile command with the kit's folder written `<kit>`, of the content of the kit's `include/` and of the `ppc/*.h`. A new kit with the same headers and flags keeps every object. `shader_cache` is compiled from `obj/shader_cache_literals.cpp`, a copy with its byte arrays as string literals (the same bytes; clang reads Giants' 200 MB of numbers in 30 s, the literals in 2 s; `shader_cache_literals.h`), deleted after the compile; `ppc/shader_cache.cpp` stays as XenosRecomp wrote it. A layout-record kit adds `layout/guest_layout_record.cpp` with `compile.args`, as `obj/guest_layout_record.o`, its key also covering the copied `guest_layout.h`; the header is in the kit's `share/`, not `include/`, so the game objects' keys are the same with either kit |
| `link` | `link.key` differs (the link command with the kit's folder written `<kit>`, plus the content of every kit library and runtime object it names), or an object is newer than the game. A kit whose runtime libraries changed always relinks. The link must name exactly one `guest_layout_record` object with a layout-record kit (the install's, from the object list) and none with another kit, or the install stops before linking; a failed or cancelled link leaves the previous game in place and removes `link.key` |
| `movies` | `movies/done.txt` is missing/invalid or the complete containers in `shaders/input/runtime/` number fewer than `movie_shaders`. A rerun resumes a short capture; it retries a failed rebuild before committing the marker. Required run-time translations are checked even on cached installs |

So an update whose only change is the runtime (the usual case) redoes `link` and nothing else (a minute or two); a fixed
XenonRecomp redoes `recompile` and compiles only the files whose translation changed; a fixed XenosRecomp redoes
`shaders` and compiles only the shaders whose text changed, then the objects it feeds (`shader_cache`) and `link`. The
`.key` files are new in 0.5.3: a game built by an older Arkeyan-Builder is rebuilt once, then updates are incremental.
Measured: the update from the earlier 0.8.0 candidate kit (before this change) to this Arkeyan-Builder and XenosRecomp, on an installed game, takes 2 s (SSA, Swap
Force), 3 s (Trap Team) and 10 s (Giants): `shaders` reuses every compiled shader (none compiled), then `shader_cache` and
`link`.

Where a clean install's time goes (8-core Apple silicon Mac, other work running, 2026-11-02; `finish` is the pass after
`movies`; the earlier 0.8.0 candidate kit took 155, 543, 214 and 630 s):

| Step | SSA | Giants | Swap Force | Trap Team |
|---|---|---|---|---|
| `disc` | 0 % | 0 % | 0 % | 52 % (75 s) |
| `recompile` | 4 % | 1 % | 3 % | 1 % |
| `shaders` | 25 % (18 s) | 62 % (95 s) | 10 % (15 s) | 12 % (18 s) |
| `compile` | 68 % (48 s) | 24 % (37 s) | 62 % (90 s) | 28 % (41 s) |
| `link` | 0 % | 1 % | 0 % | 0 % |
| `movies` | 1 % | 5 % | 23 % (33 s) | 5 % |
| `finish` | 1 % | 6 % (9 s) | 1 % | 1 % |
| total | 71 s | 152 s | 144 s | 144 s |

Arkeyan-Builder prints one event per line on stdout, UTF-8:

| Line | Meaning |
|---|---|
| `@step <id> <title>` | a stage starts: `toolchain`, `disc`, `recompile`, `shaders`, `compile`, `link`, then `movies` ("Adding the movie shaders": the movie pass repeats `shaders`, `compile`, `link`) |
| `@progress <done> <total>` | progress of the current stage (megabytes for the download, files for the rest) |
| `@log <text>` | information, including the last lines of a failed tool's log |
| `@warn <text>` | launcher-visible degradation or advice: skipped disc shaders, unavailable D3D12 versions (use Vulkan), unsupported archive entries, icon/resource fallback, or low free space; one UTF-8 line, CR/LF replaced with spaces. This can precede `@done`; the launcher should retain and show it |
| `@error <text>` | the build failed; the process exits with 1 (2 for bad arguments) |
| `@done <path>` | the game was built; `<path>` is its executable: `<runtime>.exe`, `<runtime>.app/Contents/MacOS/<runtime>`, or `<runtime>` on Linux, with `<runtime>` from `game.txt` (the full titles listed above); exit 0 |

Incomplete required shader capture or rebuilding emits `@error <text>`, exits 1 and emits no `@done`.
The full failure/skip audit and proof limits are in [the install audit](../../tools/arkeyan-builder/INSTALL_AUDIT.md).

Work files stay in `<game folder>/arkeyan-work` (2-3 GB); an existing `ssabuild-work` from an older kit is reused
in place for compatibility. The player may delete the work folder once the game runs.

A Windows kit run on macOS or Linux (cross build, for tests under Wine) reuses the `ppc/` and shader cache of a native
run and stops with an `@error` saying so when they are missing, or when that run was made without `SKY_WINDOWS_SHADERS=1`
(its cache then has no DXIL and no SPIR-V).
To check a Windows kit on the Mac: copy `arkeyan-work/{disc,ppc,shaders}` of a native (macOS) run made with `SKY_WINDOWS_SHADERS=1` into a new game folder's `arkeyan-work/`, then run the
**macOS kit's** `Arkeyan-Builder --iso <disc> --out <new folder> --kit <the Windows kit> --toolchain <llvm-mingw 20260922, macOS build>`: it compiles and links
`"Skylanders Spyro's Adventure.exe"` / `"Skylanders Giants.exe"` with the Windows kit's libraries and templates (~2 min), which `tools/windows/wine_smoke_test.sh` (`EXE=`) then runs.

## Compatibility checks

The retained file, binary and setting names are listed in [naming.md](../naming.md).

**Compatibility** (each has a unit test):

| Rule | How | Test |
|---|---|---|
| No save, setting, collection or figure moves; `Skylanders`, `SSARecomp`, `Skylauncher` keep their names | the data folder helper and its migration are unchanged; a 0.7.x data folder is left as it is | `ArkeyanDatadirTest` (`TestRenameKeepsData`) |
| An install in `ssabuild-work` keeps working without a rebuild | the builder uses an existing `ssabuild-work` in place (`WorkFolder`, `game_config.h`); a new install uses `arkeyan-work`; the launcher cleans either | `ArkeyanBuilderTest` (`TestWorkFolder`), `ArkeyanLauncherTest` (install housekeeping) |
| The launcher accepts a kit with `Arkeyan-Builder`, or `SkyBuild` (to 0.7.2), or `SSABuild` (0.4.x) | `BuilderNames` and the kit zip check, in that order | `ArkeyanLauncherTest` (shell checks: kit zip and lookup) |
| Older launchers use the kit tagged for their release | until the 0.9.0 kit `make_kit.sh` put a copy of the builder named `SkyBuild[.exe]` in every kit; the 0.9.2 kit has none (a launcher fetches the kit of its own tag, and from 0.8.0 runs `Arkeyan-Builder` first; only an older launcher whose own tag has no release would fall back to the latest kit and not find a builder) | recorded install/update runs |
| A game's log is read under either name | `runtime_names::ReadLog` (`runtime/host/runtime_names.h`, copied to the launcher): the newer of the two | `ArkeyanDatadirTest`, `ArkeyanLauncherTest` (game log, problem report) |
| `launcher.ini` keys, the self-update, the kit lookup by tag, the release names `skylanders-kit-*` and `ArkeyanLauncher*` | unchanged | `ArkeyanLauncherTest` (`UpgradeChecks`, release names) |
| A game built by an old kit starts from the new launcher | the launcher runs the recorded `runtime=`; the game's own `SKY_LOG` is now `Arkeyan.log` | recorded install/update runs |

## Windows: the compiler (0.9.2)

Arkeyan-Builder downloads **llvm-mingw 20260922** (`llvm-mingw-20260922-ucrt-x86_64.zip` from GitHub, 190.7 MB, SHA-256 pinned in
`tools/arkeyan-builder/main.cpp`) into the data folder's `toolchains/` (750 MB extracted) without starting another program:
`HttpDownload` (WinHTTP, the system proxy settings, redirects, 3 retries 5 s apart) and `ExtractZip` (`toolchain_fetch.cpp`,
miniz's inflate and CRC-32 with our own zip directory reader; 9053 files in 15 s on an M1). Before 0.9.2 it ran the system's `curl.exe` and
`tar.exe`: two more programs for antivirus heuristics to weigh on a program that already downloads and unpacks a compiler. `ExtractZip` refuses an entry that would leave the folder (`..`, absolute or
drive paths), a symbolic link, an encrypted or Zip64 archive and a bad CRC-32 or size; the archive's SHA-256 is checked
first, as before. [V] `ArkeyanToolchainFetchTest` (invented archives, every refusal; with `SKY_LLVM_ZIP=<the real zip>` also the
real one). macOS and Linux are unchanged (Linux below).
The extracted folder (and the finished download) is moved into place by `MoveIntoPlace`: an antivirus that is still
scanning the new files makes Windows refuse the rename ("Access is denied", a tester's Trap Team install, 2026-11-12), so it
tries again for about ten seconds, then copies and deletes; if that fails too, the install stops with "another program,
often the antivirus, keeps its files open; press Retry in a minute" [?] (not reproduced on a Windows PC yet).

## Linux: the compiler

Arkeyan-Builder on Linux downloads **zig 0.17.0** (`zig-x86_64-linux-0.17.0.tar.xz` from ziglang.org, 57.3 MB, SHA-256
`1cbe9df9...2026` pinned in `tools/arkeyan-builder/main.cpp`), extracts it with `tar -xJf` into
the data folder's `toolchains/` (`$XDG_DATA_HOME/Skylanders/toolchains`, else `~/.local/share/Skylanders/toolchains`; 406 MB) and compiles with `zig c++
-target x86_64-linux-gnu.2.34`, zig's clang 22. Its global cache (`zig-x86_64-linux-0.17.0-cache` next to it) gets
the libc++ zig builds for that target on the first link (about 90 MB, a few minutes once); its local cache, which
copies every object and the game, is `arkeyan-work/zig-cache`, emptied after each compile and link. curl, else wget,
downloads it.

Why zig:
- **No root, no -dev package.** zig carries clang, lld, libc++/libc++abi/libunwind sources, the Linux kernel headers,
  and the glibc headers with link stubs for any glibc version. Nothing comes from the system: it works the same on a
  read-only SteamOS, Ubuntu, Fedora or Arch, with no gcc, no libc6-dev, no libstdc++-dev.
- **One C++ library and ABI.** The kit's runtime libraries are built with the same zig in the kit image
  (`tools/linux/kit.Dockerfile`), so they and the player's game code are compiled against zig's libc++ headers and
  linked with zig's libc++ (statically). The system libstdc++ is never involved, so the distribution's GCC version does
  not matter. `kit.txt` records `zig=0.17.0`; Arkeyan-Builder refuses a kit made with another zig.
- **A fixed glibc baseline.** Every binary of the kit and the game target glibc 2.34 (Ubuntu 22.04 has 2.35, Debian 12
  2.36, SteamOS 3.5+ and Fedora/Arch newer), whatever glibc the build machine has. 2.34 is the floor anyway: the
  prebuilt dxc (XenosRecomp's dxc-bin, Microsoft's Linux build) needs GLIBC_2.34 and GLIBCXX_3.4.29 (libstdc++ of GCC
  11+), and libz.so.1; every desktop has those runtime libraries (they are not -dev packages).
- **Small.** 57 MB to download. An LLVM release (`LLVM-23.1.4-Linux-X64`) is 1.19 GB (zstd) or 2.0 GB (xz), and still
  needs a glibc sysroot and a C++ library built for it; that is what zig already packs.
- **The game code needs clang** (`__builtin_assume`, `__builtin_rotateleft*`): zig's driver is clang.
- **The system clang** would need clang, lld and the libc/libstdc++ -dev packages (root), and its libstdc++ ABI would
  differ from the kit's libraries: not used, not even as a fallback (`--toolchain` takes a zig folder).

What the built game needs to start: `NEEDED` is only `libc.so.6`, `libm.so.6` and `ld-linux-x86-64.so.2` (see
below for the measured list); SDL loads X11, Wayland, the audio servers, libudev and libusb itself at run time (dlopen),
volk loads `libvulkan.so.1`. zlib (Ubuntu's `libz.a`), FFmpeg and libc++ are linked in.

The portal: `/dev/hidraw*` (HID portals) and `/dev/bus/usb/*` (Xbox portals, through libusb) belong to root. The kit's
`README.txt` gives the udev rules, the only step that needs sudo (on the Steam Deck: `passwd` first):

    printf '%s\n' 'SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1430", ATTRS{idProduct}=="0150", MODE="0660", TAG+="uaccess"' 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1430", MODE="0660", TAG+="uaccess"' 'SUBSYSTEM=="usb", ATTRS{idVendor}=="24c6", ATTRS{idProduct}=="fa00", MODE="0660", TAG+="uaccess"' | sudo tee /etc/udev/rules.d/70-skylanders-portal.rules
    sudo udevadm control --reload-rules && sudo udevadm trigger

When a portal is listed but cannot be opened, the game logs `[portal] hid: Linux needs this udev rule ...` or
`[portal] usb: Linux needs this udev rule ...` with the rule. Before 0.9.4 the rule covered the HID portals only
([portal.md](../runtime/host/portal.md), "Linux: the Xbox portals need their own udev rule"). Arkeyan Launcher's README and
`docs/install.md` still show the one-line HID rule: they need the same three lines (its repository).

## Making a kit

    tools/make_kit.sh [windows] [macos] [linux]

Default: `windows macos`. `KIT_GAMES="ssa"` packs some games only (default `ssa giants swapforce trapteam superchargers imaginators`). Needs the submodules,
`private/<id>/ppc/ppc_config.h` and `ppc_context.h` of every game (SSA: `tools/prepare_game.sh <disc>`; the Alchemy games:
Arkeyan-Builder's output or XenonRecomp on `games/<id>/config/`; the runtime is compiled against these two headers, no game code; `ppc_context.h` must be the same
for every game), `games/<id>/game.txt` whose hashes `games/<id>/info.cpp` accepts, and for Windows llvm-mingw 20260922
and the minimal FFmpeg. The kit's version is `version=` in `make_kit.sh`; `kit.txt`
gets `platform`, `version`, `build`, `games`, `zig` (Linux) or `llvm_mingw` (Windows and macOS),
`game_version.<id>` and `ppc_config_sha256.<id>` per game and one `ppc_context_sha256`. `KIT_LAYOUT_RECORD=1` (#72, off by
default) builds a format-2 kit from public sources only: the runtimes compiled with `ARKEYAN_LAYOUT_RECORD`, without a
record object, against the headers `tools/abi/headers.py` emits (`build-kit-abi/<id>/`); no `private/<id>/ppc`, no
disc variable, no `fingerprints.txt`; `share/guest_layout.h`, `kit_format=2`, `layout_record=1` and the ABI ids
`abi.context`, `abi.policy.<id>` ([kit_contract.md](../release/launcher/kit_contract.md)). It runs from a checkout with no
`private/`: `KIT_DEPS=<folder>` holds llvm-mingw (`toolchains/`), the FFmpeg prefixes and source (`ffmpeg/`); the Linux
container mounts it, and `KIT_SCAN_WORDS`, when they are outside the checkout. `KIT_HEADERS_FROM_PRIVATE=1` builds the
same kit against `private/<id>/ppc` (the parity gate only): `python3 -I tools/tree_parity.py <that kit> <public kit>
--rules tools/parity/public.rules` must find every file identical. Compare a record kit with the default kit with
`--rules tools/parity/kit.rules`, and two installs of one disc with `tools/parity/install.rules`. `linux` runs in Docker on any host: it builds the image `ssarecomp-kit-linux` from `tools/linux/kit.Dockerfile` (Ubuntu
24.04, amd64, the -dev headers SDL's configure looks for, zig 0.17.0 checked by SHA-256) and runs the same script in it
with the checkout mounted at `/src`; FFmpeg goes to `$KIT_DEPS/ffmpeg/linux-x64` (`tools/ffmpeg/build.sh linux-x64`).
`tools/linux/kit_test.sh` runs a kit on a player's machine (`tools/linux/player.Dockerfile`: Ubuntu 22.04 or Arch with
no compiler) from the disc to `@done`, then optionally the game offscreen. Every
compile maps the checkout path to `.` (`-ffile-prefix-map`), binaries and libraries are stripped, and
`tools/kit_scan.sh` must pass before a kit is zipped. `make_kit.sh` deletes macOS metadata (`._*`, `.DS_Store`) and zips with `COPYFILE_DISABLE=1`.

## Trap Team and disc-free checks

`games/trapteam/game.txt` now supplies `shaders=igz:all`: Arkeyan-Builder walks all `.arc` and `.bld` files from the disc's
read-only index, reads IGA v11 / IGZ v8 with `igz_shaders.cpp`, and writes each distinct container once across the archives.
Decoding the LZMA of the 1916 archives is the step's time: one archive per job at once (`CollectArchiveShaders`), written
in disc order (`WriteArchiveShaders`), 246 s one by one and 43 s with 8 jobs on an Apple silicon Mac (2026-11-01).
The existing `igz:<path>` recipes still read one archive for Giants and Swap Force; SSA still reads its LSA files and XEX image.
The measured Trap Team disc has 862 containers. The 16 additional containers observed in a run to the first level cannot
be extracted from the disc and are never shipped. The movie pass tries an empty emulated portal, A every 4 s and a target
of 16; reaching all 16 within 120 s without skipping movies remains unverified. Details and the manual merge:
[Trap Team's shaders](../../games/trapteam/docs/shaders.md).

To verify without game data, a kit build, or a game run (from the repository root):

    cmake --build build --target Arkeyan-Builder ArkeyanBuilderTest ArkeyanVersionResourceTest -j4
    ctest --test-dir build -R '^ArkeyanBuilderTest$' --output-on-failure
    build/tools/arkeyan-builder/ArkeyanVersionResourceTest
    sh -n tools/make_kit.sh
    python3 -I tools/check_docs.py

The existing `build/` must be configured with `ARKEYAN_TOOLS_ONLY=ON`. `ArkeyanBuilderTest` reads the games' actual metadata,
checks each accepted hash against its runtime's check (including both Swap Force editions), rejects an unknown hash, and
checks the old shader recipes and movie defaults. Synthetic IGA v8/v10/v11 and IGZ v6/v7/v8 fixtures check the container
bytes, pixel/vertex pairing, unsupported input and deduplication across archives. Fixtures stay in the build folder.
These tests do not prove a full disc extraction, native/cross linking or the run-time collection; those require separate
validation with the player's disc.
