# Collection library: API for the launcher's UI

> **Purpose:** what does the collection library offer a UI (the launcher, the Toy Box), function by function? · **Status:** current (2026-10-26) · **Tags:** [Xbox]

The engine behind "My Skylanders": everything a UI needs to show and manage the player's figures, with no UI in it.
Formats, folders and rules: [collection.md](collection.md). The headers are the reference; this page is the map.

## Files

| Path (ArkeyanRuntime) | What |
|---|---|
| `runtime/host/figures/figure_data.*` | `FigureData` (the 1 KB tag), checksum, blank figure |
| `runtime/host/figures/figure_crypto.*`, `figure_tag.*` | decrypt/encrypt, `FigureStats`, levels, nicknames |
| `runtime/host/figures/figure_catalog.*` | SSA's own toy table (`figures::Catalog()`), now built from the catalogue |
| `runtime/host/collection/json.*` | small JSON reader/writer |
| `runtime/host/collection/catalogue.*` | every toy of the six games, categories, variants, which game takes what |
| `runtime/host/collection/collection.*` | the folder: list, read, create, write stats, rename, metadata, locks, backups, migration |
| `runtime/host/collection/icons.*` | a picture for any toy (disc picture or drawn), variant badges |
| `resources/collection/catalogue.json` | the data, built in with `cmake/embed_file.cmake` (symbol `g_embeddedCatalogue`) |

Dependencies: C++20, SDL 3 (files, PNG); `datadir` needs only the C++ library. No other library. Everything is in namespace
`collection` (the tag code in `figures`). Includes inside these folders are relative, so the files can be copied
as they are.

## Using it from Arkeyan Launcher

Arkeyan Launcher's `src/figures/figure_catalog.*` is a copy of ours (only the include path differed). To use the library:

1. Copy the files: `tools/sync_skylauncher.sh <skylauncher checkout>` copies `host/figures/` (without the portrait
   decoder and the test, which need the game's disc code) to `src/figures/`, `host/collection/` (without the test) to
   `src/collection/`, the catalogue to `resources/collection/` and `cmake/embed_file.cmake`; `--check` only lists the
   differences. Edit the files here, in ArkeyanRuntime, and copy them over: this repo has the tests.
2. In Arkeyan Launcher's `CMakeLists.txt`, add the `.cpp` files to `SkylauncherCore`, plus the generated catalogue:

   ```cmake
   add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/embedded_catalogue.cpp
       COMMAND ${CMAKE_COMMAND} -DNAME=g_embeddedCatalogue -DINPUT=${CMAKE_SOURCE_DIR}/resources/collection/catalogue.json
           -DOUTPUT=${CMAKE_BINARY_DIR}/embedded_catalogue.cpp -P ${CMAKE_SOURCE_DIR}/cmake/embed_file.cmake
       DEPENDS ${CMAKE_SOURCE_DIR}/cmake/embed_file.cmake ${CMAKE_SOURCE_DIR}/resources/collection/catalogue.json)
   ```
3. Replace `PreferencePath("figures")` (in `App::LoadFigures` and the folder buttons) with `collection::Folder()`, and
   call `collection::Migrate(collection::LegacyFolder())` once at start, as the game does: the launcher then shows
   the same figures as the game, migrated or not.
4. Pass nothing new to the game: it finds the same folder. `[portal] place` / `SKY_PORTAL_PLACE` take paths or names
   relative to the collection.

## Calls

Folder and catalogue:

```cpp
std::string collection::Folder();                    // where the collection is (SKY_COLLECTION overrides)
const Catalogue& cat = collection::Catalogue::Get(); // built-in + the folder's catalogue.json
cat.Toys(); cat.Games(); cat.Find(type); cat.FindGame("ssa");
cat.Accepts("ssa", type);                            // can this game read this toy?
cat.Accepted("giants");                              // the toys a game reads (for "what works with...")
cat.Describe(toy, variant);                          // {name "Series 2 Gill Grunt", kind, known}
CategoryName(c), VariantKindName(k), ElementId(e), ElementRgb(e), VariantKindRgb(k)  // labels and colours
```

Listing (one `Figure` per dump: file, toy, variant and its kind, metadata, stats, level, lock):

```cpp
collection::Filter filter;
filter.game = "ssa";                         // only what this game reads; empty = all
filter.category = int(Category::MagicItem);  // or -1
filter.element = int(Element::Water);        // or -1
filter.kind = int(VariantKind::Legendary);   // variant group, or -1
filter.favouritesOnly = true;
filter.text = "gill";                        // name, label, nickname, file
for (const collection::Figure& f : collection::List(filter))
{
    f.DisplayName();          // label, else edition, else variant name
    f.toy->category; f.toy->element; f.Kind();
    f.level; f.stats.experience; f.stats.gold; f.stats.playSeconds; f.stats.heroPoints; f.stats.hat;
    figures::NicknameUtf8(f.stats);
    f.state;                  // Blank (never played), Valid, Damaged
    f.lock.Busy();            // on a game's portal right now: show it, do not write it
    f.favourite; f.lastGame; f.lastUsed;
}
```

Group the list by `Category` for the tabs (Skylanders, Giants, Swap Force, Trap Masters, minis, traps, vehicles,
SuperChargers, Senseis, magic items, adventure packs, trophies...) and by `Kind()` for the variant groups
(Legendary, Dark, Crystal Clear, Series...). `List()` already sorts by category, element, toy type and variant.

Changing things (every call returns false, with `error` filled when given, if refused):

```cpp
std::string path = collection::Create(type, variant);          // new blank figure; returns its path
collection::WriteStats(path, stats, &error);                   // refused while another program uses it
collection::SetNickname(path, "Ace", &error);
collection::Rename(path, "My Gill Grunt.sky", &error);         // file, metadata and backups follow
collection::SetLabel(path, "Ace's"); collection::SetFavourite(path, true);
collection::SetEdition(path, "Crystal Clear Wham-Shell", VariantKind::Crystal);  // same-tag editions: toy->variants
std::string backup = collection::Backup(path);                 // backups/<file>/<UTC time>.sky
collection::Backups(path);                                     // newest first
collection::Restore(path, backup, &error);                     // the current dump is backed up first
collection::GetLock(path); collection::BreakLock(path);        // "it is not in use any more" button
```

For the Toy Box and the Collection page ([toybox_ux.md](toybox_ux.md) section 9):

```cpp
cat.FindGame("ssa")->seats;                          // [{ id "p1", player 1, accepts {Core, Giant...} }, item, adventure]
collection::UntouchedStarter(f); collection::Tidy(); collection::TidyDone();   // never-played dumps the game made
collection::Recent("ssa", 8);                        // paths, newest first (Touch keeps per-game times and counts)
collection::Delete(path); collection::Trash(); collection::Undelete(id); collection::PurgeTrash(30);
collection::Duplicate(path); collection::ResetToNew(path); collection::PruneBackups(path, 10);
collection::Import({ folder }); collection::ImportTag(tag); collection::Export(paths, folder);
collection::GetView("ssa", "launcher"); collection::SetView(...);    // group, order, show, fold foreign (unused by the screens since 2026-10-26)
collection::GetWheel("ssa", "p1"); collection::SetWheel(...);       // 8 places: file, pinned
collection::OnWheel(game, file); collection::SetOnWheel(game, file, on);  // the player's own wheel, every player seat that reads it
collection::GetWheelSkipped(game, seat);                                    // taken off: the recents do not bring it back
Catalogue::Get().Read(game, toy, variant); VersionsFor(game, toy); KindIn / NameIn  // what a game makes of a figure (collection.md)
collection::Group(list, GroupBy::Smart, OrderBy::Recent, "ssa");    // the shelf: one section, each figure once, then Unknown, Foreign
collection::Shelf(list, Sort::Name, "ssa");                          // Group's shelf in the player's sort: Recent (default), Name, Level, Element, Game
collection::SearchMatches("niveau 10 feu", card, "ssa");             // the smart search (search.h): names, elements, games, types, versions, levels, typos
collection::LevelText(f, "ssa");                     // the card's level line: "Level 6", "Level 10 (15)", "New", "" (items)
collection::StandardVariant(toy); collection::VersionCount(toy);   // the "base" entry's id (0: none) and the other versions: "Standard" is made under that id
collection::HasProgress(f);                          // characters (core to minis) keep a level and gold
```

Swappers (Swap Force: a top and a bottom, two dumps linked by `partner`; one card; [collection.md](collection.md), "Swappers"):

```cpp
f.partner;                                           // file name of the linked other half, empty if none (a lone half is a card)
f.IsSwapTop(); f.IsSwapBottom();
std::vector<collection::Card> cards = collection::Cards(list);   // what a shelf shows: a linked pair once
card.first;  card.second;  card.Swapper();           // first: the figure or the swapper's TOP (name, label, star, picture); second: the bottom or null
card.Halves(); card.Busy();                          // both dumps (to place, delete, export); busy if either is in use
collection::CardOf(list, index);                     // the card of list[index], from either half
collection::CardName(card); collection::CardLevelText(card, "swapforce", french);   // "Wash Buckler" / "Boom Jet / Wash Buckler", "Level 6"
collection::Group(list, ...);                        // a linked pair once: the sections hold the TOP's index only
collection::CreateSwapper(2003, 1003);               // {top, bottom} paths: both dumps, linked; MatchingHalf(2003) == 1003
collection::Pair(top, bottom, &error); collection::Unpair(path, &error);
collection::ChangeHalf(keep, replacement, &error);   // "Change the bottom": the card of `keep` takes `replacement`; a swapper that lost it takes the old half
collection::Duplicate(half);                         // both halves, copies linked; Rename follows the link; Delete(half) one at a time
```

Arkeyan Launcher's rules for a swapper: list with `List()`, build the shelf with `Cards()` or `Group()`, act on **both halves**
of a card (`Card::Halves()`) for Delete, Export, Put on the portal and the lock test (`Card::Busy()`), act on the
**top** (`Card::first`) for label, favourite, nickname and level; "Change the bottom" lists the dumps of the other
category (`Figure::IsSwapBottom()`) and calls `ChangeHalf(top, chosen)`; a lone half offers the same to find a partner.
Its **portal at the next start** (`portal.txt`, `<slot> <path>` lines) puts a swapper's top and bottom in two slots: for
Swap Force **Player 1 uses slots 0 and 1, Player 2 slots 2 and 3, the magic item 4, the adventure pack 5** (one slot per
seat for a figure that is not a swapper: the first slot of its seat's pair); the game pairs them by the link at start
([toybox_ux.md](toybox_ux.md), "Swappers").

`WriteStats`, `Restore` and `ResetToNew` back the dump up first and keep the newest 10 backups. `Create` marks the
figure as the player's (`origin: "created"`); `Import` marks `"imported"`, `ImportTag` `"portal"`.

Edits of stats follow `figures::FigureStats` (`docs/runtime/host/figures.md`: which fields are verified); levels come from
`figures::LevelForExperience` and `LevelExperience`, gold is capped at `figures::MaxGold`.

Icons:

```cpp
bool fromDisc;
collection::Image icon = collection::Icon(f.toy, f.Kind(), 128, &fromDisc); // RGBA, R in the low byte
// A swapper's picture, its halves joined at the waist as the game's swap screen (empty when a half has none from a disc: draw the top's picture and the bottom small):
collection::Image swapper = collection::SwapperPicture(*top.toy, top.variant, *bottom.toy, bottom.variant);
// HalfPicturePath(toy, variant): a half's file, else whole-figure art; never pass a whole-figure fallback to JoinHalves.
// CropToVisible(image): the alpha bounding box; JoinHalves(top, bottom): genuine half frames only.
// fromDisc: the player's disc picture (cached by the game); else drawn. Show "drawn" icons over the element colour,
// or ask GeneratedIcon(toy, size, true) for an opaque tile.
collection::SavePng(icon, path);
collection::PortraitPath("ssa", *toy, variant);      // newest cached variant, else plain art, else family; "" = draw it
collection::GameKindPicturePath("ssa", Category::MagicItem); // SSA's potion icon, when cached
collection::KindGlyph(Category::Adventure, x, y);    // the drawn mark (potion, compass, paw), a signed distance in [-1, 1]
```

The disc pictures exist once the game's Toy Box opened with the disc, or Arkeyan Launcher checked it (both cache them in
`<data>/<Game>/portraits`, or `SKY_PORTRAITS`: `art::Extract`, [game_art.md](game_art.md)). `collection::PortraitPath(game,
toy, variant, large)` and `BestPicturePath` share one newest-first selector: reverse catalogue order, independent of the
viewed game, variant art before plain art. A half first uses its own half image, then whole-figure art (including its
matching top for a bottom), then a generated icon. `SwapperPicture` still requires two genuine half images. Existing
caches need no re-extraction or version change.

## Threads and processes

The calls are synchronous file operations; run `List()` off the UI thread for big folders (it reads every dump:
1 KB each). The game and the launcher can run at the same time: dumps on a portal are locked, `collection.json` is
re-read before each change and replaced atomically. Two writers changing metadata at the very same moment can lose
one change (the last write wins).
