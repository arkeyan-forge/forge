# Lux: the engine of Skylanders: Spyro's Adventure

> **Purpose:** what is known about the Lux engine, where its hooks are, and which page answers which question · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

## What it is
SSA's binary is **"lux"** (C++ namespace `lux::`, 1087 RTTI names: 762 `lux`, 65 `ltl2`, 41 `XAI`), renderer **NGL**
(`ngl*`), RAD **Bink 1** movies, Audiokinetic **Wwise**, a job system ("JQ Worker", `ListSendModule`). It is fully
ported (SSA). **Giants is not Lux**: it is Alchemy ([../alchemy/README.md](../alchemy/README.md), proven by booting its
disc on 2026-10-25); whether Trap Team and Imaginators are Lux is unverified
([../../porting/franchise.md](../../porting/franchise.md)). So far Lux knowledge applies to SSA only.

Addresses in these pages are **SSA's**: *never reuse an address in another game*, re-find it with the clue given
([binary_layout.md](binary_layout.md), [sigmatch](../../tools/toolchain/sigmatch.md)). Tags: **[Lux]** engine
behaviour, **[SSA]** this build, **[Xbox]** platform.

### Structure of the engine [Lux]
- **Threads**: the main thread ("Game Master" in profiles), **"JQ Worker"** job threads (`JobWorker_Loop` `0x82635CF8`), the render job **ListSendModule** (`0x826320C8`), LOD workers, the portal thread (`sub_8264ABB8`), XAudio2/Wwise threads
  fed by the kernel audio driver, XMA work inside the Wwise thread. Names come from the MSVC `SetThreadName` exception (the only exception the title raises).
- **Job system**: `JobGroup_Wait` (`0x826360F8`) runs pending jobs on the caller while the group counters are non-zero. **Idle waiting is polling**: queues are polled with `NtYieldExecution` (`sub_82635A80` -> `sub_822DCD18`); LOD workers and
  spin-lock waits use zero-length `KeDelayExecutionThread`; `sub_82520638` also spins. Our kernel backs off ([threads_sync.md](../../runtime/xbox360/threads_sync.md)). The render thread idles in `sub_82635740` (`WaitForSingleObject(0x82BB4F28, 1 ms)` in a loop).
- **Locks**: `RecursiveSpinLock_Acquire` (`0x8213EFB0`): owner = `GetCurrentThreadId`, `ldarx/stdcx.` on `*(lock+12)`; released with `lwsync; stw 0` (**needs real fences**, D1). Small-block pool allocator `sub_82132418` / `sub_82132618` (`lwarx/stwcx.` spin locks).
  Main heap: `sub_828A25F8` free, `sub_828A2360` coalesce, `sub_828A1EC8` unlink ([gotchas.md](../../porting/gotchas.md)).
- **Boot**: intro movies (legal 4.95 s, `atvilogo` 10.4, `xpeclogo` 8.5, `tfb` 10.0) -> startup state machine (`STATE_GAME_FINALCHECKS` ...) -> frontend -> level load (`levels\<name>\mission.str`) ([boot_saves.md](boot_saves.md)).
- **Rendering**: 3D scene at **1120x704**, UI/composite at **1280x720**, present interval 2 -> 30 fps.
- **Time**: variable step `F += 0.3 * (dt*60 - F)` clamped to `]0, 3]` from `mftb`; scene and shader time count vblanks: 60 fps needs no game patch ([frame_pacing.md](frame_pacing.md)).
- **Disc formats**: `.str` = `strm` + one zlib stream of records, images `APKF`, shaders `.lsa` (`\0ASL`), text/levels `.lxb`, Bink 1 movies, Wwise banks/packages ([str_levels_loading.md](str_levels_loading.md)).
- **Saves**: XamContent mounted as `lux:`; `GLOBAL.DAT`, `SLOT.DAT`, `SLOT.NFO`.
- **One language index** drives text, UI, localized movies and (separately hooked) Wwise voices and the Bink voice track ([languages.md](languages.md)).
- **Portal and figures**: xam ordinals `0x4A1/0x4A2`, protocol version >= `0x011F` ([portal_title_side.md](portal_title_side.md)); tags are AES-128-ECB blocks with an MD5-derived key containing `" Copyright (C) 2010 Activision. All Rights Reserved. "` ([figures.md](../../runtime/host/figures.md)).

## Where the code is
- [`engines/lux/`](../../../engines/lux/README.md): the class-**b** engine hooks (NGL guards, the heap free, the movie
  player, light tiles, ultrawide targets), built when `ARKEYAN_GAME=ssa`.
- [`games/ssa/`](../../../games/ssa/README.md): SSA's addresses (`addresses.h`), adapter, languages, portal prompt.
- The hook classes (Xbox, XDK, engine, game): [../../architecture/hooks.md](../../architecture/hooks.md).

## How to test it
- `tools/test_all.sh --only ssa` (`--quick` for the short pass): SSA's unit tests and an isolated run to `level_027`
  ([testing.md](../../getting-started/testing.md#regression-harness-toolstest_allsh)).
- One level frame: `SKY_D3D_CAPTURE` + `SKY_DUMP_FRAME`, compared with the pass list in
  [level_frame_lightprepass.md](level_frame_lightprepass.md) ([debugging.md](../../porting/debugging.md)).

## Gotchas
- NGL writes render state and float constants **straight into the PM4 ring**: shadows lie, the ring is the truth; mirror it ([ring_mirror.md](../../runtime/gpu/ring_mirror.md), [ngl_renderer.md](ngl_renderer.md)).
- Meshes and materials cache **recorded command buffers** (64-entry cache), run each frame with inherit tags ([command_buffers.md](../../runtime/gpu/command_buffers.md)).
- A level frame = Z-prepass -> shadow maps -> G-buffer -> SSAO -> light pre-pass (omni, **sun tiles classified by a CPU job reading the depth resolve in guest memory**, **ambient cube rendered face by face**) -> main colour -> bloom/LUT -> UI ([level_frame_lightprepass.md](level_frame_lightprepass.md)).
- Clears are draws inside ClearF's worker; depth-only passes have no pixel shader; Bink shaders are built at run time; depth is reversed.
- The statically linked zlib is slow when recompiled: replace it with the host's on day one ([zlib.md](zlib.md)).
- **Heap**: a rare free-list corruption guard (`sub_828A25F8`); the indirect-call guard logs and skips bad targets ([stubs.md](../../runtime/xbox360/stubs.md)); particles and billboards are the suspected victim ([ngl_renderer.md](ngl_renderer.md)).
- Every trap of the SSA port, numbered: [gotchas.md](../../porting/gotchas.md) (D1-D44); every rendering bug: [render_bugs.md](../../runtime/gpu/render_bugs.md).

## Pages
| Page | Answers |
|---|---|
| [binary_layout.md](binary_layout.md) | Sections, RTTI, strings to grep first, how to find your way without symbols |
| [boot_saves.md](boot_saves.md) | Entry, intro movies, the startup state machine, the save manager |
| [ngl_renderer.md](ngl_renderer.md) | Frame structure (`nglListSend`, `ListSendModule`), scenes, the NGL ring writes, command-buffer cache, the two crash guards, billboards |
| [level_frame_lightprepass.md](level_frame_lightprepass.md) | The pass list of a level frame, sun tiles, ambient cube, light pre-pass |
| [str_levels_loading.md](str_levels_loading.md) | Disc files and formats (`.str`, `APKF`, `.lxb`, `.lsa`, `.bik`, `.bnk`, `.pck`), level loading, WideLoad |
| [zlib.md](zlib.md) | The statically linked zlib and its host replacement |
| [bink.md](bink.md) | Bink structures, YUV, run-time shaders, movie voice tracks (and where Bink 2 is) |
| [wwise_audio.md](wwise_audio.md) | Wwise init, banks, the XMP mute trap, XMA, languages of voices |
| [frame_pacing.md](frame_pacing.md) | The variable step (`mftb`), vblank-counted time, 30 vs 60 fps |
| [languages.md](languages.md) | `sku.lxb`, the language table, text/voice/movie split, hooks |
| [portal_title_side.md](portal_title_side.md) | The title's portal driver and the protocol it expects |
| [ssa_addresses.md](ssa_addresses.md) | Every engine function and data address SSA depends on, with its role |
| [engine_notes.md](engine_notes.md) | The archive: the 2026-10-23 knowledge base (cited as "engine_notes §n"), with a table section -> current page |
| [../../runtime/gpu/render_bugs.md](../../runtime/gpu/render_bugs.md) | Rendering bugs and their fixes |
