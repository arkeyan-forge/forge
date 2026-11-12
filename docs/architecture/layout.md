# Layout: where everything is, and how a build picks its game

> **Purpose:** where is each folder and important file, which code is runtime, engine or game, and how does one tree build several games? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants]

The Arkeyan forge builds native ports of the Skylanders games by static recompilation. One
runtime serves every game; what belongs to an engine or to a game is kept apart from it.

## The tree

```
runtime/          the shared runtime: kernel and XAM, d3d (PM4/Xenos), gpu (plume), apu, host (window, input, audio,
                  portal, the --launcher window, Toy Box, collection, art), platform, host/uwp. Game-neutral. README.md
engines/lux/      hooks of the Lux engine (Spyro's Adventure): light_tiles.cpp, movie.cpp, patches.cpp (NGL guards,
                  heap free guard), ultrawide.cpp (scene targets). README.md
engines/alchemy/  hooks of the Alchemy engine (Giants; Swap Force and SuperChargers later): ultrawide.cpp (the
                  scene targets' size). README.md
games/ssa/        Spyro's Adventure: addresses.h, adapter.cpp, info.cpp, language.cpp (voice language hooks), ssa.h,
                  ssa_portal_prompt.{h,cpp}, game.txt, art.json, config/ (SSA.toml, SSA_switch_tables.toml,
                  SSA_symbols.txt), pipelines/baseline.zst, docs/. README.md
games/giants/     Giants: addresses.h, adapter.cpp, info.cpp, giants_portal_prompt.{h,cpp}, game.txt, art.json,
                  config/ (Giants.toml, Giants_switch_tables.toml), pipelines/baseline.zst, docs/. README.md
tools/            scripts and small programs (prepare_game.sh, make_kit.sh, test_all.sh, sigmatch, ppcinfo.py,
                  check_docs.py, docs_move.py, arkeyan-builder/...)
patches/          our changes to the submodules, applied at configure time
thirdparty/       submodules (SDL, imgui, plume, XenonRecomp, XenosRecomp), stb (vendored) and plume.cmake
resources/        what is not code and not game: the figure catalogue, fonts, the macOS bundle files, kit licences
cmake/            CMake helpers and toolchain files
docs/             the documentation (docs/architecture/ is this folder)
ArkeyanRecompLib/     the CMake target that compiles a game's generated code (ppc_recomp.*.cpp) into a library
private/<id>/ppc/ the generated code of each game (git-ignored, never committed)
```

`private/` is git-ignored as a whole. It holds everything derived from a player's disc: `private/<id>/ppc/`
(XenonRecomp's output and the shader cache), SSA's `private/image.bin`, `private/default.xex`, `private/xexdump.txt`
and `private/shaders/` (Giants keeps its own under `private/giants/`), the toolchains and test runs.

## Three kinds of code, and who owns each

| Kind | Where | Knows about | Reads its addresses from |
|---|---|---|---|
| Runtime | `runtime/` | the Xbox 360, the XDK, middleware (Bink, XMA, zlib), the host machine | `games/<id>/addresses.h` |
| Engine hooks | `engines/<engine>/` | one engine's behaviour (NGL, its heap, its movie player, its scene targets) | `games/<id>/addresses.h` |
| Game | `games/<id>/` | one game: addresses, the portal prompt, voices, the disc's identity, recompiler input | itself |

The rule for a new piece of code: if it would be the same in the next game, it is the runtime; if it would be the same in
every game of the engine, it is an engine hook; otherwise it is the game's. [hooks.md](hooks.md) lists every hook with
its class.

The runtime asks the game for seven things only, through the adapter in `runtime/game.h` ([hooks.md](hooks.md#the-adapter-interface-runtimegameh)):
its identity (`info`), the portal prompt reader, whether ultrawide has a camera hook, the voice language, the glyph
textures, the intro movies, and whether it presents at every vblank. Everything else is a hook or a constant in
`addresses.h`.

## How a build picks its game

`cmake -DARKEYAN_GAME=ssa|giants` (default `ssa`), in the root `CMakeLists.txt`:

- `games/<id>/` is on the include path (`addresses.h`), and its `*.cpp` are compiled into the runtime;
- the engine (`ssa` -> `lux`, `giants` -> `alchemy`) gives `engines/<engine>/*.cpp`, compiled in the same way;
- the generated code is `private/<id>/ppc` (override: `-DARKEYAN_PPC_DIR=<dir>`); `tools/make_kit.sh` points it at a
  game-free stub;
- the pipeline baseline is `games/<id>/pipelines/baseline.zst`;
- `ARKEYAN_GAME_ID="<id>"` is defined for the data folder, the saves and the log.

The CMake target is `ArkeyanRuntime` for every game and the build tree keeps `build/ArkeyanRuntime/` (the runtime is built
with `add_subdirectory(runtime ArkeyanRuntime)`); the **executable** is `ARKEYAN_RUNTIME_NAME` (`runtime/CMakeLists.txt`):
`Skylanders Spyro's Adventure` for SSA, `Skylanders Giants` for Giants, also the name of the macOS bundle and the Windows `.exe`. Launchers, kits,
scripts and the app bundle depend on those names.

## Per folder, one line each

### `runtime/`

| Path | Role |
|---|---|
| `main.cpp` | entry: settings, log, resolve the disc (`ResolveGameSource`), `datadir::MigrateOnce`, load the XEX, SDL window, input loop, quit with `_Exit` |
| `loader.{h,cpp}` | XEX mapping, import resolution, kernel variables, host thunks |
| `memory.{h,cpp}`, `thread.{h,cpp}`, `vfs.{h,cpp}` | 4 GB guest block, heaps, physical windows; guest threads, PCR/KTHREAD, APCs; the XDVDFS reader |
| `game.{h,cpp}`, `game_hooks.h`, `language.h` | the adapter interface, the `GAME_HOOK` macros, the text-language interface |
| `inflate.cpp` | the host's zlib for the game's `inflate` (hook addresses from `addresses.h`) |
| `indirect_call_guard.cpp` (+ `ArkeyanRecompLib/indirect_call_guard.h`) | a bad indirect call is logged and skipped |
| `kernel/` | the imports ([kernel_exports.md](../runtime/xbox360/kernel_exports.md)): `objects`, `sync`, `threads`, `system`, `memory`, `files`, `overlapped`, `video`, `audio`, `xma`, `xmp`, `xam*`, `xconfig`, `input`, `net`, `module`, `portal`, `printf`, `rtl_time`, `strings`, `stubs`, `imports_giants`, `import_list.inc` |
| `d3d/` | the D3D replacement ([gpu/](../runtime/gpu/README.md)): `render.cpp`, `device.{h,cpp}`, `ring.{h,cpp}`, `command_buffers.{h,cpp}`, `null_gpu.cpp`, `shaders.{h,cpp}`, `xenos.{h,cpp}`, `bink_display.cpp`, `ultrawide.{h,cpp}`, `capture.{h,cpp}` (`SKY_D3D_CAPTURE`), `depth_readback.h` |
| `gpu/` | the host renderer on plume: `gpu.cpp`, `draw.cpp`, `worker.cpp`, `context.h`, `buffer_cache.*`, `d3d12_diagnostics.cpp` (DRED), `d3d12_pipeline_cache.cpp`, `shader_cache.h`, `shaders/`, `embed*.cmake`, `selftest.cpp` (`ArkeyanRecompGpuTest`) |
| `apu/` | `xma_context.*` (the hardware context model), `xma_decoder.h`, `xma_decoder_ffmpeg.cpp`, `xma_decoder_null.cpp` |
| `host/` | SDL3 host ([host/](../runtime/host/README.md)): `audio*`, `display`, `input`, `glyphs`, `settings`, `log`, `launcher*`, `launcher_games.cpp`, `launcher_ui*`, `launcher_assets`, `widgets/`, `portal*`, `figures/`, `collection/`, `art/`, `toybox*` and `toybox/`, `texture_pack`, `imgui_software`, `dds`, `stb_impl.cpp`, `embedded_fonts.h` |
| `host/uwp/` | the UWP host: `main.cpp` (CoreApplication), `sdl_subset.cpp`, `portal_backends.cpp` (GIP and HID routes), `winrt_portal.h`, `winrt_extra.h`, `settings_uwp.cpp`, `cpu_check.cpp`, `uwp_compat.cpp` |
| `platform/` | `platform.h`, `platform_posix.cpp`, `platform_win32.cpp` (also UWP), `timebase.cpp`, `win32/` (resource files) |
| `CMakeLists.txt` | the target `ArkeyanRuntime` and the tests `ArkeyanPresentTest`, `ArkeyanRuntimeLauncherTest` (links, `EXCLUDE_FROM_ALL`), `ArkeyanTexturePackTest`, `ArkeyanSettingsTest`, `ArkeyanPortalTest`, `ArkeyanPortalAsyncTest`, `ArkeyanFigureTest`, `ArkeyanArtTest`, `ArkeyanCollectionTest`, `ArkeyanDatadirTest`, `ArkeyanSavesTest`, `ArkeyanGuestFormatTest`, `ArkeyanProgressTest`, `ArkeyanAchievementsKernelTest`; `gpu/CMakeLists.txt` adds `ArkeyanRecompGpuTest` |

`ArkeyanRecompLib/`: builds `private/<id>/ppc/` into `libArkeyanRecompLib` with the guard header force-included, or imports
`ARKEYAN_PPC_LIBRARY`.

### The rest

| Path | What |
|---|---|
| `CMakeLists.txt`, `cmake/` | the root project (`ARKEYAN_GAME`, `ARKEYAN_PPC_DIR`, `ARKEYAN_TOOLS_ONLY`, `ARKEYAN_UWP`...), `embed_file.cmake`, `embed_fonts.cmake`, `join_json.cmake`, `ffmpeg.cmake` (+ `ffmpeg_license_probe.c`), `toolchains/`, `mingw/` |
| `patches/` | `xenonrecomp.patch`, `xenosrecomp.patch`, `plume.patch`, `plume_queue_order.patch` |
| `thirdparty/` | submodules `XenonRecomp` (MIT), `XenosRecomp` (MIT, with `dxc-bin`, `xxHash`, `zstd`, `smol-v`), `plume` (MIT), `SDL` (zlib), `imgui`; `stb` (vendored, not a submodule); plus `plume.cmake` |
| `resources/` | `collection/catalogue.json` (271 toys), `launcher/fonts`, `toybox/fonts` (Outfit, Bungee), `macos/{Info.plist.in,make_icon.py,ArkeyanRuntime.icns}`, `kit-licenses/` (COMPONENTS.md and licence texts) |
| `tools/` | `prepare_game.sh`, `build_windows.sh`, `build_linux.sh`, `make_kit.sh`, `kit_scan.sh`, `sync_skylauncher.sh`, `test_all.sh`, `check_docs.py`, `docs_move.py`, `find_switch_tables.py`, `ppcinfo.py`, `xmatch.py`, `xexdump/`, `sigmatch/`, `shaders/`, `pipelines/`, `perf/`, `regress/`, `ffmpeg/`, `arkeyan-builder/`, `windows/`, `linux/`, `xbox/` ([tools/](../tools/README.md)) |
| `README.md`, `CHANGELOG.md`, `LICENSE` (MIT), `THIRD_PARTY_NOTICES.md` | the README, the changelog, the notices (shipped in every package) |

## Choices made in the reorganisation (2026-10-25)

- `runtime/` holds the shared runtime; its target and build folder are `ArkeyanRuntime`.
  Player-visible filenames come from `games/<id>/game.txt`; compatibility names are in [naming.md](../naming.md).
- `ArkeyanRecompLib/` stays where it is (a build target for generated code, not a folder of sources).
- SSA's config and pipeline baseline moved to `games/ssa/`; its generated code from `ppc/` to `private/ssa/ppc/`.
- The disc table (title ID, media ID, SHA-256) moved from `runtime/host/launcher_games.cpp` to `games/<id>/info.cpp`, a
  file with no dependencies so that the launcher test can link it. A runtime now knows the releases of its own game
  only: a Giants disc given to the SSA runtime is reported as "another game (title ID ...)" instead of by name.
- The window title uses the game's name.
- SSA's `private/image.bin`, `private/default.xex`, `private/xexdump.txt` and `private/shaders/` stay in `private/`;
  `build_cache.sh` and `ppcinfo.py` know both layouts.

## Add a game

The worked example, step by step: [porting/day1.md](../porting/day1.md).
