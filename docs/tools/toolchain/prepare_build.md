# `prepare_game.sh`, the build options, the prebuilt game code library, the platform builds

> **Purpose:** how is a port built on the developer's machine, from the disc to the executable, and with which CMake options? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [SSA]

The build is generic; `prepare_game.sh` is SSA's (it hard-codes `games/ssa/config/SSA.toml` and `private/ssa/ppc/`).
Giants' `private/giants/ppc/` comes from Arkeyan-Builder's output on its disc (`tools/make_kit.sh` header; [../arkeyan-builder.md](../arkeyan-builder.md))
or from XenonRecomp run by hand as in [../../porting/day1.md](../../porting/day1.md).

## `tools/prepare_game.sh <iso> [--shaders] [--shader-dump <dir>]` (macOS or Linux)

Derives everything the runtime build needs from the player's disc, on the build machine; **none of it may be committed or distributed**:
1. `git submodule update --init --recursive` when `dxc.exe` or SDL is missing.
2. Host tools: `cmake -S . -B private/build-host-tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_FFMPEG=OFF -DARKEYAN_TOOLS_ONLY=ON` then `ninja XenonRecomp xexdump` (the root project applies `patches/*.patch` to the submodules at configure time; `ARKEYAN_TOOLS_ONLY` configures only these tools so a fresh clone works before `ppc/` exists).
3. `xdvdfs.py <iso> private /default.xex`, `xexdump default.xex private > private/xexdump.txt` (also writes `private/image.bin`).
4. XenonRecomp **on a copy laid out like the TOML expects** (`config/`, `private/default.xex` symlink, `ppc/` in `private/ppc-gen/`; it reads paths relative to the TOML), then **moves into `private/ssa/ppc/` only the files whose content changed** (a rerun does not recompile the game code; ~4 s): "private/ssa/ppc/: N file(s) updated".
5. The shader cache when missing, when it has no DXIL, or with `--shaders`: `tools/shaders/build_cache.sh <iso> [<dump dir>]` (**build it on the Mac**: AIR + SPIR-V + DXIL for every platform).

## CMake options

| Option / variable | Meaning |
|---|---|
| `ARKEYAN_GAME` (`ssa` default, or `giants`) | the game to build: selects `games/<id>/`, the engine (`engines/lux/` or `engines/alchemy/`), `private/<id>/ppc/` and the executable name (`Skylanders Spyro's Adventure` or `Skylanders Giants`, `ARKEYAN_RUNTIME_NAME` in `runtime/CMakeLists.txt`); one build dir per game (`build`, `build-giants`) |
| `ARKEYAN_PPC_DIR` | XenonRecomp output (`ppc_*.cpp/h`, `shader_cache.cpp`), default `private/${ARKEYAN_GAME}/ppc` |
| **`ARKEYAN_PPC_LIBRARY=<libArkeyanRecompLib.a>`** | link a **prebuilt library of the same `private/<id>/ppc/`** instead of compiling it again (extra build trees). **Rebuild it whenever `private/<id>/ppc/` changes** (a 10-13 Oct library crashed HEAD at the main menu) |
| `ARKEYAN_TOOLS_ONLY` | configure only the host tools (XenonRecomp, xexdump, Arkeyan-Builder) |
| `ARKEYAN_FFMPEG` (ON) / `ARKEYAN_FFMPEG_ROOT` / `ARKEYAN_FFMPEG_AUTOBUILD` (ON) / `ARKEYAN_FFMPEG_JOBS` | XMA decoding with a minimal LGPL FFmpeg ([audio_xma_xaudio.md](../../runtime/xbox360/audio_xma_xaudio.md)); `-DARKEYAN_FFMPEG=OFF` = null decoder |
| `ARKEYAN_KIT_BUILD=ON` (only `make_kit.sh`) | no development path in the build: leaves out the `ARKEYAN_SOURCE_FONT_DIR` definition (fonts looked for in the source tree) so the kit holds no checkout path (the runtime is configured against stub `ppc` headers with empty sources: `-DARKEYAN_PPC_DIR=<build>/stub-ppc`) |
| `ARKEYAN_UWP` (set by the UWP toolchain file) | UWP host instead of SDL |
| project version | `project(ArkeyanRuntime VERSION 0.1.0)`; `ARKEYAN_VERSION` (= `PROJECT_VERSION`) appears in `--version`, the window title and the `[version]` log line |
| `ARKEYAN_UWP_MARCH` (`btver2` default, or `x86-64-v3` for a Series-only package; also an env var) | CPU baseline; v3 adds `-ffp-contract=off`, `host/uwp/cpu_check.cpp` makes an Xbox One exit cleanly with the reason in the log |
| `LLVM_MINGW_ROOT` | the llvm-mingw 20260922 ucrt release (Windows/UWP cross builds) |
| toolchains | `cmake/toolchains/x86_64-w64-mingw32-clang.cmake` (Windows desktop, `-march=btver2`), `cmake/toolchains/uwp-x64-llvm-mingw.cmake` (Xbox: `--appcontainer`, static libc++/libunwind/winpthreads, umbrella libs windowsapp/ucrtapp) |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | distribution builds: 11.0 or 13.0, with `ArkeyanRecompLib` and SDL built with the same target |

Build commands: macOS `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && ninja -C build ArkeyanRuntime`; the post-build step ad-hoc signs `<runtime>.app` (`codesign --force --sign - --timestamp=none`) and replaces `build/ArkeyanRuntime/<runtime>` by a symlink into the bundle (`<runtime>` = `Skylanders Spyro's Adventure`, or `Skylanders Giants` in `build-giants`).
Test targets (`ninja -C build <target>`): `ArkeyanFigureTest`, `ArkeyanPortalTest`, `ArkeyanPortalAsyncTest`, `ArkeyanSettingsTest`, `ArkeyanCollectionTest`, `ArkeyanArtTest`, `ArkeyanDatadirTest`, `ArkeyanSavesTest`, `ArkeyanRecompGpuTest`; `ArkeyanRuntimeLauncherTest` and `ArkeyanTexturePackTest` are not built by default. The harness runs them all: [../../getting-started/testing.md](../../getting-started/testing.md).
Third-party licences: `THIRD_PARTY_NOTICES.md` ships in every package; `XenonRecomp` GPL disassembler is **not** in the runtime (`ArkeyanXenonUtils` = XenonUtils without it).

## Arkeyan-Builder and the player kit
On the player's machine Arkeyan-Builder does the derive and build steps from the kit, for every game of the kit: internals in
[../arkeyan-builder.md](../arkeyan-builder.md), the contract with Arkeyan Launcher in
[../../release/launcher/kit_contract.md](../../release/launcher/kit_contract.md). `tools/make_kit.sh [windows] [macos]
[linux]` makes the kits; it needs `private/<id>/ppc/ppc_config.h` and `ppc_context.h` of every game it packs.

## Windows and Linux builds

Both scripts run `prepare_game.sh` first, so they build SSA; for Giants on Windows or Linux, use the kit.
`tools/build_windows.sh <iso> [--shader-dump <dir>]` (cross-compile on the Mac with llvm-mingw 20260922 ucrt, downloaded and SHA-256-checked, FFmpeg for Windows, `build-win64`, Release `-O2 -march=btver2`; first build 10-30 min at -j4/-j5), output `build-win64/dist/ArkeyanRuntime/` (`"Skylanders Spyro's Adventure.exe"` 55 MB self-contained, `fonts/`, `THIRD_PARTY_NOTICES.md`).
`tools/build_linux.sh <iso> [--arch arm64|amd64] [--shader-dump <dir>]` (Docker, Ubuntu 24.04, clang 18, Vulkan, lavapipe; `tools/linux/Dockerfile`). Details and traps: [platforms](../../getting-started/platforms/README.md).
