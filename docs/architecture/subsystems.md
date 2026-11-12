# Runtime subsystems: role, main files, key functions, invariants, gotchas

> **Purpose:** for each subsystem of `runtime/`, what is its role, which files and functions matter, and which invariants must hold? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [XDK] [Lux] [SSA]

One section per subsystem of `runtime/` (paths below are relative to it unless they start with `engines/` or `games/`).
**Class**: [Xbox] generic, [Lux]/[SSA] marked. Each section links to the page that holds the detail; this page is the
*map with the invariants in one place*. Guest addresses never appear in `runtime/`: hooks read them from
`games/<id>/addresses.h` ([hooks.md](hooks.md)).

## kernel (`kernel/*.cpp`)
- **Role**: the xboxkrnl imports ([xbox360/kernel_exports.md](../runtime/xbox360/kernel_exports.md)). **Files/functions**: `objects.{h,cpp}` (`KernelObject`, `Event/Semaphore/Mutant`, `LookupDispatcher`, `WaitForObjects`, `g_kernelLock`/`g_kernelSignal`), `sync.cpp` (critical sections, spin locks, events, waits), `threads.cpp` (`ExCreateThread`, `Yield()`, TLS, `RtlRaiseException`), `memory.cpp`, `system.cpp` (`StartTimeStampBundle`), `files.cpp` (Nt file API, content roots, `SKY_LEVEL`), `overlapped.cpp`, `stubs.cpp`, `rtl_time.cpp`, `strings.cpp`, `printf.cpp` (`vsprintf`, `_snprintf`; their formatting, shared with the portal log, in `guest_format.{h,cpp}`).
- **Invariants**: one mutex + one condvar for every wait; a dispatcher header is re-bound through `'SSAK'` after inlined initialisers overwrite it; `InternalLow` is published last; the critical-section owner is the KTHREAD stored big-endian; a wait on 0 handles fails; every transfer is synchronous and completes APCs on the calling thread.
- **Gotchas**: stubs that return success without outputs ([xbox360/stubs.md](../runtime/xbox360/stubs.md)); zero-length sleeps are the job system's idle loop; the 9th argument is at `r1+0x54`.

## XAM (`kernel/xam*.cpp`, `xconfig.cpp`, `xmp.cpp`, `net.cpp`, `module.cpp`, `input.cpp`, `audio.cpp`, `video.cpp`, `xma.cpp`)
- **Role**: user-mode services: content and saves, profile, achievements, XConfig, music player, network, input, audio render driver, video interrupts, XMA ([xbox360/xam.md](../runtime/xbox360/xam.md), [xam_content_saves](../runtime/xbox360/xam_content_saves.md), [audio_xma_xaudio](../runtime/xbox360/audio_xma_xaudio.md), [video_vblank](../runtime/xbox360/video_vblank.md)).
- **Key functions**: `XamContentCreateEx` (overlapped at `r1+0x54`), `XamShowDeviceSelectorUI` (picks the hard drive without UI), `ExGetXConfigSetting`, `XMsgInProcessCall` (XMP), `XNetGetTitleXnAddr` (NONE), `VdSetGraphicsInterruptCallback` (starts the interrupt thread), `XAudioRegisterRenderDriverClient`, the six hooked XMA register writers (8 XMA hooks with the two `Init`s, `kernel/xma.cpp`).
- **Invariants**: every XAM call with an overlapped completes it at once and returns `ERROR_IO_PENDING`; XMP answers "title has control"; the vblank status register `0x7FC86544` bit 0 is set before the callback; the audio context cell is a pointer to the context.
- **Gotchas**: the language comes through XConfig *country* for nl/sv/no/da/fi; save names need `vsprintf`; XMA lock order (engine 1 critical section first).

## d3d (`d3d/*`)
- **Role**: replace the game's D3D9LTCG emission functions and rebuild each operation from the device state ([gpu/README.md](../runtime/gpu/README.md)). **Files**: `render.cpp` (`PrepareDraw`, `BuildDraw`, `IssueDraw`, `Resolve`, `Clear`; 6 hooks: draws, BeginVertices, clear, resolve, swap), `device.{h,cpp}` (offsets, `RegisterOffset`, `RegisterGroups`), `ring.{h,cpp}` (`ScanRing`, `GpuRegister`, `GpuFloatConstants`, `SendInheritedShadows`), `command_buffers.{h,cpp}` (`RecordDraw`, replay), `null_gpu.cpp` (fences, callbacks), `shaders.{h,cpp}` (`Describe`, `GuestShader`), `xenos.{h,cpp}` (formats, tiling, enums), `bink_display.cpp` (the `BinkDoFrame`, `BinkOpen`, `BinkClose` hooks; the Lux movie player's `Movie_Update` is in `engines/lux/movie.cpp`), `ultrawide.{h,cpp}`, `capture.{h,cpp}`.
- **Invariants**: scan the ring **after** the guest's draw call; hash a shader container **before** `Create*Shader`; param_gen from the pixel shader header; BeginVertices draws are snapshotted at the call; replay restores the device bytes it overwrote; resolve parameters come from the call arguments.
- **Gotchas**: [gpu/render_bugs.md](../runtime/gpu/render_bugs.md), [gpu/ring_mirror.md](../runtime/gpu/ring_mirror.md); the addresses and XDK constants come from each game's `addresses.h` (`namespace game`).

## gpu (`gpu/*`)
- **Role**: host renderer on plume ([gpu/plume_backends.md](../runtime/gpu/plume_backends.md), [renderer_threads](../runtime/gpu/renderer_threads.md), [pipeline_cache](../runtime/gpu/pipeline_cache.md), [shaders](../runtime/gpu/shaders.md)). **Files**: `gpu.cpp` (`Init`, `Present`, readbacks, `LogPerf`, `AutoScale`, backend choice, `SetOverlay`), `draw.cpp` (`Draw`, `FindXenosShader`, `PipelineKey`, the list, `WaitForPipelineList`, `SharedConstants`), `worker.cpp` (GPU thread, per-frame arena, upload blocks, texture workers), `context.h`, `buffer_cache.cpp`, `d3d12_diagnostics.cpp`, `d3d12_pipeline_cache.cpp`, `shaders/*`.
- **Invariants**: the GPU thread never takes `g_producerMutex`; producers copy guest data at the call; one frame in flight (Present waits for the frame before); bind framebuffers only on change; `PipelineRecord` is 832 bytes (bump `PipelineListVersion` on any key change); textures are destroyed only after the frame is done.
- **Gotchas**: Metal needs the queue-order patch; typeless depth SRVs; committed D3D12 targets; DXIL signing; smol-v SPIR-V; shader cache rebuilt after any XenosRecomp change; the Metal `.app` cache.

## host: display, input, audio, settings, log, glyphs (`host/*.cpp`)
- **display** (`display.{h,cpp}`): `CreateDisplayWindow`, `SubmitDisplayFrame` (CPU frames before the game renders: movies), `PresentDisplayFrame` (main thread, vsync-paced), `SetGameRendering`, snapshots; offscreen output when the window has no native surface.
- **input** (`input.{h,cpp}`): `SetHostGamepad`, `GetGamepad(packetNumber)`, `LoadInputScript` (`SKY_PRESS`); [xbox360/input.md](../runtime/xbox360/input.md).
- **audio** (`audio.cpp`, `audio_xaudio2.cpp`): `HostAudioInit/Submit/Active/QueuedFrames`; 5.1 -> stereo downmix.
- **settings** (`settings.{h,cpp}`, `settings_test.cpp`): `ParseSettings(config, gameSection, getenv, argc, argv, ...)`: command line > env > ini > defaults; `DescribeSettings` -> the `[settings]` line; `g_keyActions[22]`. Unknown option -> exit 2.
- **log** (`log.{h,cpp}`): `LogInit/LogHeader`: stderr becomes the log file (written directly per print; raw OS reads for the terminal copy because the C runtime's `_read` stops at the first EOF on Windows); previous log kept as `.prev.log`; `SKY_LOG` overrides.
- **glyphs** (`glyphs.{h,cpp}`): hash table of the title's glyph textures; `Replace(hash, w, h, levels, texture, contentHash)` in `TextureFor`; signed-distance drawing with Bungee; `auto` follows the last device.
- **Invariants**: settings apply at startup only (no live change); the scale is fixed for the session; `SetFrameRateLimit` before the title starts.

## portal (`host/portal*.cpp`, `kernel/portal.cpp`)
- **Role**: choose and serve a portal ([portal.md](../runtime/host/portal.md)). **Key functions**: `PortalInit` (mode choice, `AsyncPortal` wrapping for real devices, emulated setup, stale-lock cleanup, migration, restore of `portal.txt`), `PortalWrite/Read/IsConnected`, `PortalPlaceFigure(At)`, `PortalRemoveFigure/All`, `PortalSeen`, `PortalProbeMain`; backends `EmulatedPortal`, `CreateHidPortal`, `CreateUsbPortal`, `AsyncPortal`, `StallingPortal`; `kernel/portal.cpp` adds/removes the 2-byte header and logs the title's portal log.
- **Invariants**: backends speak bare protocol; an inactive portal is silent; the emulated portal never writes block 0 or trailers; `portal.txt` is rewritten at every change; locks are removed at exit (atexit); real device I/O never runs on the title's threads.
- **Gotchas**: HID usage page `FF00` hidden by hidapi, SET_REPORT writes, a blocking write freezes a game frame; GIP framing unverified.

## figures (`host/figures/*`)
- **Role**: the tag library ([figures.md](../runtime/host/figures.md)): `figure_crypto` (MD5/AES-128/CRC-48 from the specs), `figure_tag` (`FigureStats`, decrypt/encrypt, areas, checksums, levels), `figure_data` (`FigureData`, blank figure), `figure_catalog`, `figure_test`. The disc's pictures (portraits, logos) are `host/art/*` ([game_art.md](../runtime/host/game_art.md)).
- **Invariants**: write both areas with sequences s+1/s+2 and recomputed checksums; clamp XP/gold; unknown bytes preserved; self-contained (usable from the launcher).

## collection (`host/collection/*`)
- **Role**: the shared collection ([collection.md](../runtime/host/collection.md), [collection_api.md](../runtime/host/collection_api.md)). **Key functions**: `Folder`, `List`, `Read`, `Create`, `WriteStats`, `Backup/Restore/PruneBackups`, `Lock/Unlock/BreakLock/ClearStaleLocks`, `Delete/Undelete/PurgeTrash/Tidy`, `Import/ImportTag/Export`, `GetView/SetView`, `GetWheel/SetWheel`, `Group`, `Migrate/MigratedPath`, `Touch/Recent`; `catalogue.*`, `icons.*`, `json.*`, `datadir.*` ([platforms/data_folders.md](../getting-started/platforms/data_folders.md)).
- **Invariants**: `collection.json` re-read before each change, written to a temp file then renamed, **a file that fails to load is kept and never overwritten**; writers refuse a figure locked by a live process; backups before every destructive write; originals of a migration are never deleted; no code outside this directory is needed to copy it into Arkeyan Launcher (relative includes).
- **Gotchas**: two writers changing metadata at the same instant can lose one change (no lock on the index); locks record only the game id.

## toybox (`host/toybox.cpp`, `host/toybox/{model,draw}.cpp`, `imgui_software.cpp`)
- **Role**: the in-game figure manager ([figures.md](../runtime/host/figures.md#toy-box-menu), [toybox_ux.md](../runtime/host/toybox_ux.md)). `model` = state and logic (`Screen`, `Command`, seats, `Grid`, dialogs, name entry, Manage), `draw` = look only, `toybox.cpp` = input and frame (`ToyboxFilterGamepad`, `ToyboxHandleEvent`, `ToyboxUpdate`, script), `imgui_software` = CPU rasteriser.
- **Invariants**: everything runs on the host main thread; the title gets no pad input while open; redraw only on change; a figure swap is lift + place in the same slot; sticks/triggers use dead zones when releasing the menu.

## platform (`platform/*`)
`platform.h`: `PlatformInit`, `PlatformInstallFaultHandler`, `PlatformReserveMemory` (lazy commit fallback on Windows), `PlatformProtectNoAccess`, `PlatformStartThread` (16 MB stacks), `PlatformSetThreadName`, `PlatformTimeBaseFrequency`, `PlatformOpenFile/ReadFile/CloseFile` (positional reads; kernel-mode writes commit reserved pages first). POSIX: fault report on an alternate stack with frame-pointer walk; Linux thread names <= 15 bytes. Win32: unhandled-exception filter, `timeBeginPeriod(1)`, `CreateFile2` (UWP-safe).

## uwp (`host/uwp/*`)
CoreApplication entry (`main.cpp`: Activated event, swap chain + black frame before the game loads, UI-thread presents, publisher cache collection folder, LocalState settings/log), `sdl_subset.cpp`, `portal_backends.cpp` (GIP + HID routes), `settings_uwp.cpp` (`[gpu]` safe-mode keys), `cpu_check.cpp`; invariants and the nine-run history: [platforms/xbox_uwp.md](../getting-started/platforms/xbox_uwp.md).

## launcher inside the runtime (`host/launcher*.cpp`, `launcher_ui*.cpp`, `widgets/`, `launcher_assets.*`, `launcher_games.cpp`)
The first-run flow and the original built-in window (`--launcher`, the screenshot test;
[runtime_launcher.md](../runtime/host/runtime_launcher.md)); the player-facing launcher is **Arkeyan Launcher** (separate
repo, [launcher/README.md](../release/launcher/README.md)). This code still holds the disc verification and the
settings file; the disc table is each game's `games/<id>/info.cpp`. `CheckGameSource`, `ReadTitleMetadata`, `LoadLauncherConfig/SaveLauncherConfig`, `ResolveGameSource`, `ResolveLanguages`.

## apu, loader, language, inflate, engine and game hooks
`apu/` XMA context + FFmpeg wrapper ([xbox360/audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md)); `loader.cpp` ([xbox360/loader_xex.md](../runtime/xbox360/loader_xex.md)); `inflate.cpp` ([lux/zlib.md](../engines/lux/zlib.md)); `games/ssa/language.cpp` (SSA's voice language hooks, [lux/languages.md](../engines/lux/languages.md)); `engines/lux/patches.cpp` (NGL guards, heap free guard, [lux/ngl_renderer.md](../engines/lux/ngl_renderer.md)), `engines/lux/movie.cpp`, `engines/lux/light_tiles.cpp`, `engines/{lux,alchemy}/ultrawide.cpp`. Every hook's address comes from `games/<id>/addresses.h`.
