# SSA engine function and data addresses (Lux + XDK) with roles

> **Purpose:** every SSA engine function and data address we depend on, with its role · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

**Every address here is SSA's** (`default.xex` sha256 `e195f513...`). Use them as *clues*, never as values for another game: re-find each one with the method in [binary_layout.md](binary_layout.md) or `tools/sigmatch` ([../toolchain/sigmatch.md](../../tools/toolchain/sigmatch.md)).
Status: `[V]` verified (static analysis, runtime log/capture, test or measured run), `[?]` lead. D3D functions and the device layout are in [../gpu/d3d9ltcg_device.md](../../runtime/gpu/d3d9ltcg_device.md); the hooks (what we override) in [../toolchain/sigmatch.md](../../tools/toolchain/sigmatch.md).
Detail: [engine_notes.md](engine_notes.md) §3.17 (verbatim) + `games/ssa/config/SSA_symbols.txt`; the second table adds rows found in the code and the port notes.

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

## Extra rows from code and measurements

| Address | Name / role | Status |
|---|---|---|
| 0x822E5B88 | `D3DXGetShaderConstantTable` (lux shader ctor calls it) | [V] |
| 0x82261138 | particle Action scalar deleting destructor (resets vtable to 0x820ABE20, frees through pool 0x82BB5488 via 0x82132CC0) | [V] |
| 0x829B71D0 | `apsBillboardNode` render callback (type record 0x820BB0B0) | [V] |
| 0x8267A880 | HUD object builder at frontend load (hudmenu.str) | [V] |
| 0x82846398 / 0x828464E8 / 0x828465C0 | WideLoad completion wait (count may be 0) and callers | [V] |
| 0x821404D8 | light tile classification CPU job (reads the scene depth resolve 4 KB per tile) | [V] |
| 0x82891B80 / 0x82891A40 | job dispatch of the tile job | [V] |
| 0x82BC07D0 | light manager singleton; class-byte VB at physical 0x045AEA80; depth constants at 0x82BC09D0.. | [V] |
| 0x82635740 / 0x82BB4F28 | render thread's job-loop idle wait (event) | [V] |
| 0x827F6850 / 0x827F6628 / 0x827F3378 | per-frame update; step factor F; bank-load throttle | [V] |
| 0x82BA1F30 | timebase frequency used by the step | [V] |
| 0x82C7CD20 | game clock object (+148 paused, +184 / +188 factors, +196 F) | [V] |
| 0x82631A20 / 0x82630B78 / 0x8285B738 | vblank snapshot / scene time / shader time splat | [V] |
| 0x82631EA8 | sets present interval 2 | [V] |
| 0x826314C0 | scene traversal call site of billboard render | [V] |
| 0x82A3CC30 | XMA engine 1 critical section | [V] |
| 0x82B73AB0 / 0x82B7FA70 | XMA engine 1 / 2 context array globals | [V] |
| 0x82503F40 / 0x82616A80 | XMA engine init (read ContextArrayAddress once) | [V] |
| 0x82616DA8 | XMA hardware-to-local context copy after lock | [V] |
| 0x82617390 | `XMASetLoopData` | [V] |
| 0x82A426C0 / 0x82A42B80 | render-state / sampler-state setter tables ({Get,Set,Default} x 101 / x 20) | [V] |
| 0x82A42D74 | embedded shader pointer table (81 containers 0x8208DD18..0x820A1C58) | [V] |
| 0x828FE7B0 / 0x828574D8 | LSA loader and its caller | [V] |
| 0x8296E750 / 0x8213AEB8 / 0x8213A958 | lux shader ctor / CreateVertexShader / CreatePixelShader wrappers | [V] |
| 0x822F0000-0x823FFFFF (approx.) | D3DX HLSL compiler 2.0.20500.0 linked in | [?] |
| 0x825F5C90 / 0x825F5CE0 | `savegprlr_14` / `restgprlr_14` | [V] |
| 0x825F6430 / 0x825F647C | `savefpr_14` / `restfpr_14` | [V] |
| 0x825FF140 / 0x825FF3D8 / 0x825FF1D4 / 0x825FF46C | `savevmx_14` / `restvmx_14` / `savevmx_64` / `restvmx_64` | [V] |
| 0x825F8430 / 0x825FECA0 | `longjmp` / `setjmp` | [V] |
| 0x825FFC00 / 0x829E6114 / 0x8260CA80 | EH data words (`invalid_instructions`): `__CxxFrameHandler`+FuncInfo, `__C_specific_handler`+scope table, custom handler | [V] |
| 0x826854A8 | first **packed leaf** found missing (ctr of the "Indirect call to unrecompiled guest code" at the slot menu; 27 such in all) | [V] |
| 0x82A1E308 | XAPI file function table pointer (NtQueryDirectoryFile slot never read) | [V] |
| 0x82B94E98 | present parameters (D3DPRESENT_PARAMETERS) | [V] |
