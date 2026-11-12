# Steam Deck Game Mode startup (issue #22)

> **Purpose:** why the game may show nothing in the Steam Deck's Game Mode, what the 0.9.6 runtime logs and retries · **Status:** current (2026-11-11) · **Tags:** [?]

The supplied report establishes that the guest boots without usable rendering. It does **not** record SDL's driver,
initialization result, window result or Vulkan surface error. A video initialization/window failure specific to the
Game Mode environment is the leading hypothesis, not a proven root cause. A present-pipeline failure remains possible.

## Evidence and startup paths

References below are to `private/issue22/report.txt`; no game content is reproduced here.

- Lines 153–172 begin a runtime session and reach audio initialization; lines 180–190 reach portal initialization.
  Lines 340–341 start the skipped-shader messages. The previous session (541 onward) and launcher capture (932 onward)
  have the same pattern. None of those runtime sections contains a `[gpu]` line. Audio reporting zero device channels
  at 172, 560 and 953 supports checking SDL initialization, but is not proof of a video failure.
- Lines 1376, 5252 and 5506 report successful shader translation during installation. Missing translations are therefore
  a weaker explanation than a missing GPU context: `gpu::FindXenosShader` in `runtime/gpu/draw.cpp` returns null before
  consulting the cache when `g_context` is null. The skipped-shader wording does not distinguish these cases.
- Before this change, `runtime/main.cpp` ignored the result of combined video/gamepad/audio `SDL_Init`.
  `ResolveGameSource` (`runtime/host/launcher.cpp`) also initialized video without checking it, so the runtime could inherit
  an already selected SDL driver from its built-in source picker. Failure to initialize video, or having no usable
  displays, can make both window creation calls fail. `CreateDisplayWindow` then short-circuits
  `window != nullptr && gpu::Init(window)` without any explanation. This best fits the absence of all GPU messages.
- An inherited `SDL_VIDEO_DRIVER`, or SDL automatically selecting `offscreen`/`dummy`, can prevent a native Vulkan
  window. The old second window attempt removed graphics flags on **every** driver. If that succeeds, Linux
  `GetRenderWindow` rejects the window without `SDL_WINDOW_VULKAN` and silently chooses offscreen rendering.
  `dummy` may also fail both window attempts. Offscreen rendering that initializes successfully still emits GPU adapter
  messages, so that outcome alone does not fit this report. Zero displays had no explicit runtime check: it would
  affect SDL's window creation rather than deliberately skip `gpu::Init`.
- In `Init` (`runtime/gpu/gpu.cpp`), interface/device creation failure already logs `no Vulkan device`; that does not fit
  the complete absence of GPU lines. `CreatePipelines` failure reset the context and returned false silently, however,
  so it is another possible explanation. A null command queue was unchecked rather than a supported silent exit.
- `GetRenderWindow` can also decline a missing Cocoa window, Metal flag/view, CoreWindow or HWND; an explicit null
  window requests offscreen output. These paths previously gave no immediate reason, but successful initialization
  still logged offscreen output. The native handle branches are not Steam Deck paths.
- With `SDL_WINDOW_VULKAN`, plume creates the surface through SDL. If creation fails, the existing code marks
  `swapChainNoSurface`, skips its images and uses swapchain recovery. Initialization still logs the adapter and
  swapchain output. A gamescope surface failure can explain a blank screen, but alone cannot explain **no** GPU lines.
  Acquire/present failures after startup likewise belong to the already logged recovery path, not this earlier gap.
- `SKY_HEADLESS` deliberately bypasses the entire window/GPU branch. The audio initialization visible in this report
  is inside the non-headless branch, so this is not a good fit either.

`private/launcher/src/launcher/game_run.cpp` starts the child with `CreateGameEnvironment(launch.variables)`.
Previously `CreateGameEnvironment` copied all inherited variables, including SDL video overrides. The checked-in Linux
AppImage workflow uses linuxdeploy; it does not force a video driver for normal launches. Its `offscreen` test setting
is scoped to the test command. No generated AppRun or captured player environment is available to establish who, if
anyone, set a driver override. Neither gamescope nor the AppImage is proven to be the source of the failure.

## Targeted changes

- On Linux, the launcher's child environment drops inherited `offscreen`/`dummy` values of `SDL_VIDEO_DRIVER` and
  legacy `SDL_VIDEODRIVER` before applying explicit child overrides. Native driver preferences, compositor connections,
  Vulkan configuration and all other inherited settings remain available. The launcher's own environment is unchanged.
  Windows and macOS keep their old policy.
- The runtime initializes video independently of audio/gamepads. It logs the requested and selected SDL driver,
  display count, initialization error and both window errors, with graphics flags. Existing source-picker video
  initialization is detected rather than counted again.
- The normal working window is kept. Only failed Linux startup retries `wayland`, then `x11`, skipping an explicitly
  requested driver that already failed. Each retry releases video references left by the source picker and overrides
  the SDL hint, so a higher-priority inherited environment setting cannot defeat it. This happens before game threads
  or GPU resources exist. Audio and gamepad subsystems are kept.
- Linux native windows retain `SDL_WINDOW_VULKAN`. Automatic `offscreen`/`dummy` or zero-display selections trigger
  native retries. Explicit `offscreen`/`dummy` requests retain the existing test behavior and never open a native
  window after failure. Other platforms keep their external-graphics window fallback and do not try Linux drivers.
- GPU startup now names missing command queues, failed present shaders/pipelines, and a missing native render surface.
  Vulkan surface failure logs SDL's error immediately, before recovery. A missing GPU gets an explicit final message;
  deliberately headless startup also says why no GPU was initialized. No shader-cache or renderer translation changes.

This is a proposed fix for the most likely startup failure. It does not demonstrate that Game Mode is repaired on the
player's hardware; the new log is needed if native video succeeds but GPU pipelines or the Vulkan surface fail.

## Verification and player follow-up

`ArkeyanVideoDriverTest` uses fake startup attempts and no SDL: 17 checks cover the normal desktop, retry ordering,
total failure, explicit driver selection, zero displays, automatic versus explicit offscreen/dummy, and other platforms.
It is registered in `runtime/CMakeLists.txt` and `tools/test_all.sh`. The launcher environment checks cover child-only
filtering, compositor variables, preserved native drivers, explicit child overrides and unchanged non-Linux inheritance.

The standalone video test passed, as did `ArkeyanSwapChainRecoveryTest` (30 checks) and `ArkeyanGpuBackendTest` (20),
compiled with C++20, `-Wall -Wextra -Werror -pedantic`. Full runtime/launcher compilation and launcher environment tests
could not be run: the SDL and other dependency checkout folders are empty. No dependencies were fetched, no game or
launcher was run, and no on-device, Windows or macOS graphical validation was performed.

Ask the player to update both launcher and runtime, then try the same installed game in Steam Game Mode, first with
the existing fullscreen setting, then windowed if necessary. Repeat in Desktop Mode to check for regressions. Ask for
a fresh problem report containing `[display]` and `[gpu]` startup lines, including every retry/error and whether the
window and movies became visible. If it still fails, those lines distinguish SDL video/window setup, GPU pipeline
creation and Vulkan surface recovery without requesting any disc, shader, figure or memory data.

## 0.9.6 test build 2: Game Mode, no video driver (2026-11-12) [?]
A tester's log (Steam Deck, Game Mode, the launcher added to Steam): every SDL video start failed (auto, then Wayland,
then X11) while the sound played, and Steam's environment was inherited (its overlay in `LD_PRELOAD`). The likely cause:
Steam starts non-Steam games with its runtime libraries in `LD_LIBRARY_PATH`, and SDL's display drivers loaded those.
The launcher now starts the game with the system's library path (`GameLibraryPath`, launcher docs/install.md). The log
also hid SDL's own error (reading the driver's name first replaced it with "Video subsystem has not been initialized"):
the error is now read first, and a failed start logs `DISPLAY`, `WAYLAND_DISPLAY`, `XDG_RUNTIME_DIR` (set or not), the
SDL drivers built in and whether `LD_LIBRARY_PATH` holds Steam's libraries, never a path.

