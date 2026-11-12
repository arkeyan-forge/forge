# Each game's pictures from the player's disc: the provider and the rule

> **Purpose:** how are each game's logo, background, title icon, toy portraits and toy artworks read from the player's disc and cached? · **Status:** current (2026-10-28) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [SuperChargers]

The game's own logo, background, title icon and toy portraits, read from the player's disc and cached in the game's
data folder, for the in-game Toy Box and for Arkeyan Launcher (game page, sidebar, Home, collection). One flow for every
game; what differs per game is one row of a table. **Nothing is shipped or downloaded**: the pictures exist only on the
player's machine, decoded from their own disc. [Xbox] for the texture decoding, [Lux] / [Alchemy] for the containers,
[SSA] / [Giants] for where each picture is.

## The code (`runtime/host/art/`, shared with Arkeyan Launcher)

| File | What |
|---|---|
| `xenos_texture.{h,cpp}` | [Xbox] Xenos texture bytes to RGBA: DXT1, DXT3, DXT5, A8R8G8B8, 8-bit; tiled (Xenia's `Tiled2D`, BSD) or linear (256-byte rows); 8-in-16 / 8-in-32 byte order; `FlipVertically`. No runtime dependency. |
| `lux_images.{h,cpp}` | [Lux] the `strm` files (zlib) and their APKF texture packages, read by the GPU fetch constant each package carries (was `host/figures/figure_portraits.cpp`); `ForEachPackage` gives every texture package of any stream, a level's too, undecoded (Arkeyan-Builder's HD textures, [hd_textures.md](../../tools/hd_textures.md)). |
| `alchemy_archive.{h,cpp}` | [Alchemy] **one versioned reader**: IGA archives (entries, name table, chunked LZMA; also used by Arkeyan-Builder for Giants' shaders) and IGZ files (object tables, `igImage2`, format hashes), each version a row of a table (below). Formats: [games/giants/docs/art.md](../../../games/giants/docs/art.md). |
| `game_art.{h,cpp}` | The provider: `art::Extract(game, disc, titleIconPng, parts)`, `art::ReadLogo` (the logo alone, uncached: Arkeyan-Builder's icon), the cache folders, and the interpreter of the per-game manifests. |
| `games/<id>/art.json` | **The manifest**: where the game keeps its logo, background and portraits (names only, no game data), built in (`cmake/join_json.cmake` + `embed_file.cmake`; Arkeyan Launcher gets them as resources/games/(id)/art.json). |
| `art_test.cpp` | `ArkeyanArtTest`: every reader on synthetic fixtures; with `SSA_ISO` / `GIANTS_ISO`, the whole extraction of each disc. |

A malformed disc fails cleanly, never ends the launcher: file sizes from the disc's directory are checked (512 MB at most)
before anything is allocated, an IGA entry's offset and size must fit the archive (a compressed entry gives at most 32 KB
per chunk it can hold), `lux::Inflate` stops at 512 MB of output, and `Extract`, `ExtractScenes` and `ReadLogo` catch
what is left (`std::bad_alloc`), log `[art] <game>: ... not read` and return nothing. `ArkeyanArtTest` checks crafted archives,
streams and a disc claiming 1 TB files.

The game's main menu theme comes from the same disc at the same moment, decoded by the kit's Arkeyan-Builder: [game_music.md](game_music.md).

`tools/sync_skylauncher.sh` copies the four pairs to Arkeyan Launcher's `src/shared/art/`, and the manifests, and `host/dir_list.h`
(`ListDirectory(folder, error)`: the entries of one folder, never throwing: a range-for over `directory_iterator(path, error)` checks
the error only when the iterator is built, and `operator++` throws when a later read fails, a folder the system refuses to list or
one that vanishes mid-walk; that crashed Arkeyan Launcher 0.4.1 at start on macOS). Walks of shared code use it, or `increment(error)`.

## The versions of the Alchemy formats

| Format | Version | Games | Status |
|---|---|---|---|
| IGA | 8 | Giants | [V] every archive of its disc: header 0x34, entry count at 0x0C, name table at 0x1C (u32) / size 0x20, entries `{offset, size, mode}`, mode `0xFFFFFFFF` stored, top 4 bits 2 = chunked LZMA |
| IGA | 10 | Swap Force | [V] every archive of its disc: **big-endian** (magic reads `\x1aAGI`), header 0x38, name table at 0x2C / size 0x30, 16-byte entries `{?, offset, size, mode}`, chunks 0x800-aligned, their u16 sizes still little-endian ([its art.md](../../../games/swapforce/docs/art.md#formats-alchemy-swapforce-v)); replaced the little-endian v10 row of public descriptions on 2026-10-27 |
| IGA | 11 | Trap Team | [V] all 1916 archives (.arc and .bld) of its disc open, and all 48132 entries unpack to their size (2026-10-30; each starts `IGZ\x01` or `FSB5`): **little-endian**, header 0x38, table size at 8 (20 bytes per entry, more when it holds the chunk table), name table at 0x28 (u64) / size 0x30, 16-byte entries `{offset, ?, size, mode}`, chunks 0x100-aligned, mode `0xFFFFFFFF` stored or `0x2000000n` chunked LZMA as in v8. The second word is no size or offset we use (multiples of 0x100, or `0xFFFFFF00`) [?]; the mode's low bits, the header words at 0x10 and 0x14 and the table of u16 chunk offsets after the entries (bit 15 set, in 0x100 units) are not used [?]; only LZMA is decoded, which is all the disc has |
| IGA | 11 | SuperChargers | [V] its archives open (2026-11-04: `shaders_xenon.pak` 105 entries, the UI and collection archives): **big-endian** (magic `\x1aAGI`), header 0x38, the name table's 64-bit position **high word first** at 0x28, size at 0x30, Swap Force's 16-byte entries `{?, offset, size, mode}`, chunks 0x800-aligned; entry names are build paths with `/` (`IgaArchive::Find` takes either separator) |
| IGA | 11 | Imaginators | [V] 4959 of its 4995 `.pak` archives open (2026-11-05), and 130157 of 130158 IGZ entries unpack: SuperChargers' layout, but most entries (the level textures among them) have the mode's top 4 bits **1 = raw deflate**: chunks `[u16 n][n bytes of deflate]` giving 0x8000 bytes each, the next chunk at a multiple of 16 after the last, though sometimes one or two 16-byte steps later; the reader takes the first of those whose deflate gives the next chunk's size exactly (the rule behind the extra steps is not known [?]). `lux::InflateRaw` decodes them |
| IGZ | 6 | Giants | [V] sections `{offset, size, alignment, id}`, section 0 with its own header, numbered fixups (0 types, 2 external ids, 5 objects, 0xA handles), pointer pool in the top 8 bits |
| IGZ | 7 | Swap Force | [V] every picture ArkeyanArtTest reads from its disc (2026-10-27): sections at **0x14** `{?, offset, size, alignment}`, tagged fixups `{tag, count, length, data start}` with the tags **stored byte-swapped** (`TMET` reads `TEMT`), pool in the top 5 bits, memory handles in `TMHN` `{kind<<24 \| size, pool<<27 \| offset}`, `igImage2` as v6 (width/height +0x30, format +0x3C, handle +0x48) but its name (+0x08) an index in the strings (`TSTR`); a material names its texture in `TDEP` ([its art.md](../../../games/swapforce/docs/art.md#formats-alchemy-swapforce-v)) |
| IGZ | 8 | Trap Team | [V] its pictures and shaders (2026-10-30): the layout of v7 (sections at 0x14, tags stored byte-swapped, `TMHN` handles, `igImage2` as v6) with one difference: **every name in the types and strings records is padded to 2 bytes**; a file may have no `TSTR` at all: a picture's name is then empty and the archive entry's source path names it (`level.bld` has none) (a reader that steps `length + 1` reads an empty name between two, and every class index is off by one). All 61 streamed trap pictures, the 288 toy portraits and the 92 villain pictures decode ([its art.md](../../../games/trapteam/docs/art.md)); the objects of the big `level.bld` entries (7374 in the collection screen) decode with the right class names, their pictures carry no name [?] |
| IGZ | 9 | SuperChargers | [V] its shaders and pictures (2026-11-04): Trap Team's v8 layout (sections at 0x14, tags stored byte-swapped, `TMHN`, types padded to 2 bytes) with three changes: a packed object distance is the distance itself (v6-v8: minus 4), igImage2's size, format and handle are 4 bytes later (+0x34, +0x40, +0x4C), the dependencies' strings are not padded. The public descriptions' v9 (tags not swapped, sections at 0x10, `RHND`) is not what this disc holds |

`IgaVersions()` / `IgzVersions()` list them with `verified`; `Version()` / `Verified()` tell which one a file is; a
version not in the tables is refused, not guessed. Each version reads a synthetic fixture built from its row in
`ArkeyanArtTest` (that checks the reader follows the table, not that the table matches a real disc). To confirm a [?] row:
open one archive of that game's disc and compare.

## The manifests

```json
{ "format": 1, "game": "giants", "engine": "alchemy",
  "logo": { "archive": "level/Title.bld", "entry": "level.bld", "picture": "giantslogo/rig/textures/spritelogo.png",
            "compose": "sprite_sheet_logo", "plate": "giantslogo/rig/textures/title_giants_3d.png" },
  "background": { "archive": "permanent/global.arc", "entry": "ui_levelfullscreen/mainmenu.png" },
  "portraits": { "archive": "misc/UI_Collection_Champions.bld", "entry": "level.bld", "folder": "/collections/sky2/portraits/" },
  "artworks": { "archive": "misc/PvP_MainControl.arc", "folder": "/ui_pvp_sprites/" } }
```
Swap Force's ([games/swapforce/art.json](../../../games/swapforce/art.json)) reads its portraits and artworks through
material instances instead (`"materials": [{archive, folder, ending}]`, below).
- A picture is `{stream, picture}` [Lux] (a `strm` file of UI images, the picture's name) or `{archive, entry, picture}`
  [Alchemy: the IGA file, a fragment of the IGZ entry's name, the end of the picture's source path; no `picture`: the
  entry's first]. `compose` names a composition step coded once ("sprite_sheet_logo": Giants' logo sheet, below);
  `plate` is the second picture it uses.
- Portraits: [Lux] `stream`, `pictures` (patterns, `{name}` = the catalogue's internal `names`, lower case) and
  `images: true` (the catalogue's `images`); [Alchemy] every square picture of the entry under `folder`, named as on
  the disc, done when every catalogue `portraits` file is there; or [Alchemy] (IGZ v7) `materials`: a list of
  `{archive, folder, ending}`, every entry of `archive` whose name starts with `folder` (and ends with `ending`) is a
  material instance that names its texture (`TDEP`: `textures:/<name>.igz`, the entry `textures\<name>.igz` of the
  same archive); the texture's picture is written as `<material>.png` (Swap Force: `collection_zook_normal.png`, the
  name its toy data gives).
- [Alchemy] `streams` (portraits): a list of `{archive, folder}`: every entry of `archive` whose name has `folder` then `.png` is one picture
  (its own IGZ), written under the last part of that path (`<portraits>/216_12291.png`). Trap Team's: the collection screen's toys
  (`<type>_<variant>_<name>`, year folders), its traps and the villain vault's two pictures per villain.
- Upgrades (not pictures; the same manifest, read by `host/art/game_text.cpp` into `<data>/<Game>/upgrades.json`):
  [Lux] `{stream, text, languages}` (`global.str`'s records, `text/{language}/global.lxb`); [Alchemy] `{costs: {archive,
  set}, languages, archives: {"<toy type>": "character/<model>.bld"}}`. [collection.md](collection.md#upgrades).
- Artworks [Alchemy]: every entry of `archive` under `folder` is one picture (a streamed texture: its own IGZ),
  written as `<portraits>/artworks/<name>.png`, `<name>` from the entry's path (`ui_pvp_sprites/eruptorseries2.png/0x....igz`);
  or `materials` as for the portraits (Swap Force's face-off portraits and the swappers' halves); done when every
  catalogue `artworks` and `halves` file is there (so a cache made before the halves is completed at the next disc check).
- Dashboard art (2026-11-05): every disc has at its root `nxeart`, an STFS package (`art/stfs.h`) holding the dashboard's
  1920x1080 backdrop `nxebg.jpg` and its tile `nxeslot.jpg`. A picture `{package, file}` reads one of them (JPEG or PNG, stb);
  `scenes` groups take `{package, names}` too. SuperChargers' clouds and Imaginators' night sky come from there (both discs) [V].
- More compositions: `light_on_black` (a logo drawn opaque on black made transparent, `art::LightOnBlack`) and `feather`
  (a picture fading out from an ellipse to its edges, `art::Feather`: Imaginators' dashboard tile over its own sky).
- `logo_download: {url, credit}` (Imaginators): a cleaner logo the launcher downloads once the disc is verified, kept as
  `logo-download.png` beside the cache and preferred to the disc's on the game page; the runtime and Arkeyan-Builder (the game
  icon) keep the disc's. Credited in the launcher's README and About page.
- [V] the manifests give the same 210 files as the per-game code they replaced, byte for byte (both discs, 2026-10-26). The disc is read through
`art::Disc` (two callbacks: a file's size, a range of it): the runtime's and Arkeyan Launcher's `DiscFileSystem` both fit
(`art::ReadFrom(disc)`), so the files need neither's headers.

## What each game gives

| Game | Logo | Background | Portraits (`<data>/<Game>/portraits/`) |
|---|---|---|---|
| SSA [Lux] | `uiimage.str`: `skylanderslogo` (1024x512, DXT5) | `uiimage.str`: `fullscreen_000` (512x512, the painted Skylands sky) | `uigameimage.str`: `toy_<name>_hud.png` (128 px), `<name>_vs.png` (512 px) of the catalogue's `names`, its `images` (items, 64 px) and `kind_pictures` (3 family icons, 128 px): 81 files |
| Giants [Alchemy] | composed from `level/Title.bld`'s sprite sheet: arch, word, 8 medallions, TM, on a rebuilt dark blue plate (1033x541) | `permanent/global.arc`: `ui_levelfullscreen/mainmenu.png` (1600x960) | `misc/UI_Collection_Champions.bld`: `<name>.png` (256 px, the collection screen's): 123 files; artworks: `misc/PvP_MainControl.arc`: `artworks/<name>.png` (the versus screen's, about 1000x960, not square): 99 files |
| Swap Force [Alchemy] | `archives/defaultloadscreen.pak`: `juicestandard_title_00` (872x400, 32-bit lossless: the full title logo, `ui\Loading\Title.tga`; the DXT5 copy `_10` and every other logo on the disc are the same size) | `archives/juicedomain_frontend.pak`: `juicestandard_mainmenu_background_10` (1280x720, the main menu's sky) | `archives/toycollectionimages.pak`: the collection icons `collection_<toy>_<deco>.png` (192 px) and the UFO Hat's icon (`collectibleicons.pak`, 160 px): 209 files; artworks: `archives/faceoffportraitsimages.pak`: `artworks/<toy>[<deco>]_fullscreen_materials.png` (the face-off portraits, 640x320, not square): 179 files; plus the swappers' halves (`characterillustrations.pak`: `artworks/<toy>{body,top,legs}_wii_material.png`, 256, 512 or 1024 px frames, on transparency: 51 files) |

| Trap Team [Alchemy] | `permanent/global.arc`: `ui_sky4/titlelogo/titlelogo_001.png` (683x341, DXT5, on transparency: the title logo) | the same archive: `ui_sky4/screens/loadingscreen/textures/levelload_313.png` (1280x720, the title's loading picture) | `misc/UI_Collection_Champions.arc` (288 toys of the four games, 128 px, `<type>_<variant>_<name>.png`: the variant id a figure carries), `misc/ui_collection_traps.arc` (60 traps, `<type>_<variant>.png`), `misc/ui_villainvault_stream.arc` (46 villains, `<id>_villaincaptured.png` and `<id>_villainescaped.png`, 171 px): 441 files (345 toys and traps, 92 villains, the trap screen's two backgrounds), no artworks |

Each: `title_icon.png` is the dashboard icon of `default.xex` (its XDBF resource), written when the caller passes it
(Arkeyan Launcher does; the game doesn't need it). Details and formats: [figures.md](figures.md) (SSA's UI image records),
[games/giants/docs/art.md](../../../games/giants/docs/art.md) (Giants), [games/swapforce/docs/art.md](../../../games/swapforce/docs/art.md) (Swap Force).

## The cache

```
<data>/<Game>/launcher-cache/      logo.png, background.png, title_icon.png, manifest.ini (version=, what was found)
<data>/<Game>/portraits/           the toys' pictures, named as on the disc
<data>/<Game>/portraits/artworks/  the large ones (Giants), named as on the disc
```
- `<data>` is the shared data folder (`datadir.h`, `SKYLANDERS_DATA` for tests); `SKY_PORTRAITS` overrides the
  portraits folder (tests).
- **Kept:** a second `Extract` reads nothing when everything is there (0.00 s). `manifest.ini`'s `version` is
  `CacheVersion` in `game_art.cpp`: bump it when a picture is read or composed differently, and the logo and background
  are made again. Portraits are only ever added (a missing one is decoded again).
- Files are written as `<name>.part` then renamed: an interrupted run leaves no truncated PNG.
- Before 0.4 Arkeyan Launcher kept its cache in `<data>/SSA/launcher-cache/<title id>/` for every game: those folders are
  removed on the next extraction.
- Deleting either folder is always safe.

Who extracts: Arkeyan Launcher after a disc is verified (everything, on the check's worker thread: SSA 1.2 s, Giants 1.8-2.1 s
on an M1 Pro); the game when its Toy Box first opens (portraits only, the same folder: if the launcher already did, it
finds them all). The swappers' halves (artworks) are only read by the launcher: a game started on its own from a disc, with no
launcher check before, shows a swapper as its top's round portrait with the bottom in a corner, until the launcher has read the disc
(`tools/regress/swapper_toybox.sh` takes `SWAPPER_ART_DATA` for that). `CacheVersion` 4 (2026-10-27): Swap Force's logo comes from
its 32-bit picture.

## The rule for a toy's picture

`collection::PortraitPath(gameId, toy, variant, large)` (`host/collection/icons.h`), and `collection::Portrait(...)` for
the picture itself (resized, with the variant's badge):

1. **the variant's own picture**, searching games in reverse catalogue order (newest last in the catalogue);
2. **else the plain toy's picture**, in the same newest-first order;
3. **else the toy family's picture**, again newest first (`collection::GameKindPicturePath`, catalogue
   `games[].kind_pictures`): SSA's potion for magic items, compass for adventure packs and paw for minis;
4. **else the generated icon**, through `collection::GeneratedIcon` or Arkeyan Launcher's `figures::GenerateIcon`.

Only existing files in the player's caches count. The viewed `gameId` does not affect selection; it remains in the
API for caller compatibility. A variant's older dedicated picture wins over newer plain art: its appearance identifies
that version. When no dedicated picture exists, it uses the newest plain picture. Known figure records pass their
actual variant id through the UI and its image caches; the kind alone is used only when the id is unavailable.

**The best picture** (`collection::BestPicturePath`) uses the same selector as `PortraitPath`: within each game it
prefers artwork to a portrait. `PortraitPath` prefers a portrait (SSA's larger versus portrait when `large`), with artwork
as a fallback. Game recency takes precedence over size: an older plain artwork never beats a newer plain portrait.
Both icon sizes therefore choose the same game and version. This changes lookup only: existing PNG names, extraction
and `CacheVersion` stay unchanged; no re-extraction is needed. Missing art can be added by the normal disc check.

### Picture lookup audit

| Surface / API | Selection and fallback |
|---|---|
| Launcher Collection cards, seats, half chooser, bottom inset and Add a figure | `CollectionPage::Icon` / `DrawIcon` → `PortraitPath`; PNG load failure → `figures::GenerateIcon` |
| Launcher game-page portal panel, figure sheets and version rows | `DrawPicture` → `BestPicturePath`; fallback → `DrawIcon` |
| Toy Box cards, seats, wheel, management and half chooser | `FigurePortrait` / `Portrait` → `IconOf` → `collection::Portrait` → `PortraitPath`; fallback → `GeneratedIcon` |
| Toy Box figure sheets and version rows | `ArtworkPicture` → `BestPicturePath`; portrait fallback uses the same selector and exact variant |
| Library icons at every size | `Icon`, `Portrait`, `DiscImagePath` → `PortraitPath` |
| Swapper composites in both apps | `SwapperPicture` → the shared selector restricted to genuine half files → `JoinHalves`; incomplete pairs use the two individual icons |
| Individual swapper halves | `HalfPicturePath`, `PortraitPath`, `BestPicturePath`: own half art, then whole-figure art, then generated icon; a bottom can use its matching top's whole picture |
| Low-level per-game reads | `GamePortraitPath` and `GameKindPicturePath` intentionally inspect only the requested cache; they do not choose art across games |
| Villains | `VillainPicturePath` keeps Trap Team's captured/escaped state pictures; these are not interchangeable toy portraits |
| Traps, magic items and other families | The ordinary shared selector preserves their own variant pictures, then plain art, family glyph and generated fallback |

Previously the launcher and Toy Box already agreed when they called the same API. However, portraits used the viewed
game first, then oldest first; large pictures used a separate search; halves searched oldest first and could choose a
plain half before another game's variant. All cross-game toy selection now goes through `PicturePath` in `icons.cpp`.
No toy family needs an exception to the fallback rule. Synthetic caches in `ArkeyanCollectionTest` and the launcher's
no-disc tests cover recency, variants, sizes, missing art and incomplete swappers.

**A swapper's picture** (Swap Force: a top and a bottom, two toys): the swap screen's, its two halves joined at the waist, the same in
Arkeyan Launcher and the Toy Box. The catalogue's `halves` (keys as `portraits`: `"2004"`, `"2004:0x2402"`) name each half's
picture in `<portraits>/artworks/` (the variant's own before the plain half, newest cache first; a top is `<toy>body`,
`<toy>`, or `<deco><toy>top`, a bottom `<toy>legs`: 50 entries, all 16 swappers and their 9 other versions).
Each half is drawn in place on one shared square frame (256, 512 or 1024 px, depending on the toy), so laying a top's
frame over a bottom's frame gives the whole figure, waist joined, same scale: [V] for all 16 swappers, their 9 other
versions and mixed pairs. `collection::SwapperPicture(top, topVariant, bottom, bottomVariant)` loads both and
`JoinHalves` brings both frames to the larger one's size (`Resize`), composites the top over the bottom (straight alpha)
and cuts the result to its visible box (alpha above 8); a half is never cropped or rescaled on its own. Empty when a half
has no genuine half picture or no visible pixel, and the caller then draws the top's picture with the bottom small in a corner.
Whole-figure fallbacks are never joined at the waist. A dedicated variant picture also wins over a composite made
from plain halves: the caller uses the individual variant pictures instead. An individual half uses `HalfPicturePath`: its own half picture,
then the newest whole-figure artwork or portrait (a bottom may use its matching top's entry, needed because Trap Team
lists tops only), then the generated icon. One missing half never suppresses the other's icon. Pure
image logic, tested on synthetic images (`ArkeyanCollectionTest`, `TestSwapperPicture`). Any top and any bottom work,
matched or mixed. (Until 2026-10-28 the halves were cropped and stacked at the wider one's width: a gap at the waist and
two scales.)

When only a variant's kind is known (the Toy Box's cards, the collection's), `collection::VariantFor(toy, kind)` picks
the toy's one variant of that kind. `large` asks for the game's biggest picture (SSA's 512 px "versus" art; Giants has
one size). The catalogue says which picture is which toy: SSA's `names` and `images`, Giants' `portraits`
(`"<type>"` or `"<type>:<variant id>"` to a file name; [collection.md](collection.md)).

Where it is used: the in-game Toy Box (`toybox/draw.cpp`, the game being played first), Arkeyan Launcher's collection (the
game shown first). A later design pass can build on `PortraitPath` / `Portrait` without knowing any game.

## The app icon
**Every game's icon is its own disc title logo**, made on the player's machine, never from git.
SSA, Giants, Swap Force and Trap Team use the same crop, transparent background and proportional margin.
Arkeyan-Builder renders each size from the original disc art, using `FitLogoIcon` in `tools/arkeyan-builder/game_icon.cpp`.
The recipe and all generated formats are documented in [Arkeyan-Builder](../../tools/arkeyan-builder.md#running-arkeyan-builder).

- `game_icon.png` (512 px) is used for the Windows and Linux window icon and Linux desktop shortcuts.
- `game_icon.ico` holds seven sizes, 16 through 256 px, embedded by `ExecutableResources` using windres.
  Windows desktop shortcuts reuse these bytes.
- `game_icon.icns` holds 16 through 1024 px plus Retina entries, copied into the macOS bundle before signing.
  macOS desktop shortcuts reuse these bytes. No platform image conversion tools are required.
- With no readable logo, the disc's title icon is fitted by the same rule. With neither, the build logs why
  platform defaults apply; a stale family from a different disc is removed.
- `Arkeyan-Builder --iso <disc> --kit <kit> --out <private folder> --icon-only` makes only the icon family and stops.
  The disc and recipe are cached in `arkeyan-work/icon/game_icon.key`.

`ArkeyanBuilderTest` checks generated PNGs, every ICO/ICNS entry, centring, margins and transparency using
synthetic art only. Actual disc logo extraction, Explorer/taskbar and Dock caches, X11 and Wayland desktop
integration still need a manual run on each system. Wayland compositor icon matching also depends on the
runtime's application id and desktop registration; a loose desktop entry alone may not supply the taskbar icon.

## Adding a game
A manifest `games/<id>/art.json` (where the pictures are), the toys' picture names in the catalogue (`portraits`, or
`names`/`images`), a `TestDisc` line in `art_test.cpp`; then `tools/sync_skylauncher.sh`. No code, unless its archives
are of a version the tables mark [?] and turn out different (fix the row), or its logo needs a new composition step.

## Verify
- `ArkeyanArtTest` (in `tools/test_all.sh`, both builds): 128 checks without a disc (every format version's fixture, IGZ
  v7's names, dependencies and linear DXT5, the manifests); with `SSA_ISO` 144, `GIANTS_ISO` 158, `SWAPFORCE_ISO` 138, all three 184;
  with the discs, logos, backgrounds, title icons, 78, 123 and 209 portraits, Giants' 99 and Swap Force's 230 artworks (179 face-off portraits and 51 swapper halves), the joined pictures of all 16 swappers, every toy the catalogue names has its picture, every portrait and artwork it
  maps is on the disc, and the best picture of Series 2 Eruptor is its artwork (1475x960), of Series 2 Zook its
  face-off portrait (640x320); every toy a game reads has a picture from that disc alone (its own or its family's).
- Counts (2026-10-26): SSA, 78 files, no variant has a picture of its own (Dark Spyro and the Legendaries are toy types);
  Giants, 123 portraits (57 variants with their own) and 99 artworks (56 variants with their own); 133 portrait and 109
  artwork mappings in the catalogue (a picture can serve two ids: Giants shows its Series 2 picture for a later
  repose); Swap Force, 209 portraits (100 variants with their own) and 179 artworks (99 variants with their own), 233
  portrait and 200 artwork mappings, 4 s the first time ([its art.md](../../../games/swapforce/docs/art.md)). The first Giants extraction takes about 10 s (the artworks: 8 s, 60 MB); later ones read nothing. The
  artworks are their own part (`art::Artworks`, in `art::All`): Arkeyan Launcher reads them while it checks the disc (its
  worker thread); the in-game Toy Box asks for `art::Portraits` only, so its first opening does not wait for them.
- Arkeyan Launcher's `ArkeyanLauncherTest` with `SKY_LAUNCHER_TEST_GIANTS_ISO`: the same through the launcher's provider, and the
  rule (Eruptor on each game, LightCore Eruptor, Jet-Vac on SSA).
- A game run: `[toybox] 123 portraits in <data>/Giants/portraits` (81 for SSA) when the Toy Box first opens.
