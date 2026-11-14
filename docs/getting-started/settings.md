# Runtime settings: the command line, launcher.ini, the data folder, saves and the log

> **Purpose:** which settings the game runtime reads, from where, which source wins, and where it keeps the player's data, saves and log · **Status:** current (2026-10-28) · **Tags:** none (host); game-specific values are tagged [SSA], [Giants] or [SwapForce]

Code: `runtime/host/settings.cpp` (parser), `runtime/main.cpp` (applies it), `runtime/host/uwp/settings_uwp.cpp`
(console); test: `ArkeyanSettingsTest`. Environment variables that are not settings (test aids, debug output):
[../porting/debugging.md](../porting/debugging.md).

**Precedence, highest first: the command line (`"Skylanders Spyro's Adventure" [options] [<disc image .iso>]`; Giants: `"Skylanders Giants"`), then the
`SKY_*` variables that mirror a setting (`SKY_LANGUAGE`, `SKY_VOICE_LANGUAGE`, `SKY_TOYBOX_AUTO_OPEN`,
`SKY_TEXTURE_DUMP`, `SKY_TEXTURE_PACKS`), then `launcher.ini` (or the file `SKY_SETTINGS` / `SKY_LAUNCHER_INI` names),
then the defaults.** (The older `SSA_*` names, such as `SSA_LANGUAGE`, still work; `SKY_*` wins when both are set.) `launcher.ini` is shared with Arkeyan Launcher; its game section is the game's id (`[ssa]`, `[giants]`).

At startup the effective configuration is logged on one line, for example:

```
[settings] scale auto, vsync 0, fullscreen, anisotropy 16, fps limit 30, intro movies played, input keyboard, gamepad first, keys default, glyphs auto, language en, voices game
```

A bad value (out of range, unknown key name) is reported as `[settings] <source>: <key>=<value> is not ...; ignored`
and the value from the lower source stays. An unknown option or a missing value stops the program (exit code 2).
Options take their value as the next argument or after `=` (`--scale 3`, `--scale=3`); repeated options: the last wins.

## Command-line options

| Option | Values | Default | `launcher.ini` key |
| --- | --- | --- | --- |
| `--scale N` | auto, 1 to 10 | auto | `[video] resolution_scale` |
| `--aspect A` | auto, 16:9 | auto | `[video] aspect` (the launcher's Settings > Graphics key): auto shows more to the sides on a window wider than 16:9 ([ultrawide.md](../runtime/gpu/ultrawide.md)); 16:9 keeps the game's picture, with bars |
| `--vsync N` | 0, 1 | 1 | `[video] vsync` |
| `--fullscreen`, `--windowed` | | windowed | `[video] fullscreen` (0, 1) |
| `--anisotropy N` | 1, 2, 4, 8, 16 | 1 | `[video] anisotropy` |
| `--fps-limit N` | 30, 60 (0.9.4: an older 120, 144, 240, 0 or `display` reads as 60, with a warning but for `display`); at 60: 30 while the frames do not fit in a 60 Hz blank, [video_vblank.md](../runtime/xbox360/video_vblank.md#pacing-waitforflippresentinterval-in-the-swap-hook)) | 30 | `[video] fps_limit` |
| (settings file only) | 0, 1 | 0 | `[<game id>.video] show_fps` (or `[video]`): the FPS counter, a small readout in a corner; the F3 menu turns it on and off ([gamemenu.md](../runtime/host/gamemenu.md)) |
| (settings file only) | 0, 1, 2 | 0 | `[<game id>.video] debug_screen` (or `[video]`): the debug screen off, compact or full; the F3 menu and Shift+F3 change it ([gamemenu.md](../runtime/host/gamemenu.md), "Debug screen") |
| (settings file only) | 0, 1 | 0 | `[video] reduce_motion` (or `[<game id>.video]`): the in-game messages and achievements fade in and out without sliding ([ingame_ui.md](../runtime/host/ingame_ui.md#motion)) |
| (settings file only) | 0 to 100 | 100 | `[<game id>.audio] volume` (or `[audio] volume`; the game's own section wins): the master volume; the F3 menu changes it |
| (settings file only) | 0, 1 | 1 | `[<game id>.audio] portal_sound` (or `[audio] portal_sound`): the traps' voices from the Traptanium speaker, on the real portal or mixed into the computer's audio at the game's volume; 0 silences both ([portal.md](../runtime/host/portal.md), "The speaker"). The F3 menu and Trap Team's launcher page change it |
| (settings file only) | auto, stereo, 5.1 | auto | `[<game id>.audio] speakers` (or `[audio] speakers`; env `SKY_SPEAKERS`): the audio output's layout; auto is 5.1 when the output device has 6 channels or more, else stereo. Applies at the next start ([audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md#51-the-speakers-setting)). The F3 menu changes it |
| (settings file only) | 0, 1 | 0 | `[video] skip_intro_movies` (skip the start-up logos, never the story movies) |
| `--texture-dump N` | 0, 1 | 0 | `[video] texture_dump` (or `SKY_TEXTURE_DUMP`): writes the game's textures as DDS files ([texture_packs.md](../runtime/gpu/texture_packs.md)) |
| `--texture-packs N` | 0, 1 | 1 | `[video] texture_packs` (or `SKY_TEXTURE_PACKS`): replaces textures with the files of the game's `textures/` folder (not `textures-hd/`) |
| (settings file only) | 0, 1 | 1 | `[<game id>] hd_textures` (or `SKY_HD_TEXTURES`): reads `textures-hd/`, the HD textures the launcher made from the disc; alone decides it ([texture_packs.md](../runtime/gpu/texture_packs.md)) |
| `--original-resolution N` | 0, 1 | 0 | `[<game id>.video] original_resolution` (Arkeyan Launcher's per-game video section; `[video] original_resolution` too, the game's section wins). [SwapForce] 0: the 3D at the frame's size (1280x720 before the resolution scale); 1: the console's 1040x620 (81 %) stretched to the frame. No effect in SSA and Giants, whose 3D already fills the frame |
| `--edge-smoothing M` | auto, game, light, off | auto | `[<game id>.video] edge_smoothing` (`[video]` too, the game's section wins) [SwapForce] [TrapTeam]: the game's FXAA pass. `game` leaves it as the game sets it, `light` filters only strong edges (contrast threshold 0.5, minimum 0.2), `off` draws a plain copy. **auto (not chosen): light from resolution scale 2 in a level, game at scale 1 and on the title and the game's menus** (`game::InMenuLevel`); a chosen value stays whatever the scale. The launcher and the F3 menu call the three values Game, Gentle and None. Only a game with a known FXAA pass (`HostKernelPixelShader`; SSA and Giants have none): the setting does nothing there and the F3 menu has no row for it ([edge smoothing](#edge-smoothing)) |
| (none) | off, fsr3 | off | `[<game id>.video] upscaler` (`[video]` too; or `SKY_UPSCALER`): AMD FSR 3.1 upscales the 3D scene to the window's scale; the scene's own scale comes from `fsr_quality` ([upscaling.md](../runtime/gpu/upscaling.md)). The F3 menu switches it at once, except in a game whose scene was full size at the start (Swap Force, Trap Team, SuperChargers, Imaginators with FSR off): there at the next start |
| (none) | native, quality, balanced, performance, ultra_performance | quality | `[<game id>.video] fsr_quality` (or `SKY_FSR_QUALITY`): `native` (AMD's Native AA) keeps the scene at the window's scale, FSR as anti-aliasing (frame generation at full size); the others: the scene's scale is the window's scale over FSR's ratio (1.5, 1.7, 2, 3), rounded to a whole scale: at 1440p every preset renders at x1, at 4K quality to performance at x2 |
| (settings file only) | d3d12, vulkan | d3d12 | `[<game id>.video] gpu_backend` (`[video]` too, the game's section wins) [Windows]: the graphics API, tried first; when it makes no device (or the shader cache has no DXIL, for D3D12) the game starts the other and logs why (`[gpu] D3D12 did not start (no device was created): trying Vulkan`). `SKY_GPU_BACKEND=d3d12\|vulkan` forces one, with no fallback. When D3D12 shows nothing at the start, the game writes `vulkan` here and `gpu_backend_note` (why, for the launcher) and restarts once (0.9.6). Ignored on macOS, Linux and Xbox. The launcher's Video tab (Windows only) and the F3 menu set it ([plume_backends.md](../runtime/gpu/plume_backends.md)) |
| `--input D` | auto, keyboard, gamepad | auto | `[input] device` |
| `--gamepad GUID` | SDL GUID (`--list-gamepads`) | the first one | `[input] gamepad` |
| `--key ACTION=KEY` | SDL scancode name: a key's **place** (`W` is where a US keyboard prints W, Z on AZERTY) | see below | `[input] key_<action>`, then `[<game id>.input] key_<action>` (Arkeyan Launcher's per-game keys; the game's section wins) |
| `--pad BUTTON=BUTTON` | a pad button, or `none`: `rb=b` makes RB do B's job | every button its own | `[<game id>.input] pad_<button>` (Arkeyan Launcher's Controller tab; `[input] pad_<button>` too, the game's section wins). Buttons: `a b x y lb rb lt rt start back ls rs dpad_up dpad_down dpad_left dpad_right` |
| `--rumble S` | off, low, normal, strong | normal | `[<game id>.input] rumble_p1` / `rumble_p2` (Player 1, Player 2; `[input]` too): the strength of the vibration the game asks for ([input.md](../runtime/xbox360/input.md), "Remapping and rumble") |
| `--nightmare-from-start N` | 0, 1 | 0 | `[swapforce] nightmare_from_start`, `[imaginators] nightmare_from_start` (or `SKY_NIGHTMARE_FROM_START`) [SwapForce] [Imaginators]: the Nightmare difficulty is offered before the story is beaten; the save is not changed ([history.md](../../games/swapforce/docs/history.md), "Nightmare difficulty"; [Imaginators' status](../../games/imaginators/docs/status.md), "Nightmare"). The other games ignore it with a line in the log: Spyro's Adventure and Giants have no Nightmare, SuperChargers offers it from the start, Trap Team's lock is not hooked yet |
| `--glyphs G` | auto, xbox, xbox360, xboxone, playstation, nintendo, keyboard | auto (Xbox: xboxone) | `[input] glyphs` |
| `--language C` | SSA: en fr de es it nl sv no da fi; Giants: en fr de es es-419 it nl pt sv no da fi; Swap Force: en fr (others show English); Trap Team: en fr de es es-419 it nl pt sv no da fi pl ru cs ja ko zh tr (the languages of the disc: Arkeyan Launcher lists those only) | the game's | `[<game id>] language` (or `SKY_LANGUAGE`); Giants: [boot.md](../../games/giants/docs/boot.md#text-language), Trap Team: [boot.md](../../games/trapteam/docs/boot.md#language) |
| `--voice-language C` | SSA: the same codes; Giants: en fr de es es-419 it nl pt sv no da fi; Swap Force: en fr; Trap Team: en de fr nl, only when speech in that language is on the mounted disc | SSA: English; Giants, Swap Force, Trap Team: the text's | `[<game id>] voice_language` (or `SKY_VOICE_LANGUAGE`); Trap Team detects speech in character archives, using `character/Init_Setup.bld` (then `permanent/global.bld`) for the regional text languages; absent speech logs a fallback to following text. [Trap Team checks](../../games/trapteam/docs/boot.md#lb-and-independent-voices), [Giants](../../games/giants/docs/boot.md#voice-language), [Swap Force](../../games/swapforce/docs/boot.md) |
| `--select-iso`, `--launcher` | | | open the runtime's own disc picker ([runtime_launcher.md](../runtime/host/runtime_launcher.md)) |
| (none) | 0, 1 | 1 | `[portal] toybox_auto_open` (or `SKY_TOYBOX_AUTO_OPEN`) |
| `--list-gamepads` | | | prints `GUID  name` per connected gamepad and quits |
| `--portal-probe` | | | prints one JSON line per real portal found and quits ([portal_probe.md](../runtime/host/portal_probe.md)) |
| `--version` | | | prints the version and build and quits |
| `--help`, `-h` | | | prints every option and quits |

Example:

```
"Skylanders Spyro's Adventure" game.iso --fullscreen --scale 3 --vsync 0 --anisotropy 16 --input keyboard --language en
```

## What each setting does

- **`resolution_scale`**: every host render target is N times its EDRAM surface (the 3D at 1120x704 becomes
  2240x1408 at x2), and so is every texture resolved from them. Viewports, scissors, resolve rectangles and
  destination points, and clear rectangles are scaled by N in `d3d/render.cpp` (`Scale()`); screen-space draws
  (viewport transform off) keep converting guest pixels. In the shaders (XenosRecomp patch, shared constant
  `g_ResolutionScale` at +300), point sprite sizes are multiplied by N and the pixel position (`vPos`, and the
  param_gen position) is divided by N, so screen-tile light passes and full-screen passes see guest pixels. The UI is
  drawn into the same targets, so it is sharp too. The presented image is scaled to the window as before. x1 renders
  exactly as before (every factor is 1). The shader cache must be rebuilt after the XenosRecomp change
  (`tools/shaders/build_cache.sh`).

  **`auto`** (the default) is chosen when the GPU device is created (`AutoScale()` in `gpu/gpu.cpp`), from the adapter
  the backend picked: **2** on Apple GPUs (M-series) and on discrete GPUs with 2 GB of video memory or more, **1**
  everywhere else: integrated GPUs (shared memory: D3D12's UMA, Vulkan's integrated type; Intel and AMD iGPUs, a
  Radeon 840M among them), discrete GPUs with less than 2 GB, virtual or software adapters. The rendering is
  fill-bound with the pixel count on integrated GPUs (a Radeon 840M lags at x3, a little at x2, is fine at x1). The
  log says what was chosen and why: `[gpu] resolution scale auto: 1 (integrated GPU (shared memory); ...)`. **Auto
  steps down when the GPU is the limit** (2026-11-03, `gpu/auto_scale.h`): after 30 s under 90 % of the limit with 40 %
  or more of each frame waiting for the GPU, the run writes the next scale down for its adapter to `auto_scale.txt` in
  the game's data folder (`[perf] resolution scale auto: the GPU limits the frame rate at scale 2; the next start
  renders at scale 1`), and the next start with auto uses it (`... (an earlier run found the GPU the limit ...)`):
  Trap Team's tutorial on the M1 Pro, 52.6 fps at 2 then 60.0 at 1. Another adapter, or the file deleted, starts from
  the device's choice again. A number
  in `launcher.ini` or `--scale` always wins: the first Windows PC's logs (Radeon 840M, 4 CUs, display at 200%) say
  `[settings] scale 2 (2240x1408)` and no `resolution scale auto` line, so that 2 was `resolution_scale=2` in its
  `launcher.ini`, not auto (auto ignores the display scale, and gives 1 there). At x2 that GPU was the limit: 46.7 fps
  in level_027 with the GPU thread 97% busy and 9 ms per frame in present (waiting for a swapchain image). The scale is fixed for the session: render targets and the textures
  resolved from them are created at it, so a change needs a restart (a live change would have to recreate every host
  render target and drop every resolved texture at a safe point between frames; not done). When the frame rate stays
  under the limit for 30 s while much of each frame waits for the GPU, at a scale above 1, the log says so once:
  `[perf] hint: the GPU limits the frame rate (...) at resolution scale 2; resolution_scale=1 ...`. The scale is never
  changed in play.
- **`vsync`**: the swapchain's vsync (`setVsyncEnabled`). 0 also stops pacing the offscreen output at 60 Hz. The
  game itself stays paced by the emulated vertical blanks at 30 or 60 (`fps_limit`, `kernel/video.cpp`).
- **`aspect`**: `auto` (default) or `16:9`. With `auto`, a window (or fullscreen display) more than 1 % wider than 16:9
  gets a Hor+ camera, a scene wide enough to fill it, and the HUD centred at its own 16:9 layout, in SSA and Giants
  ([ultrawide.md](../runtime/gpu/ultrawide.md)); movies keep 16:9 with black bars.
  The shape is read once, when the window is created: resizing the window later only changes how the picture is scaled.
  A 16:9 window, or `16:9`, behaves exactly as without the feature.
- **`fullscreen`**: the SDL window is created fullscreen on the desktop (no display mode change).
- **`original_resolution`** [SwapForce] [TrapTeam]: Swap Force and Trap Team draw their 3D scene at 1040x620 and stretch it to the 1280x720
  frame (the console's 10 MB of EDRAM). By default the runtime asks the game for a 1280x720 scene (with a wide window, the
  scene at the window's shape, 720 high), so nothing is stretched; `1` keeps the console's picture. Read once at start
  (`game::Adapter::fullScene`, `games/swapforce/ultrawide.cpp`; how: [boot.md](../../games/swapforce/docs/boot.md#the-scene-at-full-resolution-2026-10-27-alchemy-swapforce-v)).
  It costs about 43 % more scene pixels.
- **`edge_smoothing`** (<a id="edge-smoothing"></a>[SwapForce] [TrapTeam]): both games run FXAA over the picture, which also softens fine texture detail
  (grass, ground, cliffs) that a higher resolution scale would show. One frame each at scale 2 against Game (`tools/regress/analyze.py sharpness`): Trap Team Light +30 %, Off +34 %; Swap Force Light +24 %, Off +27 %
  ([games/trapteam/docs/status.md](../../games/trapteam/docs/status.md), "The constant blur"). SSA and Giants have no known FXAA pass: the same frames differ by 0 to 5 % (the scene moves), the setting does nothing. Off keeps the stair-stepping of silhouettes (little at scale 2, more at 1). Applies at once from the F3 menu (Trap Team's title, scale 1: Gentle +24 %, None +36 % of detail against Game, 2026-11-02). **The title and menus keep Game under auto** (2026-11-02): their art (Trap Team's logo) has hard cut-out edges that only the game's pass smooths; under Light at scale 2 the logo's outline was stair-stepped (the owner's "jagged logo"). The log says the mode in force at each change: `[gpu] edge smoothing game (setting auto)`. Swap Force's title does not change with it (+0.1 %, -0.5 %).
- **`anisotropy`**: `maxAnisotropy` of samplers whose min and mag filters are linear, for textures with mipmaps only
  (`SamplerFor`; render targets and colour tables keep 1: [textures_formats.md](../runtime/gpu/textures_formats.md)). The first such sampler is logged once: `[gpu] sampler <n>: anisotropy 16 (the first anisotropic one)`.
- **`fps_auto_fallback`**: `[<game>.video]`, `1` or `0`; unset (0.9.6): on for integrated and Apple GPUs, off for
  discrete ones, written to the section at the game's first start. Allows the shared automatic drop from the chosen
  60 FPS limit to 30 when the game cannot hold 60 (a quarter of the frames late for 5 s; then 30 for at least 30 s:
  [video_vblank.md](../runtime/xbox360/video_vblank.md)). `0` keeps the chosen limit even if frames stutter and suppresses
  the fallback toast. The launcher exposes it directly below Frame rate limit at 60; other limits keep its saved value.
  The runtime reads the game's section over the shared `[video]` fallback, so starting the game directly works too.
- **`fps_limit`**: 30 (the runtime's default, for a run with no launcher and for a game with no saved value; no game file carries another) or 60 (experimental). **Giants is capped at 60** (2026-10-30, `game::Adapter::maxFpsLimit`, set in `games/giants/adapter.cpp`): a higher `fps_limit`, or 0, is lowered to 60 with `[settings] fps_limit=<n>: this game is capped at 60 FPS, using 60`, in the F3 menu too; its summon cutscene lasts 10 % less at 120 ([video_vblank.md](../runtime/xbox360/video_vblank.md#above-60-measured)). The launcher's list stops at 60 for Giants and for Trap Team (`games/trapteam/adapter.cpp`: it too paces itself by the interval, `presentIntervalPacesGame`; [research.md](../../games/trapteam/docs/research.md), "Frame rate"). **The launcher's default per game** (2026-10-30, the owner's decision; `DefaultFrameRate` in Arkeyan Launcher's `game_settings.cpp`, the one place): 60 for Giants and Swap Force, 30 for SSA until its 60 at scale 2 is merged; the page shows it and PLAY writes it when the game has no saved value, which is never changed. At 60 the frames the title asks to show every 2 vertical blanks
  are shown at every blank. The game keeps its speed: it steps its logic by the frame time it measures (details and
  measurements in [frame_pacing.md](../engines/lux/frame_pacing.md)). Gameplay at a real 60 FPS is unverified (the level renders at 20-30 FPS
  on the test machine). [Giants] presents at every blank by itself, so 30 shows every second frame and 60 shows all (`game::Adapter::presentsAtEveryBlank`). [SwapForce] asks for interval 2 like SSA and, unlike SSA, paces itself by the interval it reads from its device (a fixed 1/30 s step): at 60 the runtime writes 1 there before each swap (`Adapter::presentIntervalPacesGame`), and the game times its frames and keeps its speed (52-56 fps in the tutorial and `brn2` on a loaded machine; proof in games/swapforce/docs/boot.md, "Frame rate"). The rates are 30, 60, 120, 144, 240 and 0 (no limit); above 60 the host paces the frames ([video_vblank.md](../runtime/xbox360/video_vblank.md), "Above 60"); `display` (the launcher's default) is 60 until 120 is proven; another number goes down to the rate below it, with the log line
  `fps_limit=<n>: the frame rate is 30 or 60, using <rate>` (0.9.4: no limit above 60 any more; the paragraph's measurements
  above 60 below are history). **A machine that cannot hold 60** gets a steady 30 FPS
  instead of an uneven 40-50 (the frames are shown every 2 blanks once 20 of the last 120 miss theirs, under ~50 fps for
  2 s; 60 is tried again after 10 s at 30, then 20, 40 and at most 60 s while the tries fail; the Toy Box's toast says "30 FPS: 60 not held" once per run;
  [video_vblank.md](../runtime/xbox360/video_vblank.md)). The launcher's "60 (experimental)" is this key. **Above 60 needs VSync off**
  (or a display that fast): with VSync on, the present waits for the display (offscreen: 60 Hz). Measured 2026-11-02 from the F3
  menu: SSA and Swap Force 60.0 fps at 120, 144, 240 and no limit with `vsync=1`; SSA 103-112 fps at 120 and no limit with `vsync=0` at scale 2.
- **Where the video keys come from** (all of `[video]`'s keys): `launcher.ini`'s `[video]`, then the game's own
  `[<game>.video]` (the launcher keeps one per game, which its game page edits; PLAY copies it into `[video]`), then the
  environment, then the command line. The own section wins for every key it has, so a game started without PLAY (its own
  icon, a shortcut to the runtime) still gets what its page shows. Check: `tools/regress/play_settings.sh`
  ([testing.md](testing.md)).
- **The F3 menu writes the settings** (2026-10-28): Volume, FPS counter, Debug screen, Fullscreen, Frame rate, the 30 FPS drop and Edge smoothing apply at once, Resolution
  scale, Texture filtering, VSync and Skip the start-up logos at the next start. Each is written to the game's own
  `[<game id>.video]` / `[<game id>.audio]` in the file `SKY_SETTINGS` / `SKY_LAUNCHER_INI` names (so the launcher shows it); a
  command-line option or `SKY_*` variable still wins for the run. [gamemenu.md](../runtime/host/gamemenu.md).
  **A launcher left open while the game runs keeps its own copy** [?]: Arkeyan Launcher reads `launcher.ini` when it starts and not
  again when the game ends (its `launcher_ui.cpp`, `App::PollGame`, read 2026-11-02), so its page shows the values from before
  the menu's changes and its next save (PLAY, any setting) writes them back over them. The fix is the launcher's: read the file
  again when the game ends.
- **`skip_intro_movies`**: 0 (default) or 1. Skips the start-up logos only (SSA: `legal`, `atvilogo`, `xpeclogo`, `tfb`,
  `Movie_Update` ends each after its first frame, as `SKY_SKIP_MOVIES` does; Giants: `atvinewlogo_640`,
  `tfb_logo_640x456`, made one frame long at `BinkOpen`; Trap Team: the same two, 2026-11-02, before which its switch did nothing) and logs `[movie] skipped <name>`. The
  story movies always play. The launcher's Settings > Graphics has the same switch.
- **`input`**: `auto` reads the keyboard and the gamepad, `keyboard` only the keyboard, `gamepad` only the gamepad.
  With `gamepad=<GUID>` that gamepad is used, and replaces another one when it is plugged in later; without it, the
  first gamepad connected. Keyboard keys are SDL key names (`SDL_GetScancodeFromName`: `Space`, `Return`, `A`,
  `Up`...). Escape is always back as well (the Toy Box menu uses it).

Keyboard actions and their defaults: `a` Space, `b` X, `x` C, `y` V, `start` Return, `back` Backspace, `lb` Q,
`rb` E, `lt` 1, `rt` 3, `left_up/down/left/right` W S A D (movement), `right_up/down/left/right` I K J L (camera),
`dpad_up/down/left/right` the arrow keys. (Before these settings: Space or Z for A and Escape for back, no camera keys
or triggers on the keyboard.)

- **The mouse** (off by default; [../runtime/xbox360/input.md](../runtime/xbox360/input.md), "Mouse"): `[<game id>.input] mouse=1` turns it
  on for that game (only the game's section: `[input]` does not); `mouse_<action>=<mouse name>|none` puts an action on `Mouse Left`,
  `Mouse Right`, `Mouse Middle`, `Mouse 4`, `Mouse 5`, `Wheel Up` or `Wheel Down` (the names share the key list; any case). Once on and
  with no line, attack 1 is on the left button and attack 2 on the right (`game::Adapter::mouseDefaults`); a button stored for an action
  leaves the action that had it by default. A file without these lines reads as before.
- **The Toy Box keys**: `[input] key_toybox` (F6), `key_toybox_add` (F7), `key_toybox_clear` (F8), SDL key names; Tab also opens the Toy
  Box while `key_toybox` is not set. Tap Tab to open it, hold for 0.25 s for the quick swap wheel, release to choose. Once
  `key_toybox` is set, that key takes both the tap and hold actions and Tab is free for the game. The add and clear keys
  place the next figure and remove all figures from the emulated portal. Saved keys apply on startup and immediately after
  rebinding in F3 > Commands; Reset restores F6/F7/F8 and the Tab alias. Files without these lines keep the defaults.
- **`glyphs`**: the button glyphs the game shows (SSA: its A, B, X, Y, START and right-stick textures; Giants: the
  buttons of its fonts, A, B, X, Y, START, BACK, LS, RS, and its title's A; in menus, dialogs and prompts). `xbox360` keeps the original ones (the Xbox 360 pad); `xboxone` draws the Xbox One / Series pad's:
  dark round buttons with the letters in their colours, Menu (three lines) for START, View for BACK, LS and RS for the sticks;
  `xbox` (the value older `launcher.ini` files have) follows the pad among those two, like `auto` without the other
  families; `playstation` shows ✕ ○ □ △ by position (Xbox A is ✕, B ○, X □,
  Y △), Options for START, Share for BACK and L3/R3 for the sticks; `nintendo` shows the letters by position (Xbox A, at the bottom,
  is "B"; B is "A"; X is "Y"; Y is "X"), + for START, - for BACK and L/R for the sticks; `keyboard` shows keycaps with the key bound
  to each action (`key_a` for A, `key_start` for START, `key_back` for BACK, a stick's four keys), named as on the
  current keyboard layout (Space, Enter...). `auto` follows the last device used: a key press shows keycaps, a button,
  stick or trigger of the gamepad in use shows that gamepad's family (SDL's gamepad type: Xbox One/Series, PS3/PS4/PS5,
  Switch Pro and Joy-Cons, anything else, the Xbox 360 pad included, Xbox 360); before any input it is the gamepad
  opened at startup, else the keyboard. Devices the game does not read (`input`) are ignored. The glyphs are drawn by
  the runtime (shapes, and the Bungee font built into the executable), `runtime/host/glyphs.cpp`; the change shows
  on the next frame. The Toy Box's key caps follow the same style: View + Y and Menu (Xbox One), Back + Y and Start
  (Xbox 360), F6 and Del (keyboard). On the Xbox console the default is `xboxone`.

- **`pad_<button>`**: the pad's button `<button>` does the job of the named one, for **every real pad** (not the keyboard,
  which has its own keys). `pad_rb=b` gives RB B's job and RB no longer does its own; a swap is two lines (`pad_a=b`,
  `pad_b=a`); `none` turns the button off; a trigger counts as pressed past 30 of 255 and a button that becomes a trigger
  presses it all the way. The sticks' axes are not remapped. Each game has its own lines (`[ssa.input]`, `[giants.input]`,
  `[swapforce.input]`, `[trapteam.input]`), edited on the game's settings page, Controller tab. The log's `[settings]` line says
  `pad rb->b`.
- **Trap Team LB**: the runtime's Commands page offers LB alongside Y (Attack 3) and RB (Attack 2), for both
  keyboard and controller binding. The default keyboard key is Q; `[trapteam.input] key_lb=R` binds it to R.
  `pad_a=lb` makes the physical A send LB; a swap also needs `pad_lb=a`. Normal LB reaches the game as `0x0100`.
  The standalone launcher's LB row still needs updating in its separate checkout. Contextual trap behavior needs
  a play check ([Trap Team](../../games/trapteam/docs/boot.md#lb-and-independent-voices)).
- **`rumble_p1`, `rumble_p2`**: what the game asks (`XamInputSetState`) is sent to that player's pad (SDL rumble):
  `off` nothing, `low` 40 %, `normal` as asked (default), `strong` 160 % (capped at the motors' maximum).

- **`toybox_auto_open`** (`[portal]`, default 1): when the game asks for a figure ("Player 1: place a Skylander on the
  Portal of Power"), the Toy Box opens by itself on that player's seat (emulated portal only), and closes again when
  the prompt goes away; closing it by hand does not open it again for the same prompt. [portal.md](../runtime/host/portal.md).

`SKY_SETTINGS=<file>` (or `SKY_LAUNCHER_INI=<file>`) makes the runtime use that file instead of the player's
`launcher.ini`; a missing or empty file means none, so the defaults apply. Every test run sets it
([testing.md](testing.md#isolation-every-game-run)).

## Where everything is: one folder, "Skylanders"

Everything the game, the launcher and Arkeyan-Builder keep for the player is under **one root per OS** (code:
`runtime/host/collection/datadir.{h,cpp}`, shared with the launcher through `tools/sync_skylauncher.sh`):

| OS | Root |
|---|---|
| Windows | `%APPDATA%\Skylanders` (everything, the built game and the compiler too) |
| macOS | `~/Library/Application Support/Skylanders` |
| Linux | `$XDG_DATA_HOME/Skylanders`, by default `~/.local/share/Skylanders` |

`SKYLANDERS_DATA=<folder>` replaces the root (tests). **Portable mode**: a file `portable.txt` next to the launcher (on macOS, next to the
`.app`) makes `<that folder>/data` the root: every setting, save, figure, log and built game goes there, for the launcher and the games it starts
([platforms/data_folders.md](platforms/data_folders.md#portable-mode)). Inside:

| Path | What |
|---|---|
| `Collection/` | the figures, shared by every game ([collection.md](../runtime/host/collection.md)) |
| `launcher.ini` | the settings shared by the games and the launcher |
| `kit/<release tag>/` | the kits the launcher downloaded (older ones are deleted once a game is built with a newer one) |
| `toolchains/` | the compilers Arkeyan-Builder downloads (Windows: llvm-mingw; the downloaded archive is deleted once extracted) |
| `logs/` | the launcher's logs: `skylauncher-game.log` (what the game printed when started from the launcher, with a `.prev`), `migration.log` |
| `SSA/`, `Giants/`, `SwapForce/` | one folder per game (`GameFolderName`): the built game (the full game title, with `.app` or `.exe` on those platforms), `install.log`, the install logs in `install-logs/`; below, `SSA/` stands for either |
| `SSA/saves/`, `SSA/saves-backup/`, `SSA/pipelines.bin`, `SSA/portal.txt` | its saves, the previous versions kept before each save is replaced (`[saves]` below), pipeline list and last portal |
| `SSA/figures/`, `SSA/portraits/`, `SSA/launcher-cache/` | its figure dumps from before the collection, its toys' portraits and its logo, background and title icon, read from the player's disc ([game_art.md](../runtime/host/game_art.md)) |
| `SSA/Arkeyan.log`, `SSA/Arkeyan.prev.log` | its log and the previous session's |
| `SSA/achievements.txt`, `SSA/playtime.txt` | the achievements the game unlocked (`<id> <unix time>` per line) and its play time (`total=` seconds, `last_end=` unix time, `sessions=`), written by the runtime and read by Arkeyan Launcher ([xam.md](../runtime/xbox360/xam.md#achievements)); `SKY_SAVE_DIR` holds them in test runs |
| `SSA/presence.txt` | the running game's Discord presence for Arkeyan Launcher: `level=<id>` (the exact `game::LevelStarted` id, empty before the first level), `menu=1` for the title/front end or `0` otherwise (`game::InMenuLevel`), and `players=1` or `2` while the Toy Box's `p2` character seat is occupied (virtual or physical). Each key has its own line. This is seat occupancy, not a controller connection or the game's join/drop-out state; it follows the existing seat mapping rather than counting portal tags, items or vehicles. Written at startup and on changes, atomically through a temporary sibling and rename; seat changes are flushed by the Toy Box link worker even when the link is disabled. Lives beside achievements and play time in the per-game player profile folder (`datadir::PlayerGame`), or `SKY_SAVE_DIR` when set. The launcher should read it only for a running game; it can remain after exit. |
| `SSA/launcher-cache/achievements/` | Arkeyan Launcher's cache of the game's achievements (`list.tsv` and one PNG each), read from the player's disc at its disc check |

The environment variables that name one file or folder keep winning over the root: `SKY_SETTINGS` and
`SKY_LAUNCHER_INI` (the settings file), `SKY_SAVE_DIR` (saves, `pipelines.bin`, `portal.txt`), `SKY_COLLECTION`,
`SKY_PORTRAITS`, `SKY_LOG`, `SKY_LAUNCHER_DATA` (the launcher's kits, see its `docs/install.md`). The player's profile
(name, picture, XUID, and Player 2's) is `SKY_PLAYER_*` and `SKY_PLAYER2_*`: [profiles.md](../runtime/host/profiles.md). The Xbox signing
certificate (`xbox-signing`, Xbox development only) stays in `<application data>/SSARecomp/xbox-signing`
(`tools/xbox/package.sh`): it is a developer's key, not part of a player's data.

Before 0.3.1 the data was spread over several folders; `datadir::MigrateOnce()` moved them on the first start of the
new game or launcher (never in a test run): [platforms/data_folders.md](platforms/data_folders.md).

## Saves and their backups

A save file is never written in place. `kernel/files.cpp` writes it to `<name>.savetmp` in the same folder, flushes
that to the disk (`fsync`), and when the handle closes renames it over the save (Windows:
`MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`). A crash, a power loss or a kill during a save
therefore leaves the previous save exactly as it was: a half-written file can never be seen, and a save whose bytes
did not change is not rewritten.

Before it replaces a save, the runtime keeps the previous version in `<saves>-backup/<time>/` with the same relative
paths (`00000002/Skylanders - Global Data/GLOBAL.DAT`). `<saves>` is `SSA/saves` (or `SKY_SAVE_DIR`), so the folder is
`SSA/saves-backup`; `<time>` is `YYYYMMDD-HHMMSS` in local time, with `-2`, `-3`... when the same file changes twice in
one second. Every backup logs one line: `[saves] backup 20261025-140322`. Two save files written in the same second
share one backup folder. Rotation keeps the last 10 backups plus the newest one of each of the last 7 days; the rest is
deleted when a new backup is made.

While the game runs it holds `<saves>-backup/game.lock` with its process id. Arkeyan Launcher refuses to restore a backup
over a running game; a marker a crash left behind is dropped once its process is gone. The backup, restore and atomic
write code is `host/collection/save_backup.{h,cpp}`, shared with Arkeyan Launcher (Settings > Game data, its
`docs/install.md`) through `tools/sync_skylauncher.sh`.

## The game log

The desktop runtime (Windows, macOS, Linux) writes everything it prints into **`Arkeyan.log`** in its game folder
(`SSA/`, or `Giants/` for Giants: the file keeps the name `Arkeyan.log`):

- Windows: `%APPDATA%\Skylanders\SSA\Arkeyan.log`
- macOS: `~/Library/Application Support/Skylanders/SSA/Arkeyan.log`
- Linux: `~/.local/share/Skylanders/SSA/Arkeyan.log`

The previous session's log is kept next to it as `Arkeyan.prev.log`. Every line is in the file as soon as it is
printed (stderr is the file itself; a crash loses nothing), and the file can be read while the game runs; a terminal
still sees the lines too. `SKY_LOG=<file>` writes that file instead (test runs: never the player's); an empty
`SKY_LOG` writes no file. `--help`, `--version`, `--list-gamepads` and `--portal-probe` write no log. The Xbox (UWP)
build has its own, `LocalState\Arkeyan.log`. Code: `runtime/host/log.cpp`.

The first lines say what the game runs on (an integrated-GPU PC; the numbers are an example):

```
[log] C:\Users\...\AppData\Roaming\Skylanders\SSA\Arkeyan.log
[version] ArkeyanRuntime 0.1.0, build 3e66a80
[system] Windows 10.0 build 26100
[system] CPU AMD Ryzen AI 7 445 w/ Radeon 840M, 12 logical cores, AVX yes, AVX2 yes; 15688 MB RAM
[settings] scale auto, vsync 1, windowed, anisotropy 1, fps limit 30, input auto, ...
[portal] mode auto
[gpu] Vulkan on AMD Radeon(TM) 840M Graphics, swapchain output
[gpu] adapter AMD Radeon(TM) 840M Graphics: vendor 1002, integrated, unified memory, 512 MB of video memory, driver 2.0.302
[gpu] window 1280x720 (1920x1080 pixels), display 2880x1800 at 150%
[gpu] resolution scale auto: 1 (integrated GPU (shared memory); resolution_scale=1 to 10 sets it)
[gpu] resolution scale 1: the 3D renders at 1120x704
```

A fault or a fatal error adds, after the `Fault (...)` line, `[crash] where: guest thread N, lr 0x..., r1 0x..., r3 0x...` and, at the end of
the host stack, `[crash] guest function: sub_XXXXXXXX, called from ...`; the F3 menu's "Report a problem" collects the log, the
previous log and that block into one file ([gamemenu.md](../runtime/host/gamemenu.md)).

(`build` is the commit the runtime was configured from, `-dirty` with local changes; Wine adds `(Wine <version>)` to
the system line.) Then every 10 s for the whole session, the `[perf]` lines (frame rate, frame time median and p95,
where a frame's time goes, the GPU thread's busy share; `gpu/gpu.cpp`, `LogPerf`); `SKY_PERF_LOG=0` turns them off.

## Pending

- `[portal] mode` and `place` are still passed through `SKY_PORTAL` / `SKY_PORTAL_PLACE` by the launcher; the runtime
  does not read them from `launcher.ini` itself.
- The UWP (Xbox) build has no command line: it reads `LocalState\launcher.ini` (`[video]`, `[input]`, `[ssa]` and
  `[portal] mode`, written with console defaults at the first start; `settings_uwp.cpp`,
  [platforms/xbox_uwp.md](platforms/xbox_uwp.md)).
