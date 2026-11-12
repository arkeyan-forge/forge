# HD textures: the game's own textures doubled at install

> **Purpose:** what does Arkeyan-Builder's `--hd-textures` do, what does it cost, and why our own pass rather than an upscaling model? · **Status:** current (2026-11-08) · **Tags:** [Xbox] [Alchemy] [Lux] [Giants] [SSA]

An optional install step. Arkeyan-Builder reads every texture of the player's own disc, doubles the ones that gain from
it with our own classical pass, and writes them as a texture pack in the player's data folder,
`<game data>/textures-hd/`. The game loads it through the texture packs it already has
([texture_packs.md](../runtime/gpu/texture_packs.md)): nothing changes in the renderer, a pack file replaces the game's
texture by the hash of its content. Nothing of it is shipped, downloaded or kept in git: the pack exists only on the
player's machine, made from their disc.

    Arkeyan-Builder --iso <disc> --out <game folder> [--kit <kit>] --hd-textures off|2x|auto    # after the build
    Arkeyan-Builder --iso <disc> [--kit <kit>] [--jobs N] --hd-textures off|2x|auto              # the step alone

| Mode | What |
|---|---|
| `off` (or no option) | no pack; `off` removes `textures-hd/` |
| `2x` | every texture that gains, up to 1024 texels a side (2048 after) |
| `auto` | the same with 8 GB of memory or more; below, up to 512 a side |

A rerun with the same disc, mode and dump is a no-op (`textures-hd/hd.txt`); anything else redoes the pack. The
player's own `textures/` wins over `textures-hd/` for the same hash. `[<game>] hd_textures` (the launcher's "Use HD textures",
offered for all six games, SSA 2.3 GB and Giants 4.8 GB shown) alone turns `textures-hd/` on or off; `texture_packs` has no
say in it (0.9.4, [texture_packs.md](../runtime/gpu/texture_packs.md)).
`textures-hd/manifest.txt` lists every distinct texture met: hash, kind, alpha, format, size, what was done, source path.

**Games** (2026-11-08): every game of the port. Checked on each disc (the dump match, a run with the pack holding 60,
below): Swap Force, SuperChargers, Imaginators, Giants and SSA [V]; Trap Team [?] (same reader, not checked here).
Arkeyan Launcher offers the step per game (`HdTexturesOffered`); Giants and SSA are to be added there.

## Where the textures come from

- **The disc** [Alchemy]: every `.arc`, `.bld` and `.pak` archive, every IGZ file in it, every `igImage2` picture in DXT1,
  DXT3 or DXT5 of at least 32 texels a side (`hd_textures.cpp`, `PicturesOf`). The top level is untiled and
  byte-swapped exactly as the renderer uploads it, and named by the same hash (`texpack::ContentHash`, seed format 18,
  19 or 20). Checked against what the game itself dumps (`texture_dump=1`) in one level, 2026-11-05: Swap Force
  `barrens/brn1` 292 of the 295 DXT textures of 32 texels or more, SuperChargers' Academy all 395, Imaginators
  `hub_2016` all 318 (the other dumped textures are DXN normal maps, RGBA and small ones). Imaginators needed its
  archives' raw deflate chunks, which the shared reader now decodes ([game_art.md](../runtime/host/game_art.md)).
  Giants (IGA v8, IGZ v6) needed nothing new: `level_008_islandtown`, all 486 of the 486 dumped DXT textures of 32
  texels or more (2026-11-08) [V].
- **The disc** [Lux] (SSA): every `.str` stream (`levels/<name>/mission.str` and its hub stages, the UI streams) and
  every bare package (`textures/*.apk`), every `APKF` texture package in them (`lux_images.cpp`, `ForEachPackage`;
  `hd_textures.cpp`, `LuxPicturesOf`), with the same rules. A level's stream starts with a table the UI streams do not
  have and nests records in others, so a package is found by its body (`APKF` after a record header whose third word
  is 0), not by walking the records. The storage is read with the package's own fetch constant (pitch, tiling, byte
  order). Checked against the game's dump in `level_008`, 2026-11-08: all 467 of the 467 DXT textures of 32 texels or
  more (the other 37 dumped are RGBA, DXN and small ones) [V].
- **The game's own dump** (any game, SSA included): every file of `<game data>/textures-dump/`
  ([texture_packs.md](../runtime/gpu/texture_packs.md)), named by the hash already.

## What is doubled (the survey)

Each texture is decoded and classified (`hd_upscale.cpp`, `Analyse`); only `color` is doubled:

| Kind | Rule | Why not doubled |
|---|---|---|
| tiny | a side under 32 or fewer than 64x64 texels | nothing to gain; the base level of a small texture may sit in the packed mip tail |
| flat | every channel's deviation under 1/255 | one colour |
| normal | unit vectors in RGB (95 %, blue above 190 on average), or two channels constant and the other two varying (X and Y in green and alpha) | a vector field, not a picture: interpolation and sharpening bend it |
| mask | grey only and opaque, or one colour under a varying alpha (fonts, glows, flares, clouds) | soft by design; a font's edges come from its alpha |
| color | the rest | |

Whole discs (2x, distinct textures; [V] 2026-11-05):

| Game | Archives | Pictures | Distinct | Doubled | Mask | Tiny | Flat | Normal | Over 1024 |
|---|---|---|---|---|---|---|---|---|---|
| Swap Force | 2231 | 28 720 | 7 291 | 6 666 | 367 | 127 | 16 | 7 | 108 |
| SuperChargers | 2661 | 30 640 | 13 477 | 12 078 | 1 036 | 279 | 69 | 12 | 3 |
| Imaginators | 4995 | 45 413 | 19 549 | 17 811 | 1 247 | 357 | 117 | 14 | 3 |
| Giants (2026-11-08) | 891 | 56 509 | 11 843 | 9 133 | 342 | 98 | 13 | 2 175 | 82 |
| SSA (2026-11-08; `.str` and `.apk` files) | 239 | 41 858 | 9 899 | 7 855 | 463 | 132 | 45 | 1 403 | 1 |

Normal maps are mostly DXN (BC5) in the later games, which the disc reader does not take at all; Giants and SSA keep
theirs in DXT1/DXT5, so the survey finds and keeps them (2 175 and 1 403). In the levels measured, the textures the
game shows are about 60 % `color` (opaque DXT1 world textures and DXT5 UI pictures with alpha), the rest masks, FX
sprites, normal maps and small textures.

## The pass (ours)

Per texture, in premultiplied alpha (no dark fringes at cut-out edges), on floats:
1. **DXT deblocking**: where two flat neighbours meet at a 4x4 block seam with a small step (under 0.09, each side
   varying under 0.035), the step is spread over the four texels around the seam; a larger step is an edge and stays.
2. **2x Catmull-Rom**, one axis then the other, each result clamped between its two nearest inputs (no ringing). The
   output keeps the input's texel centres (each texel becomes a 2x2 square), so the game's texture coordinates still
   fit. A picture that tiles (its last column as close to its first as neighbours are) is filtered across its edge.
3. **Smoothing along edges**: the source's structure tensor (Scharr gradients of luminance and alpha, 3x3) gives each
   texel an edge direction and a clarity; along a clear edge the result is averaged along the edge only (7 taps), which
   takes away the staircase without blurring across.
4. **Warp sharpening**: near a clear edge each texel is resampled a little further from the edge (down the slope of the
   blurred edge map, at most one texel): the doubled ramp narrows, with no halo and no new value.
5. **Unsharp mask** (amount 0.5), each result clamped to its 3x3 range: no halo.
6. Alpha: a **cut-out** (alpha 0 or 255 but for 0.2 % of the texels) is thresholded again at 0.5 (crisp edge, the same
   coverage); a blended alpha stays graded. Under alpha 0 the colour is the source texel's, so mips and filtering show no
   black edge.
7. **Mips**: the full chain, 2x2 boxes in premultiplied alpha.
8. **Encoding** (our own BC1/BC3 encoder: principal axis, then two least-squares refinements of the endpoints; BC3's
   alpha the better of its two modes): opaque textures become **BC1** (a DXT5 that is opaque costs half its memory),
   DXT1 cut-outs BC1 with its transparent texel, the rest BC3.

Speed on an M1 Pro (4 jobs, the machine shared, load 10-15): the whole step, archives decoded and every texture done,
**128 s** for SuperChargers, **136 s** for Swap Force, **174 s** for Imaginators, **162 s** for SSA and **319 s** for
Giants (2026-11-08; Giants has the most large textures: 3 660 of 512 and 570 of 1024 texels a side doubled); one texture alone 8 ms (128x128),
40 ms (256), 120-135 ms (512), 520 ms (1024), one thread.

## Cost

| | Imaginators `hub_2016` | SuperChargers' Academy | Swap Force `barrens/brn1` | Giants `level_008_islandtown` | SSA `level_008` |
|---|---|---|---|---|---|
| Textures the level shows (dump) | 453, 267 replaced | 610, 316 replaced | 440, 247 replaced | 509, 378 replaced | 504, 347 replaced |
| Their video memory, game's | 79 MB | 76 MB | 108 MB | 50 MB | 126 MB |
| With the 2x pack | 123 MB (+44) | 125 MB (+50) | 183 MB (+74) | 150 MB (+100) | 179 MB (+53) |
| At 4x (computed) | 303 MB (+224) | 330 MB (+254) | 480 MB (+372) | not computed | not computed |

Disk: the 2x pack of a whole disc is 2.2 GB (Swap Force), 2.6 GB (SuperChargers), 3.0 GB (Imaginators), 2.3 GB (SSA),
**4.8 GB (Giants)**; a 4x pack would
be about four times that (BC1/BC3, 16 times the texels of the source). 4x is not offered: the cost grows 4 to 5 times for
a picture the screen rarely shows larger than 2x.

## Frame rate and load time (scale 2, 60 FPS limit)

`tools/perf/game_perf.sh ... --scale 2` with `PACK=<textures-hd folder>` (the run's `textures-hd/`), offscreen, M1 Pro,
2026-11-05; the machine was shared by other lanes, its load average in brackets ([testing.md](../getting-started/testing.md):
above 4, numbers compare only with each other).

| Level | Game's textures | 2x pack |
|---|---|---|
| Imaginators `hub_2016` | 60.0 fps, p99 16.8 ms, no fallback [6-9] | **60.0 fps**, p99 16.8 ms, max 17.5 ms, no fallback [5-8] |
| SuperChargers' Academy | 59.7 fps (the survey run, dump on) [6-11] | **59.9 fps**, p99 18.8 ms; two 30 FPS fallbacks, both during the intro ride before the Academy [9-12] |
| Giants `level_008_islandtown` (2026-11-08) | 60.0 fps, p99 16.7 ms, max 20.7 ms, no fallback [9-29] | **60.0 fps**, p99 16.7 ms, max 20.4 ms, no fallback; 378 replacements indexed in 67 ms [4-10] |
| SSA `level_008` (2026-11-08, the disc's pack, 347 replacements) | 60.0 fps, p99 16.7 ms, no fallback [5-6]; again 60.0, p99 16.7 [4-8] | **59.9 fps**, p99 18.3 ms, no fallback [5-8]; again **60.0**, p99 16.7 ms, max 28.7 ms [6-12] |

Imaginators, SuperChargers, Giants and SSA hold 60 with the pack. The SSA runs of 2026-11-05 (a pack from the dump,
taken at load 8 to 60: 48.9-54.5 fps with it, 56.5-59.8 without) followed the load, not the pack: on a quiet machine
both hold 60, and a pair taken at load 24 fell to 30 without the pack (36.6 fps, two fallbacks) as with it (58.8, two
fallbacks). The GPU's share per frame (`[perf]` draws) is 3.4-4.5 ms with the pack, 3.7 ms without.

Giants and SSA were measured with the level's part of the pack only (the files of the textures its dump holds: 378 and
347; the disk of the shared Mac could not hold Giants' 4.8 GB): the same replacements as with the whole pack, the
index smaller (46-86 ms; the whole packs' index is the load-time cost below).

Load time: the index of the pack at start-up read one file header after another, 2.3 s for SuperChargers' 12 078
files and 2.6 s for Imaginators' 17 811 in those runs; it now reads them on up to 8 threads (Imaginators' pack, warm
cache: 670 ms before, 340-390 ms after; 4.0 s before when the files were not cached). The level itself starts at the
same moment (Imaginators' hub 43.7 s without, 44.7 s with the pack, the second lost to the index), and the worst frame
gaps while it loads are the same (88-96 ms without, 88 ms with); afterwards no frame over 17 ms with the pack. A
replacement is read on the texture threads like the game's own texture.

Frames for the owner (`private/hdtex/shots/`, `<game>-frame_t<s>.png` and `-zoom.png`, the same second of a base run and a
pack run): the offscreen window is 1280x720, where a texture is rarely drawn larger than itself, so the frames differ
little (slightly crisper wood, rings and walls); the 2x pays off at 1440p and above and close to the camera. The texture
crops (`private/hdtex/compare/`) show the difference texel for texel.

## An upscaling model instead (compared, not used)

The owner allowed an outside upscaling model for this step (2026-11-05), on condition it wins clearly and runs on
modest PCs. Compared on the same textures (Imaginators' hub: world textures, UI pictures with alpha, the logo, a flare;
`private/hdtex/compare/imaginators-compare.png`, crops at 6x: original, bilinear 2x, our pass as packed, the models):

| | Our pass | `realesr-animevideov3` x2 | `realesrgan-x4plus` |
|---|---|---|---|
| Runs with | the CPU, Arkeyan-Builder alone | realesrgan-ncnn-vulkan (27 MB, Vulkan; on macOS through MoltenVK) | the same |
| Speed, M1 Pro | 41 ms a texture (one thread, encoding and writing included) | 60 ms a texture on the GPU (100 hub textures, PNG out, no BC encoding) | 13.7 s a texture |
| Whole disc (Imaginators) | 3 min | about 20 min on this GPU, plus the encoding; hours on a weak GPU or the CPU | days |
| Edges of flat art (logo, rings) | crisper than bilinear, a slight staircase left | the cleanest | clean |
| Textures (rock, snow) | the source's detail, sharper | smoothed: detail painted over | smoothed |
| Colour | the source's | shifts (a yellow ring gains a pale green stripe) | slight shifts |

The small model draws flat art better, but it repaints textures and colours, needs a Vulkan GPU and an outside program on
every platform, and is slower; the large one is far too slow. Neither wins clearly within "modest PCs too": **the
classical pass is the step**, and no model or library is in the repository, the kit or any build. The downloads (source,
SHA-256 and licences in `THIRD_PARTY_NOTICES.md`) stayed in `private/hdtex/ml/`.

## Code and tests

- `tools/arkeyan-builder/hd_upscale.{h,cpp}`: classification, the pass, mips, the BC1/BC3 encoders.
- `tools/arkeyan-builder/hd_textures.{h,cpp}`: sources (Alchemy pictures, `PicturesOf`; Lux packages, `LuxPicturesOf`;
  dump files), one texture to one pack file.
- `runtime/host/art/lux_images.{h,cpp}`, `ForEachPackage`: every APKF texture package of a Lux stream, undecoded.
- `tools/arkeyan-builder/main.cpp`, `HdTextures`: the step (one job per archive or stream, each with its own disc reader).
- `runtime/host/texture_pack.cpp`: `textures-hd/` indexed before `textures/`.
- `ArkeyanHdTexturesTest` (no game data): the kinds, the 2x (exact flat halves, monotonic edge, no overshoot, cut-out
  coverage within 3 %, no dark fringe, tiling seams), mips, BC1/BC3 round trips, the pack file, the dump reader and the
  Lux reader (a level-like stream: a table, another record, two tiled 8-in-16 DXT1 packages, and a bare package: the
  textures untiled, swapped and named by the renderer's hash).
  `ArkeyanTexturePackTest`: the player's file wins over `textures-hd/`.
