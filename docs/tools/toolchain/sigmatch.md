# sigmatch and the SSA hook inventory

> **Purpose:** how are SSA's hooks and addresses found in another game's binary, and what is the full inventory? · **Status:** current (2026-10-26) · **Tags:** [Lux] [Xbox] [SSA]

`tools/sigmatch/sigmatch.py` finds SSA's hooked functions and data in **another build** by masked byte signature. The
committed inventory [`docs/giants/ssa_hooks.json`](../../giants/ssa_hooks.json) holds **metadata only** (name, kind,
address, role, files, scope, signature *shape*); **the signatures themselves are SSA game bytes and are generated into
`private/giants/ssa_sigs.json` (untracked) from the user's own disc**. The method is engine-generic; the addresses are
**[SSA]**. Giants turned out to be Alchemy, not Lux: sigmatch found 171/293 there, with confidence only for the CRT/XDK
(`xbox` scope) entries [V] ([Giants boot](../../../games/giants/docs/boot.md)). A later Lux game is where the `lux` scope pays off [?].

## Signatures

A signature is a function's first instructions (up to 64 words; at least 6, extended past the end until unique) with the bits that change from one link to the next masked:
- **strict**: branch targets leaving the function (`b`, `bl`) and absolute addresses (`lis` + the D-form that completes it);
- **loose**: also every 16-bit immediate and displacement, and every branch target (struct offsets, constants and code inside the function may differ between games).
A **data address** (a global the runtime reads) is found through a function that references it: once the function is found, its `lis/addi` pair at the recorded offset gives the new address (`ref: {function, lis_offset, pair_distance, delta}`).

## Commands

```
python3 -I tools/sigmatch/sigmatch.py make <hooks.json> [--image private/image.bin] [--funcs private/ssa/ppc/ppc_func_mapping.cpp] [--sigs private/giants/ssa_sigs.json]
python3 -I tools/sigmatch/sigmatch.py match <hooks.json> <image.bin | default.xex> [--sigs ...] [--out map.json] [--toml]
```
`make` reads SSA's image (needs `private/image.bin` + `private/ssa/ppc/` from `tools/prepare_game.sh` on the SSA disc) and writes the signatures; `match` prints one line per entry: **name, SSA address, new address, confidence, method**; `--toml` also prints the XenonRecomp `[main]` addresses it found
(the 10 `*_address` lines). A `.xex` is first dumped with `xexdump`. `tools/day1.sh <game> <iso>` wraps extract + xexdump + hash + make + match ([`docs/tools/day1.md`](../day1.md)).

**Confidence**: **1.0** strict match, unique; **0.9** loose match, unique; `1/n` with n candidates (the SSA occurrence index picks one); below that, the similarity of the best fuzzy alignment (difflib over loose-masked words) for code that moved or changed. **Take 1.0/0.9; anything under 0.9 is a lead to check by hand**
(`tools/ppcinfo.py asm <addr>`). Tested on the SSA disc itself (2026-10-24): 293/293 found, 291 at 1.0. `config_function` entries are SSA's layout: drop them and regenerate with `find_switch_tables.py`.

Not signed (done by hand, see `not_signed` in the JSON): **switch tables** (regenerate with `find_switch_tables.py`, never port by hand); setjmp/longjmp (two config addresses); **mid-asm hooks: none** (every runtime hook is a whole-function override); **D3D struct offsets** (`d3d/*.cpp` and `d3d/ring.h` rely on
device `+0x328C` pixel shader, `+0x3290` vertex shader, `+0x2B1C` fence, `+0x35E8` command buffer... of the XDK build SSA links; check each against the matched functions: same XDK = same offsets); **loose matches** mask immediates, so a hook found loose may have a changed struct offset: **read its asm before trusting the hook's offsets**;
test aids (`SKY_SKIP_MOVIES` needs `Movie_Update`; `SKY_LEVEL` is paths, not addresses; `SKY_PORTAL_LOG` needs `Portal_Log`).

Scopes: **lux** = Lux engine code (absent from Giants: Alchemy), **xbox** = XDK/CRT code (found in Giants, XDK 21076), **ssa** = this build's layout only. 293 entries on 2026-10-24 (297 today: 4 `hooked` entries added since): 26 hooks, 6 replaced, 3 patches, 5 data, 10 config, 3 config data, 104 symbols, 136 config functions
(121 lux, 36 xbox, 136 ssa by scope). Each game's addresses live in `games/<id>/addresses.h` ([layout](../../architecture/layout.md)).

## Giants' inventory, for the next Alchemy game [Alchemy] [Giants]

SSA's inventory finds only XDK/CRT code in an Alchemy game. For the Alchemy games after Giants, a second inventory holds
Giants' own addresses: `tools/sigmatch/giants_inventory.py` writes [`tools/sigmatch/giants_hooks.json`](../../../tools/sigmatch/giants_hooks.json)
(every `GAME_HOOK_` and nonzero data address of `games/giants/addresses.h`, the `[main]` block of `Giants.toml`; the
field `ssa` holds the Giants address). The signatures are made from Giants' image (its own disc, `private/`):

```
python3 -I tools/sigmatch/giants_inventory.py
python3 -I tools/sigmatch/sigmatch.py make tools/sigmatch/giants_hooks.json --image <Giants image.bin> \
    --funcs private/giants/ppc/ppc_func_mapping.cpp --sigs private/<game>/day1/giants_sigs.json
python3 -I tools/sigmatch/sigmatch.py match tools/sigmatch/giants_hooks.json private/<game>/day1/image.bin \
    --sigs private/<game>/day1/giants_sigs.json --out private/<game>/day1/giants_sigmap.json --toml
```

Swap Force (2026-10-26): 34/38 found, 21 at 1.0; the ten `[main]` addresses agree with SSA's inventory; the
Giants-only ultrawide hooks are not found (engine code that changed) [V] ([status](../../../games/swapforce/docs/status.md)).

## The 35 hooked functions at a glance (SSA addresses)

Hooked in `engines/lux/` and `runtime/`, addresses in `games/ssa/addresses.h` ([hooks](../../architecture/hooks.md)): `sub_821404D8` (light tile job), `sub_822895E0` (NGL list submit), `sub_82290160` / `sub_82290508` (CreatePixelShader / VertexShader), `sub_8229A850` / `8229AD58` / `8229B170` (BeginVertices, DrawVertices, DrawIndexedVertices), `sub_822A1560` (Resolve),
`sub_822A2D78` (ring segment), `sub_822A3BB0` / `3CB8` / `3DB0` (KickOff, BlockOnFence, InsertCallback), `sub_822A6BC8` / `6FB8` / `73D8` (command buffers), `sub_822A95C0` (Swap), `sub_822ADDE0` (ClearF worker), `sub_82503F40` / `3FA0` / `47C8` / `4938` (XMA engine 1),
`sub_82616A80` / `6B50` / `6D10` / `6EF8` (XMA engine 2), `sub_825F2298` / `24F8` / `3B80` (zlib), `sub_82631098` (NGL traverse), `sub_826434D8` (portal log), `sub_827F4400` (movie voice track), `sub_82852D90` (Movie_Update), `sub_82864B90` (audio language), `sub_828A25F8` (heap free guard), `sub_829EDCF8` (BinkDoFrame).
Each is a file/function to re-find in another game; hooks a game lacks are compiled out by their absence from its address file.

## The inventory (generated from `ssa_hooks.json`, metadata only)

### Function hooks (override the guest function and usually call the original) (26)

| Name | SSA address | Scope | What | Why | Files |
|---|---|---|---|---|---|
| `Audio_SetLanguage` | `0x82864B90` | lux | audio start-up language (r3 = language code): sets Wwise's language folder and every file package's language | voices in another language than the text (settings voice_language) | language.cpp |
| `Movie_VoiceTrack` | `0x827F4400` | lux | picks a cutscene's Bink voice track (r3 out, r4 = has localized voices) | cutscene voices follow the voice language | language.cpp |
| `Xma1_Init` | `0x82503F40` | xbox | XMA engine 1 (XAudio2 copy) init: creates the context array first | each engine reads the context array address once at init | kernel/xma.cpp |
| `Xma2_Init` | `0x82616A80` | xbox | XMA engine 2 (Wwise copy) init: creates the context array first | same | kernel/xma.cpp |
| `Xma1_VoiceClear` | `0x82503FA0` | xbox | engine 1 register writer (voice group create/clear) run under the XMA mutex (CallWriter) | the APU context registers are emulated; writes must be observed | kernel/xma.cpp |
| `Xma1_VoiceKick` | `0x82504938` | xbox | engine 1 register writer (kick) | same | kernel/xma.cpp |
| `Xma2_VoiceClear` | `0x82616B50` | xbox | engine 2 register writer (create/clear) | same | kernel/xma.cpp |
| `Xma2_VoiceLock` | `0x82616D10` | xbox | engine 2 register writer (lock) | same | kernel/xma.cpp |
| `Xma2_VoiceKick` | `0x82616EF8` | xbox | engine 2 register writer (kick) | same | kernel/xma.cpp |
| `Xma1_LockAll` | `0x825047C8` | xbox | engine 1 locks all voices; the hook takes Engine1CriticalSection first | lock order guest-then-ours avoids a deadlock | kernel/xma.cpp |
| `Portal_Log` | `0x826434D8` | lux | the title's printf-like portal log, silent in retail; echoed with SKY_PORTAL_LOG | portal debugging | kernel/portal.cpp |
| `D3DDevice_BeginCommandBuffer` | `0x822A6BC8` | xbox | D3DDevice_BeginCommandBuffer(device, cb) | recorded command buffers are replayed by the host (docs/command_buffers.md) | d3d/command_buffers.cpp |
| `D3DDevice_EndCommandBuffer` | `0x822A6FB8` | xbox | D3DDevice_EndCommandBuffer(device) | same | d3d/command_buffers.cpp |
| `D3DDevice_NextRingSegment` | `0x822A2D78` | xbox | moves the ring write position to the next segment (from ReserveCommandSpace, KickOff, Swap) | the PM4 ring is read by the host at segment boundaries (d3d/ring.h) | d3d/ring.cpp |
| `D3DDevice_CreateVertexShader` | `0x82290508` | xbox | CreateVertexShader(container) -> object | maps guest shaders to the XenosRecomp cache | d3d/shaders.cpp |
| `D3DDevice_CreatePixelShader` | `0x82290160` | xbox | CreatePixelShader(container) -> object | same | d3d/shaders.cpp |
| `D3DDevice_KickOff` | `0x822A3BB0` | xbox | D3DDevice_KickOff | no real GPU: kick-off executes immediately (null_gpu) | d3d/null_gpu.cpp |
| `D3DDevice_DrawVertices` | `0x8229AD58` | xbox | DrawVertices(device, prim, start, count): drawn once the guest wrote its state into the ring | host draws | d3d/render.cpp |
| `D3DDevice_DrawIndexedVertices` | `0x8229B170` | xbox | DrawIndexedVertices(device, prim, baseVertex, startIndex, count) | host draws | d3d/render.cpp |
| `D3DDevice_BeginVertices` | `0x8229A850` | xbox | BeginVertices(device, prim, count, stride) -> buffer the game fills | host draws (UP draws) | d3d/render.cpp |
| `D3DDevice_Resolve` | `0x822A1560` | xbox | Resolve(device, flags, rect, dest, point, level, slice, clear...) | EDRAM resolves done by the host | d3d/render.cpp |
| `D3DDevice_ClearF_Worker` | `0x822ADDE0` | xbox | ClearF/BeginTiling/BeginZPass worker: clears with a rectangle draw | host clear | d3d/render.cpp |
| `D3DDevice_Swap` | `0x822A95C0` | xbox | Swap(device, frontBuffer, ...): presents the resolved texture | host present | d3d/render.cpp |
| `LightTile_ClassifyJob` | `0x821404D8` | lux | light pre-pass tile classification job (worker): r4 first tile of the depth resolve, r8 tiles/row, r10 first row; records the resolve address for WriteBackDepth | the job reads tiled EDRAM depth from guest memory: host must write depth back first (shadow tile job) | d3d/render.cpp |
| `BinkDoFrame` | `0x829EDCF8` | lux | BinkDoFrame(HBINK) | Bink frames drawn by the GPU path (d3d/bink_display.cpp) | d3d/bink_display.cpp |
| `Movie_Update` | `0x82852D90` | lux | Movie_Update(movie) returns movie+0x80 (playing); SKY_SKIP_MOVIES=1 clears it after one frame | test runs skip movies | d3d/bink_display.cpp |

### Replaced functions (the original never runs) (6)

| Name | SSA address | Scope | What | Why | Files |
|---|---|---|---|---|---|
| `inflateInit2_` | `0x825F2298` | lux | zlib inflateInit2_ (inflateInit_ 0x825F23B8 branches here, windowBits 15) | host zlib is much faster than the recompiled one (file/STR decompression) | inflate.cpp |
| `inflate` | `0x825F24F8` | lux | zlib inflate(strm, flush) | host zlib, same reason | inflate.cpp |
| `inflateEnd` | `0x825F3B80` | lux | zlib inflateEnd(strm) | host zlib, same reason | inflate.cpp |
| `D3DDevice_RunCommandBuffer` | `0x822A73D8` | xbox | D3DDevice_RunCommandBuffer: guest version emits INDIRECT_BUFFER nobody executes; host replays and marks non-persisted groups dirty | autosave panel texture etc. | d3d/command_buffers.cpp |
| `D3DDevice_BlockOnFence` | `0x822A3CB8` | xbox | BlockOnFence: a fence in unsubmitted commands needs a kick-off; nothing else is pending | the guest spins on a GPU-written fence otherwise | d3d/null_gpu.cpp |
| `D3DDevice_InsertCallback` | `0x822A3DB0` | xbox | InsertCallback queues the callback instead of writing CP-interrupt packets | no CP interrupts | d3d/null_gpu.cpp |

### Guards on game code (3)

| Name | SSA address | Scope | What | Why | Files |
|---|---|---|---|---|---|
| `Ngl_SceneTraversal` | `0x82631098` | lux | NGL scene traversal from ListSendModule; skipped when the current scene (NglCurrentScene) is null | a frame submitted with no scene (first movie) dereferenced null | patches.cpp |
| `Ngl_ListSubmit` | `0x822895E0` | lux | NGL render list submission (scene+0x348); skipped when r3 is null | frames drawn while a movie has no picture: empty list crashed | patches.cpp |
| `Heap_Free` | `0x828A25F8` | lux | main heap free (r3 heap, r4 payload); a double free or a free next to a corrupt block is logged and leaked | users crashed in coalescing while the frontend loads (docs/handoff_sigbus.md); cause not found | patches.cpp |

### Data globals the runtime reads or locks (found through a function that references them) (5)

| Name | SSA address | Scope | What | Why | Files |
|---|---|---|---|---|---|
| `LanguageIndex` | `0x82AA7048` | lux | the title's language index (set by sub_82808190) | read or locked by the runtime | language.cpp |
| `LanguageCodes` | `0x82AA5EBC` | lux | table of 15 char* language codes (disc folder names) | read or locked by the runtime | language.cpp |
| `NglCurrentScene` | `0x82BB4F4C` | lux | scene list nglListSend hands to ListSendModule | read or locked by the runtime | patches.cpp |
| `Engine1CriticalSection` | `0x82A3CC30` | lux | XMA engine 1 critical section | read or locked by the runtime | kernel/xma.cpp |
| `DeviceGlobal` | `0x82BB4F94` | lux | NGL global holding the D3D device pointer | read or locked by the runtime | d3d/device.h |

### XenonRecomp `[main]` addresses (found by byte pattern) (10)

| Name | SSA address | Scope | What | Why | Files |
|---|---|---|---|---|---|
| `restgprlr_14_address` | `0x825F5CE0` | xbox | XenonRecomp [main] restgprlr_14_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `savegprlr_14_address` | `0x825F5C90` | xbox | XenonRecomp [main] savegprlr_14_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `restfpr_14_address` | `0x825F647C` | xbox | XenonRecomp [main] restfpr_14_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `savefpr_14_address` | `0x825F6430` | xbox | XenonRecomp [main] savefpr_14_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `restvmx_14_address` | `0x825FF3D8` | xbox | XenonRecomp [main] restvmx_14_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `savevmx_14_address` | `0x825FF140` | xbox | XenonRecomp [main] savevmx_14_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `restvmx_64_address` | `0x825FF46C` | xbox | XenonRecomp [main] restvmx_64_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `savevmx_64_address` | `0x825FF1D4` | xbox | XenonRecomp [main] savevmx_64_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `longjmp_address` | `0x825F8430` | xbox | XenonRecomp [main] longjmp_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |
| `setjmp_address` | `0x825FECA0` | xbox | XenonRecomp [main] setjmp_address | CRT register save/restore helpers or setjmp/longjmp, needed by the recompiler | games/ssa/config/SSA.toml |

### Data words for `invalid_instructions` (3)

| Name | SSA address | Scope | What | Why | Files |
|---|---|---|---|---|---|
| `invalid_instructions@0x825FFC00` | `0x825FFC00` | xbox | XenonRecomp invalid_instructions (EH handler + data before EH functions) | the analyzer must skip these words | games/ssa/config/SSA.toml |
| `invalid_instructions@0x829E6114` | `0x829E6114` | xbox | XenonRecomp invalid_instructions (EH handler + data before EH functions) | the analyzer must skip these words | games/ssa/config/SSA.toml |
| `invalid_instructions@0x8260CA80` | `0x8260CA80` | xbox | XenonRecomp invalid_instructions (EH handler + data before EH functions) | the analyzer must skip these words | games/ssa/config/SSA.toml |

### Named symbols (analysis aids, not hooks) (104)

| Name | SSA address | Scope | What | Why | Files |
|---|---|---|---|---|---|
| `D3DResource_Release` | `0x82296948` | lux | atomic --refcount at +4, destroys at 0; 97 callers | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetPixelShader` | `0x82290250` | lux | stores r4 at device+0x328C; clears PS-constant dirty bits (device+0x8); shader flush 0x822A5E08 IM_LOADs +0x328C with type 1 (pixel). Was mislabelled VS | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetVertexShader` | `0x822905F8` | lux | stores r4 at device+0x3290; object has 0x368-byte VS header (CreateVertexShader 0x82290508); clears VS-constant dirty bits (device+0x0). Was mislabelled PS | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_InitializeRingBuffer` | `0x822A4210` | lux | calls VdInitializeRingBuffer, VdEnableRingBufferRPtrWriteBack; emits INDIRECT_BUFFER | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3D_SetDisplayMode` | `0x822B21B0` | lux | only caller of VdSetDisplayMode | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3D_InitializeEngines` | `0x822B2348` | lux | only caller of VdInitializeEngines, VdSetGraphicsInterruptCallback | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3D_GraphicsInterrupt` | `0x822A4000` | lux | registered via VdSetGraphicsInterruptCallback; source 1 calls info+16(info+20), source 0 handles vblank | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `Direct3D_CreateDevice` | `0x822A09F0` | lux | allocs 0x5F80-byte device (128-aligned), stores it via r6; NGL global device ptr 0x82BB4F94 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetStateToDefaults` | `0x822B1AB8` | lux | walks RS table 0x82A426C0 {Get,Set,Default}x101 and SS table 0x82A42B80 x20, copies Set/Get ptrs to device+0x40/+0x224/+0x1D4/+0x3B8 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_DrawVerticesUP` | `0x8229AD10` | lux | BeginVertices + memcpy + inline EndVertices | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_DrawTessellatedVertices?` | `0x8229B5E8` | lux | uncertain: draw writing VGT_OUTPUT_PATH_CNTL/VGT_GROUP_* (+0x2974,+0x2988..); caller 0x8262B3D0 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_DrawIndexedTessellatedVertices?` | `0x8229BA88` | lux | uncertain: as above with index buffer; caller 0x8262B430 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetVertexShaderConstantF` | `0x8228FDB8` | lux | (dev, start, data, vec4Count, dirtyMask): copies to dev+0x780+16*start, dev+0x0 |= mask | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetPixelShaderConstantF` | `0x8228FE90` | lux | dev+0x1780+16*start, dev+0x8 |= mask | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetVertexShaderConstantB` | `0x8228FF68` | lux | bits at dev+0x2780, dev+0x20 |= 1<<56 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetPixelShaderConstantB` | `0x8228FFC8` | lux | bits at dev+0x2790 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetVertexShaderConstantI` | `0x82290028` | lux | loop consts at dev+0x27A0+4*i | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetPixelShaderConstantI?` | `0x82290080` | lux | sibling of 0x82290028 (unverified offset) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `XGGetMicrocodeShaderParts` | `0x822900D8` | lux | (pFunction, parts): cached part = header, physical part at +0x28 or +0x368 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DVertexShader_Init` | `0x82290410` | lux | resets VS bind slots | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_CreateVertexDeclaration` | `0x82290908` | lux | elements (12 bytes, end 0xFF); object 0x38+12n, Common 0x00100005 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DVertexDeclaration_Init` | `0x82290818` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetFVF` | `0x822907C8` | lux | stores FVF at +0x3188, derives declaration -> +0x2F58 (current vertex declaration), dirty +0x10 bit 19 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetShaderGPRAllocation` | `0x82290988` | lux | (dev, flags, vsGPRs, psGPRs), runs 3 indirect buffers; ListSendModule sets 32/96 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetTexture` | `0x8229E100` | lux | (dev, sampler, tex, dirtyMask): tex fetch const (+0x1C..+0x30) merged into dev+0x480+0x18*s, ptr at +0x31F8+4s | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetStreamSource` | `0x82299E98` | lux | (dev, stream, vb, offset, stride, dirtyMask): vfetch const 95-s at dev+0x778-8s, ptr +0x31A4+4s, stride byte +0x31E8+s | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetIndices` | `0x82299FB8` | lux | ptr at +0x318C | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderTarget` | `0x8229A048` | lux | (dev, idx, surf): +0x3190+4i, copies surf+0x1C to RB_COLOR_INFO shadow (+0x2884/+0x288C..) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetDepthStencilSurface` | `0x8229A3D8` | lux | +0x31A0, RB_DEPTH_INFO shadow +0x2888 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_UpdateSurfaceInfo?` | `0x82297430` | lux | RB_SURFACE_INFO/PA_SC_AA_CONFIG from surface, resets scissor+viewport (called by SetRT/SetDS) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetViewport` | `0x82299E18` | lux | D3DVIEWPORT9 ints -> SetViewportF | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetViewportF` | `0x822977B0` | lux | floats f1..f6; viewport at +0x3260..+0x3274, PA_CL_VPORT_* shadows +0x2908..+0x291C | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetScissorRect` | `0x82299D20` | lux | rect at +0x327C..+0x3288, clipped with viewport, calls 0x822974C0 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_WriteWindowScissor` | `0x822974C0` | lux | writes PA_SC_WINDOW_OFFSET/SCISSOR (0x2080..0x2082) DIRECTLY into the ring (per tile when tiling) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetPredication` | `0x822AE608` | lux | (dev, mask) tile predication (SET_BIN_MASK/SELECT) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_BeginTiling` | `0x822AE778` | lux | predicated tiling; tile rects +0x32CC.., count +0x32C4; caller 0x8262D3E8 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_EndTiling` | `0x822AEAD0` | lux | resolves every tile via Resolve; caller 0x8262D608 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_BeginZPass?` | `0x822AEF88` | lux | uncertain; from nglSceneTraverse | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_EndZPass?` | `0x822AF218` | lux | uncertain; builds fixup list +0x34B0 used by predicated draws | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_BeginConditionalSurvey?` | `0x8229E290` | lux | uncertain: VIZ_QUERY packet, PA_SC_VIZ_QUERY (+0x29B0) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_EndConditionalSurvey?` | `0x8229E360` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_BeginConditionalRendering?` | `0x8229E440` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_EndConditionalRendering?` | `0x8229E5E8` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_FlushShaders` | `0x822A5E08` | lux | dirty +0x10 bits 17-20: links VS/PS, IM_LOAD microcode (type 0 VS / 1 PS), SQ_PROGRAM_CNTL | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_WriteDirtyRegisters` | `0x822A4C50` | lux | (dev, mask<<, firstReg, shadowPtr): type-0 packets for set bits (bit63 = firstReg) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_WriteDirtyAluConstants` | `0x822A4FE8` | lux | 1 mask bit = 4 vec4 constants | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_WriteDirtyFetchConstants` | `0x822A4E90` | lux | bit63 = fetch slot 0, 6 dwords from dev+0x480 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_FlushHiZ?` | `0x822A48B8` | lux | called when dirty+0x10 & dirty+0x28 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_MakeSpaceForRegisters` | `0x822A4608` | lux | ring overflow path of the writers | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_ReserveCommandSpace?` | `0x822A25E8` | lux | 19 callers | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_AllocateTransient?` | `0x822A2BD8` | lux | used by BeginVertices | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DResource_LockInternal` | `0x822963B8` | lux | blocks on resource fence, ++lock count (Common bits 8-11), CPU cache coherency packets | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DResource_UnlockInternal` | `0x822966B8` | lux | dcbst ranges BaseFlush(+0x14)/MipFlush(+0x18), --lock count | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DResource_AddRef` | `0x822968D0` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DResource_GetDevice` | `0x822969C8` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DResource_GetType` | `0x82296A40` | lux | 3 tex, 0x11 vol, 0x12 cube, 0x13 array, 0x14 line, 0x10 volume | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DTexture_GetLevelDesc` | `0x8229CCB0` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DVolumeTexture_GetLevelDesc` | `0x8229CDB0` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DTexture_LockRect` | `0x8229D3F0` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DVolumeTexture_LockBox` | `0x8229D480` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DSurface_GetDesc` | `0x8229DFC8` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DSurface_LockRect` | `0x8229E060` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DSurface_UnlockRect` | `0x8229E080` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DBaseTexture_GetLevelCount` | `0x8229DA90` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_UnbindAll?` | `0x822A0C50` | lux | sets RT/DS/shaders/streams/textures/indices to NULL | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_CaptureStateBlock?` | `0x822A86C8` | lux | uncertain (caller 0x82850698) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DStateBlock_Apply?` | `0x822A8A40` | lux | uncertain | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_FrameEnd?` | `0x822A9088` | lux | uncertain; ListSendModule before Swap | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetGammaRamp?` | `0x8229A6B0` | lux | uncertain (+0x425C, +0x5EAC) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `XGraphics_ShaderAssembler?` | `0x82291448` | lux | 0x82291448-0x82296028 runtime microcode assembler (ctx stride 0x26D0); D3DX HLSL compiler "2.0.20500.0" lives at 0x822F..0x823F | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_ZENABLE` | `0x82298148` | lux | RB_DEPTHCONTROL shadow +0x2934 (reg 0x2200) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_CULLMODE` | `0x82297A10` | lux | PA_SU_SC_MODE_CNTL +0x2948 (0x2205) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_ALPHABLENDENABLE` | `0x82297AA8` | lux | RB_BLENDCONTROL0..3 +0x2938/+0x2958..+0x2960 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_SRCBLEND` | `0x82297BC8` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_DESTBLEND` | `0x82297C58` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_ALPHATESTENABLE` | `0x82297A70` | lux | RB_COLORCONTROL +0x293C (0x2202) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_ALPHAREF` | `0x82297ED0` | lux | RB_ALPHA_REF +0x2904 (0x210E) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_COLORWRITEENABLE` | `0x82298700` | lux | RB_COLOR_MASK +0x28DC (0x2104) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_SCISSORTESTENABLE` | `0x82298520` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `D3DDevice_SetRenderState_PRESENTINTERVAL` | `0x822992C0` | lux | +0x3630 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `nglInit` | `0x826324D0` | lux | CreateDevice(1,1,&pp 0x82B94E98,&0x82BB4F94) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `nglPresent` | `0x8262D690` | lux | Swap(dev, frontBufferTex+0x14), double-buffered front textures 0x82C54698/0x82C54720 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `nglResolveTiles` | `0x8262D540` | lux | per tile: SetPredication(3<<2i) + Resolve | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `ngl_WriteStencilRefDirect` | `0x8213BBB8` | lux | writes type-0 RB_STENCILREFMASK (0x210D) straight into the ring: bypasses the shadow | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `ngl_FlushHiZDirect?` | `0x82893F10` | lux | EVENT_WRITE 0xF straight into the ring + RB_HIZCONTROL shadow bit | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `ngl_DrawQuadUP` | `0x8228B3E8` | lux | inline SetVertexDeclaration (+0x2F58) + BeginVertices(QUADLIST) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `ngl_UnbindStream` | `0x82289248` | lux | SetStreamSource(NULL) + inline decl NULL | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `ListSendModule` | `0x826320C8` | lux | job module "ListSendModule": kicks D3D, inserts callback 0x826319B8, traverses the submitted frame | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `nglListBeginScene` | `0x82633FD8` | lux | allocates a scene, links it under the current one (0x82BB4F44) or as next root (0x82BB4F48) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `nglInitScene` | `0x82633E78` | lux | clears the scene, render list (+0x348) = 0 | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `nglListSend` | `0x829075F8` | lux | frame submit: waits for the previous ListSendModule, root = next root, queues the job; string "nglListSend called while..." | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `GameMaster_IntroMovies` | `0x827F5E80` | lux | main-thread loop over the 4 intro movies (legal, atvilogo, xpeclogo, tfb) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `JobWorker_Loop` | `0x82635CF8` | lux | "JQ Worker" thread body | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `JobGroup_Wait` | `0x826360F8` | lux | executes pending jobs on the caller while the group counters are non-zero | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `RecursiveSpinLock_Acquire` | `0x8213EFB0` | lux | owner = GetCurrentThreadId, ldarx/stdcx on *(lock+12) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `GetTickCount` | `0x822DF0D0` | lux | KeTimeStampBundle->TickCount | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `GetCurrentThreadId` | `0x822DCD50` | lux | KTHREAD+0x14C | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `BinkWait` | `0x829EA3F0` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `BinkShouldSkip` | `0x829EA388` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `BinkNextFrame` | `0x829EDD00` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `RADTimerRead` | `0x829EEF90` | lux | ms timer on GetTickCount | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `Game_InitSound` | `0x82865448` | lux | MemoryMgr, StreamMgr, low-level IO, then AK::SoundEngine::Init | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `AK_SoundEngine_Init` | `0x825852C0` | lux |  | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |
| `XAudio2_RenderDriverCallback` | `0x82522E80` | lux | registered via XAudioRegisterRenderDriverClient (wrapper 0x82523060 dereferences the context cell) | identified function (tools/ppcinfo.py, docs) | games/ssa/config/SSA_symbols.txt |

### `config_function` (136 entries)

The 136 `functions = [ { address, size } ]` entries of `games/ssa/config/SSA.toml` (layout of SSA's binary: sigmatch *drops* them and `find_switch_tables.py` regenerates them). Scope `ssa`.

