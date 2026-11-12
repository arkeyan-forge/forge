# Swap Force boot: what broke on the way to the title and to a level

> **Purpose:** what stood between Swap Force's disc and a playable level, the causes and the fixes · **Status:** current (2026-10-27) · **Tags:** [Xbox] [XDK] [Alchemy] [SwapForce]

What the boot work learned about *Skylanders: Swap Force* (`default.xex` sha256 `5d9a3bc3...4ac3`, title
`415608E9`). Tags: **[SwapForce]** this game only; **[Alchemy]** the engine Giants and Swap Force share; **[XDK]** the
libraries of XDK 21250; **[Xbox]** every game. Addresses are Swap Force's. Day 1 (identity, sigmatch, engine):
[status.md](status.md).

## What broke on the way to the title

### XenonRecomp (2026-10-26) [Xbox] [SwapForce] [V]
- **Instructions no earlier game used**: `vminsw`, `vsubuwm`, `vavguh`, `vminuh`, `vmaxuh`, `vcmpgtsw.` (296, 72, 67,
  12, 12 and 2 sites) and `mulhdu` (1). Added to `patches/xenonrecomp.patch` (per-lane SIMD, the same in either lane
  order; `mulhdu` through a 128-bit product).
- **C++ exception handling is back** (Giants had none): 5930 FuncInfo records (magic `0x19930522`, `LIBCPMT`), so
  `invalid_instructions` has SSA's three handler entries again, re-found: `__CxxFrameHandler` `0x82FF3068` (5938 sites),
  the `__C_specific_handler` import thunk `0x8307CE6C` (61 SEH functions; Giants linked its own copy), and the custom
  handler `0x830064A0` before `0x83006474` (the same code as SSA's `0x8260CA5C`).
- `tools/find_switch_tables.py`: **454 jump tables** (15 `bctr` left unresolved, all virtual tail calls in switch cases
  or through function tables: `0x8246C77C`-`0x8246C858`, `0x82D8751C`-`0x82D875EC`); 25586 explicit functions in pass 1,
  21 more in pass 2 (with pass 1's mapping), merged: **25587**. XenonRecomp: no unknown instruction, no unhandled table;
  **82017 functions** in `ppc_func_mapping.cpp` (Giants 23631), 324 files, 228 MB.
- The Bink 2 decoder's masked tables (Giants' four) need nothing by hand here: 5 of the 6 `bctr` of the `BINK` section are
  tables the tool finds, the sixth a call through a pointer [V] (the pass-1 list) [?] (no movie played yet).
- Regenerate: `games/swapforce/config/` laid out as the kit does, as for Giants ([day1.md](../../../docs/porting/day1.md), step 2).

### Imports (2026-10-26) [Xbox] [SwapForce]
- **44 imports** no earlier game had (the first link failed on exactly these: `private/swapforce/imports_missing.txt`),
  all implemented and added to `runtime/kernel/import_list.inc` (now 295 names, 218 implemented). The list and the file
  of each: [kernel_exports.md](../../../docs/runtime/xbox360/kernel_exports.md#swap-forces-extra-imports-44-all-implemented-xbox-swapforce).
  Check: `comm -23` of the XEX's import names (`private/swapforce/xexdump.txt`) against `import_list.inc` prints
  nothing [V] (2026-10-26). Signatures from Xenia (BSD) where it has them; the others read from the title's own calls.
- **The rule: an offline console with one local profile, and every output written** (gotcha D5). Families:
  - **XHttp (14) [V] (code read, not run)**: the title starts XHttp, then XAuth, opens an async session
    (`0x10000000`) and checks every result (`sub_82C8DD58`, `sub_82C8DEA8`, the `SendRequest` site at `0x82C8E570`
    logs `GetLastError` and gives up). Handles open as on a console without a cable; `SendRequest` fails at once with
    ERROR_WINHTTP_NAME_NOT_RESOLVED; no status callback is ever made, so nothing can wait for one. The last error is
    written to KTHREAD+0x160, where XAPI's `GetLastError` (`sub_82F48170`) reads it. Xenia fails `XHttpOpen` instead
    (ERROR_NOT_SUPPORTED): the fallback if a hang ever shows here. `XHttpCrackUrl` fails (not parsed).
  - **XAuth and tokens**: `XampXAuthStartup` fails with Xenia's 0x80158406, so the title leaves its online service off.
    `XamGetToken` (not in Xenia; `sub_82C8D488`: context, URL, length, token out, then probably an XOVERLAPPED) clears the
    token and fails synchronously with ERROR_NOT_LOGGED_ON without touching the fifth argument [?].
  - **XNet**: `XNetDnsLookup` completes at once, failed (status WSAENETDOWN, event signaled); WSA events are manual-reset
    kernel events (these three take no caller argument, read from the title's thunks).
  - **Live UI** (gamer card, player review, marketplace) and `XNotifyPositionUI`: nothing shown, success; their
    signatures have no XOVERLAPPED.
  - **XAM tasks**: `XamTaskSchedule` runs callback(message) on a new guest thread (as Xenia); the title schedules one
    task and closes its handle at once (`0x82F43E34`).
  - **Saves: `XamContentCreateEnumerator` lists the real packages**, not zero items: the title enumerates its saved
    games (type 1, 4 per call, `0x829D85F0`) before it opens one, so an empty list would hide existing saves.
    `XamEnumerate` now hands out items (`kernel/xam_enumerator.h`, shared with the achievement enumerator, still empty).
    [xam_content_saves.md](../../../docs/runtime/xbox360/xam_content_saves.md#calls).
  - **Console key**: XAPI signs and verifies a small header with `XeKeysConsole*` (magic `0x4A6F7368`; `sub_82F42CB0`
    verifies, `sub_82F42D30` signs; on a failed check it rebuilds the header). The "signature" is Xenia's fixed retail
    certificate plus the hash, so this console verifies what it signed.
  - **Gamer pictures**: none (`XamParseGamerTileKey` ids 0 and ERROR_INVALID_PARAMETER; `XamReadTileToTexture` clears
    the texture, ERROR_NOT_FOUND through its XOVERLAPPED).
  - **Kernel**: `MmQueryStatistics` fills the 104-byte struct (512 MB, title pages from the guest heaps);
    `IoDismountVolume*` and `FscSetCacheElementCount` succeed; `NtDeviceIoControlFile` fails with
    STATUS_INVALID_DEVICE_REQUEST. Its only codes in the title are XMountUtilityDrive's disk queries (`0x70000`,
    `0x74004`, `sub_82F50338`) on the cache partition, which the runtime does not open [?].
  - **Loader**: `XamLoaderSetLaunchData` stores up to 0x400 bytes where `XamLoaderGetLaunchData(Size)` read them.
  - `XamUserWriteProfileSettings`: accepted, not kept. Note: `XamUserReadProfileSettings` is still a weak stub (returns
    0, writes nothing) for every game: a lead if profile settings misbehave [?].
- Files: `kernel/net.cpp`, `kernel/xam_content.cpp`, `kernel/xam_achievements.cpp`, `kernel/xam_user.cpp`,
  `kernel/xam.cpp`, `kernel/memory.cpp`, `kernel/files.cpp`, `kernel/imports_giants.cpp` (launch data), and the new
  `kernel/imports_swapforce.cpp` (Live UI, tasks, XAuth and tokens, gamer pictures, console key). After this, `ninja
  -C build-swapforce ArkeyanRuntime` links.

### First boot runs (2026-10-26) [Xbox] [SwapForce] [V]
Run: an isolated, bounded run (`SKY_SKIP_MOVIES=1`, every test isolation variable, killed by PID after 60 s).
1. **Fault at guest address 0x4C in the CRT's heap creation** (`sub_82F45998`, called from `_xstart`): the recompiled code
   called the **import thunks as ordinary functions**. `tools/find_switch_tables.py` listed all 267 thunks
   (`0x8307CDEC`-`0x8307DE8C`, 16 bytes each) in `functions`, because Swap Force reaches them through data, so they looked
   like vtable targets; XenonRecomp then compiled each thunk's placeholder (`nop nop nop blr`) and direct calls went
   there: `NtAllocateVirtualMemory` returned "success" without writing the base. The tool now drops any function over an
   import thunk (SSA's and Giants' configs had none: unchanged). [Xbox] (any game)
2. **`NtQueryVirtualMemory` was a weak stub** (returns 0, writes nothing): SSA and Giants never called it; the XAPI heap
   creation asks it whether its region is committed. Implemented (`runtime/kernel/memory.cpp`): a heap allocation or the
   image is one committed region, anything else free. [Xbox]
3. Now the boot runs past the CRT: 14 threads (Alchemy's `igWin32StorageDevice disc:`/`hdd:`, `igFileRedirector`, six
   `JQ Worker` job threads, `igArchiveManager`), opens `\archives\loosefiles.pak`, reads its first 4 KB, then the main
   thread **spins on a GPU fence** (`sub_82F60738` / `sub_82F58460`, 100 % of one core): with no D3D hook nothing
   consumes the ring. Six XAM ordinals are asked through `XexGetProcedureAddress` and not provided (`0x305`, `0x30B`,
   `0xAFF`, `0xB00`, `0xB0B`, `0xB10`).

### Direct3D (2026-10-26) [XDK] [SwapForce] [V]
- XDK libraries 2.0.**21250**, linked as `D3D9` (not `D3D9LTCG`): the setters are **real functions** now, not inlined
  (`SetVertexDeclaration` `sub_82F51E68` is `stw r4,0x2FD0(r3)` and a dirty bit, `SetPixelShader` `sub_82F51A48`,
  `SetVertexShader` `sub_82F51C50`, `SetIndices` `sub_82F553B0`, `SetStreamSource` `sub_82F55290`, `SetTexture`
  `sub_82F5BED0`, `SetRenderTarget` `sub_82F55AF8`, `SetDepthStencilSurface` `sub_82F55E88`, `SetViewportF`
  `sub_82F55760`, `SetScissorRect` `sub_82F55198`, `SetRenderState_PRESENTINTERVAL` `sub_82F54740`; shape 0.97-1.00
  against SSA's). The renderer still reads the state from the device at draw time, so nothing changes for it.
- Found with `tools/xmatch.py`, now also with Giants as the reference (`python3 -I tools/xmatch.py .:giants swapforce
  ...`; SSA's tree `.` gives the names through its symbol file). Seeds: `VdSwap`'s and `VdInitializeRingBuffer`'s only
  callers. Every hook was then read side by side with Giants' (`tools/ppcinfo.py --game giants asm` / `--game swapforce
  asm`):

| Function | Giants | Swap Force | How | Confidence |
|---|---|---|---|---|
| D3DDevice_Swap | 823DA640 | 82F5D018 | only caller of `VdSwap` (and `VdGetSystemCommandBuffer`) | certain |
| D3DDevice_InitializeRingBuffer | 823D2298 | 82F59BC0 | only caller of `VdInitializeRingBuffer` | certain (not hooked) |
| D3DDevice_KickOff | 823D1C10 | 82F59840 | call graph (SSA names), 5/5 callees; same code, plus `r7 = 0` to BlockOnFence | certain |
| D3DDevice_BlockOnFence | 823D1D10 | 82F58460 | call graph (SSA names); one more argument (below) | certain |
| D3DDevice_InsertCallback | 823D1E08 | 82F58EF0 | shape 1.00, same size 0x280, same callees | certain |
| D3DDevice_NextRingSegment | 823D0DB8 | 82F58760 | shape 0.62 (the best), same code side by side (+0x2B3D flag, +0x42F0 segment, the call to BlockOnFence with 3) | certain |
| D3DDevice_DrawVertices | 823E03E0 | 82F651B8 | mapped callees 3/3, same size 0x418, 4 lines differ | certain |
| D3DDevice_DrawIndexedVertices | 823E07F8 | 82F655D0 | same size 0x478; takes a base vertex again (below) | certain |
| D3DDevice_Resolve | 823DED00 | **82F64AB0** | the public function, a wrapper over the worker `sub_82F63958` (mapped callees 4/4, shape 0.65 against Giants' Resolve) | certain |
| ClearF worker | 823DDF38 | 82F626B0 | shape 1.00 against Giants' and SSA's, same size 0x680, only constants differ | certain |
| D3DVertexShader_Init | 823DC7E8 | 82F527F8 | identical instructions | certain |
| D3DPixelShader_Init | 823DC5F0 | 82F51A10 | identical instructions | certain |
| D3DDevice_BeginVertices | not found | 82F64CF8 | shape 1.00 against SSA's `8229A850`; **not hooked**: its only caller is D3D's own `sub_82F5C4B0`, called from Swap | certain |
| Direct3D_CreateDevice | 823DB340 | 82F60FB0 | allocates 0x6080 bytes (128-aligned); its init `sub_82F6F8C0` matches Giants' by call graph | certain (not hooked) |

- **Device layout: Giants' (XDK 21076), unchanged.** Every offset the runtime reads was found in Swap Force's own code:
  the vertex declaration at **0x2FD0** (`SetVertexDeclaration`), indices 0x320C, render targets 0x3210-0x321C,
  depth-stencil 0x3220, scissor 0x32FC-0x3308, pixel/vertex shader 0x330C/0x3310, tile count 0x3344, present interval
  0x36B0, the fence words 0x2B10/0x2B14/0x2B1C, the ring cursor +0x30 (Resolve, ClearF, Swap, KickOff use the same
  displacements as Giants'; `tools/xmatch.py .. offsets` against SSA's setters gives +0x80 for every field from 0x2F68
  to 0x35EC, as for Giants). So `game::DeviceStateShift` stays 0x80 and `game::DeviceVertexDeclaration` 0x2FD0.
  New fields past Giants' end only (0x4578 in the draws, where Giants read 0x3690), which the runtime does not read.
- **Signature changes against Giants (XDK 21076):**
  1. `DrawIndexedVertices(device, primitive, baseVertex, startIndex, indexCount)`: **the base vertex is back** (r5,
     written as `VGT_INDX_OFFSET`, register 0x2102, before the draw; Giants wrote 0): `game::DrawIndexedBaseVertex =
     true`, as SSA.
  2. `Resolve`: the public function has **SSA's arguments again** (`device, flags, rect, destination, point, level,
     slice, clearColor, f1 clearZ, stencil at sp+0x5C, parameters at sp+0x64`) and calls a new worker with two more
     arguments (r6 = r7 = 0, the rest moved down by two): hook the wrapper, `game::ResolveHasLevel = true`.
  3. `BlockOnFence(device, fence, r5, r6, flags byte in r7)`: one more argument than Giants' (whose flag byte is r6);
     a fence of 0 returns at once. Our hook reads only r3 and r4: unchanged.
- **XMA** [XDK]: the same two engines (shape 1.00 each): engine 1 init `82FAB058` (reads `0x7FEA1800`), voice clear
  `82FAB0B8`, kick `82FABA50`, lock-all `82FAB8E0` (critical section `0x831106E4`, the first thing it enters); engine 2
  voice clear `82F9FAF0`, lock `82F9FCF0`, kick `82F9FED8`, and **its own init again**, `82F9FA20` (Giants had the read
  inline in a voice set-up; Swap Force's set-up `sub_8268B9F0` calls it).
- **Bink 2**: `BinkOpen` `83083260` (shape 1.00 against Giants'; calls the header reader `sub_83081CE8`), `BinkClose`
  `83084128` (0.94: sets HBINK+76, then frees, as Giants').

### Boot with the D3D hooks (2026-10-26) [SwapForce] [V]
Runs: `private/swapforce/run.sh <name> <seconds> ...` (the test isolation of docs/getting-started/testing.md; no shader cache, so every draw is skipped
and counted as "skipped (shader)" by `SKY_RENDER_STATS=1`).
1. **The fence spin is gone**: with KickOff/BlockOnFence/InsertCallback hooked the main thread runs; 30 fps from the
   first frame (`[video] present interval 0x2 seen (fps limit 30)`: Swap Force asks for interval 2, unlike Giants).
2. **Start-up, without movies skipped** (`d3d1`, 60 s): the `Language Screen` and `LegalScreen` threads, the archives
   (`shaders_xenon.pak`, `permanent_xenon.pak`, `essentialui.pak`, `legal.pak`, `gamestartup.pak`...), then the two
   logo movies: `[bink] open gsbp_atvi 1280x720, 311 frames` (a Bink 1 `.bik`: the `.bk2` is missing on the disc and
   the game falls back) and `[bink] open gsbp_vvisions 1040x620, 270 frames` (Bink 2). Not drawn (no shaders yet).
3. **Title, menu, a save, the opening movie and the first level** (`d3d3`, 170 s, `SKY_SKIP_MOVIES=1`,
   `SKY_PORTAL=emulated`, an SSA figure (Eruptor) on the portal, A presses from 40 s): the front end draws 353-355
   draws and 46 resolves a frame; the presses give the storage device, `create save: "SWAPFORCE_OPTIONS"`, then a new
   game `create save: "SWAPFORCE_0"`, `[bink] open fmv01 1024x620, 7897 frames (skipped)`, the tutorial level
   (`tutorial_a.pak`, `tutorial_a_characters.pak`, `ta_volcanointro.pak`...: 337-408 draws a frame), then **the game
   reads the figure and loads it** (`eruptor.pak`, `champ_l11_eruptor_bank*.pak`) and plays on at 30 fps (385-604
   draws a frame). No crash, no `[guard]`, no unrecompiled call in any run.
4. **Sound** (`d3d4`, 75 s, `SKY_AUDIO_DUMP`): -23 to -31 dBFS per second from the 3rd second on (SSA's average is
   -24.5): the XMA hooks work.
- **The XAM ordinals asked through `XexGetProcedureAddress`** (`0xAFF`, `0xB00`, `0xB0B`, `0xB10`, `0x305`, `0x30B`,
  and later `0x24`): **not needed**, left unprovided. The six are fetched together by a static initializer
  (`sub_830799B8` -> `sub_82FF13A0`, a table at `0x833C7730`); one missing marks the whole set unavailable and its
  wrappers (`sub_82FF1320`, `sub_82FF1350`, `sub_82FF1378`) return E_NOTIMPL / ERROR_CALL_NOT_IMPLEMENTED. `0x24` (and
  `0x50`) are looked up by `sub_82FED300` / `sub_82FED540` only when a system version check passes (`>= 0x200A3200`)
  and fall back to the old path when absent. [?] which XAM functions they are (party/social, from the XPARTY library, is the likely family).

## What broke on the way to a level

### The tutorial's level frame: the pass list (2026-10-27) [SwapForce] [Alchemy] [V]
Captured in the tutorial (`tutorial_a`, Eruptor on the ship's deck) with `SKY_D3D_CAPTURE=f2300 SKY_DUMP_FRAME=f2300`
(both aids on the same frame, [debugging.md](../../../docs/porting/debugging.md#frame-capture)); runs `cap3` and `cap7`
under `private/swapforce/runs/`. About 390-630 draws and 46 resolves a frame. Every surface is a tile range of EDRAM:
`tileNNN` = base tile, colour formats 0 `k_8_8_8_8`, 7 `k_16_16_16_16_FLOAT`, 14 `k_32_FLOAT`; depth `D24FS8` (format 1).
**The 3D scene is 1040x620, the frame 1280x720** (the public fact holds), and the scene uses **reversed depth through
the viewport**: depth cleared to 0, GEQUAL, viewports with MinZ 1 and MaxZ 0.2 (the world), 1..0 (full-screen passes),
0.1..0 and 0.2..0.1 (layers in front of the world).

| # | Pass | Targets (EDRAM) | Reads | Resolved to |
|---|---|---|---|---|
| 1 | Shadow maps, depth only: 2 draws (the player's character) at 256x512, then 4 cascades of 512x512 stacked (190-340 draws) | depth tile 000, 256x512 then 512x2048 | - | `k_24_8` @`B9A4F000`, @`B9ACF000` |
| 2 | Three 256x256 passes (a ping-pong over a noise texture) | `k_8_8_8_8` tile 000 | the previous one | @`B9ECF000`, @`ACDxx000` |
| 3 | Clear depth and stencil (z 0); 14-17 **HUD stencil masks**: the HUD's own quads with its textures, stencil 1, z written | depth tile 000, 1040x620 | HUD textures | - |
| 4 | Clear a `k_32_FLOAT` surface and resolve it (zeroes) | tile 1FB | - | @`BA1A3000` |
| 5 | **G-buffer**: clear, 90-200 draws (stencil 2), then decals (colour mask 7, no z write) | MRT tiles 1FB (albedo), 3F6 (normal), 5F1 (third), depth 000 | material textures | depth `k_24_8_FLOAT` @`B9F0F000` (x3), albedo @`BA437000`, normal @`BA6CB000`, third @`BA95F000` |
| 6 | One full-screen draw: packed depth and normal | MRT 1FB `k_8_8_8_8` + 3F6 `k_32_FLOAT` (**linear view depth**, negative: -32 to -65000) | depth, normal, third | @`BA1A3000` (RT0); RT1 later @`BABF3000` |
| 7 | Reductions 104x620, 104x62 (x2), 64x2, 64x1 | tile 1FB | step 6 | @`BAE87000`, @`BAE8F000`, @`BAE91000` |
| 8 | Clear (0, 0, 0, 0.55); 15 HUD panel meshes drawn into the scene (z 0.1..0, stencil 1) | tile 1FB, depth 000 | 64x2 | - |
| 9 | **Shadow mask**: clear white, 3 full-screen draws (cascade choice), two bilateral blurs (unnormalized fetches) | tile 3F6 `k_8_8_8_8` (over the linear depth) | shadow maps, linear depth | @`B9ACF000`, @`B9A4F000` |
| 10 | Tile 3F6 resolved as `k_16_16_16_16_FLOAT` though nothing drew it in that format (EDRAM reinterpretation, black on the host) [?] | - | - | @`BB127000` |
| 11 | **Deferred composite**: 2-3 draws, stencil NOTEQUAL 1 (not under the HUD), unnormalized fetches | tile 1FB, depth 000 | albedo, normal, third, linear depth, shadow mask, 64x1/64x2, environment cube, @`BB127000` | - |
| 12 | **The player's character "x-ray"**: clear a depth surface at tile 3F6 (over the shadow mask), the character's depth, then its colour, alpha from the linear depth (visible where the scene hides it) | tile 1FB, depth 3F6 | linear depth, cube | @`BAE93000` |
| 13 | Forward draws: background, a pass into 3F6 `k_32_FLOAT` with depth 5F1, fog | tiles 1FB, 3F6, depth 000 / 5F1 | linear depth, cube, @`BAE93000` | @`BAE93000`, depth again, linear depth again @`B9A4F000` |
| 14 | **Bloom**: 520x310 bright pass and blurs, 260x155 (x3), composite; 520x620 and 520x310 (a second glow); particles (z test); a 256x16 colour table | tile 1FB | the lit scene | @`B9CE3000`, @`B9D8D000`, @`B9F77000`, @`B9A4F000` |
| 15 | Tone map with the colour table (stencil from depth 3F6), then one 1040x620 pass (9 fetches: FXAA 3.11, the public fact [?]) | tile 1FB, then tile 000 | the scene, the table | @`BA437000`, @`B9A4F000` |
| 16 | **Frame**: the scene stretched to 1280x720, the HUD (22-28 draws, clears depth tile 530) | tile 000 1280x720, depth 530 | the scene, HUD textures | the front buffer @`AC385000`, then Swap |

The HUD stencil masks (3) are why a dark shape sits under the HUD portrait in the scene's own targets: the lighting
skips it and the HUD covers it.

### What broke (2026-10-27)
1. **The whole 3D view a colour wash** (yellow, orange, dark red, with a faint silhouette of the character; snapshots
   `private/swapforce/runs/png3/grid.png`). Cause: **inverted viewport depth ranges**. The capture shows the world drawn
   with `viewport ... z 1.00..0.20` into a depth buffer cleared to 0 with GEQUAL; `PrepareDraw` swapped MinZ and MaxZ when
   MinZ > MaxZ, which inverted every depth: the sky dome, drawn last at the far plane, won every test. Evidence: the
   scene depth resolve was 1.0 everywhere (run `cap1`, files `d_77r`/`d_78r`), the G-buffer only showed the sky.
   Fix: keep the inverted range (`runtime/d3d/render.cpp`; Metal and Vulkan viewports take MinZ > MaxZ [V] on Metal,
   D3D12 [?] not run). The depth resolve then holds 0.2 to 1.0 (median 0.226) and the G-buffer the ship's deck. [Xbox]
   The code changes nothing for a game that never sets MinZ > MaxZ; SSA and Giants were not captured for it, but their
   levels draw right with the swap, which suggests they never do [?].
2. **The lit scene black** (only the emissive lava and particles; run `cap2`, `private/swapforce/runs/cap2_snaps.png`).
   Cause: XenosRecomp ignored the fetch instruction's **`texCoordDenorm` bit** (unnormalized, texel coordinates). The
   shadow-mask blur, the deferred composite and FXAA fetch at the pixel position (`abs(vPos)`, in texels); read as
   0..1 they sampled one corner texel. Evidence: the blur's output was one value, (239, 0, 1), over the whole frame
   (`cap3`, file `d_109r`) though its input varied (`d_107r`). Fix: `tfetch2DDenorm` / `getWeights2DDenorm`
   (`patches/xenosrecomp.patch`, [shaders.md](../../../docs/runtime/gpu/shaders.md)). 19 of Swap Force's 556 shaders use the
   bit (SSA 6 of 1113 dumped ones, its soft-particle depth fetches; Giants none of 4444). [Xbox]
3. **The lit scene a zoomed top-left quarter of the G-buffer** at resolution scale 2 (`cap5`: the deck in the G-buffer,
   the lava wall in the composite: `private/swapforce/runs/cap5_gbuf_vs_lit.png`). Cause: the texel coordinates are guest texels, and a resolve's host
   texture is scale x its guest size. Fix: the runtime marks the sampler registers that hold a resolve
   (`gpu::DrawDesc::scaledTextures`, from `HostTexture::resolved`) in a new shared constant `g_ScaledTextures` (+312);
   an unnormalized fetch of such a texture multiplies by `g_ResolutionScale` (`runtime/gpu/draw.*`,
   `runtime/d3d/render.cpp`). [Xbox]
- After the three: the tutorial looks like the game: the ship's deck with its crates, Eruptor lit and glowing with his
  shadow, the lava canyon, the bloom, the HUD (`private/swapforce/runs/cap7_snaps.png`, `cap7_s5.png`; the shader cache
  rebuilt from the same 556 dumps).
- Also new for this: `SKY_D3D_CAPTURE=f<N>` and `SKY_DUMP_FRAME=f<N>` (the same frame by swap count), the dump's file
  number on each `[dump] resolve` line and raw statistics of float targets
  ([debugging.md](../../../docs/porting/debugging.md#frame-capture)).

### XMA: the freeze ~58 s into the tutorial (2026-10-27) [Xbox] [XDK] [SwapForce] [V]
1. **Symptom**: every run, in the Greeble ships' cutscene (~97 draws a frame), the picture stopped and the sound went on.
   `sample` (run `x3`): one guest thread at 100 % in the Wwise voice service `sub_8268CE70` and `sub_82FA0000`, no
   `Sleep`; the main thread waiting.
2. **What the thread waits for** (the asm of `sub_8268CE70`): it services a voice of several XMA contexts (the count at
   voice+280). After `Xma2_VoiceKick` (`sub_82F9FED8`), when no context gave samples, it spins at `loc_8268D3CC` on
   `sub_82FA0000` (the samples available: output write offset minus read offset, from the hardware context) for every
   context not finished, until the decoder writes some. Nothing kicks again inside that loop.
3. **Which contexts**: a watch thread dumping the 320 contexts every 5 s (run `x7`, a temporary debug build) showed one
   voice of **4 stereo contexts, 5 to 8** (input buffers 0x800 apart: packets 0..3 of one buffer, 16, 15, 14, 13 packets:
   an 8-channel stream in 4 interleaved XMA2 streams), from the moment the level starts (~25 s). Contexts 5, 6 and 7
   had **never been kicked** (output empty, input valid); 8 had been kicked once. The service thread spun from then
   on; the cutscene is only where the main thread starts waiting for it.
4. **Cause**: the XDK's XMA voice code writes the Kick (and Clear, Lock) register **once per context of the voice**,
   `1 << (index & 31)` to the word `index >> 5`, a plain store each time. On the console each store reaches the
   decoder. Here the registers are memory read after the hooked writer returns, so of the contexts that share a word
   only the last store survived: 5, 6 and 7 were never decoded. A voice of one context (or of contexts in different
   words) loses nothing, which is likely why SSA and Giants (the same code, both engines) never showed it [?]. [Xbox] [XDK]
5. **Fix** (`runtime/kernel/xma.cpp`): each hooked writer also reads the voice it was given (r3: the context count at
   +0, the 96-byte records at +8, each with the context's address at +64 and its index at +80) and adds what the writer
   stored for it: a kick for every context of the voice (`Xma1/2_VoiceKick`), a clear for every context the writer
   created (`Xma1/2_VoiceClear`: the record's address was 0 before). Locks need nothing (decoding is synchronous). No
   per-game constant: the layout is the XDK's, the same in the three games.
6. **Evidence** (run `fix1`, 175 s, the testing.md recipe): the tutorial plays through the cutscene and on to the fight
   with the Greebles, then loads the next part; frames to 5040 at 30 fps, no fault. Snapshots
   `private/swapforce/runs/fixsnap/s_t90.png` (the ships over the deck), `s_t150.png` (13 coins, Eruptor fighting),
   `s_t170.png` (the loading screen of the next zone). The swapforce harness runs 100 s again (`tools/test_all.sh`).

### Text and voice language, the intro movies (2026-10-27) [Xbox] [Alchemy] [SwapForce] [V]
**What the disc holds** [V]: **two languages, English and French, for the text and for the voices** (the disc is En,Fr; the
title's own `AllowedTextLanguages`, `AllowedMovieLanguages` and `AllowedAudioLanguages` lists, the strings of `sub_82BB4688`,
say which it allows). Read from the disc's file list and the paks:
- Voices: 112 `*_lv_en.pak` + 112 `*_lv_fr.pak` (level voice banks), 93 + 93 `*_en.pak`/`*_fr.pak` (`streamed_vo_<zone>`,
  `<scene>_genbank`, `loaded_vo_*_bank_v`), a few more pairs (`_ww_`, `_vo_`, `_ro_`, `_pv_`, `_hc_`, `_gb_`, `_fg_`). No other
  language suffix exists.
- Text: the UI packages list both files (`languageselect.pak`, `prompt_text.pak`: `lang_file` entries `ui/<name>_en.igz` and
  `ui/<name>_fr.igz`); there is a `LocalizationData.xmlb` reader (`sub_82B14E38`) and a "Language Screen" thread
  (`languagestartup.pak`) at start-up.
- Movies: the 50 story, zone and cutscene `.bk2` have 5 Bink audio tracks, ids `0 1 2 5 6`; the 32 `Soul_Gem_*_Mix_360` ones
  7 (`0 1 2 5 6 7` and one more); the logos 3 or 4 (ids 0-2/0-3). 5 is the English track and 6 the French one (below).

**How the title picks** [V]: one language for everything, read through **`sub_823EFC68`** (`Language_Get`): it maps `XGetLanguage`
(`sub_82F41C48`: `ExGetXConfigSetting`, category 3, setting 9) to an `IG_CORE_LANGUAGE` value (6 English, 8 French, 9 German... the same numbers as
Giants') and returns it in r3 (a byte at `0x83403DB4` forces the value at `0x83125BE0` instead: unused here). Its readers are
the sound package opener, the localization data, the license text, the `SwapForce.LanguageAbbreviation` script variable.
The abbreviation (`en`, `fr`, `de`... from the table at `0x83125BE8`, 12 bytes an entry) builds the file name.
- **Text**: the console language does it, no hook: `--language fr` (`SKY_LANGUAGE`) gives French menus, difficulty names,
  prompts and the "PASSER" of the opening movie (GPU snapshot of the difficulty screen: "DIFFICULTÉ: FACILE, MOYENNE,
  DIFFICILE, CAUCHEMAR", "Sélectionner / Retour"). **Every other language falls back to English, text and voices**
  (`--language de`: the title shows "PRESS START", the voices are `*_en.pak`): the host accepts the code, the title does not
  have it. The "Language Screen" is the title's own start-up screen and is not shown with a console language it accepts.
- **Voices**: the sound package opener **`sub_825F5B48`** builds `<set>_<abbreviation>.pak` (the strings `%s/%s_%s.pak` and
  `sounds-lowmem:/%s_%s.pak`, found by `find-string`), once per package of a sound set (its caller `sub_825F5AE0`); a package
  with the "localized" flag (byte +27) gets the language suffix. Giants' `Language_Region` and `Language_LoadPack` have no
  counterpart: Swap Force has no per-archive language pack, so `tools/xmatch.py .:giants swapforce shape` finds nothing
  for them (the best candidates score 0.18-0.57; only the movie start is a plausible match, 0.55, and it was checked by
  hand) and the three hooks below were found by strings and the call graph.
- **Movies**: the movie start **`sub_825A71D0`** (r3 the movie, its description at +32) reads the description's language track
  id at **+56** (5 with an English console, 6 with a French one [V]: runs) and its mode at +96 (1 for the story movies: the
  tracks `0 1 2` and that id), and gives them to `BinkSetSoundTrack` (`sub_8307EC40`, shape 1.00 against Giants'
  `sub_82150BF0`) just before `BinkOpen`.

**The hooks** (`games/swapforce/language.cpp`, `addresses.h`): `Language_SoundPak` raises a flag while the original runs and
`Language_Get` answers with the **voice language's** value while it is up (the text and everything else keep the console's);
`Movie_Open` puts the voice language's track id (5 or 6) in the description when it holds the text's. Setting
`voice_language` (`--voice-language`, `SKY_VOICE_LANGUAGE`; unset = the text's language, as the game does) takes `en` or `fr`;
any other code is refused ("unknown voice language ... using English"). Logs: `[language] sound packages: voices fr (8),
text 6` and `[language] movie voice track 6 (fr), the text's was 5`.

**Proof** (2026-10-27, four isolated runs of 125 s from the logos to the opening movie `fmv01` and the tutorial, an A every 2 s,
`SKY_TRACE_FILES=1`, `SKY_AUDIO_DUMP`; `private/sfparity/voices.sh`):
| Run (text / voices) | Sound packages read | Movie track | Voice banks |
|---|---|---|---|
| en / en (no setting) | `*_en.pak` (16 opens) | 5 | `streamed_vo_en`, `loaded_vo_common_bank_v_en`, `streamed_vo_tutorial_a_en`, `ta_*_genbank_en` |
| en / **fr** | `*_fr.pak` (17 opens, 11 reads of the tutorial's) | **6** (the text's was 5) | the same set in `_fr` |
| fr / **en** | `*_en.pak` | **5** (the text's was 6) | the same set in `_en` |
| fr / fr (`--language fr` alone) | `*_fr.pak` | 6 | the same set in `_fr` |
The audio dumps corroborate it for the movie: the loudness envelope (25 ms) of the last 30 s of each dump, aligned within
+-6 s, correlates 0.51 (en/en with fr/en voices) and 0.47 (en/fr with fr/fr) between the pairs that share a **voice**
language and 0.27-0.33 between the pairs that differ, whatever the text. [?] Nobody listened to the French lines, and the
`champ_*_lv_*` figure voice banks were not reached in these runs.

**Intro movies** [V]: `[video] skip_intro_movies=1` (settings file; `SKY_SETTINGS`) ends the start-up logos at their first frame
through `BinkOpen`: the adapter's `IntroMovies` are `gsbp_atvi` (the Activision logo, a Bink 1 `.bik`: the `.bk2` is missing
on the disc, the game falls back to it), `gsbp_alchemy` (the other `.bik`, never opened in the runs: [?]) and
`gsbp_vvisions` (Bink 2). The story and zone movies (`fmv01`..`fmv14`, `<zone>_intro`...) always play. Run (settings file
with the key, `--language fr`, the same presses): `[bink] open gsbp_atvi ...`, `[movie] skipped gsbp_atvi`,
`[movie] skipped gsbp_vvisions`, then the title, the save, Story Mode, the difficulty screen at 20 s (in French) and
`[bink] open fmv01` (played) at 30 s with "PASSER" on the picture. Without the key the same run plays both logos
(311 and 270 frames) first.

### Ultrawide, frame rate, glyphs, texture packs, icon, saves (2026-10-27) [Xbox] [Alchemy] [SwapForce]
What the parity rows of [parity.md](parity.md) rest on; the runs are the tutorial's recipe, isolated, macOS Metal.

**Ultrawide** [V] (code: `games/swapforce/ultrawide.cpp`, constants in `addresses.h`; the shared part and the tests:
[ultrawide.md](../../../docs/runtime/gpu/ultrawide.md), "Swap Force"). Giants' hooks are not in this engine version
(`tools/xmatch.py shape`: 0.12-0.29 for every one). Found with the store watch ([debugging.md](../../../docs/porting/debugging.md),
"Finding who writes a value": a build that logs the guest functions storing 1040, 620, their floats and 16:9, ~8 minutes
to build here, `private/watch.h`) and the call graph:
- **The scene is 1040x620 because of two scales.** `sub_8258EBE8` computes a render-target pass's size: the display's
  (the object at `[0x8340C304] + 2960`, +16 width, +20 height: 1280x720) times the pass's scales (floats at +40, +44:
  0.8125 and 0.861), stored at +88 and +92. A pass with a parent reads the parent's size instead, so the half (520x310),
  the quarter (260x155) and the reductions (104x620...) follow. `sub_8258F190` builds the surface from it
  (`sub_824DCDF0`), `sub_825138F8` lays the EDRAM out. The hook widens a pass that comes out 1040x620 after the original.
- **The camera.** `sub_82B992A8` builds the world camera's projection from the aspect in its viewport object (`[camera +
  44] + 8`, 16:9, written once by `sub_82BABF78` / `sub_82BABA48` from the display's size). The hook puts the window's
  aspect there for that call only: the full-screen composite is a 16 x 9 world-unit quad seen by a camera that reads the
  same field, so changing the field for good shrank it to 74 % of the frame (seen, then fixed). Hor+ (the vertical field
  of view stays).
- **The composite** is one indexed quad (6 indices) into the frame, not Giants' 4 vertices: `GAME_ULTRAWIDE_COMPOSITE_INDEXED`
  in `addresses.h` (`d3d/render.cpp`); the HUD draws after it into the same 1280x720 frame and is centred as in the other
  games.
- **Limits.** The HUD's dark stencil masks are drawn into the scene (pass 3 and 8 above: the lighting skips them), in the
  scene's own space: in a wide scene the portrait's mask is not behind the portrait any more (a black shape left of it).
  Not fixed: it needs those draws centred like the frame's.

**Frame rate** [V] (`--fps-limit`, [settings.md](../../../docs/getting-started/settings.md); solved 2026-10-27): the title asks for
present interval 2 (`Gfx/@presentationInterval`) and **reads that interval from its own device** (`D3DRS_PRESENTINTERVAL`, offset
14000 of the device, read by `sub_82F5CB58` and the frame code): at 2 the engine steps its logic by a fixed 1/30 s and waits two
blanks in its own D3D code, whatever the host does at the swap (so lifting the host's wait, as for SSA, changed nothing: 29.8
fps). At 1 it times its frames. The fix is in the swap hook (`d3d/render.cpp`): with `--fps-limit 60`, and
`game::Adapter::presentIntervalPacesGame` (Swap Force: true), a 2 in the device becomes 1 before each swap. Limit 30 is untouched.
Looked at and ruled out on the way: no `Sleep` or timed wait on the main or render thread (`KeDelayExecutionThread` is only the
workers'); the D3D helper thread (`sub_82F67D20`) waits 30 ms on a fence event but is a watchdog that never swaps; the two
`igRenderContext` threads hand a frame to each other by semaphores (`sub_823F23A4`).
Proof (macOS Metal, `--vsync 0`, scale 1, load average 25-90 from other jobs, so the fps are floors): a probe on the engine's
stopwatch read (`sub_823ED7D0`, the call in the frame function `sub_82515D68`'s helper `sub_8250F068`, which returns the frame time
the game steps by; temporary, not in git) logged frames and game seconds every 5 s wall:

| Level | Limit | fps (`[perf]`) | Frames / 5 s | Game seconds / 5 s |
|---|---|---|---|---|
| tutorial | 30 | 29.7-30.0 | 147-151 | 4.99-5.03 (the value is a constant 1/30 s a frame) |
| tutorial | 60 | 50.3-53.9 | 241-293 | 4.99-5.03 (measured frame times) |
| `barrens/brn2` | 30 | 29.9-30.0 | 147-150 | 5.00-5.02 |
| `barrens/brn2` | 60 | 52.9-56.1 | 257-282 | 5.00-5.01 |

The game's clock keeps 1.00 game second per wall second at both limits; 60 shows 1.8x the frames. (Before the fix the same
probe read a constant 0.03333 at 60 as well, 150 frames per 5 s.) A real 60 needs a machine that is not loaded: the run shows
the pacing is gone, not the ceiling.

**Glyphs** [V]: the texture dump holds no font atlas because the game composes strings on the CPU; the buttons come from the fonts' `button` images, redrawn in guest memory (`glyphs::MemoryImage`): [input.md](../../../docs/runtime/xbox360/input.md), "Glyphs", [art.md](art.md), "Button pictures". (Negative result of the first pass, superseded 2026-10-27.)

**HD texture packs** [V]: `SKY_TEXTURE_DUMP=1` wrote 318 textures to `<data>/Swapforce/textures-dump/` by the tutorial; a
solid-colour 872x400 DDS named as the dump file of the title logo, in `<data>/Swapforce/textures/`, was found (`[texpack] 1
replacement textures in .../Swapforce/textures (0 invalid, 0 ignored)`) and the legal screen showed it.

**The icon** [V]: Arkeyan-Builder's `BundleIcon` takes `default.xex`'s XDBF title icon, whatever the game: Swap
Force's is a 64x64 PNG ("Skylanders SWAP Force", read with `ReadTitleMetadata`), and sips and iconutil turn it into a
`.icns`. Nothing game-specific is missing (`game.txt`, `art.json` and the kit's per-game `Info.plist` are there). A whole
kit install with the icon in the Dock was not run [?].

**The portal prompt and the saves** [V]: with an empty emulated portal the run logs `portal prompt: magic moment state 5`,
`prompt for player 1`, `open (0 figures, emulated portal)`; the saves are `SWAPFORCE_OPTIONS` and `SWAPFORCE_0`
(`<saves>/00000001/<name>/<name>.xsg`), each rewrite keeping the previous one in `<saves>-backup/<time>/`.

### The scene at full resolution (2026-10-27) [Alchemy] [SwapForce] [V]
The user's decision (`office/decisions.md`): the 3D at the frame's size by default, the console's 81 % scene as a setting
(`--original-resolution`, `[swapforce.video] original_resolution=1`, [settings.md](../../../docs/getting-started/settings.md)).
Code: `games/swapforce/ultrawide.cpp`, `game::Adapter::fullScene` (`games/swapforce/adapter.cpp`),
`ultrawide::SetScene16x9` (`runtime/d3d/ultrawide.h`, called by `runtime/main.cpp` before `Configure`). Three hooks, all
needed; without any of them the picture is wrong:
1. **The size.** The scene's chain of render-target passes starts at a pass with an **explicit** 1040x620 (`+32`, `+36`;
   its scales at `+40`, `+44` are 1 in the tutorial; the 0.8125 / 0.861 above were seen by the store watch, on a pass the
   hook never sees again [?]). `sub_8258EBE8`'s hook writes the scene's size there before the original (1280x720, or the
   wide one): the half, quarter and reductions follow (640x360, 320x180, 128x720, 128x72), and so do the game's own
   constants (the post effects' 1/size, the FXAA's +-0.5/size are computed from the passes). Log: `[scene] full
   resolution: scene 1040x620 -> 1280x720`.
2. **No predicated tiling.** A 1280x720 G-buffer (3 colours + depth, 720 EDRAM tiles each) does not fit the console's
   2048 tiles: `sub_825138F8` splits the set into 2 tiles of 864x720 (the original's tile list: count at `+40`,
   rectangles from `+44`, the tile size in each surface's `+108`, `+112`). The runtime does not replay draws per tile, so
   the frame was **white**. The hook writes back what the original writes for a set that fits: one tile, the whole
   surface (`[scene] 1280x720 targets kept in one piece`).
3. **The console's EDRAM plan.** When the scene's passes fit, the game plans their EDRAM once (`sub_82593870` per pass,
   bytes `+125` = 1 on each surface: depth at 0, colours at 507, 1014, 1521 at 1040x620) and some passes rely on what
   another pass left in the same tiles: the composite skips the HUD's stencil masks and leaves there what pass 8 drew
   (the clear 0.55 and the HUD panels). Past 2048 tiles there is no plan (`+125` = 0) and every set is laid out from
   tile 0 (`sub_82514448`: colours from 0, depth at 2048 minus its size); the shadow mask then cleared the composite's
   target to white, and the bloom made a **white halo around the HUD** (portrait, health bar, "Press X" banner). The hook
   on `sub_82516EB8` (a surface's EDRAM tile count) gives the plan's call (the one returning to `0x825938B0`) the count of
   the original size for the chain's surfaces (sizes that scale exactly from the scene to 1040x620: 1280 x 13/16,
   720 x 31/36), so the plan and every base are the console's; the host's targets are keyed by base, format and size and
   keep their real size. Found by logging `sub_82514448`'s sets (bases and `+125`) in both modes. The count must stay
   the original's when the targets are set too (`sub_82514448` counts each planned surface again): with the real 720,
   the depth at 0 overlapped the colour at 507 and the game saved the depth to memory after every pass, **84 resolves a
   frame instead of 46**, and the GPU then limited the frame rate (the harness's fps check failed at 25 fps, the GPU
   thread 90 % busy). The hook counts a surface whose byte `+125` is set like the plan's.

Checked [V] (tutorial and `barrens/brn1`, scale 2, macOS Metal, offscreen, 2026-10-27): every pass of the chain at
1280x720 or its fraction (capture), the same EDRAM bases and the same **46 resolves and ~650 draws a frame** as at
1040x620, the HUD clean, no new stub or fault; the menus and the frame keep their own sets. Cost, in a 2560x1440 window
(the 3D at 2560x1440 host pixels instead of 2080x1240), the tutorial: 29.7 fps, GPU wait 0.0-0.1 ms a frame, GPU thread
5-7 % busy, against 30.0 fps, 0.0 ms and 8-10 % at the original resolution (the machine loaded by other runs; no
GPU timer in the runtime). A 2560x1080 window: scene 1696x720, 30 fps, 46 resolves.

**Save loads, a zone change, the pause menu at exactly 1280x720** [V] (2026-10-27): another port reports that a scene
of exactly the screen's size crashes about half of Swap Force's save loads (a pooled surface record with a null
resource in `sub_82513358`, and a heap block header overrun) and uses 1264x704 instead (`office/lessons_2026-10-27.md`,
1.9). Ours, at 1280x720 (scale 2, `private/sfblur/saveload.sh`, untracked): a save made in the tutorial, then **9 runs
that load it** from the title (the slot opened for reading, `tutorial_a` loaded, the save written again), each with a
pause-menu round trip (open, the Skylander page and its stats, back), and one 230 s run through the Greeble ships to
the next zone (`tutorial_b`, drawn right): **0 crashes, 0 faults, 46 resolves a frame throughout**. Not tested: the
pause menu's MAIN MENU in the tutorial (A on it does nothing there [?]: the tutorial seems not to allow quitting).
Why ours does not crash is not proven [?]: with the hooks above, every scene surface keeps the console's EDRAM plan and
base, so the game's pooled records match the 1040x620 ones it was built for. Shots (untracked):
`private/sfblur/shots/fullres/` (tutorial, `brn1`, the 2560x1440 crops, ultrawide, each mode).

## Still open
- The fan port's "black characters from an EDRAM overlap" and "a vertex buffer rewritten in place after a draw" did not
  show here: the character draws lit, in the G-buffer and in its x-ray pass [?] (one level seen).
- Pass 10 resolves EDRAM as `k_16_16_16_16_FLOAT` that nothing drew in that format (black here); the composite reads
  it (sampler 9). What the console holds there is not known [?].
- A purple box at the top right in some frames (`cap5`, `private/swapforce/runs/cap5_final.png`): one of the HUD
  panels pass 8 draws into the scene, inside a HUD stencil mask, with no HUD element over it in that frame; it may be
  right (a panel fading in) [?]. The portal's "magic moment" showed Eruptor flat blue in one run (`cap4_s3.png`); the
  same moment later shows the blue portal effect (`cap7_s3.png`); not compared with a console [?].
- Unnormalized 1D, 3D and cube fetches are not handled (none seen).
- No glyph textures (section above); ultrawide and the portal prompt reader are in ([history.md](history.md), "Figures and the portal").
- The tutorial asks for `\models\levelassets\tutorial\*.hkx` loose files that are not on the disc (`0xC0000034`); the
  game goes on (they look like optional overrides) [?].
