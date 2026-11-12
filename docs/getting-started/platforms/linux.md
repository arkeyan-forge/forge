# Linux (Vulkan)

> **Purpose:** how the game is built on Linux (Docker or a desktop), what was tested, and what the Linux player kit needs · **Status:** current (2026-11-09) · **Tags:** none (host platform)

Code and scripts: `tools/build_linux.sh`, `tools/linux/` (`Dockerfile`, `smoke_test.sh`, `kit.Dockerfile`,
`player.Dockerfile`, `kit_test.sh`). The **player kit exists since 0.3.1** (`ssa-kit-linux-x64`); every run so far was
in a container with lavapipe (CPU Vulkan), never on a real desktop.

## Build (Docker; on a Mac through OrbStack or Docker Desktop)
```
tools/build_linux.sh "<disc>.iso" [--arch arm64|amd64] [--shader-dump <dir>]
```
`prepare_game.sh` runs on the host (**build the shader cache on the Mac**: a Linux-built cache has no AIR). The image
`ssarecomp-linux-<arch>` (`tools/linux/Dockerfile`: Ubuntu 24.04, clang 18, CMake, Ninja, SDL3's
X11/Wayland/ALSA/PulseAudio/PipeWire/udev/dbus headers, libusb, zlib, the Vulkan loader, Mesa's lavapipe, Xvfb; ~1.4 GB)
builds `build-linux-<arch>` (RelWithDebInfo, plume Vulkan) in a container that mounts the checkout at `/src`; nothing
is copied into the image and the container runs as the caller's user. FFmpeg goes to `private/ffmpeg/linux-<machine>`
(`tools/ffmpeg/build.sh host`, with `--cc=$CC` since the image has no gcc; its work directory is per OS and machine).
zlib is found, so levels inflate with the host zlib. Result: `"build-linux-<arch>/ArkeyanRuntime/Skylanders Spyro's Adventure"` with `fonts/`.

arm64 is native on an Apple silicon Mac: first build ~15 min at -j3; `--arch amd64` (emulated) ~20 min at -j4. The
prebuilt dxc (XenosRecomp's dxc-bin, x86_64 only) compiles the present shaders: on arm64 the image adds the amd64 C/C++
runtime so Rosetta (or qemu-user binfmt) runs it.

On a Linux desktop without Docker: install the Dockerfile's packages, then `tools/prepare_game.sh <iso>`,
`tools/ffmpeg/build.sh host <prefix>`, `cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
-DARKEYAN_FFMPEG_ROOT=<prefix>`, `ninja -C build-linux ArkeyanRuntime` (not tried outside the container).

## Tested (container, lavapipe)
`tools/linux/smoke_test.sh <iso> <out> [seconds]`: SDL's offscreen video driver (which gives plume a Vulkan swapchain
over lavapipe; Xvfb not needed), disk audio to /dev/null, the fast recipe, one frame's resolves dumped at
`SKY_DUMP_FRAME` seconds.
- arm64, 120 s: `[gpu] Vulkan on llvmpipe (LLVM 20.1.2, 128 bits), swapchain output`, 1118 shaders, no fault,
  level_027 loaded; 17 fps on the title, **2.6 fps in the level** (~4000 draws, CPU renderer). The frame at 30 s is the
  level's fly-in cutscene, correctly drawn.
- x86_64 (emulated by Rosetta): no fault, no `[guard]` line, the same frames, 2.4 fps; this also runs the guarded
  divides on x64.
- Title (frame at 15 s) against macOS with the same settings: the same 69 resolves (order, sizes), the 1120x704 targets
  byte-identical, the final 1280x720 image 0.96/255 mean difference (1.6 % of pixels > 8: the clouds move).

## The player kit (since Arkeyan Launcher 0.3.1)
`tools/make_kit.sh linux` builds `dist/skylanders-kit-linux-x64` (71 MB, zip 30 MB) in Docker (`tools/linux/kit.Dockerfile`)
with zig 0.17.0 for glibc 2.34; why zig: [../../tools/arkeyan-builder.md](../../tools/arkeyan-builder.md#linux-the-compiler). Tested on
2026-10-25 in amd64 containers under Rosetta, the kit mounted, the disc read only:
- `tools/linux/kit_test.sh` on plain `ubuntu:22.04` (`tools/linux/player.Dockerfile`: curl, xz, Mesa; no compiler):
  Arkeyan-Builder from the disc to `@done` (zig downloaded and checked, 1115 of 1119 shaders as SPIR-V, 124 files compiled,
  3 movie shaders, the second pass). The game's `NEEDED`: `libc.so.6`, `libm.so.6`, `ld-linux-x86-64.so.2`; highest
  symbol version GLIBC_2.34.
- That game, run offscreen with the recipe: Ubuntu 22.04's Mesa 23.2 lacks `VK_EXT_headless_surface`, which SDL's
  offscreen driver needs (a desktop uses X11 or Wayland surfaces), so it ran on `ubuntu:24.04` (Mesa 25.2 lavapipe):
  1118 shaders, `level_027` loaded, 3977 draws a frame at 2.1 fps, no fault.
- **Kit 0.5.0, both games (2026-10-26)** [Giants]: `tools/linux/kit_test.sh` on `ubuntu:24.04` (amd64 under Rosetta), the offscreen
  recipe: SSA `level_027` loaded, Giants built from its disc to `@done` and `level_213_tutotial.arc` loaded (124 draws a frame at ~8 fps on
  lavapipe), no fault. Giants first faulted at start on x86 like Windows (the XenonRecomp jump-table index,
  [../../tools/toolchain/xenonrecomp.md](../../tools/toolchain/xenonrecomp.md)); fixed.
- **Swap Force** (2026-10-27) [SwapForce]: `KIT_GAMES="ssa giants swapforce" tools/make_kit.sh linux` (the variable is passed into the
  container now), then `SKY_PORTAL_PLACE=new:9 tools/linux/kit_test.sh ssa-player-ubuntu24 ...` with an A every 3 s: built from its disc to
  `@done` and ran to `tutorial_a.pak` and `eruptor.pak` at ~2 fps on lavapipe, no fault
  ([../../../games/swapforce/docs/status.md](../../../games/swapforce/docs/history.md), "Installing Swap Force").
- **SuperChargers and Imaginators** (2026-11-05) [V]: `GAME=<id> tools/build_linux.sh <disc> --arch amd64` (x86-64-v2, in
  `ssarecomp-linux-amd64`), then `GAME=<id> tools/linux/smoke_test.sh`: SuperChargers to its intro (423 draws at 2.6 fps),
  Imaginators to its licence screen, no fault, the write watch on (SIGSEGV under Rosetta holds)
  ([README.md](README.md#proving-a-game-on-every-backend-how-to)).
- `archlinux:latest`, same image recipe: Arkeyan-Builder from scratch to `@done`.
- Arkeyan Launcher 0.3.1 (built in `ssarecomp-linux-amd64`): a fake release over `file://` with this kit, INSTALL to PLAY
  offscreen.

## Notes
- **CPU baseline: x86-64-v2** (SSSE3, SSE4.1/4.2, POPCNT; every x86-64 CPU since about 2009), 2026-11-04 [V]. zig's clang
  given a `-target` builds for the baseline x86-64 (SSE2 only), and so did the kit until then: simde emulated every
  SSSE3/SSE4.1 operation of the translated VMX code in scalar loops (each vector load and store's byte swap is a
  `pshufb`: one SSA object, `ppc_recomp.5.cpp`, has 0 `pshufb` at the baseline, 1243 at v2). `tools/make_kit.sh` passes
  `-mcpu=x86_64_v2` (zig's spelling: `-march=x86-64-v2` reads as the CPU "x86"), which the kit's `compile.args` carry to
  the player's game code; `tools/build_linux.sh` passes `-march=x86-64-v2` on amd64. A lvx / vmaddfp / stvx loop of the
  translated shape, x86-64 under Rosetta: 424 s at the baseline, 0.43 s at v2 (Rosetta exaggerates the scalar fallback;
  the direction holds on real CPUs). Windows is already `btver2` (AVX, F16C, MOVBE), macOS arm64 is `apple-m1`
  (the compiler's default). Not yet built into a kit and run on Linux (Docker only checked that zig accepts the flag).
- Frame snapshots: `SKY_GPU_SNAPSHOT` needs an offscreen output whose image can be read back; on Linux SDL's offscreen
  driver gives a swapchain, so use `SKY_DUMP_FRAME` (as `smoke_test.sh` does).
- smol-v: the cache's SPIR-V is smol-v-encoded; `gpu/draw.cpp` decodes it before `createShader` (lavapipe crashed in
  the first game pipeline until then: the Vulkan path for game draws had never run before).
- Bink movies in the GPU path need the run-time shader dump (`--shader-dump`); the container tests used
  `SKY_SKIP_MOVIES=1`. **Giants' intro movies on Linux** (2026-11-09, issue #20: black with sound on a GT 1030, Wayland):
  the kit's Giants in `ssa-player-ubuntu24` (lavapipe) has its 50 Bink shaders, draws the movie (vertex
  `713ECCBD13012EB2`, pixel `5E30C1E437323CA5`, three `k_8_8_8_8_AS_16_16_16_16` planes) and presents it lit
  (`SKY_PRESENT_PROBE=30`: 9 to 46 % of the pixels lit in the logo, 100 % in the second) windowed, fullscreen and at
  resolution scale 2 [V]; MoltenVK with the validation layer and synchronization validation: no error, no hazard during
  both movies [V]. Not reproduced, so not fixed [?]. Each movie now logs `[movie] 3 s in: the presented frame ... has N %
  of its pixels lit`: on the player's machine, 0 % means the game's drawing is black on that GPU (then a capture of the
  movie draw on NVIDIA is next), a lit frame means the window's present is (then `SDL_VIDEO_DRIVER=x11` runs it through
  XWayland to tell Wayland from the driver).
- Data root: `$XDG_DATA_HOME/Skylanders` (default `~/.local/share/Skylanders`), [data_folders.md](data_folders.md).

## Not tried
A real Linux desktop (an X11/Wayland window, a hardware Vulkan driver, audio devices, gamepads, the udev rule with a
real portal), a real x86_64 machine (only Rosetta), the Steam Deck, the AppImage itself (the plain launcher binary
was run).
