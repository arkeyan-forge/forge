# Platforms: macOS, Windows, Linux, Xbox (UWP)

> **Purpose:** what runs where, how each host is built and tested, and which traps are common to all hosts · **Status:** current (2026-11-05) · **Tags:** none (host platforms; console facts tagged [Xbox] where they appear)

## What it is

The same runtime runs on four hosts. Each page below is the platform's own facts (build, GPU backend, runtime
specifics, tests, known problems); the settings, the data folder and the test recipe are shared and live in
[../settings.md](../settings.md), [data_folders.md](data_folders.md) and [../testing.md](../testing.md).

| Platform | GPU backend | Status (2026-10-26) |
|---|---|---|
| [macOS arm64](macos.md) | Metal | plays well, played daily: 30 fps locked at x2 in levels, ~56 fps with the 60 cap (M1 Pro) |
| [Windows x64](windows.md) | Vulkan, D3D12 fallback | works on a real PC (Radeon 840M iGPU) through Arkeyan Launcher; the portal write (Arkeyan Launcher #11) and the frame rate with a portal (#12) are fixed in 0.3.1, the tester's retest pending |
| [Linux x64](linux.md) | Vulkan | player kit since 0.3.1 (`ssa-kit-linux-x64`, zig 0.17.0, glibc 2.34); tested in containers only (lavapipe), never on a real desktop |
| [Xbox Series/One (UWP, Dev Mode)](xbox_uwp.md) | D3D12 | runs at 15-21 fps, CPU-bound; **paused** (user decision 2026-10-23) |

## Where the code is

- `runtime/platform/` (`platform_posix.cpp`, `platform_win32.cpp`: memory, faults, timers), `runtime/host/uwp/` (the
  Xbox host), `runtime/gpu/` (plume backends: Metal, Vulkan, D3D12).
- Build scripts: `tools/prepare_game.sh` (the game data step), `tools/build_windows.sh`, `tools/build_linux.sh` and
  `tools/linux/Dockerfile`, `tools/xbox/` (setup, packaging, `check_uwp_imports.sh`), `tools/make_kit.sh` (player kits).
- Game data step (all platforms): `tools/prepare_game.sh <disc .iso>` writes `private/default.xex`,
  `private/image.bin` (`tools/xexdump`), `private/ssa/ppc/` (XenonRecomp with `games/ssa/config/SSA.toml`, ~4 s) and
  `private/ssa/ppc/shader_cache.cpp` (`tools/shaders/build_cache.sh`, ~1.5 min). It rewrites only the `ppc/` files
  whose content changed, so a rerun recompiles nothing. The shader cache is built when missing, or again with
  `--shaders`; `--shader-dump <dir>` adds the shaders the game builds at run time (Bink movies), saved by a run with
  `SKY_DUMP_SHADERS=<dir>`. `build_windows.sh` and `build_linux.sh` call it first. It prepares SSA only; Giants:
  [../testing.md](../testing.md#a-fresh-clone). None of this output may be distributed.

## How to test it

The isolated fast run and the harness are in [../testing.md](../testing.md). Per platform: macOS runs the harness
(`tools/test_all.sh`); Windows `tools/windows/wine_smoke_test.sh` (Wine) and `tools/windows/d3d12_test.sh` (D3D12 in an
x86_64 container); Linux `tools/linux/smoke_test.sh` and `tools/linux/kit_test.sh` (the player kit); Xbox: a package on
a console ([xbox_uwp.md](xbox_uwp.md)).

## Proving a game on every backend (how-to)

Any game with its `private/<id>/ppc/` (shader cache included, built on the Mac: [../testing.md](../testing.md#a-fresh-clone)) and
a `build-<id>` folder. Every game run through `../gamelock.sh` on a shared Mac; outputs under `private/runs/`.
1. **Vulkan on macOS** (MoltenVK): `brew install molten-vk vulkan-loader`; configure a second folder with
   `-DARKEYAN_MACOS_VULKAN=ON` ([plume_backends.md](../../runtime/gpu/plume_backends.md)), then run the game's hub test with
   `VK_GAME_EXE=<build-vk>/ArkeyanRuntime/<game>.app/Contents/MacOS/<game> LIVE_EXE=$PWD/tools/regress/moltenvk_exe.sh`
   (`superchargers_lobby.sh`, `imaginators_hub.sh`), once on each build; compare the `shots/` of the same run index. 60 FPS:
   `GAME_EXE=$PWD/tools/regress/moltenvk_exe.sh FALLBACK=0 tools/perf/game_perf.sh <id> <disc> <zone> 60 <out>`.
2. **Linux**: `GAME=<id> tools/build_linux.sh <disc> --arch amd64` (`build-linux-amd64-<id>`, x86-64-v2, ~25 min emulated at -j4),
   then `GAME=<id> SKY_PORTAL_PLACE=<figures> SKY_PRESS=<presses> tools/linux/smoke_test.sh <disc> <out> 600 --arch amd64`
   (lavapipe; `SKY_DUMP_FRAME` seconds names the frame whose resolves are saved).
3. **D3D12**: `GAME=<id> tools/build_windows.sh <disc>` (`build-win64-<id>`, ~15 min at -j4), then
   `GAME=<id> SKY_PORTAL_PLACE=... SKY_PRESS=... tools/windows/d3d12_test.sh <disc> <out> 600 [d3d12|vulkan]` (Wine 10 and
   vkd3d-proton on lavapipe, x86_64 container). On a real PC: copy `build-win64-<id>/dist/ArkeyanRuntime/`, then
   [windows.md](windows.md#testing-on-a-real-pc) (`SKY_GPU_BACKEND=d3d12`).

**SuperChargers and Imaginators (2026-11-05, M1 Pro, scale 2) [V]:**

| Step | SuperChargers | Imaginators |
|---|---|---|
| MoltenVK | lobby test PASS (new game to the Academy, then continue); shots match Metal's | hub test PASS (new game to `hub_2016`); shots match Metal's; the continue run was not run on MoltenVK (queue) |
| 60 held (`FALLBACK=0`) | Academy: Metal 59.6, MoltenVK 60.0 fps | `hub_2016`: Metal 60.0, MoltenVK 53.2 fps (GPU-bound: see [plume_backends.md](../../runtime/gpu/plume_backends.md#moltenvk-and-the-alchemy-games-v-2026-11-05)) |
| Linux (amd64 container, lavapipe) | built; `intro/intro`, 423 draws at 2.6 fps, no fault | built; title and licence screen, no fault |
| D3D12 (Wine, vkd3d-proton) | menus and the rift tunnel at 2.5 fps, 183 pipelines, none rejected, no fault | title and licence screen, no fault; the frame matches Linux's |

**Wine on Rosetta and the write watch**: with the write watch on, both games under Wine died within 15 s ("rosetta error:
invalid gdt selector index 5", D3D12 and Vulkan alike); with `SKY_WRITE_WATCH=0` the same run held 600 s [V]. The Linux build
under the same Rosetta, whose watch uses SIGSEGV, does not die: a Wine-on-Rosetta quirk of the fault path, so `d3d12_test.sh`
turns the watch off. A real PC runs with it on (not yet tried [?]).

## Gotchas

1. **x64 traps on integer divide by zero and INT_MIN / -1; ARM64 returns 0.** The generated `divw`/`divd` are patched
   to behave like ARM64 ([../../tools/toolchain/xenonrecomp.md](../../tools/toolchain/xenonrecomp.md)); a latent crash
   macOS hid.
2. **ARM64 needs real fences** for `lwsync`/`sync`/`eieio` (heap corruption otherwise).
3. **Windows cannot commit 4 GB up front** on small commit limits and on Xbox One: reserve, then commit 64 KB granules
   on first fault ([../../runtime/xbox360/memory.md](../../runtime/xbox360/memory.md)).
4. **Wine + MoltenVK says little about real drivers**; Wine's own vkd3d (1.14) cannot compile dxc DXIL: use the x86_64
   container with vkd3d-proton (`tools/windows/d3d12_test.sh`), which turns the write watch off (Rosetta kills Wine on its
   faults, [above](#proving-a-game-on-every-backend-how-to)).
5. **A shader cache built off the Mac has no Metal AIR** and breaks the macOS build: build it on the Mac; it then
   serves every platform.
6. **The Metal compiler is a separate download since Xcode 26**: `xcodebuild -downloadComponent MetalToolchain`.
7. **The `.app` has its own Metal cache** (cold start for players, warm for symlink runs): measure with an app copy
   with a new bundle id ([macos.md](macos.md)).
8. **Clang only**: GCC-based mingw cannot build `ppc/` (`__builtin_assume`, `__builtin_rotateleft*`).
9. `.iso` is not in makemsix's store list (patched); packages over 4 GB need zip64 ([xbox_uwp.md](xbox_uwp.md)).
10. **Vertex attribute types**: MoltenVK rejects some pipelines whose vertex attribute format type (float, uint)
    differs from the shader input's type. Vulkan leaves that undefined; lavapipe accepts it, a strict driver could
    drop those draws. Not investigated [?].

## Pages

| Page | Answers |
|---|---|
| [macos.md](macos.md) | How do I build, bundle, sign, profile and measure on macOS? |
| [windows.md](windows.md) | How does the Windows cross build work, which GPU backend runs, what is Windows-specific in the runtime (memory, HID portal), how is it tested? |
| [linux.md](linux.md) | How do I build on Linux (Docker or a desktop), what was tested, and what does the Linux player kit need? |
| [xbox_uwp.md](xbox_uwp.md) | How is the Xbox UWP host built and packaged, and what happened in the nine console runs? |
| [data_folders.md](data_folders.md) | Where does the `Skylanders` data root live on each OS, and how did 0.3.1 migrate the older folders? |
