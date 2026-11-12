# Engine & port knowledge base — Skylanders: Spyro's Adventure (Xbox 360) → the next games

> **Purpose:** everything the SSA port knew on 2026-10-23 about the game, the Lux engine, the XDK libraries and our toolchain, with the evidence · **Status:** archive (the 2026-10-23 knowledge base; the split pages in this folder are current; section numbers are cited as "engine_notes §n") · **Tags:** [Lux] [XDK] [Xbox] [SSA]

## Summary

The findings still in force, each with the heading to grep (`## 6. ... D1`, `### 4.3 ...`); read that section, not the file.
Most sections moved to the pages in "Where each section lives now" below; this file keeps the evidence.
- Missing memory barriers broke the translated code on ARM64 (fences emitted for `lwsync`/`sync`/`eieio`, 1773 sites): `## 6.` D1.
- The "pink 3D" and "stencil EQUAL 2" symptoms were device-shadow lies, not rendering bugs: `### 3.5` and `## 6.` D2.
- RunCommandBuffer must run (menus draw through it); clears are draws: `### 4.4`, `## 6.` D3 and D4.
- Frame structure, scene traversal and NGL rendering habits a D3D replacement must honour: `### 3.4`, `### 3.5`, `### 3.6`.
- Shaders (archives, embedded, built at run time) and the disc data formats: `### 3.7`, `### 3.8`.
- Loading, zlib and WideLoad; languages; saves: `### 3.9`, `### 3.10`, `### 3.11`.
- Portal of Power driver and figure tags (title side): `### 3.12`, `### 3.13`.
- Threads and the job system; allocators and locks: `### 3.3`, `### 3.16`.
- D3D9LTCG device structure, dirty masks, draws, command buffers, resources, ring buffer: `### 4.1` to `### 4.6`.
- Kernel behaviour the title depends on, memory (physical windows, the +0x1000 offset), XAM/XAPI, vblank: `### 4.8` to `### 4.11`.
- XMA hardware decoder and its FFmpeg path; XAudio2 driver; Bink; Wwise; static zlib: `### 4.12` to `### 4.16`, `### 5.8`.
- Address tables (engine, D3D) for SSA: `### 3.17`, `### 4.7`.
- Runtime architecture, XenonRecomp config and missing instructions, function boundaries, shader translation patches:
  `### 5.1` to `### 5.4`; plume limits and the Option B renderer: `### 5.5`, `### 5.6`.
- Performance findings: `### 5.7`; platforms (Windows, UWP/Xbox, licensing): `### 5.9`; debugging aids: `### 5.10`.
- The porting playbook for a next game (now `docs/porting/problem_catalogue.md`): `## 2.`; identity card of the binary: `## 1.`
- All the traps, numbered D1 and up (the ones that cost days first): `## 6. (D) Weird stuff, traps and gotchas`.
- Open problems at the time of writing (2026-10-23, mostly closed since; see the game pages): `## 7.`; sources: `## 8.`
- Giants is not Lux: read every "next game" remark as "the next Lux game" (note at the top of this page).

**Giants is not Lux** (added 2026-10-26): its boot (2026-10-25) showed the Alchemy engine; none of the Lux hooks applied,
only the XDK-level ones. Read every "next game" remark below as "the next **Lux** game"; for Giants and the Alchemy
games see [../alchemy/README.md](../alchemy/README.md).

## Where each section lives now
| Section | Current page |
|---|---|
| §1 Identity card, §3.1 binary layout | [binary_layout.md](binary_layout.md); the disc: [games/ssa/docs/status.md](../../../games/ssa/docs/status.md) |
| §2 Porting playbook | [problem_catalogue.md](../../porting/problem_catalogue.md) |
| §3.2 boot, §3.11 saves, §3.15 online and profile | [boot_saves.md](boot_saves.md) |
| §3.3 threads and jobs, §3.16 allocators and locks | [README.md](README.md) ("Structure of the engine") |
| §3.4-3.5 the frame and NGL habits | [ngl_renderer.md](ngl_renderer.md), [frame_pacing.md](frame_pacing.md) |
| §3.6 a level frame | [level_frame_lightprepass.md](level_frame_lightprepass.md) |
| §3.7 shaders | [runtime/gpu/shaders.md](../../runtime/gpu/shaders.md) |
| §3.8-3.9 disc files, loading, zlib | [str_levels_loading.md](str_levels_loading.md), [zlib.md](zlib.md) |
| §3.10 languages | [languages.md](languages.md) |
| §3.12 portal driver | [portal_title_side.md](portal_title_side.md) |
| §3.13 figures | [runtime/host/figures.md](../../runtime/host/figures.md) |
| §3.14 audio, §4.15 Wwise | [wwise_audio.md](wwise_audio.md) |
| §3.17 address table | [ssa_addresses.md](ssa_addresses.md) |
| §4.1-4.7 D3D9LTCG | [d3d9ltcg_device.md](../../runtime/gpu/d3d9ltcg_device.md), [command_buffers.md](../../runtime/gpu/command_buffers.md) (§4.4), [edram_resolve_tiling.md](../../runtime/gpu/edram_resolve_tiling.md) (§4.3, §4.6), [xenos_registers_pm4.md](../../runtime/gpu/xenos_registers_pm4.md) (§4.6) |
| §4.8 kernel | [kernel_exports.md](../../runtime/xbox360/kernel_exports.md), [threads_sync.md](../../runtime/xbox360/threads_sync.md) |
| §4.9 memory | [memory.md](../../runtime/xbox360/memory.md) |
| §4.10 XAM and XAPI | [xam.md](../../runtime/xbox360/xam.md), [xam_content_saves.md](../../runtime/xbox360/xam_content_saves.md) |
| §4.11 video | [video_vblank.md](../../runtime/xbox360/video_vblank.md), [frame_pacing.md](frame_pacing.md) |
| §4.12-4.13 XAudio2, XMA; §5.8 XMA through FFmpeg | [audio_xma_xaudio.md](../../runtime/xbox360/audio_xma_xaudio.md) |
| §4.14 Bink | [bink.md](bink.md) |
| §4.16 zlib | [zlib.md](zlib.md) |
| §5.1 runtime architecture | [subsystems.md](../../architecture/subsystems.md), [layout.md](../../architecture/layout.md) |
| §5.2-5.3 XenonRecomp, function boundaries | [xenonrecomp.md](../../tools/toolchain/xenonrecomp.md) |
| §5.4 XenosRecomp | [xenosrecomp.md](../../tools/toolchain/xenosrecomp.md) |
| §5.5 plume | [plume_backends.md](../../runtime/gpu/plume_backends.md) |
| §5.6 the renderer, §5.7 performance | [runtime/gpu/README.md](../../runtime/gpu/README.md), [renderer_threads.md](../../runtime/gpu/renderer_threads.md), [ring_mirror.md](../../runtime/gpu/ring_mirror.md) |
| §5.9 platforms | [getting-started/platforms/](../../getting-started/platforms/README.md) |
| §5.10 debugging aids | [debugging.md](../../porting/debugging.md) |
| §6 traps D1-D42 | [gotchas.md](../../porting/gotchas.md) (D1-D44) |
| §7 open problems (all solved or moved since) | [games/ssa/docs/100.md](../../../games/ssa/docs/100.md), [render_bugs.md](../../runtime/gpu/render_bugs.md) |
| §8 sources | [Source index below](#8-where-each-fact-came-from-source-index) |

Everything the SSA port found about the game, its engine, the XDK libraries it links, our toolchain, and every odd
thing on the way, collected so that **Skylanders Giants, Swap Force, Trap Team, SuperChargers and Imaginators (Xbox 360)**
can be ported with the same method. One document, meant to be searched: grep an address (`0x822A1560`, `sub_822A1560`),
a register (`RB_STENCILREFMASK`, `0x210D`), an environment variable (`SKY_D3D_CAPTURE`) or a word (`stencil`, `zlib`).

Written 2026-10-23 from: `docs/*.md`, `games/ssa/config/SSA.toml`, `games/ssa/config/SSA_symbols.txt`, `patches/*.patch`,
the comments in `runtime/**`, `tools/`, the git log (77 commits up to 0abc6b5), every port note of the first port
(now [problem_catalogue.md](../../porting/problem_catalogue.md)), the frontend work then in progress, and the original notes. A
few facts were re-checked for this document with `python3 -I tools/ppcinfo.py` or a small script over
`private/image.bin` / `private/default.xex`; they say so.

## Conventions

- **[V]** verified: static analysis of the title's code, a runtime log/capture/dump, a unit test, or a measured run.
  The evidence follows in brackets (commit, documentation, measurement, tool).
- **[?]** unverified: inferred, assumed, seen once, or reported without proof. Treat as a lead, not a fact.
- **➜ Next game:** what to check first in Giants and later. The other games are by the same studio lineage (Toys for
  Bob, with XPEC / Vicarious Visions / Beenox on some ports) and were then assumed to share the engine family ("lux" + "NGL"; wrong
  for Giants, which is Alchemy: see the note at the top),
  but newer XDKs and engine revisions move addresses and structure offsets. **Never reuse an address blindly**:
  re-find it with the pattern / string / call-graph clue given here.
- Addresses are guest virtual addresses in `default.xex` (image base 0x82000000). `sub_XXXXXXXX` is the function at
  that address as XenonRecomp names it. `device+0xNN` is an offset into the D3D device structure. Register numbers
  are Xenos register indices (dword index, Xenia's `register_table.inc` names).
- Paths like `d3d/render.cpp` are under `runtime/`. "Doc X" means `docs/X`.
- Sections are split as follows: **(A)** the game and its engine (likely to carry over, addresses will not),
  **(B)** XDK libraries (very likely identical code in the other games, possibly another XDK build), **(C)** our
  toolchain and runtime (XenonRecomp / XenosRecomp / plume quirks and patches), **(D)** the weird stuff, traps and
  gotchas.

## Table of contents

1. [Identity card of the SSA binary](#1-identity-card-of-the-ssa-binary)
2. [Porting playbook for the next game](#2-porting-playbook-for-the-next-game)
3. [(A) The game and its engine (lux / NGL)](#3-a-the-game-and-its-engine-lux--ngl)
   - 3.1 [Binary layout and how to find your way](#31-binary-layout-and-how-to-find-your-way)
   - 3.2 [Boot, startup state machine, intro movies](#32-boot-startup-state-machine-intro-movies)
   - 3.3 [Threads and the job system](#33-threads-and-the-job-system)
   - 3.4 [The frame: nglListSend, ListSendModule, scene traversal](#34-the-frame-ngllistsend-listsendmodule-scene-traversal)
   - 3.5 [NGL rendering habits that matter for a D3D replacement](#35-ngl-rendering-habits-that-matter-for-a-d3d-replacement)
   - 3.6 [What a level frame looks like (level_027)](#36-what-a-level-frame-looks-like-level_027)
   - 3.7 [Shaders: archives, embedded and run-time-built shaders](#37-shaders-archives-embedded-and-run-time-built-shaders)
   - 3.8 [Disc files and data formats](#38-disc-files-and-data-formats)
   - 3.9 [Loading, zlib and WideLoad](#39-loading-zlib-and-wideload)
   - 3.10 [Languages](#310-languages)
   - 3.11 [Saves](#311-saves)
   - 3.12 [Portal of Power driver (title side)](#312-portal-of-power-driver-title-side)
   - 3.13 [Figures (tags)](#313-figures-tags)
   - 3.14 [Audio as the game uses it (Wwise, XAudio2, XMP)](#314-audio-as-the-game-uses-it-wwise-xaudio2-xmp)
   - 3.15 [Online, achievements, profile](#315-online-achievements-profile)
   - 3.16 [Memory allocators and locks](#316-memory-allocators-and-locks)
   - 3.17 [Engine function and data address table](#317-engine-function-and-data-address-table)
4. [(B) XDK libraries](#4-b-xdk-libraries)
   - 4.1 [Direct3D 9 (D3D9LTCG): the device structure](#41-direct3d-9-d3d9ltcg-the-device-structure)
   - 4.2 [D3D: dirty masks and register groups](#42-d3d-dirty-masks-and-register-groups)
   - 4.3 [D3D: draws, BeginVertices, clears, resolves, swap](#43-d3d-draws-beginvertices-clears-resolves-swap)
   - 4.4 [D3D: command buffers (Begin/End/RunCommandBuffer)](#44-d3d-command-buffers-beginendruncommandbuffer)
   - 4.5 [D3D: resources, textures, vertex formats, shaders](#45-d3d-resources-textures-vertex-formats-shaders)
   - 4.6 [D3D: ring buffer, fences, interrupts, tiling, predication](#46-d3d-ring-buffer-fences-interrupts-tiling-predication)
   - 4.7 [D3D function address table](#47-d3d-function-address-table)
   - 4.8 [Kernel (xboxkrnl) behaviour the title depends on](#48-kernel-xboxkrnl-behaviour-the-title-depends-on)
   - 4.9 [Memory: virtual heaps, physical windows, the +0x1000 offset](#49-memory-virtual-heaps-physical-windows-the-0x1000-offset)
   - 4.10 [XAM and XAPI](#410-xam-and-xapi)
   - 4.11 [Video: vertical blank, present interval](#411-video-vertical-blank-present-interval)
   - 4.12 [XAudio2 render driver](#412-xaudio2-render-driver)
   - 4.13 [XMA hardware decoder](#413-xma-hardware-decoder)
   - 4.14 [Bink](#414-bink)
   - 4.15 [Wwise](#415-wwise)
   - 4.16 [Statically linked zlib](#416-statically-linked-zlib)
5. [(C) Our toolchain and runtime](#5-c-our-toolchain-and-runtime)
   - 5.1 [Runtime architecture in one page](#51-runtime-architecture-in-one-page)
   - 5.2 [XenonRecomp: config, patches, missing instructions](#52-xenonrecomp-config-patches-missing-instructions)
   - 5.3 [Function boundaries: find_switch_tables.py and packed leaves](#53-function-boundaries-find_switch_tablespy-and-packed-leaves)
   - 5.4 [XenosRecomp: shader translation patches](#54-xenosrecomp-shader-translation-patches)
   - 5.5 [plume (RHI) patches and limits](#55-plume-rhi-patches-and-limits)
   - 5.6 [The Option B renderer (d3d/ + gpu/)](#56-the-option-b-renderer-d3d--gpu)
   - 5.7 [Performance findings](#57-performance-findings)
   - 5.8 [XMA through FFmpeg](#58-xma-through-ffmpeg)
   - 5.9 [Platforms: Windows, UWP / Xbox, licensing](#59-platforms-windows-uwp--xbox-licensing)
   - 5.10 [Debugging aids (environment variables, tools)](#510-debugging-aids-environment-variables-tools)
6. [(D) Weird stuff, traps and gotchas](#6-d-weird-stuff-traps-and-gotchas)
7. [Open problems at the time of writing](#7-open-problems-at-the-time-of-writing)
8. [Where each fact came from (source index)](#8-where-each-fact-came-from-source-index)

---

## 1. Identity card of the SSA binary

| Item | Value | Status |
|---|---|---|
| Title | Skylanders: Spyro's Adventure, Xbox 360, 2011, Toys for Bob (+ XPEC logo in the intro) | [V] intro movies |
| Title ID / media ID / version | `415608AB` / `53CE7B97` / 0.0.0.2 | [V] `host/launcher_games.cpp` |
| Disc | Redump "USA, Europe (En,Fr,De,Es,It,Nl,Sv,No,Da,Fi)", XGD3, 8 738 846 720 bytes, 1142 files (8.10 GB of files) | [V] |
| `default.xex` | 12 595 200 bytes, SHA-256 `e195f5130c1e23fb4654baf7c383ef97d02c89aeb7d4389705ae31235646ef7b` | [V] |
| XDK libraries | XAPILIB, D3DX9, XGRAPHC, XBOXKRNL, XNET, XONLINE, XMP, XAPOBA, XAUDIO2, XPARTY, D3D9LTCG, XMCOREL **2.0.20500.0** (XHV2 2.0.20500.1); LIBCMT 2.0.11886.0; LINK 10.0.11886, C1/C2 16.0.11886 | [V] XEX static-library header 0x200FF parsed for this document |
| Image | base 0x82000000, size 0xD40000, entry 0x822DF400 | [V] `private/xexdump.txt` |
| Engine / renderer | "lux" (C++ namespace `lux::`), renderer "NGL" (`ngl*` classes and functions), RAD Bink, Audiokinetic Wwise | [V] strings, RTTI |
| XDBF (SPA) title name / icon | "Skylanders", 64×64 PNG | [V] `ReadTitleMetadata()` |
| Rendering resolution | 3D scene at **1120x704**, UI/composite at 1280x720 | [V] level_027 capture |
| Frame rate | 30 fps (present interval 2 on a 60 Hz vblank) | [V] 60 swaps / 2.00 s |
| Languages on disc | chs cht dut euk eng fre ger ita jap kor spa swe fin nor dan in the code table; disc has 10 (dut eng fre ger ita spa swe fin nor dan) | [V] `sku.lxb`, `language.cpp` |
| Recompiled code | ~160 MB of generated C++ (`ppc/`), 123 objects, 928 s CPU / 104 s wall on a 10-core M1 Pro | [V] doc launcher.md |
| Shaders | 1119 unique containers (600+17 VS, 419+57 PS in the archives, 81 embedded in the xex, plus run-time-built Bink shaders); 1118 of 1122 translate | [V] |

Sections of the image [V] (`private/xexdump.txt`):

| Section | Address | Size | Notes |
|---|---|---|---|
| .rdata | 0x82001000 | 0xD2DDC | read-only data |
| .pdata | 0x820D4000 | 0x2BC10 | function table XenonRecomp uses; **leaf/packed functions missing from it** (section 5.3) |
| BINKCONS, RADCONSTD | 0x82100000, 0x82103000 | | Bink constants |
| .text | 0x82110000 | 0x8D69A4 | game + statically linked XDK |
| BINK, RADCODE | 0x829E7000, 0x829FB000 | 0x13108, 0xAAC | Bink code (BinkDoFrame 0x829EDCF8 ...) |
| BINKBSS | 0x82A00000 | | |
| .data | 0x82A06000 | 0x29E358 | globals (device pointer 0x82BB4F94, ...) |
| .XBMOVIE, BINKDATA, RADDATA | 0x82CA5000.. | | |
| .tls | 0x82CAA000 | 0x1411 | `__declspec(thread)` template |
| .idata | 0x82CB0000 | 0x410 | 218 imports (`private/imports.txt`, `kernel/import_list.inc`) |
| .XBLD, .reloc | 0x82CC0000, 0x82CC1000 | | |

➜ **Next game:** run `tools/xexdump` first and compare the library versions line by line. If D3D9LTCG is not
2.0.20500, expect the D3D device layout above +0x2780 to move (it already differs from Unleashed Recompiled's newer
XDK past ~0x2780, section 4.1) and re-derive every offset in `d3d/device.h` from the setters.

---

## 2. Porting playbook for the next game

The order that worked for SSA, with the traps that cost the most time. Each step links to the detail.

1. **Get the image.** `tools/xexdump` → `private/default.xex` (decrypted), `private/image.bin` (loaded image),
   `private/xexdump.txt` (sections, imports). Check the XEX static-library versions (section 1). XenonUtils' XEX
   loader needed a fix: blocks can sum to less than the declared image size (section 5.2) [V].
2. **XenonRecomp config.** Find the save/restore helpers (`savegprlr_14` ...) and setjmp/longjmp by byte pattern
   (XenonRecomp README); add `invalid_instructions` for the EH data words that sit in front of `__CxxFrameHandler` /
   `__C_specific_handler` functions (section 5.2). Run `tools/find_switch_tables.py` to generate the switch tables
   **and** the extra `functions` (jump-table leaves, adapters, vtable targets, **packed leaf functions**).
3. **Apply `patches/xenonrecomp.patch` before the first real run.** Without the memory fences (lwsync/sync/eieio)
   the game's spin locks corrupt its heap on ARM64 about once in 10-94 runs (section 5.2, gotcha D1). Without the
   missing instructions you get `__builtin_debugtrap` or wrong results.
4. **Boot to the first movie** with the kernel as in section 4.8: KeTimeStampBundle must tick, Ke* objects bound
   through their dispatcher header, real PulseEvent, user APCs for ReadFileEx completions, zero-count waits must
   fail, yields must back off. Expect to need the two NGL "empty scene" patches (`engines/lux/patches.cpp`, section 3.4).
5. **Audio:** XAudio2 render driver (one driver thread calling every client, pointer to the context cell), then XMA
   (registers at 0x7FEA0000, hook the 6 register writers), then the XMP "title has playback control" answer or the
   music bus stays muted (gotcha D9).
6. **Title screen:** XConfig (language, country, AV pack, region, dashboard version), offline XNet (XNADDR_NONE),
   XamNotifyCreateListener handle, device selector + XamContent (saves), RtlTimeToTimeFields, achievements
   enumerator. Every weak stub that "succeeds" without writing its outputs is a trap (gotcha D5).
7. **Renderer ("Option B"):** find the D3D device global (from `Direct3D_CreateDevice`'s caller in the NGL init),
   the draw functions, Resolve, ClearF's worker, Swap, Begin/End/RunCommandBuffer, Create*Shader. Read state from the
   device structure at draw time. Then look for **game code that writes PM4 straight into the ring** (NGL does, and
   that is where "pink/violet 3D" and "stencil never 2" came from; section 3.5).
8. **Portal:** look for `XexGetProcedureAddress` of xam ordinals 0x4A1/0x4A2 (section 3.12). Later games add portal
   commands (lights, speaker: J, L, M, V) and a new portal model; the HID VID/PID stays 1430:0150.
9. **Performance:** pacing (present interval), buffer cache aware of the vertex ring, pipeline precompile list, host
   zlib (section 5.7).

Things worth grepping for first in a new title [V] (for SSA, ➜ expected in the others):

| Look for | Why | SSA hit |
|---|---|---|
| string `nglListSend called while one or more scenes were still active` | finds `nglListSend` → ListSendModule → scene traversal | 0x829076BC in `nglListSend` 0x829075F8 |
| RTTI names `.?AV...@lux@@` | 1087 RTTI type names, 762 in namespace `lux`, 65 `ltl2`, 41 `XAI`, `AK` (Wwise), `ngl*` classes (`nglFXShader`, `nglGeometryShader`, `nglFXSasRenderer`, ...) and `aps*` UI nodes | [V] regex over `private/image.bin` |
| `lis rX, 0x82BB / ... 0x4F94`-style accesses to one global by hundreds of functions | the D3D device pointer | 0x82BB4F94 |
| calls to `VdSwap`, `VdInitializeRingBuffer`, `VdSetGraphicsInterruptCallback` | D3DDevice_Swap, InitializeRingBuffer, InitializeEngines (single callers) | section 4.7 |
| byte-reversed stores (`stwbrx`) into 0x7FEA0000+ | XMA register writers | section 4.13 |
| `" Copyright (C) 2010 Activision. All Rights Reserved. "` | figure AES key derivation | 0x8264398C |
| strings `STATE_GAME_FINALCHECKS`, `WAIT_FOR_START_BUTTON` | startup state machine table | object 0x82C91EE8 |
| `sku.lxb`, `text/%s/*.lxb`, `movies/%s/legal.bik` | language selection | `sub_82808190` |
| `inflateInit_` / `inflate` signatures (zlib 1.2.x) | replace with host zlib | 0x825F23B8 / 0x825F24F8 / 0x825F3B80 |
| `\0ASL` archives `shaders.lsa`, `effects.lsa` | shader cache input | section 3.7 |

---

## 3. (A) The game and its engine (lux / NGL)

### 3.1 Binary layout and how to find your way

- **No symbols.** The project rule is never to use the leaked Aug 2011 prototype PDBs (Hidden Palace). Names in
  `games/ssa/config/SSA_symbols.txt` come from evidence (strings, call graphs, register writes); `?` marks uncertain ones. [V]
- **MSVC RTTI is present**: 1087 `.?AV`/`.?AU` type names (762 `lux`, 186 global, 65 `ltl2` — the engine's template
  library —, 41 `XAI`, 5 `GameStats`, 4 `AK`). Global-namespace classes include the NGL scene nodes
  (`nglDebugLineNode`, `nglDummyRenderNode`, `nglStringNode`, `nglMorphGeometryShaderNode`, `nglFXShaderNode`,
  `nglEmptyShader`) and the UI (`apsRectangleNode`, `apsColorUVARectangleNode`, `apsUVARectangleRenderer`,
  `apsAngleTrackElementYAction`), plus `bdSingletonRegistryImpl`. [V] (regex over `private/image.bin` for this doc)
  ➜ Next game: RTTI → vtables → virtual functions is the fastest way to name engine code; the `.?AV` scan is a
  10-line script.
- Strings such as `lux::AsyncFile` (referenced from `sub_821583A8`) name engine subsystems. [V] (ppcinfo find-string)
- The engine's debug log functions are compiled in but silent in retail; e.g. the portal driver logs through
  `sub_826434D8` (printf-like, integer args in r4..r10), which `kernel/portal.cpp` echoes with `SKY_PORTAL_LOG=1`. [V]
  ➜ Next game: hooking the engine's printf-like loggers gives free tracing (`[Portal] Decrypted:` lines were the
  proof that our figure crypto was right).
- Tools: `python3 -I tools/ppcinfo.py dis|asm|callers|callees|strings|find-string <hex>` (names from
  `SSA_symbols.txt`); `ppc/ppc_recomp.*.cpp` carry each PowerPC instruction as a comment above its C++, so grep works
  on the generated code too. [V]

### 3.2 Boot, startup state machine, intro movies

- Entry 0x822DF400 → XAPI startup → game. `GameMaster_IntroMovies` (0x827F5E80) is a main-thread loop over the four
  intro movies: `movies\<lang>\legal.bik` (localized, 4.95 s), `atvilogo.bik` (10.38 s), `xpeclogo.bik` (8.51 s),
  `tfb.bik` (10.0 s) [V] (Bink headers). `Movie_Update` (0x82852D90): BinkWait/BinkDoFrame/BinkNextFrame, done when
  FrameNum == Frames. A/START skips all but the legal screen (title at ~20 s). Before vblank pacing (0b74438) the
  movies ran at 6-7 game frames per second and the title came at ~65 s; paced, the movies last their Bink length and
  the title is reached at ~36 s. [V]
- **Startup state machine** [V] (runtime log): constructor `sub_82800048` (object 0x82C91EE8), table of
  16-byte entries `{handler, 0, id, name}`, current state at **+0x1250**, previous at +0x125C. Observed order:
  IDLE(0) → WAIT_FOR_START_BUTTON(5) → START → GAME_STARTING(7) → INIT_PROFILE(0xE) → CHECK_FOR_STORAGE(0xF,
  XamShowDeviceSelectorUI) → LOAD_GLOBAL_GAME_DATA_PROGRESS(0x10) → GAME_INIT_SYSTEMS(0x13) → **GAME_FINALCHECKS(0x14)**
  → GAME_INITIALIZED(0xC) → (A) → GAME_SAVE(8) → story movie `movies\sl_sc01.bik`.
- **STATE_GAME_FINALCHECKS lasted ~22 s** with stubbed imports. [?] Root cause never isolated; it disappeared after
  the console-configuration + offline-network commit (4ee1adc: XNetGetTitleXnAddr had stayed "pending" forever,
  XConfig/XamGetSystemVersion outputs were unwritten). ➜ Next game: if a startup state hangs, suspect an async XAM/XNet
  call left pending or a stub with unwritten outputs first.
- After START: autosave notice → "CHOOSE PLAY" menu → Skyland Adventures → Start a New Game → slot → story movie
  `sl_sc01.bik` (62.3 s, skippable) → first level **level_027** (`levels\level_027\mission.str`). [V scripted run:
  `SKY_PRESS="start:40,a:45,a:57,a:62,a:67,a:72,b:150,b:160"`]
- Two crashes on the way were engine assumptions about scenes (section 3.4) and one about zero WideLoad events
  (section 3.9).

### 3.3 Threads and the job system

- Threads seen [V] (profiles, logs and the names the title gives through the debugger SetThreadName exception): the
  main thread ("Game Master" in the profiles), **"JQ Worker"** job threads (`JobWorker_Loop` 0x82635CF8), the render job
  **ListSendModule**, LOD workers, the portal thread (`sub_8264ABB8`), XAudio2's/Wwise's threads (fed by the kernel
  audio driver), XMA work in the Wwise thread.
- `JobGroup_Wait` (0x826360F8) runs pending jobs on the caller while the group counters are non-zero. [V]
- **Idle waiting is polling:** the job system polls its queues with `NtYieldExecution` (`sub_82635A80` →
  `sub_822DCD18`); LOD workers and spin-lock waits use zero-length `KeDelayExecutionThread`; `sub_82520638` also
  spins (audio?). On the console this burns a hardware thread the title owns; on a PC the main thread spent 91% of
  its samples in `sched_yield`. Fix: back-off after 32 yields <100 µs apart (200 µs sleeps) (c433233). [V]
- `RecursiveSpinLock_Acquire` (0x8213EFB0): owner = GetCurrentThreadId, `ldarx/stdcx.` on `*(lock+12)`. Locks are
  released with `lwsync; stw 0` — see gotcha D1. [V]
- `GetCurrentThreadId` (0x822DCD50) reads KTHREAD+0x14C; `GetTickCount` (0x822DF0D0) reads
  KeTimeStampBundle->TickCount directly. [V]
- The thread naming exception (MSVC THREADNAME_INFO `{0x1000, name, id, flags}`) is the only exception the title
  raises. [V] (`kernel/threads.cpp`)

### 3.4 The frame: nglListSend, ListSendModule, scene traversal

[V] (doc d3d_map.md, original notes)

- Game side builds scenes with `nglListBeginScene` (0x82633FD8: allocates a scene, links it under the current one
  0x82BB4F44 or as next root 0x82BB4F48), `nglInitScene` (0x82633E78: clears it; render list at **scene+0x348** = 0),
  `nglListEndScene`, then `nglListSend` (0x829075F8): waits for the previous ListSendModule, root = next root, queues
  the job.
- **ListSendModule** (0x826320C8), a job: `SetShaderGPRAllocation` (32 VS / 96 PS GPRs) → invalidate the NGL state
  cache (0x82BA4F70) → `InsertCallback` → **nglSceneTraverse** (0x82631098, recursive, current scene in 0x82BB4F4C:
  Begin/EndTiling, ZPass, draws) → `InsertCallback` (callback 0x826319B8) → fence → `0x822A9088` (FrameEnd?, maps the
  present interval) → `nglPresent` (0x8262D690): `Swap(dev, frontBufferTex + 0x14)` with double-buffered front-buffer
  textures 0x82C54698 / 0x82C54720.
- `nglRenderListSubmit` (0x822895E0) submits `scene+0x348`; sentinel 0x82B5FAAC.
- **Two engine assumptions break when a frame has nothing to draw** (seen while the first movie starts, and while a
  movie has no picture ready): a frame submitted with no scene makes the traversal dereference 0x82BB4F4C = 0; a
  scene whose render list is still null crashes `nglRenderListSubmit`. Both are skipped in `engines/lux/patches.cpp` (the only
  two game-code patches). [V] ➜ Next game: same two guards, at the re-found addresses.
- The title asks for **present interval 2** (NGL, `sub_82631EA8`): 30 fps. [V] (60 swaps/2.00 s)
- **Game time is variable-step** [V] (frame-time measurements; section 3.4): `sub_827F6628` (each frame, from `sub_827F6850`) turns the `mftb`
  time since the previous frame into 60ths of a second and smooths it, `F += 0.3 * (dt*60 - F)`, clamped to `]0, 3]`
  (max 1/20 s a frame; below 20 fps the game slows down); F in 0x82B62CA4 and `clock+196`, clock object 0x82C7CD20
  (getter `sub_82856AA8`); game delta = `F * clock+188 * clock+184` (`sub_821491F8`, `clock+148` = paused). Scene and
  shader time count **vblanks** (callback `sub_82631968` → 0x82BB4FD8, snapshot 0x82BB4FCC by `sub_82631A20`, scene
  +1016 by `sub_82630B78`). So presenting at every vblank (60 fps) keeps the game's speed.
- `nglInit` (0x826324D0): `Direct3D_CreateDevice(1, 1, &pp 0x82B94E98, &0x82BB4F94)`. [V]
- `nglResolveTiles` (0x8262D540): per tile `SetPredication(3 << 2i)` + Resolve. Predicated tiling IS used by the
  frontend path; level_027 used no tiling, no ZPass and no tessellated draws. [V] (probe)
- One fault seen once in ~25 runs during `tfb.bik`: guest address 0x24 in `sub_822AE460` ← `sub_82630B78` ←
  `sub_82631098` (scene traversal). Not reproduced in 8 dedicated runs. [?]

### 3.5 NGL rendering habits that matter for a D3D replacement

These are the engine's, not the XDK's: they are why a "read the D3D shadows at draw time" renderer is not enough.

1. **Setters are inlined into game code** [V] (d3d_map.md): RB_DEPTHCONTROL written directly by 80 functions,
   PA_SU_SC_MODE_CNTL 82, RB_COLORCONTROL 40, the render-state cache +0x2F98 60, VS/PS constants (indexed), fetch
   constants +0x480 (8), the vertex declaration +0x2F58 (4, `SetVertexDeclaration` is fully inlined). Harmless if the
   state is read from the device at draw time.
2. **Direct PM4 ring writes bypass the shadows** — the cause of the "pink/violet 3D" and of the level "stencil == 2"
   blocker:
   - `ngl_WriteStencilRefDirect` (0x8213BBB8, light rendering): a type-0 write of RB_STENCILREFMASK (0x210D):
     reference 0, read mask = material byte `material[3]`, write mask 0xFF. Stays in effect until D3D re-sends its
     shadow at a draw where the register is dirty (device+0x10 bit 28) or a command buffer sends it. [V] (d7ac9c3)
   - The material state blocks built by `sub_828FC670` (allocates from the small-block pool) and emitted by
     `sub_828FCBA8` (called from the mesh run-state setter `sub_8285F700`): a prepared packet of RB_DEPTHCONTROL
     (0x2200), RB_BLENDCONTROL0 (0x2201), RB_COLORCONTROL (0x2202, alpha test function), PA_SU_SC_MODE_CNTL (0x2205),
     RB_COLOR_MASK (0x2104), RB_BLEND_ALPHA (0x2108), RB_STENCILREFMASK (0x210D), RB_ALPHA_REF (0x210E); the packet is
     the last 16 words written. [V levelgfx work (second round) + frontend WIP; the FC670→FCBA8 relation is [?] from the call
     graph: FCBA8's only caller is 0x8285F700, FC670's is 0x8285D6A8]
   - D3D itself writes the **shader literal constants** (VS c253-c255, PS c254-c255) only into the ring. [V] (levelgfx)
   - Others found in the D3D reverse engineering: 0x82893F10 (`ngl_FlushHiZDirect?`: EVENT_WRITE 0xF + RB_HIZCONTROL shadow
     bit), 0x82629010 (bin mask), 0x822891F0 (InsertCallback), ListSendModule, and D3D's own
     `WriteWindowScissor` 0x822974C0 (PA_SC_WINDOW_OFFSET/SCISSOR 0x2080-0x2082, per tile when tiling). A full scan was
     never finished (`private/re/cbwrite.py`, false positives). [V] (partial)
   - Level_027 truth read from the ring (levelgfx work, second round): Z-prepass RB_DEPTHCONTROL 0x8767 (stencil REPLACE,
     ref 1), main pass 0x766/0x762 (no stencil test), G-buffer 0x726. The "stencil EQUAL 2" the shadows show is never
     what the GPU runs. [V] (ring log)
   ➜ Next game: before chasing any "wrong blend/depth/stencil" bug, diff shadow state against a ring mirror
   (`d3d/ring.cpp` on the levelgfx branch, 87c0f6a, WIP) for the draw. Scan for game functions that advance
   device+0x30 (the ring cursor) themselves.
3. **Command buffers per mesh/material** [V] (command_buffers.md): `sub_828519B0` looks the combination up in a
   **64-entry cache at 0x82AA7BC0** (buffers at +0x30; refuses to evict an entry used in the last 10 frames [?]),
   records on a miss with `sub_82850F90` (BeginCommandBuffer, SetDepthStencilSurface, the material's VS/PS bool and
   float constants, VS and PS, sampler states, textures, stream source, indices, one DrawIndexedVertices), then
   EndCommandBuffer. `sub_8285C540` (virtual mesh draw) sets the run state with `sub_8285F700` (shaders, render states,
   transforms) and calls RunCommandBuffer; without a cached buffer it draws directly (`sub_8214AE58`). Menus run 5-14
   buffers per frame, levels many more (~84 replays in the level Z-prepass). [V] (0x82AA7BC0 re-checked with ppcinfo:
   `lis r10,0x82AA; addi r31,r10,0x7BC0` in sub_828519B0.)
4. **Inherit tags** [V]: the engine sets them through `sub_82850C48` (OR), `sub_82850C90` (ANDC), `sub_82850CD0` /
   `sub_82850CE8` into the global defaults at **0x82BB46D0** (5 × u64; re-checked: `sub_82850C48` is
   `ldx/or/stdx` at 0x82BB46D0 + 8·r3). Runtime values: VS FFFFFFFFFFFFFFFC, PS FFFFFFFFFFFFFFFC, regs
   03FFFFFFFFE1FFF7, fetch 007FFFFCFFFFFFC0, misc 01FFFFFFFFFFFFFF — nearly everything is inherited except c248-c255,
   the shaders and vertex fetch slots 26-31. **Before the replay was implemented, RunCommandBuffer was a no-op and
   the menus had no backgrounds.**
5. **After a run, inherited groups the buffer wrote stay on the GPU** with the buffer's values (the shadows lack
   them) until the game sets them again. [V] (frontend WIP analysis, uncommitted) Combined with item 2, the "GPU
   state" is shadows ∪ last replayed buffer ∪ direct ring writes.
6. **Vertex streaming ring**: dynamic vertices go through a **2.56 MB ring buffer at 0xAA5E0000**; each draw's fetch
   constant points at the write position with size = **the rest of the ring**, so a cache keyed on (address, size)
   explodes (~290 MTLBuffers/frame, ~2 MB converted each). Key on the range a draw actually reads. [V] (perf2)
7. **Reversed depth**: GEQUAL, far = 0; the scene depth is cleared to 0, shadow maps to 1. [V]
8. **Depth-only passes have no pixel shader** (must not be skipped). [V] (d6c98bf)
9. **EDRAM aliasing tricks**: in level_027 a k_8_8_8_8 colour target at EDRAM tile 0 aliases the depth buffer with
   write mask R only and a copy shader (`xex_82092DB8.ps`) sampling the depth resolve: on hardware this rewrites the
   **stencil byte** of the depth buffer; then 560x352 **4×MSAA** depth-only draws on the same EDRAM base (Hi-Stencil).
   [V capture; whether it matters once the ring state is right is [?]]
10. **Point sprites** (particles): 4 vertex shaders export point size (export 63); PS gets the point coordinate when
    SQ_PROGRAM_CNTL bit 18 (param_gen) is set, in register SQ_CONTEXT_MISC.param_gen_pos (bits 8-15). [V] (d6c98bf)
11. **16-bit texcoord pairs**: vertex data byte-swapped as 32-bit words swaps the halves of 16-bit pairs;
    texcoords must be swapped back (`g_SwappedTexcoords`). [V]
12. **Normals packed as DEC3N** (three signed normalized 10-bit components, X in the low bits), handed to shaders as a
    raw uint. [V] (xenosrecomp.patch)
13. **Viewport transform disabled** (PA_CL_VTE_CNTL bit 0 = 0) for screen-space draws: shaders output pixel
    positions; the host converts with `g_PositionScale` (2/W, −2/H) and draws over the whole target. [V]
14. **Rectangle lists and quad lists** are used (UI): rectangles give 3 corners (4th = v1 + v2 − v0); quads become two
    triangles. [V]
15. `ngl_DrawQuadUP` (0x8228B3E8): inline SetVertexDeclaration (+0x2F58) + BeginVertices(QUADLIST);
    `ngl_UnbindStream` (0x82289248). BeginVertices draws must be **snapshotted at call time** (state may change before
    the game fills the buffer). [V]

### 3.6 What a level frame looks like (level_027)

[V] (levelgfx: `SKY_D3D_CAPTURE` + `SKY_DUMP_FRAME`) EDRAM at 1120x704: depth D24FS8 at tile 0x000, RT0 at 0x268,
RT1 at 0x4D0 (0x268 tiles each). ~1780-2900 draws per frame.

1. 256²/512² k_2_10_10_10 passes (water ripples), resolved.
2. Z-prepass (~836 host draws, ~84 command-buffer replays), depth only; depth resolved (flags 4) into k_24_8_FLOAT at
   physical 0x06A60000 (`DepthMap`).
3. Two 1024² shadow maps (D24S8), cleared to 1, resolved (flags 0x14) into k_24_8 (`DynamicShadowMap`).
4. G-buffer: MRT RT0 k_2_10_10_10 (normals) + RT1 k_8_8_8_8 (resolved as k_8 = `GBuffer2`); depth GEQUAL/EQUAL; the
   terrain uses depth EQUAL against the Z-prepass (needs bit-identical positions: gotcha D16).
5. Half-res depth + SSAO (560x352), resolves to k_8_8.
6. 14 rect draws rewriting the depth buffer's stencil byte through an aliased colour target (section 3.5 item 9).
7. 560x352 4×MSAA depth-only draws, stencil EQUAL/REPLACE.
8. Light pre-pass: MRT of two k_2_10_10_10 lighting buffers, additive (ONE, ONE), stencil from
   ngl_WriteStencilRefDirect; ambient full-screen pass.
9. Main colour pass: ~670 draws with world shader `0BA301663D16E692` (Diffuse/SpecularLightingBuffer,
   DynamicShadowMap PCF, fog, IrisParams), ~550 more; k_8_8_8_8.
10. Bloom / luminance (auto-exposure) chain **288x176 → … → 4x4**, a **colour-grading LUT 1024x32** generated in the
    frame, final composite `LUT(saturate(scene + bloom·k))` at 1280x720, then UI.

Shader constant names seen (from the HLSL of the translated shaders): `DepthMap`, `DynamicShadowMap`, `GBuffer`,
`GBuffer2`, `DiffuseLightingBuffer`, `SpecularLightingBuffer`, `VposParams`, `IrisParams`. [V]
➜ Next game: the later titles likely keep this light pre-pass pipeline; capture one level frame first and write this
list again before touching the renderer.

### 3.7 Shaders: archives, embedded and run-time-built shaders

- `shaders.lsa` (600 VS + 419 PS) and `effects.lsa` (17 VS + 57 PS) at the disc root. Format `\0ASL` v2
  (`tools/shaders/lsa.py` docstring): 0x40 header; 32-byte shader entries {keyCount, keyOffset, type (0x1909BEB3 VS,
  0x88E6E692 PS — name hashes), size, dataOffset}; each entry points at a **standard D3D shader container** (magic
  0x102A1101 VS / 0x102A1100 PS) passed to D3D unchanged; 32-byte "program" entries (group/name hashes + 3 lists:
  techniques/permutations, constant/sampler bindings) not decoded. [V]
- LSA loader 0x828FE7B0 (called from 0x828574D8); lux shader constructor 0x8296E750 calls
  `D3DXGetShaderConstantTable` 0x822E5B88; lux wrappers of CreateVertexShader / CreatePixelShader: 0x8213AEB8 /
  0x8213A958. [V]
- **81 more containers embedded in default.xex** (0x8208DD18..0x820A1C58), pointer table 0x82A42D74 used by
  `sub_828A5E48`. Two of them (0x82095BF0 VS, 0x82095D90 PS) have **no constant table**. [V]
- **Bink's movie shaders are built at run time** (not in any archive): run once with `SKY_DUMP_SHADERS=<dir>` and feed
  the dump to `tools/shaders/build_cache.sh`. [V]
- The xex links the runtime microcode assembler (0x82291448-0x82296028, context stride 0x26D0) and the D3DX HLSL
  compiler "2.0.20500.0" (0x822F…-0x823F…). Whether the game compiles HLSL at run time besides Bink: [?] (only the
  Bink ones were seen).
- Microcode census (`tools/shaders/ucode.py`): only 4 VS output point size, 2 PS use SetTextureLod / useRegLod, no
  shader uses CondExec(bool), CondCall or memexport; CondExecPred clauses contain only predicated instructions. [V]
- Vertex fetch formats inside the containers are 0: D3D patches them at bind time. [V]
➜ Next game: same `.lsa` reader probably works (check magic/version); re-run the census; expect more run-time-built
shaders and check `SKY_DUMP_SHADERS` output against the archives.

### 3.8 Disc files and data formats

| File / format | What we know | Status |
|---|---|---|
| `sku.lxb` | read by `sub_82808190`: "languages" bitmask 0x7CF4 (dut eng fre ger ita spa swe fin nor dan allowed), "default" 0x10 (eng) | [V] |
| `text\<lang>\*.lxb` (`global.lxb`...), `ui\<lang>\uituning.lxb` | localized text/UI | [V] (file trace) |
| `movies\<lang>\legal.bik`, `movies\*.bik` | Bink 1 movies; `sl_sc01.bik` has **15 audio tracks**: 0 English, 1-5 music/effects, 6 fre, 7 dan, 8 dut, 9 fin, 10 ger, 11 ita, 12 nor, 13 spa, 14 swe | [V] |
| `audio\init.bnk`, `global.bnk`, `audio\<lang>\voice.bnk`, `audio\<lang>\levels\*.bnk` | Wwise sound banks; voice banks differ per language (BKHD language id 0x0C en-US, 0x0F fr-FR) | [V] |
| `audio\streaming.pck` | Wwise file package; 2055 streamed voice lines per language, keyed by the language codes; also streamed music (`.wem` id 0x0C45FC64 = title music) | [V] |
| `levels\level_027\mission.str` (43 MB → 85 MB), frontend `mission.str`, `screenmask.apk` | level/screen packages, **zlib streams** inflated 16 KB at a time | [V] (perf2) |
| `uigameimage.str` | `strm`, BE32 file size, BE32 1, BE32 1, BE32 record count (0x2E1), then one zlib stream (36 462 756 bytes). Records: BE32 type hash, BE32 name hash, BE32 0, BE32 size, body. Image bodies are `APKF` packages: chunk `\0XET` (name 0x30 bytes in, D3D texture header with the fetch constant after `FFFF0000 FFFF0000`), chunk `SYHP` (data offset +8 = 0x1000, size +16; the parser skips a package whose SYHP ends past the body, 0.9.2, `art_test.cpp` "toy_short_hud"). `toy_*_hud` = 37 portraits 128x128 DXT5 (format 20), tiled, 8-in-16 swapped | [V] (toybox) |
| `shaders.lsa`, `effects.lsa` | section 3.7 | [V] |
| `nxeart` | dashboard art (could give launcher visuals) | [?] |
| XEX resource named after the title ID | XDBF/SPA: title name (string 0x8000) "Skylanders", icon (image 0x8000) 64×64 PNG | [V] |

Disc names the title uses: `game:`, `d:`, `\Device\Cdrom0` and symbolic links — all the same disc. Save content is
mounted as **`lux:`** (e.g. `lux:\GLOBAL.DAT`). [V] (`kernel/files.cpp`)

### 3.9 Loading, zlib and WideLoad

- The loader inflates `mission.str` and the other zlib streams 16 KB at a time **on the game thread** with the
  statically linked zlib 1.2.x: `inflateInit_` 0x825F23B8 (tail call to `inflateInit2_` 0x825F2298), `inflate`
  0x825F24F8, `inflate_fast` 0x825F3C18, `inflateEnd` 0x825F3B80. Translated it ran ~30 MB/s: **86% of the level_027
  load** (~2.7 of ~3.4 s, 85 MB out), 0.7 s before the title. Replaced by host zlib (`inflate.cpp`, one host
  `z_stream` per guest `z_stream` address): mission.str in 0.3-0.5 s; load to first full frame 4.5 → 0.95 s. Adler-32
  still checked. [V] (61a226a)
- **WideLoad**: after the first level's mission.str loads, the title waits for all its WideLoad completion events with
  `WaitForMultipleObjects` (`sub_82846398`, callers `sub_828464E8`, `sub_828465C0`) using the count of jobs it
  queued, **which can be 0**. On NT, a count of 0 fails with STATUS_INVALID_PARAMETER_1 → WAIT_FAILED, which the
  title ignores. Blocking there hung the main thread (holding the portal lock). [V] (9f3af3e) ➜ Next game: same loader,
  same trap.
- Level start uploads ~2857 textures (25.6 MB) in the first frame (~334 ms with a warm pipeline list). [V] (perf2)

### 3.10 Languages

[V] (`language.cpp`, `kernel/xconfig.cpp`)
- `sub_82808190` reads `sku.lxb` and picks one language for everything. **Country first** (via `sub_822DECD0`):
  XC_COUNTRY 74 → dut, 90 → swe, 75 → nor, 25 → dan, 32 → fin. Otherwise `XGetLanguage`: de → ger, fr → fre,
  es → spa, it → ita, anything else → eng.
- Index stored at **0x82AA7048** (+52 holds the same value; +56/+60 the default); codes table at **0x82AA5EBC**:
  0 chs, 1 cht, 2 dut, 3 euk, 4 eng, 5 fre, 6 ger, 7 ita, 8 jap, 9 kor, 10 spa, 11 swe, 12 fin, 13 nor, 14 dan.
  Everything reads +48: text, UI, localized movies.
- Audio: `sub_827FD4D8` → **`sub_82864B90(code)`** sets Wwise's language folder (`\audio\<code>\voice.bnk`, levels'
  banks) and, through `sub_82156A00`, the current language of every file package (streaming.pck). Hooked to choose
  the voice language separately.
- Cutscene voices: **`sub_827F4400`** picks the Bink voice track (table above). Hooked.
- Port default: English text (114fe5e), English voices; `SKY_LANGUAGE` / `SKY_VOICE_LANGUAGE` or `launcher.ini`.
➜ Next game: find the `sku.lxb` reader and the language-code table; Nordic languages via country is likely unchanged.

### 3.11 Saves

[V] (9eb8b59)
- Save manager singleton = `sub_8215B030()`: +0 state (1 = selector pending), +8 user, +0xC open-content hash,
  +0x14 signed in, +0x18 XCONTENT_DATA (device id), +0x158 root `"lux"`, +0x35C XOVERLAPPED, +0x378 file handle.
- `sub_82853428` shows the device selector; `sub_82853530` polls `XGetOverlappedResult` (`sub_822DE570`, returns
  overlapped->InternalLow; 0x3E5 = pending); `sub_828536C0` opens content; SaveSlot `sub_828538E8`, LoadSlot
  `sub_828539F8`; Global exists/save/load `sub_82853BC0` / `CD0` / `DE8`; task switch `sub_8264D488`.
- Content names: "Skylanders - Global Data", "Skylanders - Slot 1..4" (named with `vsprintf`, the only caller —
  it was a stub, so names were empty). Files: `GLOBAL.DAT` (196 bytes), `SLOT.DAT`, `SLOT.NFO`.
- Flags seen: `0x13` = OpenExisting probe of a slot that doesn't exist (→ 3, ERROR_PATH_NOT_FOUND, normal), `0x12`
  create (disposition 1), then `0x13` reopen (disposition 2).
- XAPI file calls go through a table at `*(0x82A1E308)`; its NtQueryDirectoryFile slot is never read.
- `XamContentCreateEx`'s 9th argument (overlapped) and NtCreateFile's options are both at **r1+0x54** (stack
  arguments, see gotcha D12).
- Saves are stamped with local time: XAPI's `GetTimeZoneInformation` (`sub_822E4A18`) reads XConfig 3/1-3/7 and 3/12,
  `FileTimeToSystemTime` calls `RtlTimeToTimeFields`. [V] (d422434)

### 3.12 Portal of Power driver (title side)

[V] (portal disassembly + f88c07d run; `kernel/portal.cpp`, `host/portal_emulated.cpp`)
- `sub_82615A68` looks up **xam ordinals 0x4A1 / 0x4A2** (`XamInputNonControllerGetRaw` / `SetRaw`) with
  `XexGetProcedureAddress` and stores them at 0x82B7FA68 / 0x82B7FA6C; it retries on every poll while missing.
- Portal thread `sub_8264ABB8`: every **12 ms** calls `sub_826488C0`: one GetRaw, then at most one SetRaw.
- Driver object 0x82C4EF4C, vtable 0x820BA298: [2] input handler 0x82648178, [3] command builder 0x82647AB0; state at
  +28.
- `GetRaw(DWORD* state, DWORD* len /*in 32*/, BYTE* buf)`: 0 = success; non-zero = "disconnected" (resets the state
  machine). `state` non-zero = data. The title skips `buf[0..1]` (360 header) and parses `buf+2`, `len-2`.
  `SetRaw(DWORD len, BYTE* buf)`: buf = `0B 14 <cmd>`.
- Commands: `R 00` (reset/version), `A 00` / `A 01` (activate), `S` (status), `Q (n<<4|slot) block` (query),
  `W (1<<4|slot) block data16` (write), `C r g b` (light colour, no answer), `FA` (360 dongle). Later games add J, L, M,
  V (lights, speaker) — not in SSA.
- Answers expected: `R` version `(b1<<8|b2)` **≥ 0x011F** or the portal is "out of date"; `A act batt 77`, battery
  ≤ 164 = "too low"; `S <LE32: 2 bits per slot, bit0 present, bit1 changed> counter active` — **any S with active = 0
  resets the protocol, so an inactive portal must stay silent**; `Q (1<<4|slot) block data` or `Q 01` on failure;
  `W (1<<4|slot) block`.
- Hardware: Xbox 360 portal USB 1430:1F17 (also 24C6:FA00 in Xenia's list), interrupt EP 0x81 in / 0x02 out, 2-byte
  header. Wii / Wii U / PS3 / PS4 / PC portals: HID **1430:0150** (every HID model, SSA to Imaginators), output by
  SET_REPORT, no header. Xbox One portal: GIP; Santroller's capture shows VID 1430 PID 09AB, custom message 0x21,
  length 30 (assumed portal channel) [?] (untested). Its GIP descriptor names the device type
  `Activision.Xbox.Skylanders.Portal` and the interface `{1F7788A2-B860-9B40-3626-EC56C7C45256}` (the UWP build registers
  a Windows.Gaming.Input.Custom factory for it; `docs/getting-started/platforms/xbox_uwp.md`) [?] (untested).
➜ Next game: the same two ordinals are the obvious entry; check the version threshold, new commands and the
Traptanium/SuperChargers/Imaginators portal changes.

### 3.13 Figures (tags)

[V] (doc figures.md) MIFARE Classic 1K (64 blocks of 16 bytes).
- Block 0: serial (4 bytes), BCC, SAK/ATQA `81 01 0F`. Block 1: toy type LE16 at 0x10, trading card id 0x14..0x1B,
  variant LE16 at 0x1C, CRC-16/CCITT-FALSE of 0x00..0x1D at 0x1E (LE).
- Sector trailers: key A, access bits `0F 0F 0F 69` (sector 0) or `7F 0F 08 69`, key B zero. Key A sector 0
  `4B 0B 20 10 7C CB` [V]; sectors 1-15: CRC-48 (poly 0x42F0E1EBA9EA3693, init 2·2·3·1103·12868356821) of serial +
  sector [?] (not compared with a real dump; the title never sees key A).
- Blocks 8..63 except trailers: **AES-128-ECB, key = MD5(block 0 ‖ block 1 ‖ block number ‖
  `" Copyright (C) 2010 Activision. All Rights Reserved. "`)**; an all-zero block is left as is. Constant at
  0x8264398C (`sub_82643958`). [V] (title's own decryptor gives the same plaintext)
- Two progress areas: blocks 8-16 and 36-44; byte 0x09 of the first block = sequence; newer valid copy wins (A unless
  B = A + 1 mod 256). Checksums CRC-16/CCITT-FALSE LE: +0x0E (first block with `05 00` in place of the checksum),
  +0x0C (+0x10..+0x2F and +0x40..+0x4F), +0x0A (+0x50..+0x6F and +0x80..+0x8F, then 0xE0 zero bytes). [V]
- Fields: XP 24-bit at 0x00 [V], gold LE16 0x03 (max 65000) [V], play time LE32 0x05 [?], upgrades LE16 0x10 [? bit
  meanings], platform byte 0x13 (title writes 02 = Xbox 360) [V], hat LE16 0x14 (Giants+) [?], owner id 0x18..0x1F
  [V], nickname 16 UTF-16LE units at 0x20..0x2F + 0x40..0x4F [V], last placed date 0x50..0x55 [V], heroic challenges
  LE32 0x56 [?], hero points LE16 0x5A [?], last reset 0x60..0x65 [V], 0x80..0x8F unknown.
- Level thresholds (public values): 0, 1000, 2200, 3800, 6000, 9000, 13000, 18200, 24800, 33000 (cap 10). Only
  9000 = level 6 verified on the HUD; the title's own table was not found. [?]
- Toys accepted: `sub_82643DA0` accepts 0-32 and legendaries 404, 416, 419, 430; `sub_82643BE8` magic items 200-207,
  adventure packs 300-304 and **505, 514, 519, 526 (Giants-era sidekicks known to SSA's code)**. Character names from
  `sub_829BC850` (AirDragon = Whirlwind ... Cynder, `DarkSpyro` = 28). Toy type 14 = Gill Grunt. Variants: no check
  found [?].
➜ Next game: the key derivation and the area layout are shared with the later games (public tools handle all of
them); expect new toy ranges, variants, and Trap Team trap / SuperChargers vehicle / Imaginators creation-crystal data
layouts. Hats (0x14) start with Giants (public knowledge, [?] here).

### 3.14 Audio as the game uses it (Wwise, XAudio2, XMP)

- `Game_InitSound` (0x82865448): MemoryMgr, StreamMgr, low-level IO, then `AK::SoundEngine::Init` (0x825852C0).
  Wwise init needs a **XamNotifyCreateListener handle**; bank loading needs **user APCs** (ReadFileEx completions
  delivered in alertable waits). [V] (f287249-era runs)
- Wwise mutes its background-music bus (0x0DFBD437) at registration (0x825A1F38) unless **XMPTitleHasPlaybackControl**
  (0x824FC430 → `XMsgInProcessCall(0xFA, 0x0007001B)`) says the title has control; with an output-less stub the check
  read stack garbage → persistent mute ratio 0 → title music at −96 dB while SFX played. Traced through Wwise: mute map
  of CAkPBI at +24/+28 read by 0x825868B0, audible flag +865 in 0x825A7E90. [V] (5535492)
- Two copies of XMA voice code: **engine 1** (0x825xxxxx, XAudio2's XMA voices, maps contexts via `MmMapIoSpace`) and
  **engine 2** (0x826xxxxx, Wwise; all XMA seen). Wwise uses hardware XMA loops for looping in-memory sounds
  (`XMASetLoopData` 0x82617390). Section 4.13. [V]
- XAudio2 clients: two share callback 0x82523060 (wrapper dereferencing the context cell) and buffer 0x82B74000;
  `XAudio2_RenderDriverCallback` 0x82522E80. [V]

### 3.15 Online, achievements, profile

- Session code `sub_8254A4F8` treats `XNET_GET_XNADDR_NONE` as "no network". [V] (4ee1adc)
- Achievements: unlock via `XUserWriteAchievements` = `XMsgStartIORequest(app 0xFB, msg 0xB0008)`; then the title
  enumerates its **38** achievements to check whether all but one are unlocked (`sub_8264ACF8`). With a stub the
  enumerator handle stayed uninitialised → crash reading guest 0x00000008. [V] (01ea02f)
- One offline profile, XUID 0xE000000000000001, on controller 0; all privileges allowed. [V] (`kernel/xam_user.cpp`)
- Region / video checks: `sub_822DF218` warns about the video output unless the AV pack is 3, 4, 6 or 8, and tests
  the AV region for PAL (`(value & 0xFF00) == 0x0300`); the country code range 5..109 maps to territories. [V]

### 3.16 Memory allocators and locks

- Small-block pool allocator `sub_82132418` / `sub_82132618`, guarded by spin locks taken with `lwarx/stwcx.` and
  released with `lwsync; stw 0`. Its corruption (without fences) showed up as the crash in the heap coalesce
  **`sub_828A2360`** ("Fault at guest address 0x00000000" while the frontend loads). [V 32022e9: 4 threads hammering
  the pool, 8M allocations: old library crashes 4/4, fenced 0/4]
- `sub_822B52D0` was suspected to be the physical allocator [?] (d3d-re).

### 3.17 Engine function and data address table

| Address | Name / role | Status |
|---|---|---|
| 0x822DF400 | entry point | [V] |
| 0x827F5E80 | GameMaster_IntroMovies | [V] |
| 0x82852D90 | Movie_Update | [V] |
| 0x82800048 | startup state machine constructor (object 0x82C91EE8, state +0x1250) | [V] |
| 0x82635CF8 | JobWorker_Loop ("JQ Worker") | [V] |
| 0x826360F8 | JobGroup_Wait | [V] |
| 0x82635A80 → 0x822DCD18 | job queue polling with NtYieldExecution | [V] |
| 0x8213EFB0 | RecursiveSpinLock_Acquire | [V] |
| 0x82132418 / 0x82132618 | small-block pool alloc / free | [V] |
| 0x828A2360 | heap coalesce (where the fence bug crashed) | [V] |
| 0x826324D0 | nglInit | [V] |
| 0x829075F8 | nglListSend | [V] |
| 0x82633FD8 / 0x82633E78 | nglListBeginScene / nglInitScene | [V] |
| 0x826320C8 | ListSendModule | [V] |
| 0x82631098 | nglSceneTraverse (patched: skip when 0x82BB4F4C == 0) | [V] |
| 0x822895E0 | nglRenderListSubmit (patched: skip null list) | [V] |
| 0x8262D690 | nglPresent | [V] |
| 0x8262D540 | nglResolveTiles | [V] |
| 0x82631EA8 | sets present interval 2 | [V] |
| 0x827F6628 | step factor F (smoothed frame time in 60ths, max 3) | [V] |
| 0x821491F8 | uses the game delta F × clock+188 × clock+184 | [V] |
| 0x82856AA8 | game clock object getter (0x82C7CD20) | [V] |
| 0x82631968 | NGL vblank callback (counter 0x82BB4FD8) | [V] |
| 0x8213BBB8 | ngl_WriteStencilRefDirect | [V] |
| 0x828FC670 / 0x828FCBA8 | material state packet build / direct ring write | [V] (write, ? relation) |
| 0x82893F10 | ngl_FlushHiZDirect? | [?] |
| 0x8228B3E8 / 0x82289248 | ngl_DrawQuadUP / ngl_UnbindStream | [V] |
| 0x828519B0 | command buffer cache lookup (cache 0x82AA7BC0, 64 entries) | [V] |
| 0x82850F90 | records a mesh/material command buffer | [V] |
| 0x8285C540 / 0x8285F700 | mesh draw (virtual) / run-state setter | [V] |
| 0x8214AE58 | direct mesh draw without a cached buffer | [V] |
| 0x82850C48 / C90 / CD0 / CE8 | inherit-tag OR / ANDC / setters (globals 0x82BB46D0) | [V] |
| 0x828FE7B0 | LSA loader (from 0x828574D8) | [V] |
| 0x8296E750 | lux shader ctor | [V] |
| 0x8213AEB8 / 0x8213A958 | lux CreateVertexShader / CreatePixelShader wrappers | [V] |
| 0x828A5E48 | uses the embedded shader pointer table 0x82A42D74 | [V] |
| 0x82808190 | language selection (sku.lxb) | [V] |
| 0x822DECD0 | console country → language | [V] |
| 0x827FD4D8 → 0x82864B90 | audio language (hooked) | [V] |
| 0x82156A00 | file package current language | [V] |
| 0x827F4400 | cutscene Bink voice track (hooked) | [V] |
| 0x8215B030 | save manager singleton | [V] |
| 0x82853428 / 0x82853530 / 0x828536C0 | device selector / poll / open content | [V] |
| 0x828538E8 / 0x828539F8 | SaveSlot / LoadSlot | [V] |
| 0x82853BC0 / CD0 / DE8 | global save exists / save / load | [V] |
| 0x8264D488 | task switch (saves) | [V] |
| 0x82846398 | waits for WideLoad events (count may be 0) | [V] |
| 0x82615A68 | looks up xam 0x4A1/0x4A2 | [V] |
| 0x8264ABB8 / 0x826488C0 | portal thread / poll | [V] |
| 0x82648178 / 0x82647AB0 | portal input handler / command builder (vtable 0x820BA298) | [V] |
| 0x826434D8 | portal debug log (printf-like) | [V] |
| 0x82643958 | figure key derivation (constant at 0x8264398C) | [V] |
| 0x82643DA0 / 0x82643BE8 | accepted toy ranges | [V] |
| 0x829BC850 | character internal names | [V] |
| 0x82865448 | Game_InitSound | [V] |
| 0x8264ACF8 | achievement completion check | [V] |
| 0x8254A4F8 | session code (XNADDR) | [V] |
| 0x822DF218 | AV pack / region check | [V] |
| 0x822E4A18 | GetTimeZoneInformation (XAPI) | [V] |
| Data 0x82BB4F94 | D3D device pointer | [V] |
| Data 0x82BB4F4C / 4F44 / 4F48 | current scene / current / next root | [V] |
| Data 0x82BA4F70 | NGL state cache | [V] |
| Data 0x82B94E98 | present parameters | [V] |
| Data 0x82B62CA4 | step factor F (1.0 at start; 2 at 30 fps, 1 at 60) | [V] |
| Data 0x82BA8258 | NGL frame statistics (frame time +0x5C, fps +72; copy 0x82BA71E0) | [V] |
| Data 0x82C54698 / 0x82C54720 | front-buffer textures | [V] |
| Data 0x82AA7048 / 0x82AA5EBC | language index / code table | [V] |
| Data 0x82AA7BC0 | command buffer cache | [V] |
| Data 0x82BB46D0 | inherit tag defaults | [V] |
| Data 0x82B7FA68 / 6C | portal GetRaw/SetRaw pointers | [V] |
| Data 0x82C4EF4C | portal driver object | [V] |
| Data 0x82A1E308 | XAPI file function table pointer | [V] |
| Guest 0xAA5E0000 | 2.56 MB vertex streaming ring | [V] (runtime) |
| Physical 0x06A60000 | level_027 DepthMap resolve target (one run) | [V] (runtime) |

---

## 4. (B) XDK libraries

SSA links XDK **2.0.20500** libraries (section 1). The other games will link the same libraries, possibly newer
builds: the behaviour below should hold, the offsets may not. D3D9LTCG ("link-time code generation") means **many D3D
functions are inlined into game code** — there is no single setter to hook for most state.

### 4.1 Direct3D 9 (D3D9LTCG): the device structure

Device: **0x5F80 bytes**, 128-aligned, allocated by `Direct3D_CreateDevice` (0x822A09F0); the NGL global pointer is
0x82BB4F94. Same layout family as Unleashed Recompiled's `GuestDevice` up to ~0x2780; later fields shifted (newer XDK
there). [V] (device-field tracking + setters; doc d3d_map.md; `d3d/device.h`)

| Offset | Content |
|---|---|
| +0x00 u64 | dirty: VS float constants (bit 63 = c0-c3, 1 bit = 4 vec4) |
| +0x08 u64 | dirty: PS float constants |
| +0x10 u64 | dirty: register groups + shader link (section 4.2) |
| +0x18 u64 | dirty: fetch constants (bit 31 = slot 0) and 0x2280-0x2294 (bits 54..34) |
| +0x20 u64 | dirty: 0x2300-0x2325 (bits 37..0), 0x2380-0x2387 (45..38), bit 56 bool+loop constants; bits 49-54 → 0x822A4D10 [?], 62-63 → 0x822A0940 [?] |
| +0x28 u64 | mask ANDed with +0x10 to trigger 0x822A48B8 (HiZ flush?) [?] |
| +0x30 / +0x34 / +0x38 | **ring write cursor** (pre-increment, `stwu x,4(p)`: points at the last word written), end, kick-off threshold. (Not +0x35E8.) |
| +0x40 / +0x224 | SetRenderState / GetRenderState function pointers (101 each) |
| +0x1D4 / +0x3B8 | SetSamplerState / GetSamplerState pointers (20 each) |
| +0x480 | fetch constants: 32 slots × 6 dwords (textures 0-25; vertex fetch for stream s at +0x778 − 8s, i.e. slot 95 − s in 2-dword units) |
| +0x780 / +0x1780 | VS / PS float constants, 256 vec4 each |
| +0x2780 / +0x2790 / +0x27A0 | VS bool (128 bits), PS bool, loop constants (regs 0x4900-0x4927) |
| +0x2880.. | shadow registers: 0x2000-0x200F at +0x2880, 0x2080-0x2082 at +0x28C0 (**written directly to the ring, not dirty-tracked**), 0x2100-0x2114 at +0x28CC, 0x2180-0x2184 at +0x2920, 0x2200-0x220B at +0x2934, 0x2280-0x2294 at +0x2964, 0x2300-0x2325 at +0x29B8, 0x2380.. at +0x2A50 |
| +0x2A18..+0x2A44 | RB_COPY_* shadows (reset by Resolve before it returns!) |
| +0x2B08 | owner thread id (NGL writes it, 21 functions) |
| +0x2B10 / +0x2B14 / +0x2B1C | retired-fence pointer / interrupt info block / current fence |
| +0x2B3C..+0x2B40 | flag bytes (bit 0 of +0x2B3C = ZPass / predicated draw path) |
| +0x2F58 | current vertex declaration (dirty +0x10 bit 19); SetVertexDeclaration is fully inlined |
| +0x2F78..+0x3004 | D3D render-state value cache for the getters (+0x2F80 = SCISSORTESTENABLE, +0x2F98 written by 60 inlined sites) |
| +0x3188 | FVF |
| +0x318C | index buffer |
| +0x3190 | render targets [4] |
| +0x31A0 | depth-stencil surface |
| +0x31A4 | streams [16] |
| +0x31E8 | stream strides (bytes) [16] |
| +0x31F8 | textures [26] |
| +0x3260..+0x3274 | viewport floats X, Y, W, H, MinZ, MaxZ |
| +0x327C..+0x3288 | scissor rect (left, top, right, bottom) — the window scissor is **not** shadowed elsewhere |
| +0x328C / +0x3290 | **pixel shader / vertex shader** (the original symbol file had these swapped) |
| +0x329C/+0x32A0/+0x32A4, +0x32C4.. | tiling: bin masks, tile count (+0x32C4), tile rects (+0x32CC..) |
| +0x34B0.. | ZPass fixup list |
| +0x3594 | BeginVertices packet pointer (inline EndVertices sets +0x30 = +0x3594) |
| +0x35E0 | **command buffer being recorded** (0 = none) |
| +0x35E8 / +0x35EC | resources referenced while recording |
| +0x3630 | D3DRS_PRESENTINTERVAL |
| +0x425C, +0x5EAC | gamma ramp? [?] |

- `D3DDevice_SetStateToDefaults` (0x822B1AB8) walks the RS table **0x82A426C0** ({Get, Set, Default} × 101, value =
  4·index, first 10 unused) and SS table **0x82A42B80** (× 20) and copies the Set/Get pointers into the device. All
  101 render-state + 20 sampler-state setters decoded in `private/re/rsdump.txt` (local, not committed). Setters only
  write shadows + dirty bits. [V]
- `D3DDevice_UpdateSurfaceInfo?` (0x82297430): RB_SURFACE_INFO / PA_SC_AA_CONFIG from the surface, resets scissor +
  viewport; called by SetRenderTarget / SetDepthStencilSurface. [?]
➜ Next game: `Direct3D_CreateDevice` is the caller-of-allocation with 0x5F80 (check the size, it changes with the
XDK); then `SetStateToDefaults` (walks two tables of function pointers) gives every render-state setter; the setters
give the shadow offsets. Rebuild this table before trusting `d3d/device.h`.

### 4.2 D3D: dirty masks and register groups

[V] (from the setters; doc command_buffers.md) Each group lists its **first register in its highest bit** (MSB-first):

| Mask | Bits → registers |
|---|---|
| +0x00 / +0x08 | VS / PS constants, bit 63 = c0-c3 … bit 0 = c252-c255 |
| +0x10 | 0x2200-0x220B → bits 11..0; 0x2180-0x2184 → 16..12; **shader link 17-20** (FlushShaders); 0x2100-0x2114 → 41..21; 0x2000-0x200F → 57..42 |
| +0x18 | fetch slots 0-31 → bits 31..0; 0x2280-0x2294 → 54..34 |
| +0x20 | 0x2300-0x2325 → 37..0; 0x2380-0x2387 → 45..38; bit 56 bool + loop constants |

- Example: RB_STENCILREFMASK (0x210D) is bit 41 − 0xD = **bit 28 of +0x10**. RB_COLOR_INFO is bit 57 − 1, RB_DEPTH_INFO
  57 − 2 of +0x10.
- `D3DDevice_WriteDirtyRegisters` (0x822A4C50: `(dev, mask<<, firstReg, shadowPtr)`, type-0 packets for set bits),
  `WriteDirtyAluConstants` (0x822A4FE8, 1 bit = 4 vec4), `WriteDirtyFetchConstants` (0x822A4E90), `FlushShaders`
  (0x822A5E08: links VS/PS, IM_LOAD microcode type 0 VS / 1 PS, SQ_PROGRAM_CNTL), `MakeSpaceForRegisters`
  (0x822A4608, ring overflow path). [V]

### 4.3 D3D: draws, BeginVertices, clears, resolves, swap

- **No DRAW packet is built outside the D3D library.** Draw functions inline the whole dirty flush (ALU constants,
  FlushShaders, register groups, fetch constants), then emit `VGT_INDX_OFFSET` + predicated `DRAW_INDX`. In ZPass mode
  they also emit SET_BIN_MASK_LO + EVENT_WRITE_EXT and record a fixup. [V]
- `DrawVertices` 0x8229AD58 `(dev, prim, start, count)`, DRAW_INDX auto 0xC0012201; `DrawIndexedVertices`
  0x8229B170 `(dev, prim, baseVertex, startIndex, indexCount)`, DRAW_INDX 0xC0032201, index buffer = device+0x318C;
  `BeginVertices` 0x8229A850 `(dev, prim, count, stride) → ptr` (allocates via 0x822A2BD8, writes vfetch constant 95 +
  DRAW_INDX; EndVertices is inlined: device+0x30 = +0x3594); `DrawVerticesUP` 0x8229AD10 (BeginVertices + memcpy +
  inline End); tessellated draws 0x8229B5E8 / 0x8229BA88 [? names; callers 0x8262B3D0 / 0x8262B430; not used in
  level_027]. [V]
- Index buffer: Common bit 31 = 32-bit indices [? original note; `render.cpp` only handles 16-bit and says 32-bit
  index buffers were not seen].
- **Clears**: there is no separate D3DDevice_Clear in use; `D3DDevice_ClearF` — and **BeginTiling and BeginZPass** —
  end in **0x822ADDE0**, which clears with a **rectangle draw of its own** (an inline DRAW packet no draw hook sees).
  Flags: bits 0-3 colour targets 0-3, 0x10 depth, 0x20 stencil (confirmed from the depth-control values 0x76 / 0x8701
  built in 0x822AD840); the rectangle is clipped to the viewport and, with the scissor test on (+0x2F80), to the
  scissor rect. Arguments `(device, flags, rect, color float4, -, stencil, f1 = z)`. [V] (3169f63)
- **Resolve** 0x822A1560 `(dev, flags, srcRect, destTex, destPoint, level, slice, clearColor (r10), f1 = clearZ,
  clearStencil = 10th arg on the stack at caller r1+0x5C)`. Flags: bits 0-2 source (0-3 colour target, 4 depth), bits
  4-6 samples, **0x100 clear the colour target / 0x200 clear depth-stencil after the copy**, bits 26-31 exponent bias.
  **The RB_COPY_* shadows are reset before Resolve returns (0x822A2550)**: read nothing from them afterwards. D3D sets
  `copy_dest_swap` (red/blue) when the destination's fetch swizzle does not start with X (ARGB textures, ZYXW;
  0x822A17AC). The Resolve's internal copy draw writes RB_SURFACE_INFO, RB_COLOR_INFO, SQ_PROGRAM_CNTL 0x2180-0x2182,
  PA_CL_CLIP_CNTL, PA_CL_VTE_CNTL 0x300, 0x2301 = C003, RB_DEPTHCONTROL 8777 **into the ring**; D3D then restores only
  some of them by later writes (the rest presumably by re-dirtying groups) — a ring mirror must forget D3D-owned
  registers after a Resolve. [V] (6ba6ff2, levelgfx work)
- **Swap** 0x822A95C0: `VdGetSystemCommandBuffer`, `VdSwap`, EVENT_WRITE_SHD; NGL passes the resolved front-buffer
  texture. `D3DDevice_FrameEnd?` 0x822A9088 maps the present interval: DEFAULT/ONE → 1, TWO → 2, FOUR → 4, IMMEDIATE and
  THREE → no wait. [V] (0b74438)
- `D3DDevice_SetShaderGPRAllocation` 0x82290988 `(dev, flags, vsGPRs, psGPRs)` runs 3 indirect buffers. [V]
- Conditional survey/rendering (0x8229E290..0x8229E5E8: VIZ_QUERY, PA_SC_VIZ_QUERY +0x29B0), state blocks (0x822A86C8,
  0x822A8A40), UnbindAll 0x822A0C50: names uncertain [?].

### 4.4 D3D: command buffers (Begin/End/RunCommandBuffer)

[V] (command_buffers.md, command-buffer + frontend work)
- `BeginCommandBuffer(device, cb)` 0x822A6BC8: device+0x35E0 = cb; copies the **inherit tags** (5 × u64, globals
  0x82BB46D0) to cb+0x18 and the persist tags to cb+0x38; **clears the fetch-constant shadows** (device+0x480, 0x300
  bytes); writes the window scissor into the buffer; sets the dirty masks device+0x00..0x20 to **~inherit**.
- Recorded draws flush the dirty groups into the buffer as usual, so a buffer holds exactly the groups that were dirty
  at some recorded draw. Menu buffers record: VS c16-c23 and c248-c255, PS c16-c19 and c248-c255, RB_DEPTHCONTROL,
  0x2203, PA_SU_SC_MODE_CNTL, RB_MODECONTROL, shaders, the viewport, RB_SURFACE_INFO, RB_DEPTH_INFO, texture slot 0,
  vertex fetch slots 26-31.
- `EndCommandBuffer(device)` 0x822A6FB8: stores the dirty masks into the buffer and finishes it. [?] (name)
- `RunCommandBuffer(device, cb)` 0x822A73D8: emits INDIRECT_BUFFER (0xC0013F00) per segment; **sets the dirty masks to
  the complement of the buffer's inherit tags (cb+0x40..+0x60, copied from +0x18 by Begin)**; device shadows are not
  changed. So after a run, the inherited groups the buffer wrote keep the buffer's values on the GPU. [V frontend WIP;
  older docs said "ORs the buffer's masks" — the frontend reading supersedes it, [?] until merged]
➜ Next game: if menus lack backgrounds or 3D meshes are missing while UI draws, check RunCommandBuffer first.

### 4.5 D3D: resources, textures, vertex formats, shaders

- **Resource header** (0x18): +0 Common (low 4 bits type: 1 VB, 2 IB, 3 texture, 4 surface, 5 declaration, 6 VS,
  7 PS; bits 8-11 lock count; bit 30 surface-of-texture; 0x100000 D3D-created), +4 refcount, +8 Fence, +0xC ReadFence,
  +0x10 Identifier, +0x14 BaseFlush. [V]
- Texture: +0x18 MipFlush, **+0x1C..+0x30 GPU texture fetch constant** (6 dwords). VB: vertex fetch constant at +0x18
  (address dword, size dword). IB: +0x18 address. Surface: +0x18 parent texture, +0x1C RB_COLOR_INFO / RB_DEPTH_INFO
  word (EDRAM base tile, format), **+0x24 size = (w−1) << 18 | (h−1) << 3**. [V]
- `D3DResource_GetType` (0x82296A40): 3 texture, 0x12 cube, 0x13 array, 0x14 line; 0x10 and 0x11 are both noted as "volume" in the symbol file [?].
  `D3DResource_LockInternal` 0x822963B8 (blocks on fence, ++lock count, cache coherency packets), `UnlockInternal`
  0x822966B8 (dcbst BaseFlush / MipFlush ranges). Create functions for textures/surfaces/buffers were never
  identified (not needed: everything is read from fetch constants). [V] [?] (for Create)
- **Texture fetch constant** (`d3d/xenos.h`, Xenia layout): dword0 type (2 = texture) and pitch (bits 22-30, ×32
  texels); dword1 base address (physical, 4 KB aligned) and Stacked (bit 10: 2D array); dword2 size and depth; dword3
  swizzle (bits 1-12, 3 bits per component: 0-3 = XYZW, 4 = 0, 5 = 1); dword5 dimension (bits 9-10: 1D/2D/3D/cube) and
  mip address (0 = no mips). [V]
- **Mip layout** (Xenia texture_util, BSD): levels 1+ from the mip address, each padded to
  max(next_pow2(base) >> level, 1) or a 32×32-block tile; with packed mips, levels from the one whose shorter side is
  ≤ 16 texels share one tile (the packed tail) at fixed offsets (16×16, 8×8, 4×4 stacked across the short side,
  smaller along the long side); when the base itself is the tail, **D3D leaves the mip address at 0** (tail at the
  base); array slices / cube faces 4 KB aligned, 2D arrays padded to 4 slices; linear rows 256-byte aligned. [V] (8dc4d1b)
- Tiled 2D addressing: Xenia's `Tiled2D` (pitch a multiple of 32 blocks). Endian swaps: 0 none, 1 8-in-16, 2 8-in-32,
  3 16-in-32. [V]
- **Vertex declaration** element types (low 6 bits = Xenos vertex format): FLOAT1-4 0x2C83A4 / 0x2C23A5 / 0x2A23B9 /
  0x1A23A6, D3DCOLOR 0x182886 (stored ARGB), UBYTE4 0x1A2286, UBYTE4N 0x1A2086, SHORT2/4 0x2C2359 / 0x1A235A,
  SHORT2N/4N 0x2C2159 / 0x1A215A, USHORT2N/4N 0x2C2059 / 0x1A205A, FLOAT16_2/4 0x2C235F / 0x1A2360, **DEC3N 0x2A2187**,
  UDEC3 0x2A2287. Declaration object: header 0x34 bytes, element count at +0x18, elements 12 bytes,
  end marker 0xFF; Common 0x00100005. [V] (`d3d/xenos.cpp`, symbols)
- **Shader containers**: header (big-endian) flags (0x102A1101 VS / 0x102A1100 PS), virtual size, physical size …;
  `CreateVertexShader` 0x82290508 (type 6 object, 0x368 header with 2 × 0x1A0 bind slots, microcode pointer +0x20),
  `CreatePixelShader` 0x82290160 (type 7, 0x28 header, microcode in physical memory, pointer +0x18),
  `XGGetMicrocodeShaderParts` 0x822900D8. **Create*Shader rewrites parts of the container in place: hash it before
  calling the original** (the cache key is the on-disc container, XXH3-64). VS header at +shaderOffset: 6 dwords, an
  index, the input count, one dword, then (index + input) 32-bit entries: address 12 bits, usage 4 bits, usage index
  4 bits. [V] (`d3d/shaders.cpp`)
- PS booleans live at registers 128-255 while the constant table numbers them from 0. [V] (xenosrecomp.patch)
- Blend constant: RB_BLEND_RED..ALPHA 0x2105-0x2108. Xenos has no blend-enable bit: ONE/ZERO/ADD for colour and alpha
  means "no blending". [V]
- EDRAM render-target formats (`ColorTargetFormat`): k_8_8_8_8 / _GAMMA, k_2_10_10_10 (no host 10:10:10:2: 16 bits per
  channel instead), k_2_10_10_10_AS_10_10_10_10, k_16_16_FLOAT, k_32_FLOAT, k_32_32_FLOAT, others as wide floats.
  Depth k_24_8 / k_24_8_FLOAT sampled as the depth value in X (Xenia's conversion). [V]

### 4.6 D3D: ring buffer, fences, interrupts, tiling, predication

- `InitializeRingBuffer` 0x822A4210 (VdInitializeRingBuffer, VdEnableRingBufferRPtrWriteBack, INDIRECT_BUFFER);
  `KickOff` 0x822A3BB0 (submit, then BlockOnFence(current − 2)); `BlockOnFence` 0x822A3CB8 spins until
  `(dev+0x2B1C − fence) >= (dev+0x2B1C − *(dev+0x2B10))`; ring segment switches go through 0x822A2D78;
  `ReserveCommandSpace?` 0x822A25E8 (19 callers); `AllocateTransient?` 0x822A2BD8. [V]
- `InsertCallback` 0x822A3DB0 `(device, cmdbuf, flags, callback, context)`: packets make the GPU fill device+0x2B14
  and raise a CP interrupt. `D3D_GraphicsInterrupt` 0x822A4000 (registered with VdSetGraphicsInterruptCallback):
  source 1 calls info+16(info+20), source 0 handles the vblank. [V]
- Without a GPU, **submitted work counts as executed when kicked off** (fences retire, callbacks fire), but
  **unsubmitted work must not look done**, or the engine recycles frames still being built; a fence still in
  unsubmitted commands needs a kick-off; while recording a command buffer there is no GPU fence. [V] (`d3d/null_gpu.cpp`)
- Predicated tiling: `BeginTiling` 0x822AE778 (tile rects +0x32CC, count +0x32C4; caller 0x8262D3E8), `EndTiling`
  0x822AEAD0 (resolves every tile; caller 0x8262D608), `SetPredication` 0x822AE608, `BeginZPass?` 0x822AEF88 /
  `EndZPass?` 0x822AF218 (fixup list +0x34B0). For a host renderer: ignore tiles (clear at Begin, resolve once at End),
  SetPredication no-op. [V] (draft plan; level_027 uses none of them)

### 4.7 D3D function address table

| Address | Function | Status |
|---|---|---|
| 0x822A09F0 | Direct3D_CreateDevice | [V] |
| 0x822B1AB8 | D3DDevice_SetStateToDefaults | [V] |
| 0x822B21B0 | D3D_SetDisplayMode (only caller of VdSetDisplayMode) | [V] |
| 0x822B2348 | D3D_InitializeEngines (VdInitializeEngines, VdSetGraphicsInterruptCallback) | [V] |
| 0x822A4210 | InitializeRingBuffer | [V] |
| 0x822A3BB0 / 0x822A3CB8 | KickOff / BlockOnFence | [V] |
| 0x822A3DB0 | InsertCallback | [V] |
| 0x822A4000 | D3D_GraphicsInterrupt | [V] |
| 0x822A2D78 | ring segment switch (hooked for ResyncRing on the levelgfx branch) | [V] |
| 0x8229AD58 / 0x8229B170 | DrawVertices / DrawIndexedVertices | [V] |
| 0x8229A850 / 0x8229AD10 | BeginVertices / DrawVerticesUP | [V] |
| 0x8229B5E8 / 0x8229BA88 | Draw(Indexed)TessellatedVertices? | [?] |
| 0x822ADDE0 | ClearF worker (also BeginTiling/BeginZPass) | [V] |
| 0x822AD840 | builds the clear's depth-control values | [V] |
| 0x822A1560 | Resolve | [V] |
| 0x822A2550 | resets RB_COPY_* shadows | [V] |
| 0x822A17AC | copy_dest_swap from the destination swizzle | [V] |
| 0x822A95C0 | Swap | [V] |
| 0x822A9088 | FrameEnd? (present interval mapping) | [V] (mapping, ? name) |
| 0x822A6BC8 / 0x822A6FB8 / 0x822A73D8 | Begin / End? / RunCommandBuffer | [V] |
| 0x8228FDB8 / 0x8228FE90 | SetVertex/PixelShaderConstantF `(dev, start, data, vec4Count, dirtyMask)` | [V] |
| 0x8228FF68 / 0x8228FFC8 | Set VS / PS ConstantB | [V] |
| 0x82290028 / 0x82290080 | Set VS ConstantI / PS ConstantI? | [V] [?] |
| 0x82290250 / 0x822905F8 | SetPixelShader (+0x328C) / SetVertexShader (+0x3290) — **were swapped in the first symbol file** | [V] |
| 0x82290160 / 0x82290508 | CreatePixelShader / CreateVertexShader | [V] |
| 0x822900D8 | XGGetMicrocodeShaderParts | [V] |
| 0x82290410 | D3DVertexShader_Init | [V] |
| 0x82290908 / 0x82290818 | CreateVertexDeclaration / VertexDeclaration_Init | [V] |
| 0x822907C8 | SetFVF | [V] |
| 0x82290988 | SetShaderGPRAllocation | [V] |
| 0x8229E100 | SetTexture `(dev, sampler, tex, dirtyMask)` | [V] |
| 0x82299E98 | SetStreamSource `(dev, stream, vb, offset, stride, dirtyMask)` | [V] |
| 0x82299FB8 | SetIndices | [V] |
| 0x8229A048 / 0x8229A3D8 | SetRenderTarget / SetDepthStencilSurface | [V] |
| 0x82297430 | UpdateSurfaceInfo? | [?] |
| 0x82299E18 / 0x822977B0 | SetViewport / SetViewportF (PA_CL_VPORT_* +0x2908..+0x291C) | [V] |
| 0x82299D20 / 0x822974C0 | SetScissorRect / WriteWindowScissor (direct ring write) | [V] |
| 0x822A5E08 | FlushShaders | [V] |
| 0x822A4C50 / 0x822A4FE8 / 0x822A4E90 | WriteDirtyRegisters / AluConstants / FetchConstants | [V] |
| 0x822A48B8 | FlushHiZ? | [?] |
| 0x822A4608 | MakeSpaceForRegisters | [V] |
| 0x822AE608 / 0x822AE778 / 0x822AEAD0 | SetPredication / BeginTiling / EndTiling | [V] |
| 0x822AEF88 / 0x822AF218 | BeginZPass? / EndZPass? | [?] |
| 0x82296948 / 0x822968D0 / 0x822969C8 / 0x82296A40 | Resource Release / AddRef / GetDevice / GetType | [V] |
| 0x822963B8 / 0x822966B8 | Resource Lock/UnlockInternal | [V] |
| 0x8229CCB0, 0x8229CDB0, 0x8229D3F0, 0x8229D480, 0x8229DFC8, 0x8229E060, 0x8229E080, 0x8229DA90 | texture/volume/surface GetLevelDesc, LockRect/LockBox, GetDesc, Lock/UnlockRect, GetLevelCount | [V] |
| 0x82297A10, 0x82297A70, 0x82297AA8, 0x82297BC8, 0x82297C58, 0x82297ED0, 0x82298148, 0x82298520, 0x82298700, 0x822992C0 | SetRenderState CULLMODE, ALPHATESTENABLE, ALPHABLENDENABLE, SRCBLEND, DESTBLEND, ALPHAREF, ZENABLE, SCISSORTESTENABLE, COLORWRITEENABLE, PRESENTINTERVAL | [V] |
| 0x82291448-0x82296028 | XGraphics shader assembler? | [?] |
| 0x822E5B88 | D3DXGetShaderConstantTable | [V] |

### 4.8 Kernel (xboxkrnl) behaviour the title depends on

Each line was a real bug: the title blocked, crashed or misbehaved until it was implemented this way. [V] (unless noted)

- **KeTimeStampBundle must tick** every millisecond (InterruptTime, SystemTime, TickCount): XAPI's GetTickCount and
  Bink's RADTimerRead read it directly. Movies stalled without it.
- `KeQueryPerformanceFrequency` = the host counter's frequency, since `mftb` is translated to cntvct_el0 (arm64) or
  a calibrated TSC (x64).
- **Ke* objects** are passed as dispatcher headers in guest memory; the (inlined) Ke*Initialize* calls overwrite the
  header, so the host object is bound through a marker in the header's wait list ('SSAK' 0x5353414B + binding id) and
  recreated from the header type on first use. Signal state is mirrored into the header for code that reads it.
- **Real PulseEvent**: releases only threads already waiting (all for manual-reset, one otherwise).
- Event type 0 = notification (manual reset), 1 = synchronization (auto reset). XAudio2's driver callback can signal
  its event before creating it; the console tolerates that.
- **User APCs**: `NtReadFile` with a completion routine (XAPI tags the routine pointer's low bit) must queue the
  completion as an APC delivered in the thread's next alertable wait — **Wwise bank loading depends on it**.
  Alertable `KeDelayExecutionThread` ends early to run APCs.
- **NtWaitForMultipleObjectsEx with count 0 (or > MAXIMUM_WAIT_OBJECTS) must fail** with
  STATUS_INVALID_PARAMETER_1 (WideLoad, section 3.9).
- **Zero-length sleeps and NtYieldExecution are spin-waits** in this title (section 3.3): back off.
- Critical sections: owner = KTHREAD stored big-endian, X_RTL_CRITICAL_SECTION after its dispatcher header.
- Threads: r13 → KPCR (TLS pointer +0x0, self +0x30, stack base/limit +0x70/+0x74, current thread +0x100, CPU
  +0x10C; size 0x2D8); KTHREAD 0xAB0 bytes, thread id at +0x14C; `__declspec(thread)` data addressed from the TLS
  pointer with the dynamic slots (KeTlsAlloc) after it; waits on the KTHREAD resolve to the thread object. Host
  stacks 16 MB (recompiled calls nest deeply).
- `ExRegisterTitleTerminateNotification`, `KeInitializeDpc`, `KeEnableFpuExceptions`, Vd* init: harmless stubs.
- `RtlTimeToTimeFields` / `RtlTimeFieldsToTime` needed for save timestamps (1601-01-01 was a Monday).
- `vsprintf` (Microsoft conversions: `%S` wide, `I64`) needed for content names; `sprintf`/`_vsnprintf` only by D3D /
  xbmovie, left as stubs. [V]
- NTSTATUS → Win32 error mapping used by XAPI (`kernel/strings.cpp`).
- Module handles: system modules get an LDR entry with a fake DllBase (xam 0x81000000, kernel 0x80040000); some
  XAM/kernel callers inspect the XEX header through the module, so a copy is kept in guest memory.
  `XexGetProcedureAddress` must hand out callable guest addresses for the ordinals the title looks up at run time
  (only xam 0x4A1/0x4A2 here).
- Nothing is mapped at the bottom of the address space (null pointer traps). [V]
➜ Next game: implement these first; they are title-independent.

### 4.9 Memory: virtual heaps, physical windows, the +0x1000 offset

- 512 MB physical memory addressed through **0xA0000000 (64 KB pages), 0xC0000000 (16 MB pages), 0xE0000000 (4 KB
  pages)** windows, chosen by the allocation's page size (MmAllocatePhysicalMemoryEx). Our runtime does **not alias**
  the windows: each physical allocation remembers its window, and `PhysicalToGuest` turns a physical address back into
  the guest address it is used through. [V]
- **The 0xE0000000 window addresses physical memory +0x1000** (as on the console): `MmGetPhysicalAddress` /
  `PhysicalToGuest` honour it, and E allocations reserve one extra page so the GPU's physical range never overlaps the
  next allocation. Without it, Bink planes were read 4 KB too low. [V] (f287249)
- **XMA buffers must come from MmAllocatePhysicalMemoryEx** (PhysicalToGuest returns 0 otherwise; logged once). The XMA
  context array lives in the 0xA window. Engine 1 calls `MmMapIoSpace(2, physOfContext, 0x40, 0x404)` and writes
  its context through the result: hand back the guest address. [V]
- Virtual ranges for 4 KB and 64 KB pages; 0x70000000+ kept for the runtime's system allocations (PCRs, KTHREADs,
  stacks). Decommitted pages stay reserved. [V] (`memory.h`)
- PhysicalToGuest runs ~4000 times per frame (vertex/index/texture addresses): a per-thread cache of the last
  allocations (invalidated by a generation counter on free) took it from 247 to 106 profile samples. [V] (af63b9c)

### 4.10 XAM and XAPI

- **XConfig** (`ExGetXConfigSetting`): an emulated North American retail console on HD. Weak stubs returning 0
  "succeeded" with nothing written → the country came from uninitialised stack. Settings used: AV region, game region,
  language, video flags, country (XC_COUNTRY_US = 103), time zone 3/1-3/7 and 3/12. `XGetAVPack` 6 (VGA, one of 3/4/6/8
  the title accepts), `XGetGameRegion`, `XGetLanguage`, **`XamGetSystemVersion` 2.0.17559.0** — **0 sends the music
  player check down its pre-2009 path reading unwritten outputs**. Encoding `(major<<28)|(minor<<24)|(build<<8)|qfe`.
  [V] (4ee1adc)
- **XamNotifyCreateListener** must return a waitable handle (Wwise init); `XNotifyGetNext` returns FALSE when nothing
  is pending. [V]
- **XMsg**: `XMsgInProcessCall` / `XMsgStartIORequestEx` to app **0xFA (XMP)**: GetStatus 0x00070009, SetPlaybackController
  0x0007001A, **GetPlaybackController 0x0007001B → controller 0 (title), locked 0**; write only through non-null output
  pointers. `XMsgStartIORequest` to app **0xFB** carries achievement unlocks (0x000B0008). Message layouts as in
  Xenia's XMP app. [V] (5535492, 49ef969, 01ea02f)
- `XamUserCreateAchievementEnumerator` + `XamEnumerate`: return a handle that enumerates nothing (ERROR_NO_MORE_FILES).
- **Device selector** `XamShowDeviceSelectorUI(user, content type, flags, bytes, device id out, overlapped)`: pick the
  hard drive without UI (id 1) like the console when one device fits; with no drive, ERROR_CANCELLED. The game polls
  the overlapped through `XGetOverlappedResult` (InternalLow; 0x3E5 = ERROR_IO_PENDING) — **publish InternalLow
  last**. [V]
- **XamContentCreateEx** `(user, root name, XCONTENT_DATA*, flags, disposition out, license out, cache size, content
  size, overlapped @ r1+0x54)`: XCONTENT_DATA = device id, content type, WCHAR display name[128], char file name[42],
  2 padding bytes; creation flags low nibble = CreateFile dispositions; mounts the root name ("lux") on a host
  folder `<save dir>/saves/<type %08X>/<file name>/`. `XamContentGetDeviceData`: device id, type, total bytes, free
  bytes, WCHAR name[28]. [V]
- **Input**: XamInputGetState / GetCapabilities for user 0 (all inputs present 0xF3FF), other users no device;
  `XamInputSetState` (vibration) not forwarded. **Non-controller raw** ordinals 0x4A1/0x4A2 for the portal. [V]
- **Network**: XNet and Winsock start; sockets are handles accepting setsockopt/ioctlsocket(FIONBIO)/bind; traffic
  fails with WSAENETDOWN (WSAEWOULDBLOCK non-blocking); `XNetGetTitleXnAddr` → XNET_GET_XNADDR_NONE; `XNetRandom` gives
  random bytes. Every NetDll_ export takes the caller type first. [V]
- Profile: one signed-in offline profile (XUID 0xE000000000000001); `X_USER_SIGNIN_INFO` = u64 xuid, u32 flags, u32
  state, u32 guest number, u32 sponsor, char name[16]; privileges all allowed. [V]
- Imports the title links but never needed: XamShowSigninUI, FriendsUI, MessagesUI, KeyboardUI, MessageBoxUI(Ex),
  DirtyDiscErrorUI, XamLoaderLaunchTitle/TerminateTitle, NtQueryDirectoryFile (never read)… see
  `private/imports.txt` (218). [V] (for the ones implemented; the rest stay weak stubs)

### 4.11 Video: vertical blank, present interval

- One interrupt thread delivers every graphics interrupt in order (like the console's interrupt context): vblank every
  16 667 µs, **VBlank status register 0x7FC86544 bit 0 must be set** or D3D's handler ignores the vblank; then calls the
  registered callback `(source, context)`. CP interrupts (InsertCallback) are queued to the same thread. [V
  `kernel/video.cpp`]
- `XGetVideoMode` / `VdQueryVideoMode`: 1280x720, progressive, widescreen, HD, NTSC. [V]
- Pacing: `WaitForFlip(interval)` blocks until `max(next blank, previous flip + interval)` in the Swap hook. Before:
  120-500 frames/s on the title, movies at 6-7. A late frame used to wait for the next vblank (33 ms frames became
  50 ms); now a frame that missed its vblank is shown at once (no scan-out to tear). [V] (0b74438, 7f75b79)
- `fps_limit=60` (`SetFrameRateLimit`) waits 1 blank instead of 2 for interval 2: 60 fps at the game's normal speed,
  since its step is time-based (section 3.4). The vblank must stay at 60 Hz: scene/shader time counts vblanks.
  [V] (frame-time measurements; section 3.4)

### 4.12 XAudio2 render driver

- `XAudioRegisterRenderDriverClient(callbackPointer {callback, context}, ...)`: the driver calls each client once per
  audio frame (**256 samples at 48 kHz**), passing **a pointer to a cell holding the client's context** (not the context
  itself); the client answers with `XAudioSubmitRenderDriverFrame` from inside its callback. Frames: 6 planar channels of
  256 big-endian floats; downmix 5.1 to stereo on the host. [V]
- One driver thread plays the hardware: every period it asks each client in turn, sums, outputs. The first frame is
  requested one period after registration. Unregistering must wait for a running callback (recursive lock: a client
  may unregister from its own callback). Target ~6 queued frames (~32 ms). [V] (`kernel/audio.cpp`)

### 4.13 XMA hardware decoder

[V] (a6c40df, 89b18ac, `kernel/xma.cpp`, `apu/`)
- Registers are **little-endian** (lwbrx/stwbrx) at **0x7FEA0000 + 4·reg** (Xenia names): 0x1800 ContextArrayAddress
  (read **once** at init by 0x82503F40 → global 0x82B73AB0 [engine 1] and 0x82616A80 → 0x82B7FA70 [engine 2]: it must
  hold the **physical** address of the context array, little-endian, before those run); 0x1818 CurrentContextIndex
  (read by 0x82616DA8, compared with index ^ 0x200; 0 is fine); **Kick 0x1940**, **Lock 0x1A40**, **Clear 0x1A80**
  (+4·(i>>5), value 1 << (i & 31)); 0x1804 gets 2/3 around locks (ignored).
- Writers (hooked: run the original under one recursive mutex, then take the Kick/Lock/Clear bits out of the register
  words): engine 1 {0x82503FA0 create → Clear, **0x825047C8 Lock (enters critical section 0x82A3CC30 first — take it
  before our mutex to keep one lock order)**, 0x82504938 Kick}; engine 2 {0x82616B50 create, 0x82616D10 Lock,
  0x82616EF8 Kick}; 0x82617578 writes 0x1804 only.
- Context index = (MmGetPhysicalAddress(ctx) − ContextArrayAddress) >> 6 (& 0xFFFF).
- Engine 2 voice record (96 bytes): [0..63] local XMA_CONTEXT_DATA copy (copied to the hw context on kick; 0x82616DA8
  copies hw → local after lock), +64 hw context pointer, +68 output buffer, +72 work buffer, +80 u16 hw index, +84/+88
  input buffer 0/1 (zeroed when consumed), +92 MmMapIoSpace result (engine 1).
- XMA_CONTEXT_DATA (64 bytes, 16 big-endian dwords, Xenia layout): dword0 input buffer 0 packet count (12 bits, 2048-byte
  packets), loop count (8, 255 = forever), output block count (5, 256-byte blocks), output write offset (5); dword1
  **loop_subframe_end bits 12-13** (Xenia calls them "start"), **loop_subframe_skip bits 17-19**, sample rate (24/32/44.1/48
  kHz), output valid, stereo, subframe_decode_count …; dword2 input read offset (26 bits, in bits); dword3/4 loop start/end
  (bits); dwords 5-8 physical addresses; dword9 output read offset. First context seen: dword0 0x07800000, dword1
  0xB0800000 (stereo 44.1 kHz), output phys 0x046CD280 (virtual 0xE46CD280). [V] (title's XMASetLoopData 0x82617390)
- XMA2 packet header: frame count (6 bits), bit offset of the first frame (15 bits, from the end of the header),
  metadata (3 bits), packets of other streams to skip (8 bits). Frames: 15-bit length prefix, 512 samples; a
  trailing bit says whether another frame follows in the packet.
- Behaviour the title needs (differences from Xenia, from the title's code): the loop end frame plays up to its end
  subframe before the jump; **a frame split across input buffers waits for the next buffer instead of setting error 4
  (Wwise drops the voice on it)**; a frame the codec can't output still takes its slot as silence (512 samples); a
  starved context keeps the output it wrote. Output: big-endian 16-bit interleaved in 256-byte blocks into the
  [read, write) ring; full ring → output_buffer_valid = 0; consumed buffer → clear its valid bit, toggle
  current_buffer.
- Engine 1 (XAudio2 XMA voices, MmMapIoSpace path) and multi-stream (> 2 channels) contexts untested; hardware loops
  implemented, untested (frontend uses none). [?]

### 4.14 Bink

- Bink (RAD) lives in its own sections; `BinkDoFrame` 0x829EDCF8, `BinkWait` 0x829EA3F0, `BinkShouldSkip` 0x829EA388,
  `BinkNextFrame` 0x829EDD00, `RADTimerRead` 0x829EEF90 (ms timer on GetTickCount). [V]
- HBINK points at BINKFRAMEBUFFERS the game registered (`BinkRegisterFrameBuffers`): TotalFrames, YA width/height,
  CrCb width/height, FrameNum, then per frame set four BINKPLANEs (Y, cR, cB, A), each {Allocate, Buffer, BufferPitch};
  found inside HBINK by content (buffer width, then height rounded up). BT.601 limited range. [V] (`d3d/bink_display.cpp`)
- The game draws movie frames itself with **shaders built at run time** (YUV→RGB). Frame planes live in the 0xE window
  (+0x1000 offset). [V]
- Multi-track audio: the voice track is chosen per language (`sub_827F4400`). [V]

### 4.15 Wwise

- Init order and requirements in section 3.14. Banks: `init.bnk`, `global.bnk`, `<lang>\voice.bnk`,
  `<lang>\levels\*.bnk`; streamed media in `streaming.pck` with per-language sub-tables (language set through the file
  package API, `sub_82156A00`). Wwise's music bus mute (XMP) and XMA loops are the two hardware/system dependencies.
  [V]
- XMA work per writer call 2-9 µs; process CPU at the title ~350-420 % on the M1 Pro, same as without XMA. [V]

### 4.16 Statically linked zlib

zlib 1.2.x; addresses in section 3.9. Only `inflateInit_`/`inflateInit2_`, `inflate`, `inflateEnd` are called, so the
stream state can live on the host (the guest `state` field just marks the stream open). [V]
➜ Next game: find `inflate` by its error strings ("invalid block type", "incorrect header check" — both are in the SSA image, the latter referenced from `inflate`
0x825F24F8 [V] (ppcinfo find-string)) and replace it the same day; it removes most of the load time.

---

## 5. (C) Our toolchain and runtime

### 5.1 Runtime architecture in one page

- **XenonRecomp** translates `default.xex` into C++ (`ppc/`, generated, never edited, never committed: it derives from
  the game). Config `games/ssa/config/SSA.toml` + `games/ssa/config/SSA_switch_tables.toml`; XenonRecomp changes live in
  `patches/xenonrecomp.patch`, applied by the root CMake at configure time. Regenerating `ppc/` invalidates any
  prebuilt `libArkeyanRecompLib.a` (`-DARKEYAN_PPC_LIBRARY=<path>` skips compiling `ppc/` in extra build trees). [V]
- **Runtime** (`runtime/`, written from scratch, not Xenia/ReXGlue): guest 4 GB address space reserved as one block
  (guest A = base + A; big-endian accessors `g_memory.Read/Write`), kernel/XAM imports in `kernel/`, guest threads on
  host threads, XDVDFS reader over the ISO (`vfs.cpp`: game partition offsets of the plain, XGD3, XGD2, XGD1 layouts;
  directory entries form a binary tree, 0xFFFF = unused).
- **Hooking**: generated functions are weak. Override with `PPC_FUNC(sub_X)`, call the original through
  `PPC_FUNC_IMPL(__imp__sub_X); __imp__sub_X(ctx, base);`. Arguments in `ctx.r3..r10`, floats in `ctx.f1..`, return in
  `ctx.r3`; arguments 9+ are on the guest stack at **r1+0x54, +0x5C, …** (caller's parameter area). Imports:
  `PPC_FUNC(__imp__Name)`; `kernel/stubs.cpp` gives every import a weak fallback that logs `[stub] Name` once and
  returns 0 — see gotcha D5.
- `CreateHostThunk(PPCFunc*)` gives a host function a guest address: it takes a word from the zero padding (a run of
  ≥ 16 zero words) at the end of the code range and registers it in the function lookup table. Used for
  `XexGetProcedureAddress`. [V]
- Indirect calls to addresses XenonRecomp didn't treat as function starts land in `UnmappedFunction`: "Indirect call to
  unrecompiled guest code (ctr=… lr=…)" (section 5.3). [V]
- **Renderer "Option B"** (user decision): no PM4 / command-processor emulation. The game's own D3D keeps running and
  writes a ring nobody executes; hooks on draw/resolve/clear/swap/command-buffer functions rebuild each operation from
  the device state with plume (Metal / D3D12 / Vulkan). Section 5.6.
- Hosts: SDL3 desktop (`main.cpp`: window + input on the main thread, the title on its own threads), UWP
  (`host/uwp/`: CoreWindow, D3D12, Windows.Gaming.Input, XAudio2 2.9, a small SDL subset since SDL3 has no UWP).
- Audio: XAudio2 driver → `host/audio` (SDL3 stream or XAudio2), XMA through FFmpeg's WMA Pro decoder (section 5.8).
- Portal backends: emulated (figure dumps), HID (SDL hidapi, 1430:0150), USB via libusb loaded at run time (Xbox 360,
  experimental Xbox One GIP).
➜ Next game: the whole runtime is title-independent except `engines/lux/patches.cpp`, `language.cpp`, `inflate.cpp` addresses,
`kernel/xma.cpp` writer addresses, `d3d/` hook addresses and device offsets, the portal thunk ordinals, and
`host/launcher_games.cpp`. Plan one per-game file of addresses.

### 5.2 XenonRecomp: config, patches, missing instructions

Config facts (`games/ssa/config/SSA.toml`) [V]:
- save/restore helpers: `restgprlr_14` 0x825F5CE0, `savegprlr_14` 0x825F5C90, `restfpr_14` 0x825F647C,
  `savefpr_14` 0x825F6430, `restvmx_14` 0x825FF3D8, `savevmx_14` 0x825FF140, `restvmx_64` 0x825FF46C, `savevmx_64`
  0x825FF1D4 (byte patterns from the XenonRecomp README).
- `longjmp` 0x825F8430 (restores f14-f31/r13-r31 from the jmp_buf, calls RtlUnwind when an unwind function is set);
  `setjmp` 0x825FECA0 (mirror stores behind a hook-pointer check). On UWP the translated code imports `_setjmp`, which
  binds to api-ms-win-crt-private: redirected to ucrtapp's own.
- `invalid_instructions`: 0x00000000 (padding, 4 bytes); **0x825FFC00 (8 bytes: `__CxxFrameHandler` + FuncInfo in
  front of C++ EH functions)**; **0x829E6114 (8: `__C_specific_handler` import thunk + scope table in front of SEH
  functions)**; 0x8260CA80 (8: a custom handler + null data in front of 0x8260CA5C).
- `functions`: 146 hand/tool-sized entries for non-.pdata code (section 5.3); 210 switch tables.
- All `*_as_local` options true since 2026-11-04 (registers as locals, [xenonrecomp.md](../../tools/toolchain/xenonrecomp.md#registers-as-locals)), `skip_lr`/`skip_msr` false.

`patches/xenonrecomp.patch` (540 lines) [V]:
- **Memory barriers** (32022e9): `lwsync` → acquire-release fence, `sync` → sequentially consistent fence, `eieio` →
  release fence (`dmb ish` on ARM64; on x86 only `sync` costs an mfence). XenonRecomp emitted *nothing*; 1773 fence
  sites in the regenerated code. See gotcha D1.
- **Missing instructions** added: addc, addme, bdnzt, bdzf, dcbst (no-op), eqv, frsqrte, lbzux, lfdu, lfdux, lfsu,
  lfsux, lhau, lhzu, lhzux, lvehx, lwzux, stbux, stdux, stfdu, stfsu, stfsux, sthu, sthux, subfze, vaddsws,
  vcfpuxws128, vcmpgtuw, vctuxs (NaN/negatives → 0, ≥ 2^32 → UINT_MAX), vnor, vnor128, vpkswss(128), vrfip(128),
  vsel128, vsubshs, vsububm.
- **`blrl`** (was `__builtin_debugtrap()`): saves the return address in lr and calls through the lookup table;
  marks the FP control state unknown after the call.
- **`vupkd3d128` FLOAT16_2 / FLOAT16_4** (type 3: the halves of the last word → (x, y, 0, 1); type 5: last two words →
  (x, y, z, w)) via `_Float16`.
- ARM64 `setcsr`: `msr fpcr` needs a 64-bit operand.
- **XenonUtils XEX loader**: the compressed blocks can sum to less than the declared image size; the buffer is sized
  to the larger and the tail stays zeroed.
- Not in the patch but a XenonUtils trap: `ByteSwap` on a float converts it numerically (gotcha D7).

### 5.3 Function boundaries: find_switch_tables.py and packed leaves

`tools/find_switch_tables.py` (usage in its docstring) [V]:
- XenonAnalyse's fixed patterns don't match this compiler's jump tables (reordered slwi/addi, interleaved nops). For
  every `bctr` the tool finds the bounds check (`cmplwi crN, rI, N` + `bgt crN`, or `ble crN, dispatch; b default`,
  or a distant check upstream when several paths reach the dispatch block, with register aliases `mr`), then
  **concretely simulates** the instructions to the `bctr` for each index 0..N, reading tables from the image.
- It also emits `functions` entries for code outside .pdata that the analyzer sizes wrong: leaf functions containing a
  jump table (the analyzer stops at the bctr), `bdz/bdnz` chains (small switches on CTR), adapters ending in `b <next
  function>`, code reached only through function pointers in data (vtables) merged into a neighbour, and **packed
  leaf functions**: gaps without .pdata are split after every unconditional terminator, then pieces a branch or jump
  table crosses are joined again.
- **Packed leaf functions without .pdata** (27 of them, e.g. **0x826854A8**, size 4): vtable stubs/accessors only
  reached through pointers built at run time. Choosing a save slot called one → "Indirect call to unrecompiled guest
  code (ctr=0x826854A8 lr=0x8267EE10)". Fixed by the `packed_leaves` pass (d6c98bf). [V]
- Regenerate: `python3 -I tools/find_switch_tables.py private/image.bin private/xexdump.txt private/functions.toml
  [ppc/ppc_func_mapping.cpp] > games/ssa/config/SSA_switch_tables.toml`, paste the functions into `SSA.toml`, rerun XenonRecomp
  with `games/ssa/config/SSA.toml`. (With a previous mapping, only gaps where the analyzer missed a function change.)
➜ Next game: run the tool before the first boot, then treat every "Indirect call to unrecompiled guest code" as a
missing function boundary (look at the ctr address: is it inside .pdata coverage?).

### 5.4 XenosRecomp: shader translation patches

Submodule `thirdparty/XenosRecomp` = squidbus fork 4906992 (upstream PR #22 "MSL shader support", dual HLSL/MSL, MIT),
dxc from renderbag dxc-bin v1.8.2407. `patches/xenosrecomp.patch` (319 lines) [V]:
- A shader the translator or the Metal compiler can't handle is reported and **left out of the cache** instead of
  aborting the run (the Metal compiler error used to `exit(1)`).
- **PS bool address − 128**: pixel-shader booleans are registers 128-255, the CTAB numbers them from 0 (otherwise an
  undefined `b132`: most of the 173 initial Metal failures).
- **PSIZE0 vertex input** at location 16 (4 VS had no USAGE_LOCATIONS entry), **NORMAL as its own interpolator**.
- **Point sprites**: VS export 63 → `[[point_size]]` (only shaders that export one keep it: Metal rejects a point-size
  output in triangle pipelines); PS gets `[[point_coord]]` into the register `g_ParamGen` names (bit 31 = enabled).
- **`g_PositionScale`**: `oPos.xy = oPos.xy * g_PositionScale + g_HalfPixelOffset * oPos.w` — converts screen-space
  positions when the viewport transform is disabled.
- **DEC3N normals** (`tfetchR11G11B10` with the normal spec constant): three signed 10-bit components, X low.
- Results: 1111/1119 → **1118/1122** shaders translate and compile; failures were 4 PSIZE0 VS and 2 shaders without a
  CTAB (assert). Metal compiles cost ~0.5 s CPU each.
- **Not merged yet** (levelgfx branch, 727b189): Metal needs **`-fpreserve-invariance`** for XenosRecomp's
  `[[position]] [[invariant]]` to take effect (depth EQUAL passes); the shader cache must be rebuilt after it.
- Binding model of the translated shaders: push constants at `[[buffer(8)]]` (three device addresses: VS/PS/shared
  constants), texture/sampler heaps at `[[buffer(0..3)]]` (2D, 3D, cube, samplers); `g_Booleans` = VS bits 0-15, PS
  16-31; vertex inputs through USAGE_LOCATIONS (POSITION 0/15, NORMAL 1, TANGENT 2, BINORMAL 3, TEXCOORD0-3 4-7 /
  4+ 12+, COLOR 8/11, BLENDINDICES 9, BLENDWEIGHT 10, PSIZE 16). [V]
- Cache: `tools/shaders/build_cache.sh <iso> [<dump dir>]` extracts the archives (`xdvdfs.py`, `lsa.py`), adds the 81
  embedded containers from `private/image.bin` and the `SKY_DUMP_SHADERS` dump, runs XenosRecomp (asserts kept on)
  and writes `ppc/shader_cache.cpp` (Metal AIR + SPIR-V; DXIL on Windows), ~1.5 min on an M1 Pro, never committed.

### 5.5 plume (RHI) patches and limits

plume = `thirdparty/plume` (renderbag, MIT, RT64's RHI), pinned at e48a55b (no upstream tags), built by our own
`thirdparty/plume.cmake` (plume's CMakeLists always builds Vulkan and forces `CMAKE_OSX_DEPLOYMENT_TARGET` 10.15).
[V]
- `patches/plume.patch`: `setBlendFactor` (Metal setBlendColor, D3D12 OMSetBlendFactor, Vulkan dynamic blend
  constant, needs a value before any draw; dirty bit mask 0x1FF → 0x3FF); **a pipeline Metal rejects is reported as
  null** instead of one that crashes when drawn; `PLUME_D3D12_COREWINDOW` (UWP swap chain on a CoreWindow at a
  caller-chosen size); `D3D12_HEAP_TYPE_GPU_UPLOAD` missing from mingw-w64's d3d12.h (value from Agility SDK 1.613).
- `patches/plume_queue_order.patch` (applied after plume.patch): plume's Metal backend creates resources
  **`MTLHazardTrackingModeUntracked`** and keeps barrier fences per command list, so nothing orders command buffer
  N+1 after N; each command buffer now waits for the previous one's signal of a per-queue `MTLEvent`. (D3D12/Vulkan are
  already in order on one queue.) [V] (f25e80c)
- Limits: **`setFramebuffer` always ends the Metal render encoder** (bind only on change: ~3000 → ~28 render passes per
  level frame); no texture-to-buffer copies on Metal/Vulkan (compute readback pass); the swapchain's present
  completion handler captures the swapchain (destroy-after-present race); framebuffers cache sizes (rebuild after
  resize); `createShader` takes a metallib, not MSL; descriptor set N = `[[buffer(N)]]`, binding = `[[id(binding)]]`;
  Metal shaders need tier-2 argument buffers (Metal 3 / macOS 13).
- Xcode 26+ ships without the Metal compiler: `xcodebuild -downloadComponent MetalToolchain`.

### 5.6 The Option B renderer (d3d/ + gpu/)

[V] (`d3d/render.cpp` and commits f287249 → 7f75b79)
- Hooks: DrawVertices, DrawIndexedVertices, BeginVertices (snapshotted when called, sent at the next D3D call once
  filled), Resolve, ClearF worker 0x822ADDE0, Swap, Begin/End/RunCommandBuffer, Create*Shader, BinkDoFrame,
  ngl_WriteStencilRefDirect. Fences/callbacks are retired by `d3d/null_gpu.cpp`.
- EDRAM surfaces → host render targets keyed by (base tile, format, size); resolves copy into host textures standing
  in for the guest textures (marked `resolved`: the host copy is the only valid one; `swapRB` when copy_dest_swap).
- Guest textures are untiled, byte-swapped, converted (16-bit k_1_5_5_5 / k_5_6_5 / k_4_4_4_4 expanded to RGBA8,
  k_2_10_10_10 decoded on the CPU), keyed by base address/format/size/dimension, re-validated by hashing the base level
  every frame (`GuestContentHash`: 360-480 textures, 25-30 MB per frame, ~6% of the render thread). Fetch swizzles via
  texture views. 1D and 3D textures not yet.
- Vertex inputs a shader declares but the declaration lacks read a constant (0, or 1 for colours) as unbound fetches
  do. Rectangle and quad lists expanded. MRT draws need equal target **sizes** (comparing pointers dropped every MRT
  draw: 6fdbb56).
- Depth resolves (k_24_8, k_24_8_FLOAT) go through `ResolveDepthPS` into R32_FLOAT (X = depth). Exponent bias (resolve
  flags 26-31) not applied; k_8_8_8_8_GAMMA treated as linear.
- Command buffers: a draw made while recording is kept with the device bytes of every group dirty at some recorded
  draw (plus RT/depth/shader/declaration objects and the index buffer); a run writes those bytes over the device,
  draws, restores. Re-recording replaces the draws. The window scissor written by Begin is not replayed.
- Ring mirror (levelgfx branch, 87c0f6a, **WIP**): `ScanRing(device)` parses type-0, SET_CONSTANT (0x2D),
  SET_CONSTANT2 (0x55), LOAD_ALU_CONSTANT (0x2F) from the last cursor; resync after segment switches (0x822A2D78),
  KickOff, Begin/EndCommandBuffer, Swap. Open bug: after a Resolve the mirror keeps the resolve's internal values
  (section 4.3) and the title logo disappears. Next step written there: use the ring only for the NGL register set
  (0x2200-0x2202, 0x2205, 0x2104-0x2108, 0x210D-0x210E) plus literal constants.
- Frontend WIP (uncommitted at the time): "persistent state" after RunCommandBuffer + `MirrorRingWrites` around
  `sub_828FCBA8`, `SyncGpuState` before clears/resolves/swaps.

### 5.7 Performance findings

[V] (performance measurements and commits; M1 Pro)
- level_027 went from 2.4-3.3 fps to ~30 fps (29.8 average, 0 frames over 100 ms) through:
  1. bind a framebuffer only when it changes (Metal ended the encoder per draw: 160-227 ms GPU wait → 8-9 ms);
  2. buffer cache keyed by the vertex range a draw reads, first sighting staged, resident only when drawn again in a
     later frame (ring data never enters the cache), 32 MB blocks, LRU above 256 MB (0 MTLBuffers created per frame,
     was ~290; 6 → 25-30 fps; 6.0M verification checks, 0 mismatches);
  3. hashed sampler states (5% → 1% of the render thread);
  4. chunked submission (flush at a framebuffer change after 128 draws, or every 1024 draws) + the Metal queue-order
     patch (GPU wait 8.4-12.6 → 3.4-5.1 ms).
- A "2 frames in flight" attempt was **left out**: it freed buffers the GPU was still reading and broke readbacks.
- **Pipeline compiles**: ~20 ms each on a cold Metal cache; 33 at once for the first 3D frontend screen (667 ms frame),
  118 at level start (2.6 s frame). `pipelines.bin` (in `SKY_SAVE_DIR` or the pref path; magic "SSAP", versioned,
  records checksummed with XXH3, shaders by hash) is built by 4 threads while the intro movies play: render-thread
  pipeline time 4666 → 0 ms. **The `.app` bundle has its own Metal cache** (`$(getconf DARWIN_USER_CACHE_DIR)
  org.ssarecomp.SSARecomp`), so app users start cold while symlink runs share the user-wide warm cache. Bump
  `PipelineListVersion` when `KeyOf`/`CreatePipeline` change meaning. **Baseline list** (user's decision, 2026-10-23):
  `games/ssa/pipelines/baseline.zst`, built into the executable and merged with `pipelines.bin`, made by touring every
  level (`SKY_LEVEL`); it holds render states and the XXH3 of shader containers, no game content. Every `mission*.str`
  open waits until the whole list is built (`gpu::WaitForPipelineList`), so a level's first frames build none of it.
  `docs/runtime/gpu/pipeline_cache.md`.
- Host zlib: section 3.9. PhysicalToGuest cache: section 4.9. Late-frame flip: section 4.11.
- 2026-10-24 (`docs/runtime/gpu/renderer_threads.md`): level_027 had fallen to ~21 fps (18 at scale 2): `SendInheritedShadows` walked
  every register at each command buffer run (since `fee6b35`). Fixed, plus cached float constants, batched fetch
  constants and **one frame in flight** (Present waits for the frame before): 30 fps at scales 1 and 2, work median
  23-26 ms. The first level frame (150-290 ms) is mostly the game's main thread running its first update while the
  render thread idles in the job loop (`sub_82635740`, 1 ms waits on 0x82BB4F28).
- 2026-10-24, second pass: a **GPU thread** records and submits every command list (the game's render thread only
  translates state and queues copies of draws; producer upload memory tagged by frame), **texture threads** decode
  textures from snapshots; level_027 at scale 2: render-thread work 28 → 18-20 ms per frame (`docs/runtime/gpu/renderer_threads.md`).
- Remaining costs (before the 2026-10-24 work): IssueDraw ~40% of the render thread; constants hashing+swap (~114 samples; real fix = upload only
  the registers a shader uses, needs XenosRecomp metadata); texture hashing ~6% (needs write tracking / page
  protection like Xenia); first level frame = 2857 texture decodes (25.6 MB) on the render thread.

### 5.8 XMA through FFmpeg

[V] (a6c40df, 89b18ac, ff7a0ce, 85cef6c)
- FFmpeg's xma1/xma2 decoders take whole 2048-byte packets and **hold 4096 samples back until end of stream**, so input
  consumption ran ahead of output and hardware loops couldn't be honoured. Each XMA frame is instead wrapped in a WMA
  Pro packet of its own (4-bit sequence, 2 unused bits, 15-bit previous-split length = 0) for the plain **wmapro**
  decoder configured with XMA's parameters: block_align 4095, 512-sample frames, decode flags **0x10D6**, extradata
  bits-per-sample 16, channel mask 3 (stereo) / 4 (mono). The first frame of a stream is silence.
- Accuracy against FFmpeg's own xma2 decode: 75 dB (title music, 15 s) and 78 dB (menu sound), offset 576 samples
  (first frame + xma2's 64-sample skip). Windows build identical under Wine.
- Licensing: Homebrew's FFmpeg is GPL (`--enable-gpl --enable-version3`); the default build now makes a static
  LGPL-2.1+ FFmpeg 8.1.2 with only wmapro (`tools/ffmpeg/build.sh`, SHA-256 pinned) and configure refuses a GPL or
  non-free FFmpeg (`avcodec_license()` probe, or library strings when cross-compiling). `-DARKEYAN_FFMPEG=OFF` builds the
  null decoder.

### 5.9 Platforms: Windows, UWP / Xbox, licensing

[V] (commits 730f092, c430ac2, 357fc13, 30453bc, 3caf3eb)
- Toolchain: llvm-mingw 20260922 (ucrt). Homebrew's GCC mingw can't build `ppc/` (`__builtin_assume`,
  `__builtin_rotateleft*` missing). Whole `ppc/` builds for x64 in ~10 min at -j4.
- **x64 traps on integer division by zero (and INT_MIN / −1) in the generated `divw`/`divd`; arm64 returns 0.** [V
  platform note] — a latent crash on Windows that macOS hides.
  Fixed in `patches/xenonrecomp.patch`: the divides return 0 / INT_MIN like ARM64 (1133 lines of `ppc/`). [V] (platforms)
- Builds, tests and scripts per platform: `docs/getting-started/platforms/README.md` (`tools/build_windows.sh`, `tools/build_linux.sh`).
  Desktop Windows renders the game through Vulkan (no DXIL game shaders; D3D12 = present pass only). The cache's
  SPIR-V is smol-v: decode before `createShader` (the Vulkan path crashed lavapipe until then). [V] (platforms)
- `-march=btver2` (Xbox One Jaguar: AVX, no AVX2/FMA/BMI2) so one x64 binary runs on Xbox One and Series.
- fmt's `format.h` uses malloc/free without `<cstdlib>` under libc++/mingw: force-include it.
- **dxc on macOS cannot sign DXIL** (no dxil.dll) and D3D12 rejects unsigned containers: `embed_dxil.cmake` computes
  the container hash (MD5 variant of DXC's ComputeHashRetail; reproduces 15 dxil.dll-signed SDL shaders).
- Windows commits the 4 GB guest space up front (commit charge; an Xbox One game gets ~5 GB in all): fallback reserves
  only and commits 64 KB granules on first access from a vectored handler; kernel writes into reserved pages fail
  instead of faulting, so file reads commit their destination first; a race (two threads faulting on one granule)
  needed a retry. Faults otherwise use an unhandled-exception filter (VEH is desktop-only and sees GPU drivers'
  first-chance faults).
- UWP: SDL3 has no UWP; its headers `#error` under WINAPI_FAMILY_APP; `host/uwp/sdl_subset.cpp` stands in. Gamepad B
  must be handled (BackRequested) or the app is sent Home. `check_uwp_imports.sh`: all imports in windowsapp/ucrtapp
  (234). libc++'s `GetActiveProcessorCount` import replaced.
- Packaging: makemsix (msix-packaging) **deflates by extension and `.iso` isn't in its store list** →
  `tools/xbox/patches/makemsix-store-iso.patch`; bundles (`AddPayloadPackage`) not implemented in makemsix; osslsigncode 2.14
  signs MSIX; per-user self-signed cert (CN=SSARecomp Local). Dev Mode: set **App type: Game** or the app gets the small
  "App" budget.
- Licensing rules: never distribute the ISO, `ppc/`, the shader cache, saves, dumps (the pipeline baseline is the
  exception the user chose: hashes and render states only, `docs/runtime/gpu/pipeline_cache.md`);
  no outside code: Xenia, Unleashed Recompiled, Dolphin, RPCS3 and any other project are read-only references (we
  write our own); the Xenia-derived parts already in `runtime/` (`d3d/xenos.*`, `apu/xma_context.*`,
  `host/portal_usb.cpp`, `kernel/xmp.cpp`) keep their attribution. `THIRD_PARTY_NOTICES.md` ships in every
  package.

### 5.10 Debugging aids (environment variables, tools)

| Variable / tool | Use |
|---|---|
| `SKY_HEADLESS=1` | no video/dialogs |
| `SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null` | test runs through the real GPU path, no window/sound |
| `SKY_SAVE_DIR=<dir>` | fresh saves (and `pipelines.bin`) per run — always set it |
| `SKY_GPU_THREAD=0` | no GPU thread: command lists recorded on the caller, as before 2026-10-24 (`gpu/worker.cpp`) |
| `SKY_SETTINGS=<file>` | settings file instead of the player's `launcher.ini` (missing file: defaults) — set it in tests |
| `SKY_PRESS="start:40,a:45,…"` | scripted pad presses (150 ms; a b x y start back up down left right lb rb) |
| `SKY_PORTAL=emulated SKY_FIGURES=<dir> SKY_PORTAL_PLACE=new_14.sky` | emulated portal with a figure; `SKY_PORTAL_LOG=1` traces the protocol and echoes the title's portal log |
| `SKY_TRACE_FILES=1` | every file read (timestamped) |
| `SKY_RENDER_STATS=1` | draws and fps per 60 frames |
| `SKY_FRAME_TIMES=<file>` | per frame: present time, work without the vblank wait, draws (`tools/perf/frames.py`) |
| `SKY_GPU_SNAPSHOT=<prefix>` | presented image every 300 frames (offscreen only); `SKY_SNAPSHOT` = CPU frames (Bink stopgap) |
| `SKY_D3D_CAPTURE=<s>` | logs every draw/clear/resolve/swap of one frame (shaders, targets, declaration, streams, textures, main registers, stencil ref) |
| `SKY_DUMP_FRAME=<s> SKY_DUMP_PREFIX=<p>` (+ `SKY_DUMP_DRAWS`) | every resolve target (and every draw target) of one frame as PPM |
| `SKY_DUMP_SHADERS=<dir>` | keep every shader container created (run-time-built ones too) |
| `SKY_AUDIO_DUMP=<path>` | raw float32 stereo 48 kHz output |
| `SKY_BUFFER_STATS=1` | buffer cache activity |
| `SKY_LANGUAGE`, `SKY_VOICE_LANGUAGE` | text / voice language codes (en fr de es it nl sv no da fi) |
| `SKY_TOYBOX_SCRIPT`, `SKY_TOYBOX_SHOTS` | drive the Toy Box menu unattended |
| `python3 -I tools/ppcinfo.py` | dis / asm / callers / callees / strings / find-string |
| `tools/shaders/{lsa,ucode,xdvdfs}.py`, `build_cache.sh` | shader archives, census, cache |
| `ArkeyanFigureTest`, `ArkeyanPortalTest`, `ArkeyanRuntimeLauncherTest`, `ArkeyanRecompGpuTest` | unit tests (169 / 31 / … / 3 cases) |
| Fault report (POSIX) | thread name, sp/fp, pc/lr symbolized, frame-pointer walk, load address for `atos -l`, alternate signal stack |

---

## 6. (D) Weird stuff, traps and gotchas

Numbered so they can be referenced (D1 …). The ones that cost days are first.

- **D1. No memory barriers in the translated code → heap corruption on ARM64.** XenonRecomp emitted nothing for
  `lwsync`/`sync`/`eieio`. The title's spin locks release with `lwsync; stw 0`, a plain store the CPU may make visible
  before the critical section's stores; the next owner read stale free lists. Symptom: rare "Fault at guest address
  0x00000000" in `sub_828A2360` while the frontend loads, ~1 in 10 to 1 in 94 runs. Fix: real fences (1773 sites).
  x86 hides most of it. [V]
- **D2. "Pink/violet 3D" and "stencil EQUAL 2 never passes" were not rendering bugs**: NGL writes render state and
  D3D writes shader literal constants straight into the ring, so the device shadows lie (section 3.5). [V]
- **D3. RunCommandBuffer was a no-op → menus without backgrounds.** It runs 5-14 times per frame on menus; the engine
  caches one buffer per mesh/material and inherits almost all state. [V]
- **D4. Clears are draws.** ClearF (and BeginTiling/BeginZPass) clear through their own rectangle draw in 0x822ADDE0,
  invisible to draw hooks. Additive lighting buffers then accumulated stale data → white scene. [V]
- **D5. Weak stubs that "succeed" are worse than missing functions.** Returning 0 without writing outputs gave: country
  from stack garbage (XConfig), a muted music bus (XMP), "pending forever" XNADDR, a crash on an uninitialised
  achievement enumerator (guest 0x8), empty save names (vsprintf), unwritten TIME_FIELDS. Grep the log for `[stub]`
  after every new screen. [V]
- **D6. WaitForMultipleObjects on zero handles must fail**, not block (WideLoad after a level load). [V]
- **D7. `XenonUtils::ByteSwap(float)` converts numerically** (static_cast to uint32 and back): every
  `g_memory.Read<float>` returned garbage (mostly 0) — the capture showed every viewport as 0,0,0,0. Floats must swap
  their bits. [V] (f212648)
- **D8. The 0xE0000000 physical window is offset by +0x1000.** Bink planes read 4 KB too low until it was honoured.
  [V]
- **D9. Title music silent while sound effects play** = Wwise music bus muted because XMPTitleHasPlaybackControl read
  garbage. [V]
- **D10. Resolve wipes its own RB_COPY_* shadows** before returning (0x822A2550): read resolve parameters from the
  call arguments (clear stencil is the 10th argument, on the stack). [V]
- **D11. The shader setters were labelled the wrong way round** in the first symbol file (0x82290250 is
  SetPixelShader, 0x822905F8 SetVertexShader). Check labels against the object type bits. [V]
- **D12. Arguments 9+ live on the stack** at r1+0x54 (XamContentCreateEx's overlapped, NtCreateFile's options) and
  r1+0x5C (Resolve's clear stencil). [V]
- **D13. Packed leaf functions without .pdata** → "Indirect call to unrecompiled guest code (ctr=0x826854A8
  lr=0x8267EE10)" when choosing a save slot. [V]
- **D14. Shader containers are rewritten in place by Create*Shader** — hash them first, or the cache never matches.
  [V]
- **D15. Bink movie shaders are built at run time**: not in any archive; dump them once. [V]
- **D16. Depth EQUAL needs bit-identical positions** between the depth-only and MRT pipelines; Metal doesn't guarantee
  it unless invariance is preserved (`-fpreserve-invariance`). The terrain came and went between runs. [V] (/ fix ?)
- **D17. MRT draws dropped** because target textures (not sizes) were compared. [V]
- **D18. Vertex ring buffer reported as "rest of the ring"** in every fetch constant (0xAA5E0000, 2.56 MB). [V]
- **D19. 16-bit texcoord halves arrive swapped** from the 32-bit vertex swap. [V]
- **D20. Depth-only passes have no pixel shader**; skipping them loses the Z-prepass. [V]
- **D21. Reversed depth (GEQUAL, far = 0)**; the old "clear depth to 0 at every Swap" workaround had to go once real
  clears arrived. [V]
- **D22. The game renders the 3D scene at 1120x704**, not 1280x720; half-res passes at 560x352 with 4×MSAA. [V]
- **D23. EDRAM aliasing**: a colour target aliasing the depth buffer with write mask R rewrites the stencil byte. [V
  observed; emulation not done]
- **D24. NGL crashes on frames with nothing to draw** (no scene, or a scene whose render list is null) — happens at
  movie start. Two guards in `engines/lux/patches.cpp`. [V]
- **D25. Startup "20 s wait after START"** was STATE_GAME_FINALCHECKS, not input. [V] (state; cause ?)
- **D26. No frame pacing = 120-500 fps title** because fences retire instantly: Swap must wait on the vblank with the
  present interval (2 → 30 fps). [V]
- **D27. Yield spinning**: zero-length sleeps/yields are the job system's idle loop; 91% of the main thread in
  `sched_yield` until the back-off. [V]
- **D28. Quit crash**: SDL_Quit freed the audio stream while the kernel audio thread polled it (`pthread_mutex_lock+12`
  on a garbage pointer, report cut short by `_Exit`). Quit now exits at once without SDL teardown. [V] (fd05ed6)
- **D29. Weird visuals still seen by the user** (open):
  - the **Autosave notice shows the title's START glyph (white rounded box + ▷) zoomed huge and opaque** in the middle:
    PS `63F2334BD0EF3192`, blend `07060706` (src-alpha), identical shadow and ring state; likely a scale-up + fade-out
    animation whose alpha is lost [?];
  - **title background torn**: a flat blue region with a staircase (block-sized) edge over the animated clouds, the
    boundary moving between screenshots — suspected Bink texture read mid-decode, partial texture re-validation, or
    tiling order [?]; the Autosave dimmed background is split vertically at x ≈ 808 (lighter left) [?];
  - with French text some strings appeared in "random" languages; English text is the default until fixed
    (114fe5e) [?];
  - after the Toy Box re-places a figure, the HUD shows "Gill Grunt" instead of the nickname although both areas hold
    it [?].
- **D30. `perl alarm` is not a reliable bound on a run** (RULES.md; early runs saw exit code 142 = alarm reached,
  later runs kept going): use a background kill timer (`kill`, then `kill -9`). [V]
- **D31. The Mac falls into Maintenance Sleep** during long unattended runs (lid closed); only a user-activity
  assertion (`caffeinate -u -t 30`, renewed) keeps a full wake. [V]
- **D32. The app bundle has its own Metal cache**, so timings measured through the symlink (warm cache) lie about
  what users get. [V]
- **D33. FFmpeg's xma2 decoder holds 4096 samples back** until end of stream. [V]
- **D34. Error 4 on a split XMA frame makes Wwise drop the voice**; wait for the next buffer instead. [V]
- **D35. Engine 1's XMA lock enters a critical section first**: taking our mutex first deadlocks. [V]
- **D36. Homebrew FFmpeg is GPL** — linking it statically makes the binary GPL. [V]
- **D37. x64 `divw` by zero traps; arm64 returns 0.** [V] (note; generated divides guarded since the platforms work, section 5.9)
- **D38. Content "errors" in logs are probes**: `flags 0x13 -> 3` is the game checking whether a slot exists. [V]
- **D39. SDL's offscreen driver has no Metal view** and, without a graphics flag, SDL loads OpenGL on macOS (EGL fails):
  use `SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN`. `SDL_RenderDebugText` can't reach its Latin-1 glyphs. [V]
- **D40. One unexplained fault** in `sub_822AE460` ← scene traversal during `tfb.bik`, guest address 0x24, once in ~25
  runs. [?]
- **D41. The windowed "crash after slot choice"** reported once was most likely the quit crash (D28); headless saving
  works. [?]
- **D42. 32-bit index buffers**: D3D marks them with Common bit 31; the renderer assumes 16-bit (none seen). [?]

---

## 7. Open problems at the time of writing

1. **Level shading**: level_027 still presents dark/flat. The ring mirror (levelgfx 87c0f6a, WIP) explains the stencil
   state; its Resolve interaction must be fixed (section 5.6). Then: light pre-pass black (reads DepthMap, GBuffer,
   GBuffer2, VposParams), depth EQUAL invariance (patch on the branch), exponent bias, gamma formats.
2. **Frontend glitches** (D29): START glyph, torn title background, split dimmed background; frontend WIP
   (persistent command-buffer state + material packet mirror) uncommitted.
3. Unverified D3D names (ZPass, tessellated draws, state blocks, conditional rendering, EndCommandBuffer).
4. XMA: engine 1, multi-stream, loops untested in levels.
5. Figures: play time, upgrades bits, hats, heroic challenges, hero points unverified; nickname after re-place.
6. Performance: texture hashing, constant uploads, first-level-frame texture decode.
7. Windows/Xbox: D3D12 present path never exercised on real hardware; Xbox One viability unmeasured.

---

## 8. Where each fact came from (source index)

| Source | Content |
|---|---|
| `docs/runtime/gpu/d3d9ltcg_device.md` | device layout, resource header, draws, frame flow, hook plan |
| `docs/runtime/gpu/command_buffers.md` | command buffer cache, inherit tags, dirty mask bits |
| `docs/runtime/gpu/README.md` | plume layer, present path, limits |
| `docs/engines/lux/level_frame_lightprepass.md` | level_027 frame, clears, resolves, stencil, ring mirror |
| `docs/runtime/gpu/renderer_threads.md` | performance, vertex ring, pipelines, zlib, pacing |
| `docs/runtime/gpu/pipeline_cache.md` | pipeline baseline list, loading waits, the level tour (`SKY_LEVEL`) |
| `docs/engines/lux/languages.md` | languages, quit crash, saves probes |
| `docs/runtime/host/figures.md` | tag format, toy ranges, portraits, `uigameimage.str` |
| `docs/release/licensing.md`, `THIRD_PARTY_NOTICES.md` | FFmpeg licensing, notices |
| `docs/engines/lux/frame_pacing.md` | game time step (F), clamps, vblank-counted scene time, 30/60 measurements |
| `docs/runtime/host/runtime_launcher.md` | disc verification, release table, XDBF, build times, Xbox export design |
| port notes of the first port | d3d-re, gpu, launcher, platform, portal, progress (state machine), saves, shaders (LSA), xboxpkg, xma (registers) |
| XMA and command-buffer port notes | XMA engines and Wwise mute trace; command buffer replay |
| the frontend work (uncommitted then) | RunCommandBuffer dirty-mask semantics, material packet 0x828FCBA8, title/Autosave glitches |
| Original notes | engine names, job system, kernel fixes list, D3D facts, weird list |
| `games/ssa/config/SSA_symbols.txt`, `games/ssa/config/SSA.toml`, `patches/*.patch`, `tools/*` | addresses, XenonRecomp config and patches, tools |
| `runtime/**` comments | kernel, d3d, gpu, apu, host behaviour |
| git log (6130e1c … 0abc6b5) | commit bodies quoted above by hash |
| Re-checked for this document | XEX library versions (header 0x200FF), RTTI census, 0x82AA7BC0 / 0x82BB46D0 / 0x828FCBA8 callers, zlib strings (`ppcinfo.py`, small scripts over `private/`) |
