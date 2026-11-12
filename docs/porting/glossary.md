# Glossary

> **Purpose:** what the words of this project mean, one line each, with the page that explains them · **Status:** current (2026-10-26) · **Tags:** [Xbox] [XDK] [Lux]

Alphabetical. **[Xbox]** console hardware and kernel (Xenos included), **[XDK]** the XDK's libraries in the title (D3D9,
XAudio2...), **[Lux]** the engine; no tag = our own tools and words. Links go to the page that explains it.

| Term | Meaning |
|---|---|
| **20e4** | Xenos floating-point depth: 4-bit exponent (bias 15), 20-bit mantissa, range [0, 2); stored in bits 31-8 of D24FS8 (stencil in 7-0). [Xbox] [gpu/xenos_registers_pm4.md](../runtime/gpu/xenos_registers_pm4.md) |
| **ALU constants** | the 512 float4 shader constants (256 VS, 256 PS), registers `0x4000-0x47FF`. [Xbox] |
| **Ambient cube** (`AmbientMap`) | 32x32 k_8_8_8_8 **cube** texture rendered and resolved face by face every frame; only source of ambient light. [Lux] [lux/level_frame_lightprepass.md](../engines/lux/level_frame_lightprepass.md) |
| **AES key (figures)** | `MD5(block0 || block1 || block number || " Copyright (C) 2010 Activision. All Rights Reserved. ")`, AES-128-ECB per block. [Lux] [figures.md](../runtime/host/figures.md) |
| **APC** | asynchronous procedure call: `ReadFileEx` completion routines queued on the calling thread and run in its next *alertable* wait; Wwise depends on them. [Xbox] [xbox360/threads_sync.md](../runtime/xbox360/threads_sync.md) |
| **APKF** | image package inside `.str` records: chunks `\0XET` (texture header) and `SYHP` (pixel data at `+0x1000`). [Lux] [lux/str_levels_loading.md](../engines/lux/str_levels_loading.md) |
| **Area (figure)** | one of two progress copies in a tag (blocks 8-16 and 36-44) with a sequence byte; the newer valid one wins. [Lux] |
| **AsyncPortal** | real-portal device I/O on its own thread (queues, backoff 2-10 s) so the game never blocks on the device. |
| **Baseline (pipeline)** | `games/<id>/pipelines/baseline.zst`: the render-state + shader-hash records built into the executable (SSA 1570, Giants 1575). [gpu/pipeline_cache.md](../runtime/gpu/pipeline_cache.md) |
| **BeginVertices** | D3D call returning a buffer the game fills afterwards (dynamic vertices); we snapshot the draw at call time and send it at the next D3D call. [XDK] |
| **Bink** | RAD Game Tools video; Bink 1 movies, YUV frame planes, run-time-built shaders. [Lux] [lux/bink.md](../engines/lux/bink.md) |
| **Tester (the)** | the person who tests the Windows builds on a real PC, with a wired Wii/PS3/PC portal. |
| **Catalogue** | `resources/collection/catalogue.json`: 271 toys of six games with categories, variants, which game accepts what. [launcher/collection_toybox.md](../release/launcher/collection_toybox.md) |
| **Collection** | the shared folder of figure dumps (+ `collection.json`, locks, backups, trash) used by every game and the launcher. |
| **Command buffer** | a recorded list of D3D state + one draw (`BeginCommandBuffer .. EndCommandBuffer`), replayed with `RunCommandBuffer`; the engine caches one per mesh/material. [Lux] [gpu/command_buffers.md](../runtime/gpu/command_buffers.md) |
| **Config (XenonRecomp)** | `games/<id>/config/<Game>.toml`: CRT helper addresses, setjmp/longjmp, `invalid_instructions`, `functions`, switch table file. |
| **Content package** | a save directory mounted by `XamContentCreateEx` under a drive name (`lux:`). [Xbox] |
| **CTAB** | the shader constant table inside a container (names, registers). [XDK] |
| **D3D9LTCG** | the statically linked Xbox 360 Direct3D 9 built with link-time code generation: functions inlined into game code. [Xbox] [gpu/d3d9ltcg_device.md](../runtime/gpu/d3d9ltcg_device.md) |
| **Datadir / data root** | `Skylanders` folder (`%APPDATA%`, `~/Library/Application Support`, `$XDG_DATA_HOME`) holding collection, `launcher.ini`, kits, toolchains, logs, one folder per game (0.3.1). [platforms/data_folders.md](../getting-started/platforms/data_folders.md) |
| **DEC3N** | packed normal format: three signed normalised 10-bit components, X in the low bits; given to shaders as a raw uint. [Xbox] |
| **Depth write-back** | writing a host depth resolve into guest memory (tiled, 20e4) for a CPU job that reads it. |
| **Dirty mask** | the five u64 at device `+0x00..+0x20` flagging constant/register groups to send with the next draw. [XDK] |
| **Designer (the)** | the designer of Arkeyan Launcher's look and owner of its UI issues. |
| **DRED** | D3D12 Device Removed Extended Data (breadcrumbs, page faults) turned on before device creation. [Xbox] |
| **DXIL** | D3D12 shader bytecode; the cache holds one signed container per specialization variant. [Xbox] |
| **EDRAM** | the Xenos' 10 MB embedded memory that colour/depth targets render into; copied out by *resolves*. [Xbox] [gpu/edram_resolve_tiling.md](../runtime/gpu/edram_resolve_tiling.md) |
| **Edition (figure)** | a physical edition the tag cannot tell apart (Crystal Clear, Glow...), recorded in `collection.json`. |
| **Engine 1 / engine 2 (XMA)** | the two copies of XMA voice code (XAudio2's and Wwise's). [Lux] |
| **F (step factor)** | smoothed frame time in 60ths of a second (max 3), the game's variable-step delta. [Lux] [lux/frame_pacing.md](../engines/lux/frame_pacing.md) |
| **Fence (GPU)** | a counter D3D waits on; with no GPU, fences retire when work is kicked off. [Xbox] [xbox360/video_vblank.md](../runtime/xbox360/video_vblank.md) |
| **Fences (memory)** | `lwsync/sync/eieio` translated to real barriers; without them ARM64 corrupts the heap. [Xbox] |
| **Fetch constant** | 6-dword texture/vertex descriptor in the register file (`0x4800..`). [Xbox] [gpu/textures_formats.md](../runtime/gpu/textures_formats.md) |
| **Figure / toy / `.sky`** | a MIFARE Classic 1K tag image (1024 bytes, 64 blocks) dumped to a file. [Lux] [figures.md](../runtime/host/figures.md) |
| **Fast recipe** | the scripted-press run to the first level in ~75 s. [testing.md](../getting-started/testing.md) |
| **G-buffer** | MRT of normals (k_2_10_10_10) + k_8_8_8_8 (`GBuffer2`) used by the light pre-pass. [Lux] |
| **GIP** | Xbox One accessory protocol; the Xbox One portal speaks it (message `0x21`, 30 bytes). [Xbox] |
| **Guest / host** | the translated Xbox 360 program and its address space / the machine running it. |
| **Hook** | a `PPC_FUNC(sub_X)` override of a recompiled function, named by `GAME_HOOK_<name>` in `games/<id>/addresses.h` (SSA 39, Giants 26). [hooks.md](../architecture/hooks.md) |
| **HID portal** | Wii/Wii U/PS3/PS4/PC portal, USB HID `1430:0150`, commands as SET_REPORT. [Xbox] |
| **Indirect-call guard** | log-and-skip for a bad indirect call (`SsaBadIndirectCall`). |
| **Inherit tags** | five u64 in `0x82BB46D0` saying which register groups a command buffer inherits from the caller. [Lux] |
| **Interrupt thread** | the host thread delivering vblank (every 16 667 us) and command-processor interrupts to D3D. [Xbox] |
| **JQ Worker** | the engine's job-queue worker threads (`JobWorker_Loop`). [Lux] |
| **Kill vertex** | vertex-shader export that discards a vertex (we move it off screen). [Xbox] |
| **Kit (player kit)** | `skylanders-kit-<platform>.zip` (also published as `ssa-kit-<platform>.zip`): Arkeyan-Builder + translators + prebuilt runtime, **nothing from the game**. [launcher/kit_contract.md](../release/launcher/kit_contract.md) |
| **Leaf (packed)** | a tiny function without `.pdata`, reached only through run-time pointers; must be listed in `functions`. |
| **Light pre-pass** | normals -> light accumulation (omni, sun tiles, ambient) -> main colour pass using the light buffers. [Lux] |
| **Lock (figure)** | `<dump>.lock` JSON written while a game has the figure on its portal. |
| **`.lsa`** | Lux shader archive (`\0ASL`): `shaders.lsa`, `effects.lsa`. [Lux] [gpu/shaders.md](../runtime/gpu/shaders.md) |
| **`.lxb`** | Lux binary data (`sku.lxb`, text, UI tuning, levels). [Lux] |
| **Lux / `lux::`** | the Toys for Bob engine namespace; with NGL, Bink, Wwise. [Lux] [lux/README.md](../engines/lux/README.md) |
| **MIFARE Classic 1K** | the NFC tag type of every figure: 16 sectors x 4 blocks x 16 bytes. [Lux] |
| **`mftb`** | PowerPC time base register; translated to the host counter. [Xbox] |
| **MRT** | multiple render targets (up to 4 colour targets; they must share a size on the host). [Xbox] |
| **NGL** | the engine's renderer (`ngl*`): scenes, render lists, ListSendModule. [Lux] [lux/ngl_renderer.md](../engines/lux/ngl_renderer.md) |
| **Option B** | the renderer decision: hook the game's D3D, no PM4 execution, translate to plume. [gpu/README.md](../runtime/gpu/README.md) |
| **Overlapped (XOVERLAPPED)** | XAM async call state: `InternalLow` result (published last), `InternalHigh`, event, completion routine. [Xbox] |
| **`.pdata`** | XEX function table XenonRecomp uses; leaf functions are missing from it. [Xbox] |
| **Param_gen** | `SQ_PROGRAM_CNTL` bit 18: the hardware puts the pixel position (XY) and point coordinate (ZW) in a pixel-shader register (`SQ_CONTEXT_MISC.param_gen_pos`). [Xbox] |
| **PCR / KTHREAD** | per-thread control structures reached through `r13`; PCR 0x2D8 bytes, KTHREAD 0xAB0 bytes. [Xbox] |
| **Pipeline list / `pipelines.bin`** | magic "SSAP" list of pipeline keys built at startup and before level loads. |
| **plume** | the RHI (Metal, Vulkan, D3D12) from renderbag (RT64's), MIT. [gpu/plume_backends.md](../runtime/gpu/plume_backends.md) |
| **PM4** | the Xenos command packet format (types 0-3) the game's D3D writes into the ring. [Xbox] |
| **`ppc/`** | `private/<id>/ppc/`: XenonRecomp's generated C++ and the shader cache (never edited or committed). |
| **Predication / tiling** | rendering a target larger than EDRAM in tiles with `SetPredication`. [Xbox] |
| **Pulled vertex** | a vertex fetch whose index is not `r0.x`; the host gathers those vertices. [Xbox] |
| **Resolve** | copy of an EDRAM region into a texture (`D3DDevice_Resolve`), optionally clearing after. [Xbox] |
| **Ring (PM4 ring) / ring mirror** | the command ring D3D writes; our parser mirrors its register writes (`d3d/ring.cpp`). [Xbox] [gpu/ring_mirror.md](../runtime/gpu/ring_mirror.md) |
| **Seat** | a portal position as the game shows it (Player 1, Player 2, Magic item, Adventure pack, ...). |
| **Shadow (register)** | a copy of a register group in the device structure; setters only write it. [XDK] |
| **sigmatch** | masked-signature finder of SSA hooks in another Lux build. [toolchain/sigmatch.md](../tools/toolchain/sigmatch.md) |
| **Arkeyan-Builder** | the build driver of the player kit (events `@step/@progress/@log/@error/@done`). |
| **STR / `.str`** | `strm` container with one zlib stream of records (levels, UI images). [Lux] [lux/str_levels_loading.md](../engines/lux/str_levels_loading.md) |
| **Arkeyan Launcher** | the public launcher (`Arkeyan-Launcher`, MIT). [launcher/README.md](../release/launcher/README.md) |
| **Spec constants** | shader specialization (R11G11B10 normals, alpha test); one DXIL per combination on D3D12. [Xbox] |
| **Startup state machine** | the 16-byte-entry table at `0x82C91EE8` (`STATE_GAME_FINALCHECKS`...). [Lux] |
| **SSA** | Skylanders: Spyro's Adventure (title `415608AB`), the first and only ported game so far. |
| **Sun tiles** | 770 point sprites = 35x22 screen tiles of 32x32 px, each shaded by one of four sun passes by a class byte. [Lux] |
| **Toy Box** | the in-game figure manager (View + Y / F6): seats, My Skylanders, wheel, Figure page, Add, Manage. [launcher/collection_toybox.md](../release/launcher/collection_toybox.md) |
| **Thunk (host)** | a guest address given to a host function (`CreateHostThunk`) for `XexGetProcedureAddress`. |
| **UWP** | Universal Windows Platform: the Xbox Dev Mode app host. [Xbox] [platforms/xbox_uwp.md](../getting-started/platforms/xbox_uwp.md) |
| **Vblank** | the 60 Hz vertical blank; scene and shader time count them; the title paces on interval 2. [Xbox] |
| **Variant (figure)** | the block-1 variant field distinguishing editions of one toy type. [Lux] |
| **WideLoad** | the engine's async load system; completion events waited with `WaitForMultipleObjects` (count may be 0). [Lux] |
| **Wheel (quick swap)** | the in-game eight-place figure picker (hold View / Tab). |
| **Wwise** | Audiokinetic audio middleware: banks, `streaming.pck`, XMA voices. [Lux] [lux/wwise_audio.md](../engines/lux/wwise_audio.md) |
| **XAM / XAPI / XDK** | the console's user-mode services / system API library / SDK (libraries 2.0.20500). [Xbox] |
| **XDBF / SPA** | the title's resource blob in the XEX (name, icon). [Xbox] |
| **XDVDFS** | the Xbox disc file system (binary-tree directories, 2048-byte sectors). [Xbox] [xbox360/files_xdvdfs.md](../runtime/xbox360/files_xdvdfs.md) |
| **XenonRecomp / XenosRecomp** | PowerPC -> C++ / Xenos shader -> HLSL/MSL translators (hedge-dev; squidbus fork). |
| **Xenos** | the Xbox 360 GPU. [Xbox] |
| **XEX** | the Xbox 360 executable format (`default.xex`). [Xbox] |
| **XMA** | the console's hardware audio decoder (WMA Pro based), driven by 64-byte contexts and registers at `0x7FEA0000`. [Xbox] [xbox360/audio_xma_xaudio.md](../runtime/xbox360/audio_xma_xaudio.md) |
| **XMP** | the music player app the title talks to; must say "title has playback control". [Xbox] |
| **Z-prepass** | the depth-only pass before the G-buffer (no pixel shader). [Lux] |
