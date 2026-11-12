# Windows (x64; llvm-mingw cross build; D3D12 or Vulkan)

> **Purpose:** how the Windows build is made, which GPU backend runs, what is Windows-specific in the runtime, and how it is tested without and with a PC · **Status:** current (2026-11-10) · **Tags:** none (host platform); the tester's PC observations are [SSA]

Code: `runtime/platform/platform_win32.cpp`, `runtime/host/portal_hid.cpp`, `runtime/host/portal_async.h`,
`runtime/gpu/gpu.cpp`. Status: works on a real PC (Radeon 840M iGPU) through Arkeyan Launcher.

## Build (on the Mac) and the player path
```
tools/build_windows.sh "<disc>.iso" [--shader-dump <dir>]
```
One command from a checkout (submodules included) and the disc: `prepare_game.sh`; llvm-mingw **20260922 ucrt**
(`LLVM_MINGW_ROOT`, else `private/toolchains/llvm-mingw-20260922-ucrt-macos-universal`, downloaded and SHA-256-checked:
`52e5f5a7b131021d0c39a37a38fa380a1da7885cd04bd61afd0cd4ecfb8bc1f3`); the minimal LGPL FFmpeg
(`private/ffmpeg/windows-x64`); then `build-win64` (Release `-O2 -march=btver2`). `JOBS` (default 4) sets the
parallelism. The first build compiles `ppc/` for x64: **10-30 min**; a rerun with nothing changed takes ~1 min and
recompiles nothing. On Linux, `LLVM_MINGW_ROOT` set to an extracted llvm-mingw *ucrt* release for Linux should work
(not tried).

Output `build-win64/dist/ArkeyanRuntime/`: `"Skylanders Spyro's Adventure.exe"` (55 MB, self-contained: libc++, libunwind, winpthreads, SDL3,
FFmpeg linked in), `fonts/`, `THIRD_PARTY_NOTICES.md`. Imports: KERNEL32, USER32, GDI32, SHELL32, ADVAPI32, ole32,
OLEAUT32, IMM32, VERSION, SETUPAPI, WINMM, bcrypt, d3d12, dxgi and the UCRT `api-ms-win-crt-*` sets (Windows 10/11).
`vulkan-1.dll` is loaded at run time (volk), from the GPU driver.

**Player path**: Arkeyan Launcher INSTALL downloads `skylanders-kit-windows-x64.zip`; Arkeyan-Builder downloads llvm-mingw (190 MB,
~750 MB extracted) into `<data root>/toolchains` itself (WinHTTP, then its own unzip; since the 0.9.2 kit no `curl.exe` or `tar.exe` is started); ~5 GB free, 8 GB RAM, **15-45 min**
the first time ([../../release/launcher/install_flow.md](../../release/launcher/install_flow.md)).

`"Skylanders Spyro's Adventure.exe"` stays a **console program**, on purpose. Arkeyan Launcher (0.2.0 on) starts it as an SDL background process
(`CREATE_NO_WINDOW`) with its output on a pipe: no console window, and `skylauncher-game.log` gets it; only a direct
start (Explorer, a shortcut) opens a console. The GUI subsystem would not help the launcher and would break the
terminal: cmd and PowerShell do not wait for a GUI program, so `--portal-probe` or `"Skylanders Spyro's Adventure.exe" <disc>.iso` would
print after the prompt or nowhere (that is how a tester reports a portal); a GUI build would also start with no
standard handles, a path of `LogInit` (`dup2` onto an unopened stderr) that Wine cannot check. The log file exists
either way: `<data root>\SSA\Arkeyan.log` ([../settings.md](../settings.md#the-game-log)).

## GPU backend
Both backends render the game (since the D3D12 work, [plume_backends.md](../../runtime/gpu/plume_backends.md)).
**The player chooses, per game** (0.9.5, the owner's decision of 2026-11-10): `[<game>.video] gpu_backend=d3d12|vulkan`,
**Direct3D 12 by default** (before 0.9.5 Vulkan came first). The launcher's Video tab has the row (Display section,
Windows only), the F3 menu too (next start). `gpu.cpp` starts the chosen one, and the other when it makes no device or,
for D3D12, when the shader cache has no DXIL (`gpu/gpu_backend.h`, `ArkeyanGpuBackendTest`); `SKY_GPU_BACKEND=vulkan|d3d12`
forces one with no fallback. The log's first GPU lines: `[gpu] backend D3D12 first (the setting)`, then either
`[gpu] D3D12 on <GPU>, swapchain output` or `[gpu] D3D12 did not start (<why>): trying Vulkan`. Since 0.9.6, D3D12 as
the setting's choice that brings no frame to the screen (failed presents, a removed device, a GPU that never finishes)
switches the game to Vulkan for good and restarts it once ([plume_backends.md](../../runtime/gpu/plume_backends.md#when-d3d12-shows-nothing-vulkan-instead--2026-11-11-issue-27)); the debug screen
(Shift+F3) names the one that runs. `SKY_GPU_TEST_NO_DEVICE=d3d12|vulkan` makes that backend report no device, to
prove the fallback. The shader cache holds signed DXIL besides Metal AIR and SPIR-V: one container per combination of a shader's
specialization constants (D3D12 has none), the constants as root CBVs and the pulled vertices as a root SRV, point
sprites through a geometry shader ([../../runtime/gpu/shaders.md](../../runtime/gpu/shaders.md)). The present shaders
are embedded as both DXIL (signed by `embed_dxil.cmake`) and SPIR-V; no SPIR-V is handed to D3D12 (`gpu/draw.cpp`). A
cache built before D3D12 rendered the game has no DXIL: `prepare_game.sh` rebuilds it. In the log: `[gpu] Vulkan on
<GPU>, swapchain output`, `[gpu] shader cache: 1118 shaders`, no `CreateGraphicsPipelineState failed`.

## Windows specifics in the runtime
- **Memory commit**: `VirtualAlloc(MEM_RESERVE|MEM_COMMIT)` of 4 GB; if it fails, reserve only and commit 64 KB
  granules on first access from a vectored handler ([../../runtime/xbox360/memory.md](../../runtime/xbox360/memory.md)).
- **Faults** go through an unhandled-exception filter: module+offset frames and the executable's load address (frames
  shown at the linker's base 0x140000000 for `llvm-symbolizer`/`addr2line`); `timeBeginPeriod(1)` at start.
- **No zlib**: CMake finds none in the cross build, so levels inflate with the game's own translated zlib (slower
  loads). A mingw zlib, or miniz (already in the tree for the launcher), would fix it.
- **HID portal** (`host/portal_hid.cpp`): SDL's hidapi enumerates game controllers only by default
  (`SDL_HINT_HIDAPI_ENUMERATE_ONLY_CONTROLLERS` drops every device whose usage page is known and not joystick/gamepad);
  the portal's usage page is vendor-defined `FF00`, so the portal was never listed until the hint was lifted for the
  portal's own enumeration (0.1.0's fix). Then the **write** problem (Arkeyan Launcher #11): the portal has an interrupt OUT
  endpoint but takes its commands as **output reports over the control pipe (SET_REPORT)**; `WriteFile` (hidapi's
  write) fails with `ERROR_GEN_FAILURE` (0x1F) or times out (`WaitForSingleObject` 0x3E5, hidapi's ~1 s write
  timeout). Fix: on Windows the commands go through **`HidD_SetOutputReport`** (hid.dll loaded at run time, our own
  handle, report id 0 + the command zero-padded to the descriptor's output report length, 1 + 32 bytes), with
  `SDL_hid_write` as the fallback, **whichever answers first, remembered** (`[portal] hid: wrote 'R' with
  output-report ...`). `--portal-probe` tries each method with `R` and `S` and prints a block on stderr; its JSON has
  `write` and `methods` keys ([../../runtime/host/portal_probe.md](../../runtime/host/portal_probe.md)).
- **Portal I/O off the game's threads** (Arkeyan Launcher #12): the game's portal thread was blocked ~1 s per failed write,
  a game frame waited on it, then enumerate + reopen every ~2 s: levels at 18-20 fps, against **46.7 fps** on the same
  PC without the portal. `AsyncPortal` (`host/portal_async.h`) queues commands and reports so the game side never
  touches the device; the device thread reopens after a failure, waiting 2, 4, 8, then 10 s at most (31 -> 60 fps on
  the Mac with `SKY_PORTAL=stall`, frame gaps 1000 -> 30 ms; `ArkeyanPortalAsyncTest`).
- `resolution_scale=auto` gives 1 on integrated GPUs; the x2 seen on the tester's iGPU came from its `launcher.ini`
  ([../settings.md](../settings.md#what-each-setting-does)).

## The tester's PC (observed 2026-10-24/25) [SSA]
Ryzen AI 7 445, Radeon 840M iGPU (4 CUs, unified memory), 16 GB, 2880x1800 at 150-200 %, Windows 11; Vulkan on the
840M (driver 2.0.302). End to end works with Arkeyan Launcher's INSTALL; levels at 46.7 fps without the portal; the
pipeline list took 20.6 s once after a Windows/driver update (45 ms before); a few 90 ms draw-time pipeline builds.
The #11/#12 fixes shipped in 0.3.1; the retest and its `--portal-probe` output are pending.

## Testing without a PC
- **Wine** (`private/wine/wswine.bundle`, a Wine 10 build, not installed system-wide):
  `tools/windows/wine_smoke_test.sh <iso> <out> [seconds] [--gpu]`.
  - Headless (`SKY_HEADLESS=1`, Wine's display driver off), the fast recipe, 90 s: no fault, no `[guard]` line,
    reaches `levels\level_027\mission.str`; it opens the same 96 distinct files as the macOS build and prints the same
    18 distinct `[stub]` lines. Saves go to `SKY_SAVE_DIR`.
  - `--gpu` (SDL offscreen, Wine's macOS driver loaded so Vulkan has surface extensions; no window): Vulkan device
    "Apple M1 Pro" through winevulkan and MoltenVK, the shader cache loads, title frames at 25-30 fps for ~8 s, then
    winevulkan aborts on a failed `vkQueueSubmit`; MoltenVK also rejects a few game pipelines (vertex attribute type
    mismatch, [README.md](README.md#gotchas)). This stack is not what a Windows PC runs.
  - Without Wine's display driver, Vulkan has no `VK_KHR_win32_surface` and D3D12 no DXGI factory: both fail, the
    runtime says so and the game keeps running without GPU.
- **The player kit, both games (2026-10-26)** [Giants]: the Windows kit 0.5.0 driven from the Mac as a cross build
  (`Arkeyan-Builder --kit dist/skylanders-kit-windows-x64 --toolchain <llvm-mingw 20260922>` from the macOS kit, over the `arkeyan-work/{disc,ppc,shaders}`
  of a native run: [../../tools/arkeyan-builder.md](../../tools/arkeyan-builder.md)) compiles and links `"Skylanders Spyro's Adventure.exe"` (SSA, 87 s) and `"Skylanders Giants.exe"`
  (Giants, 94 files, 131 s, 48 MB). SSA under Wine, headless: `level_027`, no fault (2026-10-27, with the miniz inflate: the level's `mission.str` loads in 0.45 s instead of 0.75 s with the game's translated zlib: [../../engines/lux/zlib.md](../../engines/lux/zlib.md)). Giants under Wine, headless
  (`tools/regress/giants_press.py` as `SKY_PRESS`): **it faulted at start** (a jump-table load 4 GB off in
  `sub_821C7A70`); the cause was XenonRecomp's 64-bit `switch` on a table index ([../../tools/toolchain/xenonrecomp.md](../../tools/toolchain/xenonrecomp.md)),
  fixed; now it loads `level_213_tutotial.arc`, no fault. Not tested: Giants' GPU path on Windows (Wine's Vulkan is
  MoltenVK: see above), Bink 2 movies (the cross build skips the movie pass), and any of this on a real PC: **that is the user's test**.
- **Swap Force** (2026-10-27) [SwapForce]: the same kit and cross build reach the tutorial under Wine, headless
  ([../../../games/swapforce/docs/status.md](../../../games/swapforce/docs/history.md), "Installing Swap Force"). It first faulted at start
  (`0xC000001D`): `-march=btver2` includes SSE4a and the compiler used `extrq` in the translated vector code, an illegal instruction on
  Rosetta and on any Intel CPU; the toolchain file now adds `-mno-sse4a` (the Windows kit must be rebuilt from clean after such a change:
  cmake keeps `CMAKE_*_FLAGS_INIT` in its cache).
- **D3D12**: Wine's vkd3d 1.14 compiles no dxc DXIL and vkd3d-proton refuses MoltenVK, so
  `tools/windows/d3d12_test.sh <iso> <out> [seconds] [d3d12|vulkan]` runs the build under Wine 10 in an x86_64 Debian
  container (Rosetta) with **vkd3d-proton 3.0.1 on lavapipe**: title render targets byte-identical to Vulkan on the
  same driver, level_027 at ~2.5 fps (~4000 draws), no pipeline rejected. `VALIDATE=1` turns the Vulkan validation
  layer on; `SAVE_FROM=<dir>` reuses saves; `EXE=` runs another build (a player kit's). Rosetta sometimes kills Wine
  with "invalid gdt selector index 5": emulation, not the game. `tools/shaders/check_dxil.sh` validates every DXIL
  container of the cache with `dxil.dll` (and its hash).
- **The backend choice** (0.9.5, 2026-11-10) [V]: the runtime built for Windows without game code (the kit's way: stub
  `ppc` headers, an empty shader cache) and `ArkeyanRecompGpuTest.exe` run in the same container. Not set: `[gpu] backend
  D3D12 first (the setting)`, `D3D12 on` vkd3d-proton: the present pass (pillarbox, letterbox, odd sizes), two producer
  threads (300 presents), the readbacks and FSR 3's four cases match their references. `SKY_GPU_TEST_NO_DEVICE=d3d12`:
  `D3D12 did not start (SKY_GPU_TEST_NO_DEVICE): trying Vulkan`, then every case on Vulkan. `SKY_GPU_BACKEND=vulkan` with
  Vulkan refused: `no other`, no device. Frame generation is off on D3D12 (held back, `framegen.cpp`) and lavapipe fails
  its interpolation on Vulkan: the test's last case, as before.
- **SuperChargers and Imaginators** (2026-11-05) [V]: `GAME=<id> tools/build_windows.sh` builds `build-win64-<id>`;
  `GAME=<id> tools/windows/d3d12_test.sh` reaches SuperChargers' menus (the rift tunnel, 183 D3D12 pipelines, none rejected)
  and Imaginators' licence screen, no fault. Both died within 15 s with the write watch on (Rosetta's "invalid gdt
  selector"), so the script turns it off ([README.md](README.md#proving-a-game-on-every-backend-how-to)).

## Testing on a real PC
1. Copy `build-win64/dist/ArkeyanRuntime/` (another game: `build-win64-<id>/dist/ArkeyanRuntime/`, its executable named
   after the game, `games/<id>/game.txt` `runtime=`) and the disc image to the PC (Windows 10 or 11 x64, a GPU with a Vulkan 1.2
   driver), or install through Arkeyan Launcher.
2. Run `"Skylanders Spyro's Adventure.exe" <disc>.iso > log.txt 2>&1` from a terminal (or double-click: the launcher asks for the disc).
3. Look for `[gpu] D3D12 on <your GPU>, swapchain output` and `[gpu] shader cache: 1118 shaders`; then the title, a new
   game, level_027. `SKY_GPU_BACKEND=vulkan` runs the same on Vulkan.
4. Report any `Fault (exception ...)` block, `vkCreateGraphicsPipelines failed`, `[guard]` lines, and loading times
   against macOS.
5. **D3D12, 0.9.5** [?] (never run on a Windows driver before): in each game, the title, a level and its HUD, the F3
   menu and the Toy Box over the game, a story movie, a level change (the readbacks), FSR 3 on, then fullscreen on and off
   and Alt+Tab (the swap chain); the log has no `CreateGraphicsPipelineState failed`, no `device REMOVED`, no
   `swap chain: present failed`; the frame times against Vulkan (the launcher's Graphics API row, or the F3 menu).

## Not tried
D3D12 on a Windows driver (only vkd3d-proton on lavapipe), now the default (step 5 above); real portals after the #11 fix (pending the retest).

Shell paths containing spaces must be quoted. In cmd, use `"Skylanders Spyro's Adventure.exe" "<disc>.iso"`;
in PowerShell, use `& ".\Skylanders Spyro's Adventure.exe" "<disc>.iso"`.
