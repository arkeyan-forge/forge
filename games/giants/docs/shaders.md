# Giants' shaders: from the disc, without a runtime dump

> **Purpose:** where Giants' shaders are on the disc, the Alchemy formats that hold them, and how the kit builds the cache · **Status:** current (2026-10-26) · **Tags:** [Alchemy] [Giants]

Goal (done, kit 0.5.0): a player's Arkeyan-Builder builds the Giants shader cache from the disc alone, with no `SKY_DUMP_SHADERS` run through the
game. Tool: `tools/shaders/giants_extract.py` (Python standard library only, `python3 -I`). Everything it writes goes
to `private/` (derived from the disc, never committed).

## On the disc

### Where the shaders are [Giants] [V]

**All of them are in one file: `/permanent/global.bld`** (8.7 MB on disc, 13 entries). Its biggest entry, an
IGZ of 42 MB, holds 4394 distinct containers (264 vertex, 4130
pixel). None of the other 890 archives (`.arc` and `.bld`, levels, characters, misc) has any. The earlier note that
"378 raw containers are on the disc" was wrong: those were chance matches of the 4-byte magic.

Not on the disc: 50 containers that Bink 2 builds when it opens a movie (see "Bink" below).

### Format facts [Alchemy] [V] (reverse-engineered from the disc)
The engine's formats, cited by `runtime/host/art/alchemy_archive.*` and `tools/arkeyan-builder/igz_shaders.*`.

#### IGA v8 (little-endian), the `.arc` and `.bld` files
```
00 "IGA\x1a"   04 version = 8   08 u32 size of the tables   0C u32 file count n   10 u32 0x100 (data alignment)
1C u32 name table offset        20 u32 name table size      34 u32 hashes[n], then n x { u32 offset, u32 size, u32 flags }
```
- `flags == 0xFFFFFFFF`: the entry is stored as is (`.arc` mostly).
- Otherwise it is **chunked LZMA**, 0x8000 bytes of output per chunk, every chunk at a 0x100-aligned offset:
  `[u16 n][5 bytes LZMA props 5D 00 80 00 00][n bytes of raw LZMA]` (lc=3 lp=0 pb=2, dictionary 0x8000, no end marker, the
  decoder needs ~8 bytes past `n`). A chunk that does not start with those props is **stored** (0x8000 raw bytes, or the
  rest of the file). The archive also has a chunk table after the entries (offsets in 0x100 units, bit 15 or 31 =
  compressed); reading chunks one after the other makes it unnecessary.
- The name table (`u32 offsets[n]`, then NUL-terminated source paths; [art.md](art.md#iga-v8-name-table)) is not needed
  here: shaders are found by content. The reader (IGA, LZMA) is `runtime/host/art/alchemy_archive.cpp`, shared by Arkeyan-Builder
  (`tools/arkeyan-builder/igz_shaders.cpp`), the game and Arkeyan Launcher.

#### IGZ v6 (big-endian), the objects inside
```
00 "IGZ\x01"  04 version = 6   10: sections, 16 bytes each: { u32 file offset, u32 size, u32 alignment, u32 id }, until size 0
```
Section 0 holds the object tables; the other sections are memory pools. Inside section 0, the memory blocks an object owns
are listed as pairs `{ u32 kind<<24 | size, u32 section<<24 | offset }`; `kind` 0x18 is a **physical memory block** (the
microcode). Decode `section` as `sections[section + 1]` and `offset` as relative to that section's file offset.

A shader is two blocks: the **virtual part** (the D3D container header, starts with `102A1100` pixel or `102A1101`
vertex, then virtual size, physical size) and the **physical part** (constants and microcode) a few bytes after it. The
gap is 0 to 28 bytes (16-byte alignment in memory plus an optional 16-byte block), so it can't be guessed: the physical
block comes from the pair whose size equals the container's `physicalSize` and whose offset lies within 0x40 bytes after
the virtual part.

What the game passes to `D3DVertexShader_Init` / `D3DPixelShader_Init` and what `d3d/shaders.cpp` hashes is
`virtual part + physical part`, concatenated. The extractor writes exactly that, so XenosRecomp's hash (XXH3-64 of the
file) is the run-time hash.

## How we extract it

### The extractor (Python, for developers)

```
python3 -I tools/shaders/giants_extract.py <giants.iso> private/giants/shaders/disc [<runtime dump dir>]
```
It lists the disc (XDVDFS), opens each `.arc`/`.bld`, decodes every entry that starts with `IGZ\x01`, finds containers
(magic, sane sizes, block table match) and writes `<sha1 prefix>.vs|.ps` plus `manifest.txt`. With a dump dir it compares
by content and writes `missing.txt`. **48 s** on an M1 Pro for the 891 archives (4 processes, `JOBS=` to change).

### Coverage

Against the 4444 containers of the run-time dumps (several runs, to the title screen and past):

| | |
|---|---|
| found on the disc by the extractor | **4394** (264 vertex, 4130 pixel), all of them byte-identical to a dumped container |
| in the dumps but not on the disc | **50** (Bink 2) |
| on the disc but never dumped | 0 |

So static extraction is **complete for everything the engine loads from data (98.9 %)**. Nothing in the dumps came from a
level archive: the global archive is the only source, and it is loaded at boot.

### Bink 2's 50 run-time shaders [Giants]
Bink 2 builds its movie shaders in code: the 0x20-byte-aligned microcode is in `default.xex` (all 50 physical parts are
found whole in the loaded image), but the virtual part is a D3D header assembled at run time from a compiled-in Microsoft
shader blob (its fields are patched: sizes, offsets, the constant table). That part is not a byte string of the image.
**Done: the short run** (as for SSA's Bink shaders): after Arkeyan-Builder builds the game, it runs it headless with
`SKY_DUMP_SHADERS=<dir>` until the first Bink logo opens (9 s), rebuilds the cache with the 50 and relinks
([history.md](history.md), "Installing Giants"). Not done: shipping the 50 as a recipe (it needs Bink's assembler, the
`BinkOpen` path, understood).

**The picture was black on Windows (found 2026-11-01, by the tester's report) [V]:** the pass ended 8 s after the first new shader, and Giants' first ones come with the title, 17 s before the logo's under Wine. The Windows kit's own Arkeyan-Builder, run under Wine, found 45 of the 50 and finished without an error. The 5 missing draw the final passes: that game shows a black picture with silhouettes and the button hints. `movie_shaders=50` in `game.txt` makes the pass wait for all of them, and `MoviePassDone` makes an update repair a short pass. Not seen on the Mac: there the logo starts within the 8 s.

### What Arkeyan-Builder does for Giants (C++, `tools/arkeyan-builder/igz_shaders.cpp`, no Python)
1. Extract the XEX and translate the code (XenonRecomp), as for SSA.
2. Read `/permanent/global.bld` (`shaders=igz:/permanent/global.bld` in `games/giants/game.txt`): 4394 containers, the
   same as `giants_extract.py`, in under a second (the Python tool takes 48 s for the whole disc, 2 s for that file).
3. XenosRecomp on that folder: 4394 shaders, the same time as before (15 min on an M1 Pro, measured: 2319 s CPU; the translation, not
   the extraction, dominates).
4. Compile the runtime with the cache, then the short run for the 50 Bink shaders (above), the cache again and the link.

## Tests
2026-10-25:
Cache from the extractor's 4394 files only (XenosRecomp, no runtime dump, `[gpu] shader cache: 4394 shaders`), game
built in `build-giants`, offscreen isolated boot of 75 s with `SKY_SKIP_MOVIES=1`: reaches the title scene at 60 fps, 58
draws per frame. Shaders missing at draw time: **10 distinct, all pixel shaders and all among the 50 Bink ones**;
none from the engine's own data. Since kit 0.5.0 the 50 are in the cache too: `[gpu] shader cache: 4443 shaders` (4394 +
50 - 1 duplicate), the title scene at 60 fps ([history.md](history.md), "Installing Giants").

2026-10-27 (SSA polish, a 1-hour soak with the cache of the kit): the run logs one more missing pixel shader, **`95CAD664E1351A05`**, which
is **not among the 4394 of `global.bld`** (all 4394 translate: 0 skipped) and not in the cache's 50: a run-time container (with the
dump of a run at `SKY_DUMP_SHADERS`, 48 containers are not on the disc list). XenosRecomp rejects it: 3 `tfetch2D` and one `tfetch1D`, and the
translator has no 1D texture (`TEXTURE_DIMENSIONS` is 2D, 3D, cube). Supporting it takes both sides: the translator (a 1D fetch as a 2D one at
v = 0.5) and the runtime (guest 1D textures: `TextureForUntimed` returns no texture for dimension 0). Open; no visible effect was noted in the
runs (the draw is a rect list seen once, at boot).

**Fixed (2026-10-27) [V].** The translator now emits a `tfetch1D` as a fetch of a **2D texture of one row** at v = 0.5
(`patches/xenosrecomp.patch`: dimension "2D", `float2(u, 0.5)`, the offset on u only), and the runtime binds a guest 1D
texture like a 2D one (`TextureFor`, `d3d/render.cpp`: only 3D textures are refused now; the fetch constant's height is 1).
The cache has `95CAD664E1351A05` (4443 shaders + it), the pipeline is built at draw time (0.2 ms) and the draw is no longer skipped.
**Where it is used:** `SKY_LOG_SHADER=95CAD664E1351A05` says once where a pixel shader is first drawn: here at frame 921 of a
run from the title, as the tutorial's first scene appears (Eon's intro, offscreen shots show the painted scene and the dialogue
box, nothing visibly wrong), primitive 6 (a strip), vertex shader `51AA1C15D50798F3`, into a 2000x1200 target (the scene or a
full-screen pass, scale 2). Seen once; its look cannot be compared with a console, so "draws" is [V], "draws right" is [?].
The same change is available to SSA's translator for any 1D fetch (none known). **Not in a player's kit yet:** the install's short
headless run (the Bink 2 shaders) never reaches that scene, so the kit's cache lacks it and a player's game still skips that one
draw; adding the tutorial's first seconds to the install run (or this shader's container to the kit) is the next step [?].

## Is it engine-generic?
The explicit-LOD/cube translator update (2026-11-01) found neither feature in the 4394 locally available Giants disc
containers: all their translated HLSL stays byte-identical. Still rebuild locally with the updated kit (launcher game
update/rebuild), or rerun the developer cache recipe, configure and relink; Arkeyan-Builder's stamp hashes the translator
binary, and compiled entries hash translated text. It ships in 0.8.0 (Giants 0.8.5, 0.7.2 had 0.8.4), so
the launcher offers the update and the rebuild follows.
Preserve the runtime/movie containers when rebuilding, check the usual scene, shadows and movie at scales 1 and 2,
and keep all cache data on the player's machine. [Translation proof](../../../docs/runtime/gpu/shaders.md).

Swap Force and the later Alchemy games:
Probably the method, not the bytes:
- The **IGA** container changes (Swap Force v10 big-endian, Trap Team and later v11): header offsets, entry size and the
  compression (zlib/LZMA, chunk size) must be redone, but the entries are still plain IGZ files.
- The **IGZ** layout changes (v7, v8 big-endian, v9) but keeps the idea: shader objects own a virtual block and a physical
  block listed in an object table. The scan for `102A110x` + the physical-size match is version-independent as long as
  a table of `(size, offset)` pairs exists; the table's tag and the section-index convention have to be re-derived.
- Fallback when a game's layout is not understood: the XEX-hook run dump, as for Giants before this work.
Swap Force (2026-10-27) confirmed it: one archive (`shaders_xenon.pak`), the same container scan; v7 lists the physical
blocks in a tagged `TMHN` record, away from the virtual parts, paired in order ([its shaders.md](../../swapforce/docs/shaders.md)).
