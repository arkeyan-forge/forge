# Binary layout and how to find your way (no symbols)

> **Purpose:** how SSA's binary is laid out and how to find a function without symbols · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA] [Xbox]

Class **[SSA]** for addresses; the *method* is **[Lux]** **[Xbox]**. Detail: [engine_notes.md](engine_notes.md) §1 and §3.1, `games/ssa/config/SSA_symbols.txt`.

## Sections of SSA's `default.xex` (image base `0x82000000`, size `0xD40000`, entry `0x822DF400`)

| Section | Address | Size | Notes |
|---|---|---|---|
| `.rdata` | `0x82001000` | `0xD2DDC` | read-only data |
| `.pdata` | `0x820D4000` | `0x2BC10` | function table XenonRecomp uses; **leaf/packed functions are missing from it** ([toolchain/xenonrecomp.md](../../tools/toolchain/xenonrecomp.md)) |
| `BINKCONS`, `RADCONSTD` | `0x82100000`, `0x82103000` | | Bink constants |
| `.text` | `0x82110000` | `0x8D69A4` | game + statically linked XDK |
| `BINK`, `RADCODE` | `0x829E7000`, `0x829FB000` | `0x13108`, `0xAAC` | Bink code (`BinkDoFrame` `0x829EDCF8`, ...) |
| `BINKBSS` | `0x82A00000` | | |
| `.data` | `0x82A06000` | `0x29E358` | globals (device pointer `0x82BB4F94`, ...) |
| `.XBMOVIE`, `BINKDATA`, `RADDATA` | `0x82CA5000..` | | |
| `.tls` | `0x82CAA000` | `0x1411` | `__declspec(thread)` template |
| `.idata` | `0x82CB0000` | `0x410` | 218 imports |
| `.XBLD`, `.reloc` | `0x82CC0000`, `0x82CC1000` | | |

Recompiled code: ~160 MB of generated C++ (`private/ssa/ppc/`), 123 objects, 928 s CPU / **104 s wall** on a 10-core M1 Pro (a player machine: 3-8 min on 4 cores); `PPC_CODE_BASE 0x82110000`, `PPC_CODE_SIZE 0x8EBAAC`.

## Naming without symbols

- **The project rule: never use the leaked Aug 2011 prototype PDBs** (user decision). Names in `games/ssa/config/SSA_symbols.txt` (139 lines) come from evidence (strings, call graphs, register writes); `?` marks uncertain ones.
- **MSVC RTTI is present**: **1087** `.?AV` / `.?AU` names: 762 in namespace `lux`, 186 global, 65 `ltl2` (the engine's template library), 41 `XAI`, 5 `GameStats`, 4 `AK` (Wwise). Global classes include the NGL scene nodes
  (`nglDebugLineNode`, `nglDummyRenderNode`, `nglStringNode`, `nglMorphGeometryShaderNode`, `nglFXShaderNode`, `nglEmptyShader`, `nglFXShader`, `nglGeometryShader`, `nglFXSasRenderer`) and the UI (`apsRectangleNode`, `apsColorUVARectangleNode`,
  `apsUVARectangleRenderer`, `apsAngleTrackElementYAction`, `apsBillboardNode`), `bdSingletonRegistryImpl`. **RTTI -> vtables -> virtual functions is the fastest way to name engine code** (a 10-line regex over `image.bin`).
- `lux::AsyncFile` is referenced from `sub_821583A8`. The engine's debug log functions are compiled in but silent in retail: the portal driver logs through `sub_826434D8` (printf-like, integer args in r4..r10): **hooking
  printf-like loggers gives free tracing** (`[Portal] Decrypted:` lines proved our figure crypto).
- Tools: `python3 -I tools/ppcinfo.py dis|asm|callers|callees|strings|find-string <hex|text>` (names from `SSA_symbols.txt`); `private/ssa/ppc/ppc_recomp.*.cpp` carry each PowerPC instruction as a comment above its C++, so `grep` works on the generated code too.

## Things worth grepping first in a new Lux title [V] (for SSA)

| Look for | Why | SSA hit |
|---|---|---|
| string `nglListSend called while one or more scenes were still active` | finds `nglListSend` -> ListSendModule -> scene traversal | `0x829076BC` in `nglListSend` `0x829075F8` |
| RTTI names `.?AV...@lux@@` | 1087 types | regex over `image.bin` |
| `lis rX, 0x82BB / ... 0x4F94`-style accesses to one global by hundreds of functions | the D3D device pointer | `0x82BB4F94` |
| calls to `VdSwap`, `VdInitializeRingBuffer`, `VdSetGraphicsInterruptCallback` | `D3DDevice_Swap`, `InitializeRingBuffer`, `InitializeEngines` (single callers) | [gpu/d3d9ltcg_device.md](../../runtime/gpu/d3d9ltcg_device.md) |
| byte-reversed stores (`stwbrx`) into `0x7FEA0000+` | XMA register writers | [xbox360/audio_xma_xaudio.md](../../runtime/xbox360/audio_xma_xaudio.md) |
| `" Copyright (C) 2010 Activision. All Rights Reserved. "` | figure AES key derivation | `0x8264398C` |
| strings `STATE_GAME_FINALCHECKS`, `WAIT_FOR_START_BUTTON` | startup state machine table | object `0x82C91EE8` |
| `sku.lxb`, `text/%s/*.lxb`, `movies/%s/legal.bik` | language selection | `sub_82808190` |
| `inflateInit_` / `inflate` signatures; strings "incorrect header check", "invalid block type" | replace with host zlib | `0x825F23B8` / `0x825F24F8` / `0x825F3B80` |
| `\0ASL` archives `shaders.lsa`, `effects.lsa` | shader cache input | [gpu/shaders.md](../../runtime/gpu/shaders.md) |

## Where each kind of thing hides

CRT helpers (`savegprlr_14`, `restgprlr_14`, `savefpr`, `restvmx`, `setjmp`, `longjmp`) at `0x825F5C90..0x825FF46C` [SSA]; `__CxxFrameHandler` data words `0x825FFC00` and `__C_specific_handler` `0x829E6114` in front of EH
functions; ListSendModule and the NGL code at `0x826xxxxx`; the portal driver at `0x8264xxxx`; Wwise at `0x825xxxxx`; XAudio2 at `0x8252xxxx`; D3D at `0x8229xxxx..0x822Bxxxx`; the game's UI/state machine at `0x827xxxxx..0x829xxxxx`.
