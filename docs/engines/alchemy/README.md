# Alchemy: the engine of Skylanders: Giants

> **Purpose:** what is known about the Alchemy engine, where its hooks are, and which page answers which question · **Status:** current (2026-10-26) · **Tags:** [Alchemy] [Giants]

## What it is
Giants runs on **Alchemy** (Vicarious Visions' engine), proven by booting its disc on 2026-10-25; Swap Force is Alchemy
by two public sources, SuperChargers probably, Trap Team and Imaginators are open
([franchise.md](../../porting/franchise.md)). None of the Lux engine hooks applied to Giants; only the Xbox and XDK ones
(D3D, XMA, Bink, the kernel and XAM) carried over. Addresses in these pages are **Giants'**: re-find them in every other
game (`tools/xmatch.py`, [sigmatch](../../tools/toolchain/sigmatch.md); [../../tools/README.md](../../tools/README.md)).

| Fact | Detail |
|---|---|
| Disc content | no `.lsa`/`.str`/`.lxb`; `.arc` (295), `.bld` (596), `.mov` (Bink 2, 61), IGA v8 archives, IGZ v6 scenes ([boot.md](../../../games/giants/docs/boot.md)) |
| Binary | no C++ exception handling (no FuncInfo magic); 48 SEH functions; XDK 2.0.21076 (SSA: 20500) |
| What carried over from SSA | XDK-level code only: the D3D device (grown by 0x80), XMA (the same two engines), Bink; the kernel and XAM runtime |
| What did not | NGL scene/list guards, the Lux heap free, language tables, zlib `inflate`, LightTile classify, portal log, `Movie_Update` |
| Decompression [V] | the LZMA SDK 9.x (`LzmaDec_DecodeReal` hooked as host code, 2026-10-28: the decoders' time halved; Giants' level load from its `.arc` 1.7-2.3 -> 1.0-1.2 s, Swap Force's zone load 5.6-8.7 -> 4.9-5.3 s); zlib 1.2.3 is linked but unused by the loads: [lzma.md](lzma.md) |
| Shaders | Alchemy never calls `D3DDevice_Create*Shader`: it builds the object itself and calls `D3DVertexShader_Init` / `D3DPixelShader_Init`; constant tables are stripped. All 4394 data shaders are in `/permanent/global.bld`, plus 50 Bink 2 run-time ones ([shaders.md](../../../games/giants/docs/shaders.md)) |
| No command buffers | `Begin/End/RunCommandBuffer` have no match: Alchemy does not record command buffers |
| Device pointer | no game global: kept in an object (`sub_823001C8` stores it at `obj+0xC`); the hooks take it from r3 |
| Audio | not Wwise: the engine mixes everything into one XAudio2 source voice looping a 1024-sample 5.1 buffer, its play cursor `SamplesPlayed mod 1024` ([boot.md](../../../games/giants/docs/boot.md), "Choppy, metallic sound") |
| Archives and scenes [V] | IGA v8: entry table, a plain name table, chunked LZMA; IGZ v6: sections, the section-0 records (class names, external ids, object offsets, memory handles), `igImage2`. One reader for shaders (Arkeyan-Builder), pictures (the game, Arkeyan Launcher): `runtime/host/art/alchemy_archive.cpp`. Formats: [shaders.md](../../../games/giants/docs/shaders.md) ("Format facts"), [art.md](../../../games/giants/docs/art.md) ("Formats"); format versions: [game_art.md](../../runtime/host/game_art.md). Swap Force [V]: IGA v10 (big-endian), IGZ v7 (tagged records, string-index names, materials naming their textures): [its art.md](../../../games/swapforce/docs/art.md) ("Formats") |
| Pictures [V] | DXT5 / DXT1 / 8-bit, tiled, named by a hash of the format's name; **stored upside down** ([art.md](../../../games/giants/docs/art.md)) |
| Button glyphs [V] | characters of the UI font atlases (A X Y B, d-pad, LS, RS, START, BACK), plus a few whole textures (the title's A); one atlas holds Wii buttons: [input.md](../../runtime/xbox360/input.md) ("Glyphs") |
| Ultrawide [V] | the display aspect global the cameras read, the scene size (min(back buffer, 1000x600)), the scene kept under the EDRAM budget, a tiled 1280x720 frame: [ultrawide.md](../../runtime/gpu/ultrawide.md) ("Giants") |
| Movies [V] | Bink 2 drawn by the game on the GPU with its 50 run-time shaders: [bink.md](../lux/bink.md) ("Bink 2 in Giants") |
| Portal prompt [V] | no prompt object: a game script ("Portal Control" state machine) and two script variables, found by scanning the physical heap ([boot.md](../../../games/giants/docs/boot.md), "Portal prompt") |
| Level loads | each level is `\level\<name>.arc`; the runtime waits for the pipeline list on that open (`runtime/kernel/files.cpp`; [history.md](../../../games/giants/docs/history.md), "Level tour") |
| Presentation | presents at every vblank (`alchemy.xml` `presentationInterval=1`): the fps limit throttles it ([parity.md](../../../games/giants/docs/parity.md), "60 fps option") |

## Where the code is
- [`engines/alchemy/`](../../../engines/alchemy/README.md): the class-**b** engine hooks (`ultrawide.cpp`, `lzma.cpp`,
  `zone.cpp`), built when `ARKEYAN_GAME=giants` or `swapforce`.
- [`games/giants/`](../../../games/giants/README.md): Giants' addresses (`addresses.h`: D3D, XMA, ultrawide, Bink),
  the adapter, the portal prompt reader.
- Xbox and XDK hooks used by Giants: `runtime/d3d/` (D3D, `bink_display.cpp`), `runtime/kernel/` (XMA, audio,
  `imports_giants.cpp`); the archive reader: `runtime/host/art/alchemy_archive.cpp`; Arkeyan-Builder's shader extractor:
  `tools/arkeyan-builder/igz_shaders.cpp`.

## How to test it
- `tools/test_all.sh --only giants` (`--quick` for the short pass): Giants' unit tests and an isolated run from the title
  to Chapter 1; `--long` adds the 91-level tour ([testing.md](../../getting-started/testing.md#giants-deeper-harness-runs-giants)).
- `ArkeyanArtTest` with `GIANTS_ISO`: the IGA/IGZ readers on the real disc ([art.md](../../../games/giants/docs/art.md), "Tests").
- `ArkeyanArtTest` with `SWAPFORCE_ISO`: IGA v10 and IGZ v7 on Swap Force's disc ([art.md](../../../games/swapforce/docs/art.md), "Tests").

## Gotchas
- Instructions SSA never used (`vsrah`, `vpkd3d128` pack 3, ...), `mulchw.` shadowing `vsldoi128`, jump tables with no
  bounds check (Bink 2) and 68 tables placed right after their `bctr`: [boot.md](../../../games/giants/docs/boot.md) ("XenonRecomp").
- XDK 21076 changed two D3D signatures: `DrawIndexedVertices` lost its base vertex, `Resolve` lost DestLevel; the
  vertex declaration moved to device `+0x2FD0` ([boot.md](../../../games/giants/docs/boot.md), "Direct3D" and the run lists).
- The frontend uses predicated tiling (two tiles): host targets cover every tile (`game::ExpandTiledTargets`).
- A missing save must give ERROR_PATH_NOT_FOUND from `XamContentGetCreator`, else "Save content is damaged".
- Without the XMA hooks, engine 2's voice code deadlocks after the opening movie.
- Every problem met, with its fix: [problem_catalogue.md](../../porting/problem_catalogue.md).

## Pages
| Page | Answers |
|---|---|
| [lzma.md](lzma.md) | The LZMA SDK's decoder loop as host code: how it was found, checked and measured |
| [games/giants/docs/boot.md](../../../games/giants/docs/boot.md) | What broke on the way to the title and to Chapter 1, the D3D address table, imports, the portal prompt |
| [games/giants/docs/shaders.md](../../../games/giants/docs/shaders.md) | Where the shaders are, the IGA v8 and IGZ v6 formats, the extractor |
| [games/giants/docs/art.md](../../../games/giants/docs/art.md) | Logo, background, portraits; the IGA name table, IGZ objects, picture formats |
| [games/giants/docs/status.md](../../../games/giants/docs/status.md) | Status, install through the kit, the level tour |
| [games/giants/docs/research.md](../../../games/giants/docs/research.md) | Public facts about Giants and its engine |
| [../lux/bink.md](../lux/bink.md) | Bink 2 in Giants (the `BinkOpen` hook, pace and sync) |
| [../../runtime/gpu/ultrawide.md](../../runtime/gpu/ultrawide.md) | Ultrawide in Giants |
| [../../runtime/xbox360/input.md](../../runtime/xbox360/input.md) | Button glyphs in the font atlases |
| [../../runtime/host/figures.md](../../runtime/host/figures.md) | Giants' figure additions (level 15, hats, LightCore) |
| [../../porting/franchise.md](../../porting/franchise.md), [../../porting/prior_art.md](../../porting/prior_art.md) | The other Alchemy games, the other ports |

Add each new finding to the game's page (`games/<id>/docs/boot.md`) or here (engine-level), tagged **[Alchemy]**.
