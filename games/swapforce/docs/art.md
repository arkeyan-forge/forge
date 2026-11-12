# Swap Force's pictures from the disc: logo, background, portraits, artworks

> **Purpose:** which pictures Arkeyan Launcher and the Toy Box read from a player's Swap Force disc, and how · **Status:** current (2026-10-27) · **Tags:** [Xbox] [XDK] [Alchemy] [SwapForce]

What the launcher and the Toy Box read from a player's Swap Force disc, and the formats that hold it. Reverse-engineered
from the disc (2026-10-26/27); the code is the shared provider `runtime/host/art/`
([the provider and the rule](../../../docs/runtime/host/game_art.md)), driven by the manifest
[games/swapforce/art.json](../art.json). Nothing it writes is shipped.

## On the disc

### Where each picture is [SwapForce] [V]

Every file is under `/archives/` (2231 `.pak` files, IGA v10). A picture is a texture file (`textures\<name>.igz`, IGZ
v7, one `igImage2`), often named by a hash only; a **material instance** (`materialinstances\...\<name>.igz`) names its
texture, and the toy data names the materials.

| Picture | Archive | Entries | Size, format |
|---|---|---|---|
| Logo | `archives/defaultloadscreen.pak` (the loading screen) | `textures\juicestandard_title_00.igz` (source `ui\Loading\Title.tga`) | 872x400, 32 bits per texel (`250C4EF0`, lossless): the whole "SKYLANDERS SWAP FORCE" logo with the element row and the TM, on transparency. `_10` is the DXT5 copy; see "The logo" below |
| Background | `archives/juicedomain_frontend.pak` (the menus) | `textures\juicestandard_mainmenu_background_10.igz` (`ui\PauseMenu\MainMenu_Background.tga`) | 1280x720, DXT5: the main menu's painted sky |
| Portraits (collection icons, one per toy and per variant) | `archives/toycollectionimages.pak` | 208 material instances `materialinstances\toycollection\collection_<toy>_<normal, reposed, reposed2013, lightcore, legendary, ...>.igz`, 208 textures `textures\juicestandard_collections_<name>_10.igz` or `..._<hash>.igz` | 192x192, DXT5 (`ui\Collections\ToyInventoryIcons\<2011, 2012Characters, MagicItems...>\Collections_<Name>.tga`) |
| UFO Hat (magic item 3204) | `archives/collectibleicons.pak` (the hats' icons) | `materialinstances\haticon_ufo_materials.igz` | 160x160, DXT5: its hat's icon (its toy data names no picture) |
| Artworks (the face-off portraits, one per Skylander and per variant) | `archives/faceoffportraitsimages.pak` | 179 `materialinstances\portrait_faceoff\<toy>[<deco>]_fullscreen_materials.igz` (`zook_fullscreen_materials`, `zooks2_fullscreen_materials`), textures `<Toy>[<Deco>]Portrait640` | 640x320, DXT5, the character cut out on transparency |
| Swappers' halves (the swap screen's pictures: a top above the waist, the legs, one each per swapper and per version) | `archives/characterillustrations.pak` | `materialinstances\wiiportraits\<toy>body_`, `<toy>_` (Fire Kraken, Hoot Loop, Free Ranger, Night Shift), `<deco><toy>top_`, `<toy>legs_`, `<deco><toy>legs_` `wii_material.igz`; textures `JuiceStandard_Body_<Toy>_Combo_10` (512x512 DXT5), the decos' `_Wii_10` (1024x1024) or hash-named (256x256) | on transparency; a top fills the upper part of its canvas, the legs the lower. Also `<toy>belt_` (belts, not used) |

- **Each toy's data** (`archives/toydata.pak`, `<toy>_toydata.igx`, read by `tools/toy_tables.py`) names its pictures:
  `_toyCollectionMaterial` (the portrait) and `_faceOffPortraitMaterial` (the artwork) on the toy and on each variant
  (`Collection_Zook_Normal.Collection_Zook_Normal`, `ZookS2_FullScreen_materials.FaceOff`). The material's file is its
  name in lower case.
- **Sidekicks** (505, 514, 519, 526, 540-543) name no picture: their icons are the collection's
  `collection_pet_<terrabite, gillrunt, triggersnappy, whisperelf, liltreerex, lilthumpback, lilninjini, lileyebrawl>_normal`.
- **Swapper halves, pictures**: `characterillustrations.pak` holds, for each of the 16 swappers and their 9 other versions (Dark Blast Zone,
  Legendary Free Ranger and Night Shift, Jade Fire Kraken, Enchanted Hoot Loop, Nitro Magna Charge, Quickdraw Rattle
  Shake, Nitro Freeze Blade, Dark Wash Buckler), the top's picture and the legs' picture, cut out on transparency, drawn to be
  stacked as the swap screen does. The names are not regular (`aviatorbody`, but `fireworks` and `fortune` for those tops,
  `stormbirdtop` has no picture, `legendaryvampire` is a top), and **Quickdraw Rattle Shake's top is the material named
  `altdecocowboybelt`** (`altdecocowboytop` is its belt's small picture) [V]. The catalogue's `halves` maps
  each toy and version to its file (50 entries); the manifest extracts every `body_`, `top_`, `legs_` material and the four plain top names, and the
  one belt-named top (51 files; `altdecocowboytop` is extracted and unused).
- **Swapper halves, toy data**: only the top half (`swap_top`, part `Body_<pair>`) names pictures; a bottom half (`Legs_<pair>`)
  shows the same swapper, so the catalogue gives it its pair's top's pictures (variant by variant).
- Logo: see "The logo" below. Not used: `archives/characters_fullscreen.pak` (81 materials `<toy>_bio_materials`, 1280x720: one per character, no
  variants, the name painted in, on a sky: the bio screen), `mainmenubackground.pak` (the 3D menu scene's textures),
  the full-screen pictures `juicedomain_frontend.pak` `image_1..4_00` (a Free Ranger, a Wash Buckler, a Trigger Happy and a Stealth Elf as
  painted art, 800x800 to 1024x768, 32 bits) and `leveltitle_00` (1280x720: the logo over the painted sky, opaque), which the launcher does not use.

### The logo [SwapForce] [V] (2026-10-27)
Every logo on the disc is **872x400**: `defaultloadscreen.pak` `juicestandard_title_10` (DXT5) and `_00` (32 bits, lossless; the same
picture, the same size: the DXT5 copy loses about 3.7 levels of 255 on average), `juicedomain_permanent.pak`
`loadingsplash_10` (872x400 DXT5), and `leveltitle_00` (1280x720 with the logo about 850 px wide, on an opaque sky). SSA's
logo is 1024x512 and Giants' 1033x541, so Swap Force's is 15 % smaller, and the launcher used to stretch every logo to the
window, which showed. No larger piece exists (no sprite sheet, no 3D logo mesh with a texture bigger than this one), so there
is nothing to compose from: the manifest reads the lossless `_00` (its format, `250C4EF0`, is A8R8G8B8 tiled, 8-in-32, the
size padded to 32x32 texels: 896x416), and **Arkeyan Launcher never draws a logo above its own pixels** (`App::DrawLogo`).
Cache version 4 makes the logo again.

### Formats [Alchemy] [SwapForce] [V]

Checked on this disc with Python and by `ArkeyanArtTest` (2026-10-26/27).

- **IGA v10**: big-endian throughout. Header 0x38: `1A 41 47 49` ("IGA\x1a" as a BE u32), version 10, entry count at
  0x0C, the name table's position at 0x2C (the low half of a BE u64 at 0x28) and its size at 0x30; then u32 name
  hashes[n] (sorted) and n entries of 16 bytes `{u32 ?, u32 offset, u32 size, u32 mode}`; the name table is u32
  offsets then the names, each followed by 4 bytes of its own. Mode `0xFFFFFFFF` = stored; top 4 bits 2 = LZMA (646 of
  670 entries of `permanent.pak`; 15 = stored), in chunks of 0x8000 bytes of output that start **0x800-aligned**, each
  `[u16 n, little-endian][LZMA props 5D 00 80 00 00][n bytes]` as in v8 (the block table after the entries is not needed
  to read them).
- **IGZ v7**: big-endian. Header: `IGZ\x01`, version 7, a hash, then the section table at **0x14**, 16 bytes each
  `{u32 (0, 0, 8, 0x14...), u32 offset, u32 size, u32 alignment}`, until an empty one. Section 0 holds tagged records
  `{tag, count, length, data start (0x10)}` whose tags are stored as little-endian words (`TSTR` reads `RTST` as a BE
  u32): `TSTR` (strings), `TMET` (class names), `MTSZ` (their sizes), `EXID` (external ids, `{hash, namespace}`),
  `TMHN` (memory handles), `TDEP` (dependencies), `RVTB` (the objects), `RSTT`, `ROFS`, `REXT`, `RMHN`, `RHND` (where
  the objects hold string indexes, pointers, external ids, handles), `ROOT`, `ONAM`. The other sections are the pools
  (a pointer's pool n, in its top 5 bits, is section n + 1); the last, 0x1000-aligned, holds the pixels.
- The `R...` lists are v6's packed offsets (4-bit nibbles, 3 data bits and a continue bit; each value the distance from
  the previous in 4-byte units, minus one), so v6's object walk reads v7 unchanged.
- **`TMHN`**: 8 bytes per handle, `{u32 kind << 24 | size, u32 pool << 27 | offset}` (pictures: kind 0x50; the shaders'
  physical memory: 0x18). v6 has the size in 28 bits.
- **`TDEP`**: `{name, path}` pairs of NUL-terminated strings: a material's are its shader (`JuiceStandard`,
  `shaders:/JuiceStandard.igz`) and its texture (`JuiceStandard_Collections_Zook_10`,
  `textures:/JuiceStandard_Collections_Zook_10.igz`), the texture being the entry `textures\<name>.igz` of the same
  archive.
- **`igImage2`** (0x60 bytes, the second object of a texture file, at offset 0x10 of the second pool): v6's layout.
  `+0x08` its name, **an index in `TSTR`** (the source `.tga`, `U:\data\textures\ui\...\Collections_Zook.tga`; v6: a
  pointer), `+0x30` u16 width, `+0x32` u16 height, `+0x34` u16 (1: depth [?]), `+0x36` u16 mip levels (1 for every UI
  picture; 5 for a 1024x512 scene texture, whose size matches 5 levels), `+0x38` u16 faces (6 for a cube map) [?],
  `+0x3C` `0x80000000 | external id index` (the format), `+0x48` the memory handle of the pixels (the base level
  first).
- **Formats** seen in eight archives (`defaultloadscreen`, `juicedomain_frontend`, `toycollectionimages`, `faceoffportraitsimages`, `characters_fullscreen`, `mainmenubackground`, `faceoffloading`, `legal`: 744 pictures): `049D7FAA` (567, every UI picture) **DXT5, linear**, its rows
  **32 blocks apart** (a 192-texel icon: 64 blocks, 0xC000 bytes for 48 rows; the 256-byte alignment of Xenos linear
  textures would give 48), 8-in-16; `DC78A2E3` (91) DXT1 tiled and `B9BCDC57` (31) DXT5 tiled, as Giants'; `A3708366`
  (45, normal maps, 16 bytes per block: likely DXN) [?], `250C4EF0` (9, 32 bits per texel, size padded to 32x32 texels:
  **A8R8G8B8 tiled, 8-in-32 [V]**, decoded since 2026-10-27: the logo, the legal and level title screens), `61393F6B` (1, a
  256x16 colour lookup table, 32 bits) [?]. The normal maps and the lookup table are not decoded (no picture the launcher needs).
- Pictures are **stored upside down**, as Giants': the decoder flips them.
- **igx**: the toy data and other game data are plain XML (`<igx><object refname type>...`), not IGZ.

## Button pictures (glyphs) [Alchemy] [SwapForce] [V]
Found 2026-10-27 (name tables of every `.pak`, the font files, a probe of guest memory). Replaced for each glyph style
([input.md](../../../docs/runtime/xbox360/input.md), "Glyphs"):

| Picture | Where | Status |
|---|---|---|
| Buttons inside every string ("Press (X) to Attack.", "Select", "Back") | the two `igImage2` named `button` of `fonts\proximanova-body.igz` (`essentialui.pak`, 256x83) and `-info.igz` (`languagestartup.pak`, 256x168): 32-bit ARGB, tiled, big-endian; A B X Y LB RB LT RT LS RS (the info one also holds keyhole, bottle cap and "!", kept). The CPU text renderer copies them into the strings' textures; they sit in physical memory (`ACD62000`, `ACC45000` in a run), found by the XXH3 of their first 4096 bytes | replaced in memory (`glyphs::MemoryImage`); shown on a string composed after the switch |
| Words naming a button in the text | the strings (`igLocalizedStringList` siblings `ui\<name>_<en/fr>.igz`, TSTR records, and the `<igx>` text `*_<en/fr>.igx`, UTF-8; 17,425 strings, all read 2026-10-28): every in-text button is a font character (`ʘ` A, `^` B, `~` X, `` ` `` Y, `[` `]` LB RB, `<` `>` LT RT, `{` `}` LS RS: 114 strings, drawn from the `button` images above); the only text that spells one is the title's "PRESS START" / "APPUYER SUR START" (`ui\title_*.igz` and `strings\xenonplatformstrings_*.igx`) | **START is a picture** (2026-10-28) [V]: the intern (`String_Intern`, `engines/alchemy/prompt_text.cpp`, shared with SuperChargers and Imaginators since 2026-11-06: a list of press-Start phrases, `prompt_text.h`) replaces the last word START by U+0001, and the font's character lookup (`Font_CharMetrics`, `sub_82553E48`) answers it, in the info font, with metrics of ours pointing at an empty part of the `button` image (x 199-245, y 0-55) where the adapter adds a Start cell drawn in every style (keycap of the bound key, OPTIONS, START on PS3, +, Menu, the 360's silver ▶). Live on a device switch (nothing cached); shots of the six styles and of a Swap Force-only binding (`key_start=Tab`: "PRESS TAB") |
| Turbo race HUD green A | `textures\juicestandard_turbo_a_10.igz`, 128x128 DXT5, hash `0758AAD79A30A2C8` | in `GlyphTextures`, [?] never seen in game |
| World prompts `models\ui\buttonprompt\xenon\{a,b,x,y,mash_*,b_hold,lt,rt,rs_*}.igz` (+ `vfx` effects) | meshes (`ButtonPrompt` shader) coloured by 4x4 maps `ColorMap_Yellow_Y`, `_Green_A`, `_Red_B`, `_Blue_X`, `_Blue_Letters` and a 128x128 ring: the letters are geometry | not replaceable by a texture; stay the 360's: Trap Team's way (`glyphs::Model`: the letters' draw hidden, the glyph drawn on the card's texture) needs a texture that covers the face, and these are coloured by 4x4 maps (2026-11-02, the `.pak` names checked again). **The later games' way** (`glyphs::Prompt`, [input.md](../../../docs/runtime/xbox360/input.md), "World prompts", 2026-11-05) fits: the meshes are SuperChargers' (the faces of A, Y, B, X and LT and the letters of A and Y hash alike; RT, the other letters and the Mash ones differ), but **no prompt was met in a run**, so none is listed: the first level from the cutscenes to its first jumps (Tessa and Flynn's talk, the path along the cliff, two chests) showed none [?] |
| Controller diagrams (`controller_scheme*.pak`: `juicestandard_xbox_controller_10`, `_xboxone_`, `_ps3_`, `_wiiu_`) | pictures of one pad | not replaced |
| `models\ui\buttonprompt\ps3\rs_up_ps3` | PS3 mesh prompt, unused on the 360 | left |

Proof: 5 styles of "Press (X) to Attack." and the PlayStation "Select"/"Back" of the difficulty menu; the harness check
`glyphs follow the device (PS5 pad, buttons redrawn in memory)`.

## How we extract it

The manifest [games/swapforce/art.json](../art.json): logo and background are `{archive, entry}` (the texture's first
picture); portraits and artworks are `"materials": [{archive, folder, ending}]`: every material instance of the archive
under `folder` (and ending with `ending`: the face-off folder also holds 8 element icons) names its texture, decoded and
written as `<material>.png` in `<data>/SwapForce/portraits/` (`artworks/` for the artworks). The reader
(`runtime/host/art/alchemy_archive.cpp`, the IGZ v7 row of its table; `game_art.cpp`, `AlchemyMaterials`) is the one
Giants uses; nothing in it is Swap Force's but the table row.

The catalogue (`resources/collection/catalogue.json`, the `swapforce` game) maps toys to these files: `portraits` (233
keys: 133 toys, 100 variants; 209 files) and `artworks` (200 keys: 101 toys, 99 variants; 175 of the 179 files),
`"<type>"` or `"<type>:<variant id>"` as Giants' ([collection.md](../../../docs/runtime/host/collection.md)); `halves` (50 keys) are the
swappers' stacked pictures above. Built from
`tools/toy_tables.py`'s dump (2026-10-27): each toy's and variant's `picture` / `portrait` material in lower case, a
variant listed only when its picture differs from its toy's; the sidekicks, bottom halves and the UFO Hat as above.
Four face-off portraits no toy data names (`chills3`, `doubletroublelegendary`, `frightriderlegendary`,
`stealthelfdark`) are extracted but not mapped; one collection icon (`collection_magicitem_hiddentreasuresilver`) is
extracted but not mapped. Every toy Swap Force accepts (its own, Giants' and SSA's: 133) has a portrait from this disc.

## Tests

- `ArkeyanArtTest` with `SWAPFORCE_ISO=<iso>` (178 checks with that disc alone, 226 with all three; 4 s the first time, 0.00 s cached; `SWAPPER_PICTURES=<folder>` also writes the
  16 matched swappers, their 9 other versions and 5 mixed ones as PNG, to look at): the 32-bit format, logo 872x400 (its size is checked), 230 artworks (179 + 51), every half has its picture, background
  1280x720, title icon, 209 portraits, 179 artworks, every toy the catalogue maps has its picture and every mapped file
  exists, every accepted toy has a picture from this disc alone, Series 2 Zook's best picture is
  `artworks/zooks2_fullscreen_materials.png` (640x320). Without a disc, the IGZ v7 fixture checks the string-index
  name, the dependency and the linear DXT5 pitch. Looked at by eye (2026-10-27): the logo, the sky, Zook and Series 2
  Zook, a swapper, a sidekick, the UFO Hat, a dozen icons and face-off portraits: right way up, right colours). Also by eye (2026-10-27): the 45
  halves' pictures (contact sheets of every top and every legs), the 16 matched swappers stacked, their 9 other versions and 5 mixed
  swappers, in Arkeyan Launcher (the shelf at two window sizes, the sheet, Add a figure) and in the Toy Box; and the logo
  before and after at the default window size and a large one (`private/` shots, not committed).
- `python3 -I tools/toy_tables.py <Swap Force ISO>` reads `toydata.pak` through the same IGA v10 layout.
- Giants' extraction is unchanged by the v7 work: the same 228 PNG files, byte for byte, before and after (2026-10-27).

## Is it engine-generic?

The IGA v10 and IGZ v7 layouts are Alchemy's (Vicarious Visions' later engine) [Alchemy]; Trap Team and later (IGA v11,
IGZ v8+) have to be checked again (their rows are from public descriptions). The material-to-texture step (`TDEP`) is
the engine's; the archive names, the material names and the toy data are Swap Force's [SwapForce].
