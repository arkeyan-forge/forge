# HD texture packs: dump the game's textures, replace them with your own

> **Purpose:** how do I dump the game's textures and replace them with my own (HD packs)? · **Status:** current (2026-11-05) · **Tags:** [Xbox] [SwapForce]

A texture pack is a folder of DDS files named after the game's textures. The game loads them instead of its own, at
any size. This works the same for every game of the port; each game has its own folders.

## Where the folders are

Under the game's data folder (`docs/getting-started/settings.md`, "Where everything lives"): `<Skylanders data>/SSA/` for Spyro's
Adventure, `<Skylanders data>/Giants/` for Giants, `<Skylanders data>/SwapForce/` for Swap Force (checked 2026-10-27: 318
textures dumped by the tutorial to `SwapForce/textures-dump/`, one replaced from `SwapForce/textures/` and seen on the legal screen),
`SuperChargers/`, `Imaginators/` and so on (`datadir::GameFolderName`).

| Folder | What |
|---|---|
| `textures-dump/` | written by the game when the dump is on: one `<hash>.dds` per distinct texture |
| `textures/` | your pack: `<hash>.dds`, in this folder or in any subfolder (sort them by menu, level, character...) |
| `textures-hd/` | the HD textures Arkeyan-Builder made from the player's own disc (`--hd-textures`, [hd_textures.md](../../tools/hd_textures.md)); read first, so a file of yours in `textures/` with the same name wins. Its own switch, `hd_textures` (below) |

`<Skylanders data>` is `~/Library/Application Support/Skylanders` on macOS, `%APPDATA%\Skylanders` on Windows and
`~/.local/share/Skylanders` on Linux (`SKYLANDERS_DATA` overrides it).

## Making a pack

1. Turn the dump on (below) and play. Every texture the game shows is written once to `textures-dump/`. The file name
   is the texture's identity: `0123456789abcdef.dds`, 16 lowercase hex digits. Play the menus and the levels you
   want to cover; a texture is written the first time it is uploaded, and never again (nor over an existing file, so
   you can leave the dump on).
2. Open the dumps in an image tool and paint or upscale them. Keep the file name.
3. Save to `textures/<same name>.dds` (any subfolder). Restart the game.

The dump shows what the game has, not where it is used: find a texture by looking through `textures-dump/` with a
thumbnail viewer, or by replacing it with a solid colour and looking for the colour in the game.

## Settings

| `launcher.ini` `[video]` | Variable | Command line | Default |
|---|---|---|---|
| `texture_dump=1` | `SKY_TEXTURE_DUMP=1` | `--texture-dump 1` | 0 |
| `texture_packs=0` | `SKY_TEXTURE_PACKS=0` | `--texture-packs 0` | 1 |
| `[<game>] hd_textures=0` | `SKY_HD_TEXTURES=0` | | 1 |

`hd_textures` alone decides whether `textures-hd/` is read, `texture_packs` only `textures/` (0.9.4: before, a player with
`texture_packs=0` got no HD textures either). It is the launcher's "Use HD textures" switch, which keeps it in the game's
own section; on when not set, as the folder only exists once made. Tested in `ArkeyanTexturePackTest` (the HD folder alone) and
`ArkeyanSettingsTest`. The launcher greys "Custom texture packs", its folder and "Dump textures" ("Coming later", 0.9.4) and
starts the game with `texture_packs=0` and `texture_dump=0`; a run without the launcher keeps the defaults above.

`texture_packs` is on by default and does nothing when `textures/` is missing or holds no usable file: without a pack
the game runs exactly as before (nothing is hashed). The pack is read at start-up; a restart picks up changes.
The log says `[texpack] N replacement textures in <folder> (M invalid, K ignored; T ms)` for `textures-hd/`, then for
`textures/`, and, for every file it can't use,
why (`[texpack] ignored <file>: ...`). Such a file is skipped and the game's own texture is used.

## Formats

**Dumped** (the top mip level only, as the game uploads it):

| Game format | DDS |
|---|---|
| DXT1, DXT3, DXT5 (BC1, BC2, BC3) | the same, still compressed (`DXT1`, `DXT3`, `DXT5` FourCC) |
| DXN (BC5), DXT5A (BC4) | the same, still compressed (`ATI2`, `ATI1`) |
| 8:8:8:8, 5:6:5, 1:5:5:5, 4:4:4:4 | uncompressed 32-bit RGBA (R in the low byte) |
| 8 and 8:8 (one and two channels) | uncompressed 32-bit RGBA: the missing channels are 0 and alpha is 255 |

The game's 16-bit formats and the floating-point and 16-bit-per-channel formats are neither dumped nor replaced.
Channel meaning is the game's: a normal map is BC5 with X and Y in red and green, as the shader expects it.

**Read** (the replacement): BC1, BC2, BC3, BC4, BC5 (legacy `DXT*`, `ATI1/2`, `BC4U`/`BC5U` FourCC, or the DX10
header with the DXGI `BC1`-`BC5` UNORM or sRGB formats; the data is used as is, whichever the intent), and uncompressed
32-bit RGBA or BGRA (legacy masks or DXGI `R8G8B8A8`/`B8G8R8A8`; with no alpha mask the texture is opaque). The
replacement need not have the original's format, size or mip count: `texconv`, GIMP's DDS plug-in, Compressonator
and NVIDIA Texture Tools all produce readable files. Give it a full mip chain (or the texture will shimmer at a
distance) and keep the aspect ratio: the game draws it with the same texture coordinates. Not read: BC6/BC7,
16-bit and float formats, cube maps, volume textures and texture arrays (the file is reported and skipped).
Sizes up to 16384.

The sampler, the swizzle and the alpha test/blend stay the game's. The alpha channel of a replacement of a texture that
has alpha (cut-outs, glows) must be authored the same way.

## What is not replaced

- Render targets and what the game resolves into textures (reflections, the screen copies, shadow maps).
- Bink movies (the movie frames are not game textures).
- Cube maps, volume (3D) and 1D textures, and 2D arrays.
- Textures whose format is listed under "neither dumped nor replaced" above.
- [SwapForce] The game's strings (menus, HUD, "Press X to Attack") and the buttons inside them: it draws them into small textures it fills on the CPU (the dump skips them as dynamic); the buttons are redrawn in memory instead ([input.md](../xbox360/input.md), "Glyphs").
- Button glyphs (`docs/runtime/xbox360/input.md`) while a glyph style other than the Xbox 360 one is selected.
- The Toy Box and the launcher (they are not the game's textures).

A texture the game rewrites every frame (a buffer it fills on the CPU: a movie frame, the screen copies the title
draws itself) is not a game texture. An address whose content changes at 4 uploads in a row, each at most 2 frames
after the one before, is treated as such until it stays unchanged for 3 frames: it is neither dumped nor replaced. So
are the addresses the GPU resolves into.

Textures that a game streams in pieces or generates by writing to memory on the CPU change content as they go: each
distinct content is a different file (and a different hash).

## The name: how the hash is made

Pack tools can compute it too. The hash is **XXH3-64** (`XXH3_64bits_withSeed`) of the texture's top mip level **as the
renderer uploads it**: untiled (linear rows of blocks, no padding), byte-swapped to the PC order, the 16-bit formats
expanded to RGBA8 as above; seed `format | width << 8 | height << 24`, with `format` the Xenos texture format number
(6 is 8:8:8:8, 18 DXT1, 19 DXT3, 20 DXT5, 49 DXN, ...) and the size in texels. It is printed as 16 lowercase hex
digits. It depends on the content, the format and the size only, never on the texture's address, so it is the same on
every platform and in every run. (Changing how it is made would orphan every pack: `ArkeyanTexturePackTest` pins a value.)

## Cost

The hash is computed once per distinct storage content (cached by the same fast hash the renderer already makes of the
guest data), only while a dump is on or a pack holds at least one file, and never per draw. The replacement files are
read and copied into GPU upload memory on the texture threads, as the game's own textures are decoded; the render
thread only looks the hash up in an index made at start-up from the file headers. A replacement is on screen from the
first frame its texture is drawn. Big packs cost video memory and start-up time (the index reads one header per file).
With packs off, or no pack, the speed is the game's. Measured (macOS, M1 Pro, `level_027`, scale 1, offscreen,
`[perf]` textures time per frame): no pack 1.9-2.0 ms, a pack of one file 1.9-2.1 ms, and 29.9 fps (the limit) for
both when the machine was quiet; 14-17 fps for both when it was loaded (load average 40-70). 553 hashes in the first
60 s of a run (menus and the first seconds of the level).

## Tests

- `ninja -C build ArkeyanTexturePackTest && build/ArkeyanRuntime/ArkeyanTexturePackTest`: DDS writer and reader (BC1-BC5, RGBA8,
  mip chains, DX10 header, bad files), hash stability, the dump writer and the pack index on a scratch folder.
- An isolated offscreen run (recipe of `docs/getting-started/testing.md` with the isolation variables), title screen, 3 s in:
  - dump on: 21 files, all valid, none of them a render target;
  - the logo (a 1024x512 DXT5 file) replaced by a 4x4 RGBA8 file of pure green: `SKY_DUMP_FRAME` shows 326 169 exact
    (0,255,0) pixels where the unmodified run has none (the logo's quad is 808x404 = 326 432 pixels);
  - the sky (a 1024x512 DXT1 file) replaced by an 8x8 DXT1 file with a 4-level mip chain, solid red: 548 318 pixels of
    pure red (dimmed by the autosave dialog) in the frame.
