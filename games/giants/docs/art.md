# Giants' pictures from the disc: logo, background, portraits

> **Purpose:** which pictures Arkeyan Launcher and the Toy Box read from a player's Giants disc, and the Alchemy formats that hold them · **Status:** current (2026-10-26) · **Tags:** [Alchemy] [Giants]

What the launcher and the Toy Box read from a player's Giants disc, and the formats that hold it. Reverse-engineered from
the disc (2026-10-25); the IGZ object tables were cross-checked against the public notes of GiantsRecomp (MIT,
`tsapZ/GiantsRecomp`, read as facts, no code taken). The code is `runtime/host/art/` ([the provider and the rule](../../../docs/runtime/host/game_art.md));
nothing it writes is shipped. The archive container (IGA v8, its chunked LZMA) is described in [shaders.md](shaders.md).

## On the disc

### Where each picture is [Giants] [V]

| Picture | Archive on the disc | Entry | Picture (igImage2) | Size, format |
|---|---|---|---|---|
| Logo | `level/Title.bld` | `level.bld` (IGZ, 49 MB unpacked) | `models/ui/giantslogo/rig/textures/spritelogo.png` | 1024x1024, DXT5: a sprite sheet (see below) |
| Background | `permanent/global.arc` | `.../ui_levelfullscreen/mainmenu.png/0x7852225b.png.igb.tex.igz` | the entry's only picture (no name) | 1600x960, DXT1: the main menu's sky |
| Portraits | `misc/UI_Collection_Champions.bld` | `level.bld` (IGZ) | `levels/includes/ui_main/sprites/collections/sky2/portraits/<name>.png` | 123 pictures, 256x256, DXT5 |
| Artworks | `misc/PvP_MainControl.arc` | `.../ui_pvp_sprites/<name>.png/0x....png.igb.tex.igz`, one per picture | the entry's only picture | 99 pictures, about 1000x960 (966 to 1475 wide), DXT5 |
| Title icon | `default.xex` | the XDBF resource | | PNG (as for SSA) |

- **Portraits.** The collection screen's scene holds every portrait of the game: the Skylanders of both games with their
  Series 2, LightCore, Legendary and alternate decos (`eruptor_series2`, `eruptor_lightcore`, `rockdragonlegendary`,
  `earthgiant_alt`), the sidekicks (`landshark_sidekick`) and the magic items and adventure packs (`magicitem_cannon`,
  `magicitem_pirateship`). The same pictures are in each Skylander's own `character/NNN_<model>.bld` (NNN is the toy
  type), but this one archive has them all (checked: the 123 names are the union of every archive's). Toys map to
  pictures in the catalogue (`resources/collection/catalogue.json`, Giants' `portraits`: 76 toys, 57 variants).
- **Artworks.** The versus screen (PvP) shows each Skylander large: 99 pictures, the Skylanders of both games and their
  decos (`eruptorseries2`, `eruptorlightcore`, `yetiseries2legendary`, `icevalkyrielegendarylightcore`,
  `alchemistred`), no sidekick or item. Mapped in the catalogue's `artworks` (53 toys, 56 variants); the best picture
  of a figure ([game_art.md](../../../docs/runtime/host/game_art.md#the-rule-for-a-toys-picture)). Found by listing every
  archive's name table (85,349 entries on the disc): no other large toy art (the cards are Skystones', the level
  pictures are paintings).
- **Background.** `ui_levelfullscreen2/fullscreen_2xx.png` (1707x960, DXT1) are the levels' loading paintings; the main
  menu's is `ui_levelfullscreen/mainmenu.png`. Streamed `.arc` pictures have no name inside the IGZ: the archive entry's
  name says what they are.
- **Logo.** No archive holds a flat logo: the title screen assembles it at run time. The model `Models/UI/GiantsLogo.mdl`
  (joints `J_skylanders`, `J_giants`, `J_elements`) has two textures: `spritelogo.png`, a sheet of four parts, apart, on
  transparency (the "SKYLANDERS" arch, the word "GIANTS", the row of 8 element medallions, the TM mark), and
  `title_giants_3d.png` (512x512, opaque: the logo flat on the plate's blue, [?] the plate mesh's texture). The plate is
  a mesh of the model, not a picture: **it is rebuilt here from the pieces** (see below). [V] for the pieces, [?] the
  plate's exact shape (the mesh was not decoded).
  The provider finds the parts by their bands of opaque texels (alpha > 16, rows, then column runs for the medallions
  and the TM) and composes them at the sheet's own scale (the largest the pieces allow, 1033x541):

  | Piece | Sheet (texels) | Placed |
  |---|---|---|
  | arch | 13..997 x 16..450 | top |
  | "GIANTS" | 148..855 x 476..651 | centred as on the sheet, 0.48 of the arch's height down, **behind** the arch |
  | 8 medallions | 104..898 x 886..993 | centred as on the sheet, 0.08 of the word's height under it |
  | TM | right of the row, at its height | as on the sheet, **not** on the plate |
  | plate | not on the sheet | the silhouette of the first three grown by 10 texels, each row filled from its first to its last texel, three box blurs; fill = the mean blue of `title_giants_3d.png`'s plate (about R 0, G 46, B 100), darkened to 35 % on the rim |

  The 0.48 and 0.08 were measured on the press logo (810x411, never committed) scaled to the sheet; the flat 3D picture
  says 0.507 for the word but draws it in perspective. **Compared with the press logo** (by proportions, same width):
  "GIANTS" x 147..854 against 150..855, y 216..391 against 212..384 (scaled); alpha coverage 0.838 against 0.836; aspect
  1.89 against 1.97 (the plate's bottom margin is a little taller). Layout and proportions, not pixels: the press image is
  retouched. SSA's logo is its own picture (`skylanderslogo`), unchanged.

### Formats [Alchemy] [V]
The engine's formats, cited by `runtime/host/art/alchemy_archive.*`; the archive container (IGA v8, chunked LZMA) is in
[shaders.md](shaders.md#iga-v8-little-endian-the-arc-and-bld-files).

#### IGA v8 name table
At header `+0x1C` the name table's offset, `+0x20` its size: `u32 offsets[n]` (from the table's start), then the
NUL-terminated names, one per entry: the source path, e.g.
`C:/tfb/build/360/levels/includes/ui_levelfullscreen/mainmenu.png/0x7852225b.png.igb.tex.igz`, or `level.bld` and
`ENGLISH.pak` in a `.bld`. Plain text in every archive read so far (`global.arc`, `Title.bld`,
`UI_Collection_Champions.bld`).

#### IGZ v6 objects (big-endian)
- **Sections** at `0x10`: `{u32 offset, u32 size, u32 alignment, u32 id}` until an empty one. Section 0 holds the object
  tables ("fixups"); the others are memory pools.
- **Section 0**: its own header (its length at `+0x0C`), then records `{u32 type, u32, u32, u32 count, u32 length, u32
  data start}`, the data start counted from the record. The types used here:

  | Type | Holds |
  |---|---|
  | 0 | the class names, NUL-terminated, `count` of them: an object's first word is its index here |
  | 2 | the external ids, 8 bytes each: `u32 hash, u32 namespace`; a picture's format is one |
  | 5 | the object offsets: 4-bit nibbles (low one first), 3 data bits and a continue bit, least significant group first; each value is the distance from the previous object in 4-byte units, minus one |
  | 0xA | the memory handles, 8 bytes each: `u32 size \| flags << 28, u32 pointer` |

- **Pointers**: the pool in the top 8 bits (pool n is section n + 1), the offset in the pool below.
- **igImage2** (0x60 bytes): `+0x08` pointer to its name (the source path), `+0x30` u16 width, `+0x32` u16 height,
  `+0x3C` `0x80000000 | external id index` (the format), `+0x48` the memory handle of its pixels.

#### Picture formats
The format is the external id's hash. Seen on the disc (58,394 pictures in 891 archives, 2026-10-25):

| Hash | Pictures | Format |
|---|---|---|
| `B9BCDC57` | 30,315 | DXT5, tiled |
| `DC78A2E3` | 27,947 | DXT1, tiled |
| `19E29222` | 132 | 8 bits per texel, tiled (`globaltextures/format_tfl8/`: 128x2 lookup ramps, 4096 bytes: rows padded to 32) |

`runtime/host/art/alchemy_archive.cpp` also knows the linear DXT1/DXT5 and the DXT3 hashes of the public format lists
([?] not seen on this disc). A8R8G8B8 is decoded (`art::TextureFormat::Argb8`, unit-tested) but no Giants picture uses it.
Pixels are tiled as every Xenos texture (Xenia's `Tiled2D`, 32-block rows), 16-bit words big-endian.

**Every picture is stored upside down** (UI sprites, portraits, paintings): the decoder flips it. Seen on the aurora of
`fullscreen_203` (stars at the bottom as stored) and the logo's letters.

## Button pictures (glyphs) [Alchemy] [Giants] [V]
Every picture of a pad button found (2026-10-27: texture dumps of the title, the tutorial and levels, a static scan of the
64x64 pictures of every archive, the name tables of all 85,349 entries; 2026-10-28: every picture of every archive, by the
renderer's hash). Replaced for each glyph style
([input.md](../../../docs/runtime/xbox360/input.md), "Glyphs"):

| Picture | Where | Status |
|---|---|---|
| Buttons in the prompts' fonts (A, X, Y, B, LS, RS, START, BACK) | atlases `AD2E31ADEC576354` (512), `462EB82E5F1D8851` (1024), cells in `adapter.cpp` | replaced |
| Green A of "Press A to begin" | `F4C85169D542FC77`, 64x64, in `level/Title.bld` | replaced |
| Small gold Y coin over the tutorial's rock (`models/ui/buttonicons/360/textures/wii_c.png`, model `ButtonIcons_wii_c`, the dialogue actor's button; shown with a Giant on the portal) | `E88F26B298A077B7`, 64x64 DXT1 with mips, stored upside down; in `permanent/global.bld` and 18 PvP levels, all copies identical | replaced |
| **Large gold Y coin of the "Mash!" prompt** (the Giant lifts the rock: Y to mash; `models/ui/buttonicons_ui/360/textures/wii_c.png`, model `ButtonIcons_HD_360`, driven by `UI_Controller_Prompt.ai`, which 79 levels include: the tutorials 213/214, 200-212, 215, the challenges, arenas and PvP) | `6DE08CB96DD4DD23`, 128x128 DXT1 with mips, stored upside down, `permanent/global.bld` | replaced (2026-10-28): the Y the user still saw on the keyboard in 0.6.0; shot before/after |
| Large green A coin of the same model | `7FAE9AE5883D1AB9`, 128x128 DXT1, upside down, `permanent/global.bld` | replaced; drawn in the tutorial run (`a_coin 128 drawn as keyboard`) |
| White RT keycap of the giant robot's HUD (`ui_main/sprites/robot/triggerr_360.png`, `Level_209_GiantRobot.bld`) | `179828095390F2C2`, 86x85 DXT5, upside down | in `GlyphTextures`; [?] not seen in game, and its hash assumes a tiled pitch of 32 blocks (the width is not a power of two) |
| Wii buttons atlas | `1F11BD3F9CB5136B` | never shown on the 360; left |
| `controls_360.png` / `controls_ps3.png` / `controls_wii.png` (`levels/includes/ui_main/sprites/controls/sky2`, in the options screen and every challenge level's archive) | whole-controller diagrams with labelled lines | not replaced: a diagram of one pad, not a button; [?] only seen in the archive names |
| Other world coins (B, X), mash prompts | none: the 2026-10-28 scan hashed every picture of the 891 `.arc`/`.bld` archives (58,394 pictures, 954 names, the renderer's hash: it gives back every known one) and found no other button picture; the PS3 RT (`49F639E1E7D89C8D`) and the Wii remote's textures are never drawn on the 360 | done [V] for the disc; the letters are never geometry |

## How we extract it
The provider in `runtime/host/art/` (shared with Arkeyan Launcher and the Toy Box) reads the archives above with
`alchemy_archive.cpp`, decodes the pictures (and flips them), composes the logo from its pieces (above) and caches PNG
files in `<data>/Giants/` (`launcher-cache/`, `portraits/`): [game_art.md](../../../docs/runtime/host/game_art.md).
The catalogue maps toys to pictures (`portraits`). Nothing it writes is shipped.

## Tests
`ArkeyanArtTest` (`runtime/host/art/art_test.cpp`): the readers on synthetic fixtures, and with `GIANTS_ISO` the whole
extraction: logo 1033x541 (`TestGiantsLogo`: size and aspect, alpha coverage 0.75..0.9, the plate's blue, the medallion row's span, the TM, transparent corners), background 1600x960, title icon, 123 portraits, 99 artworks, in 9.7 s on an M1 Pro (1.8 s without the artworks; 0.0 s once cached).

## Is it engine-generic?
The IGZ object tables are the engine's; Swap Force (IGZ v7/v8) changes the record tags and the pointer's pool bits
([shaders.md](shaders.md#is-it-engine-generic)). Each game's pictures (which archive, which name)
are game data: one row of `runtime/host/art/game_art.cpp`'s table per game.
