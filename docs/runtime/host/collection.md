# The Skylanders collection: one folder for every game

> **Purpose:** how is the shared figure collection stored (one folder for every game: format, locks, backups, migration), and where do the catalogue's toys and variants come from (the games' own toy tables)? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce]

[V] = verified (test, the title's own code or a run), [?] = unverified.

The player's figures belong to no game. Every game (Spyro's Adventure today; Giants, Swap Force and the others later)
and the launcher use **one collection folder**. A game reads a figure's dump from it and writes the progress back
into the same dump, as a real portal writes the toy, so a figure keeps its level, gold and nickname from one game to
the next. Code: `runtime/host/collection/` (with `host/figures/`), API for the launcher: `docs/runtime/host/collection_api.md`.

## The design across games (2026-10-26)

Five rules, so that a new game is data (catalogue rows, an art manifest), not new code:
1. **One record = one figure** (one dump): what it is comes from its tag (toy type, variant: block 1), whichever game
   made it; the games never rewrite that. A LightCore, a Series 2, a Legendary are variants of their toy, and a
   LightCore is an ordinary figure that glows (the glow is the portal's light: nothing else to emulate). [V]
2. **The data rules** ([figures.md](figures.md#data-rules-one-figure-every-game-lux-alchemy)): each game owns its areas
   of the tag (Spyro's Adventure the first pair, Giants adds the extra pair); a writer keeps every byte it does not
   know, the newer copy wins by its sequence number, and each game shows the figure within its own limits (SSA: level
   10) without changing what a later game stored. [V] (round trips and real runs)
3. **The catalogue says who accepts what** (`accepts` or `accepts_games` per game, below), and names each game's hats
   (`hats`) and pictures (`names`, `images`, `portraits`, `artworks`). Its SSA, Giants and Swap Force toys and variants
   follow the games' own tables ([below](#the-games-own-toy-tables)).
4. **The art** comes from the player's discs through one versioned reader per engine and a manifest per game
   (`games/<id>/art.json`, [game_art.md](game_art.md)); a toy's picture falls back to another owned game's.
5. **Co-op** (below): a Toy Box per player, each with its own seat(s), emulated and real figures side by side.

What changed with it: the library keeps SSA's hat (`ssaHat`, first area) apart from Giants' (`hat`, extra area) and
keeps Swap Force's experience (101000 = level 20, the game's own table: [figures.md](figures.md)); edits follow the game's cap (`SetExperienceFor`); the art readers are tables of versions
and per-game manifests; Giants' hat picker; the four Giants sidekicks. What was already right: one dump per figure,
the per-game `accepts`, the extra areas written only when needed, variants as figures (LightCore creation was already
in both Add a figure screens).

### Co-op: a Toy Box per player (design; the code has one player today)

The owner: "each player has their own Toy Box and their physical figures".
- **Seats belong to players**: the catalogue's `seats` already carry `player` (1, 2; 0 for shared seats such as the
  magic item). Each player's Toy Box shows and changes only that player's seats; shared seats show in both.
- **Who opens which**: the pad that opens the Toy Box (View + Y) picks the player (XAM user index); keyboard and mouse
  are Player 1. Two Toy Boxes may be open at once, each in its half of the screen (or one after the other on a small
  screen); the wheel is per player too.
- **Emulated and real figures side by side**: a seat holds either a collection dump (emulated) or what a real portal
  reads (read-only, "Save a copy to My Skylanders"); the emulated portal's 16 slots are shared, so Player 2's emulated
  figure can sit next to Player 1's real one once the real and emulated portals are merged (a mixed portal: not built,
  `toybox_ux.md` section 4).
- **The collection is shared**: both players pick from the same folder; a figure on one player's seat is marked in
  the other's list ("Player 1 has it") and cannot be placed twice (one physical toy = one place).
- **Not built**: a second pad's input (only XAM user 0 gets input today), two Toy Boxes, the mixed portal.

## Where it is

| Platform | Folder |
|---|---|
| macOS | `~/Library/Application Support/Skylanders/Collection` |
| Linux | `~/.local/share/Skylanders/Collection` (`$XDG_DATA_HOME/Skylanders/Collection`) |
| Windows | `%APPDATA%\Skylanders\Collection` |
| Xbox (UWP) | the publisher cache folder `Skylanders`, `Collection` inside it; `LocalState\figures` if the folder is unavailable |

On desktops it is `Collection` in the data root (`datadir::Root()`, `docs/getting-started/settings.md`; `collection::DefaultFolder()`):
not under a game's folder, so another game's runtime or the launcher finds it at the same place.

`SKY_COLLECTION=<folder>` replaces it (tests, portable installs). `SKY_FIGURES`, the older variable, still works and
means the same when `SKY_COLLECTION` is not set.

**Xbox.** UWP apps cannot share a folder freely: each has its own `LocalState`. Apps of the same publisher can share
`ApplicationData.GetPublisherCacheFolder(name)`, a folder declared in each app's manifest
(`windows.publisherCacheFolders`, `tools/xbox/AppxManifest.xml.in` declares `Skylanders`). Decision: the collection
lives there, in `Skylanders\Collection`, so a future Giants or launcher app signed with the same publisher sees the
same figures. Conditions and limits:
- every app must use the **same `Publisher`** in its manifest (`@PUBLISHER@` of `tools/xbox/package.sh`) and declare
  the same folder name;
- the folder is removed when the last app of the publisher is uninstalled (LocalState is removed with its app too);
- it is not backed up by the system; `backups/` (below) is inside it.

`host/uwp/main.cpp` asks for the folder at start and sets `SKY_COLLECTION` (and `SKY_MIGRATE_FROM=LocalState\figures`);
if the call fails (no manifest declaration, older system) the figures stay in `LocalState\figures` as before.
[?] (not run on the console yet: the build and the import check pass, the call itself is untested.)

## What is in it

```
Collection/
  014 Gill Grunt.sky          the dumps (.sky, .bin, .dmp): 1 KB MIFARE images, the source of truth for the stats
  014 Gill Grunt.sky.lock     while a game has the figure on its portal
  collection.json             what the dumps cannot hold (below)
  catalogue.json              optional: toys added to or corrected in the built-in catalogue
  backups/014 Gill Grunt.sky/2026-10-24T101500Z.sky
```

`collection.json` (written to a temporary file and renamed over, re-read just before each change):

```json
{
  "format": 1,
  "figures": {
    "014 Gill Grunt.sky": { "label": "Ace's", "favourite": true, "edition": "Crystal Clear Gill Grunt",
                            "edition_kind": "crystal", "last_game": "ssa", "last_used": "2026-10-24T10:15:00Z" }
  },
  "migrated": { "<home>/Library/Application Support/SSARecomp/figures": { "014 Gill Grunt.sky": "014 Gill Grunt.sky" } }
}
```

- Since the new Toy Box ([toybox_ux.md](toybox_ux.md)): per figure `origin` ("created", "imported", "portal") and `games`
  (`{"ssa": {"last_used": ..., "uses": 3}}`); top level `trash` (`{"<stamp> <file>": {file, deleted, reason,
  metadata}}`, the dumps in `trash/`, kept 30 days), `views` (`{"ssa": {"game": {group, order, show, fold_foreign},
  "wheel": {"p1": [{file, pinned} x 8]}}}`) and `tidy_done`.
- The stats (experience, level, gold, play time, nickname, hat, upgrades, hero points...) are **only** in the dump
  (`docs/runtime/host/figures.md`). Nothing in `collection.json` repeats them.
- `edition`: an edition the tag does not tell apart (many Crystal Clear, Glow in the Dark, Metallic, Flocked figures
  carry the same toy type and variant as the plain one [?]); the player picks it in the launcher.
- Keys are file names: renaming through the library moves the entry and the backups; a file renamed by hand loses
  its metadata (the stats stay, they are in the dump).
- `partner` (Swap Force halves): the file name of the other half of a swapper, see "Swappers" below.

## Swappers: two dumps, one card [SwapForce] (2026-10-27)

A Swap Force swapper is **two toys**: a top (type 2000-2015, `Category::SwapTop`) and a bottom (1000-1015,
`Category::SwapBottom`; `2000 + n` and `1000 + n` are the same swapper's halves [V]), each its own dump with its own
progress (each half keeps its play time [V]). The owner's decision: the player sees **one figure**, in the Collection and
in the game's Toy Box; one click places both halves (the two tags on the portal); the sheet can change one half
("Change the bottom"; mixing a top with another swapper's bottom stays possible).

Design (the simplest that keeps the dumps the source of truth): the **link** is a `partner` entry (the other half's
file name) on **both** halves in `collection.json`.
- The link counts only when both agree: both dumps exist, each names the other, and one is a top and the other a bottom
  (`Figure::partner` is set only then; a stale or one-sided entry reads as no link). So a deleted, replaced or
  hand-renamed file never leaves a half tied to the wrong dump.
- A half with no link is a valid card of its own (one figure): the lone halves a player imported, or what a change left.
- **Rename** moves the other half's `partner` too. **Delete** moves one half to the trash with its metadata (the link
  included); the other half reads as unlinked meanwhile; **Undelete** (or both Undeletes) restores the link when both
  names are back. **Duplicate** of a half that has a partner copies both and links the copies. **Import** never links
  (it cannot know which two dumps belong together).
- `Pair(a, b)` links a top and a bottom (either order; their earlier partners are left unlinked), `Unpair(path)` cuts a
  link, `ChangeHalf(keep, replacement)` is "Change the bottom / the top": the card of `keep` takes `replacement` as its
  other half. If `replacement` belonged to another swapper, that swapper takes the old half, so no card disappears.
  `CreateSwapper(top, bottom)` makes a whole new swapper (both dumps, origin `"created"`, linked); `MatchingHalf(type)` is
  the other half of the same swapper (2003 gives 1003).
- Every one of them is refused while a game uses any of the dumps involved (including a partner).
- **Cards.** `Cards(list)` is what a shelf shows: one `Card` per figure, a linked pair once. `Card::first` is the figure
  itself or the swapper's **top** (it stands for the card: picture, label, favourite, last use, level), `Card::second` the
  bottom (null for any other figure). `CardName` is the top's label if the player gave one, else the swapper's name when
  both halves are the same swapper (`Wash Buckler`), else the mixed swapper's name as the game gives it (`SwapperName`:
  `Nitro Magna Charge` over `Boom Jet` is `Nitro Magna Jet`); `CardLevelText` is the top's
  `LevelText`; `Card::Busy()` is true when either half is in use; `Card::Halves()` is what goes on the portal and what
  Delete and Export apply to. `Group()` shows a linked pair once (as its top: the bottom's index is left out of the
  sections), and `List()` with a filter keeps both halves of a pair when either passes (a search for "Fire Kraken"
  finds the swapper whether it matched the top or the bottom).
- The **card's label, favourite and "last used"** are the top's. The picture is the top's (the disc has portraits for
  tops only: [games/swapforce/docs/art.md](../../../games/swapforce/docs/art.md)); a bottom has the toy's own picture.
  "Edit stats", "Reset" and "Back up" of a swapper's sheet act on the top.

## Locks

A figure on a game's portal is in use: the game may write it at any time. When the emulated portal places a dump of
the collection it writes `<dump>.lock`:

```json
{ "game": "ssa", "pid": 4242, "since": "2026-10-24T10:15:00Z" }
```

and removes it when the figure is taken off or the portal is cleared. The game ends with `std::_Exit`, so the locks of
the figures still on its portal stay behind when it quits; a lock whose process no longer runs is **stale**: games remove stale locks at start (`ClearStaleLocks`), and the library treats them as free.
The launcher must not write a figure whose lock is held by another live process (`LockInfo::Busy()`): the library's
writing calls refuse it (`WriteStats`, `SetNickname`, `Rename`, `Restore`); metadata (label, favourite, edition) is
not the dump and can always be changed. The Toy Box, inside the game, writes the figures its own game locked. A real
portal reads the real toy, so nothing is locked then. Dumps placed from outside the collection (`SKY_PORTAL_PLACE`
with another folder) are not locked. [V] (`ArkeyanCollectionTest`, and the run below)

## Migration from the per-game folders

Before this, Arkeyan kept dumps in `figures` of its data folder (now `Skylanders/SSA/figures`, moved there by the data
folder migration), `LocalState\figures` on the Xbox. At start the emulated portal calls `Migrate(LegacyFolder())`:

- every dump of the old folder that was not migrated yet is **copied** into the collection; one already there byte
  for byte is only recorded; a name already taken by another dump gets ` (2)`;
- the originals are **never deleted or changed**; which file became which is recorded in `collection.json`
  (`migrated`), so a second start copies nothing, and a dump added later to the old folder is copied then;
- `portal.txt` (the last session's portal, in the save folder) still names the old paths: the restore maps each to
  its copy (`MigratedPath`), and the next change rewrites `portal.txt` with the new paths.

`LegacyFolder()` is `SKY_MIGRATE_FROM`, else `<data root>/SSA/figures` only when neither
`SKY_COLLECTION` nor `SKY_FIGURES` is set: test runs never read the player's folders.

## The catalogue (data)

`resources/collection/catalogue.json`, built into the executables (`cmake/embed_file.cmake`); a `catalogue.json` in
the collection folder adds toys or replaces them by toy type, and games by id. Format:

```json
{ "format": 1,
  "games": [ { "id": "ssa", "name": "...", "year": 2011, "runtime": "SSARecomp",
               "accepts": [0, 1, ...], "names": { "14": "GillGrunt" }, "images": { "202": "potion" } },
             { "id": "giants", "accepts_games": ["ssa", "giants"], "portraits": { "9": "eruptor", "9:0x1206": "eruptor_lightcore" },
               "artworks": { "9": "eruptor", "9:0x1801": "eruptorseries2" },
               "hats": { "1": "Combat Hat", "6": "Pirate Hat" } }, ... ],
  "toys": [ { "type": 14, "name": "Gill Grunt", "element": "water", "category": "core", "game": "ssa",
              "variants": [ { "id": "0x3810", "name": "Eon's Elite Gill Grunt", "kind": "eons_elite" },
                            { "name": "...", "kind": "crystal", "same_tag": true } ] } ] }
```

- `element`: air earth fire water magic tech life undead light dark none.
- `category`: core, giant, swap_top, swap_bottom (a swapper's two halves are two toys), trap_master, supercharger,
  sensei, mini (minis and sidekicks), trap, villain, vehicle, creation_crystal, magic_item, adventure (adventure packs
  and location pieces), trophy.
- `edition` on a toy: the toy type itself is special (`legendary`: 404 Legendary Bash; `dark`: 28 Dark Spyro).
- variant `kind`: legendary, dark, crystal, glow, metallic, gold, silver, chrome, flocked, series, lightcore,
  eons_elite, alt_deco (Polar, Royal, Volcanic, Granite...), event (event and store exclusives), shape (trap and crystal shapes),
  other, and `base`: the plain figure as sold with a non-zero id (Giants-era figures carry `0x1000`, the Giants and the
  two Giants magic items `0x1206`, the Trap Team re-releases of the sidekicks `0x3000`); the games read it as the plain
  toy, the catalogue names it after the toy. A UI never lists it as a version: the plain toy is that row (named as the toy is, no "Standard" word: 2026-10-28) and creates the
  figure with this id (Trap Team's Kaos Trap, whose one shape carried its own name, is a `base` entry for that reason;
  `StandardVariant`, `VersionCount`; `CreateSwapper` does it for both halves when no version is given).
  `same_tag`: a physical edition with no id of its own.
- A game accepts the toys of `accepts` (in that order: SSA's menus follow it) and every toy whose first game is in
  `accepts_games`. SSA keeps its exact set: types 0-32, the legendaries 404/416/419/430, the sidekicks
  505/514/519/526, items 200-207 and packs 300-304 (`docs/runtime/host/figures.md`, from the title's portal code). The later games
  accept every older toy [? true for the characters; not every older magic item or adventure pack works in later
  games; to be refined when those games run]. `names` are the title's internal names (its portrait images),
  `images` other UI images that show a toy. `portraits` (Giants, Swap Force) names the game's picture of a toy (`"<type>"`) or of
  one of its variants (`"<type>:<variant id>"`): a file of `<data>/<Game>/portraits/` ([game_art.md](game_art.md)).
  `artworks` (Giants, Swap Force) names its large pictures the same way: files of `<data>/<Game>/portraits/artworks/`
  (Giants: the versus screen's character art, about 1000x960; Swap Force: the face-off portraits, 640x320).
  `halves` (Swap Force) names a swapper half's swap-screen picture (a top above its waist, a bottom's legs, on transparency;
  keys as `portraits`, tops 2000-2015, bottoms 1000-1015; files of the same `artworks/` folder) for the stacked picture of a
  swapper ([game_art.md](game_art.md), "The rule for a toy's picture").
  `hats` names the game's hats by the number a figure stores (Giants: the disc's `Hat_NNN` + 1, [figures.md](figures.md));
  the hat pickers list them in that order.
- Variants not listed: first the listed variant with the same low 12 bits, if only one has them (another release year
  of the same deco: Giants reads only those bits, [figures.md](figures.md#the-variant-id-what-each-game-reads-ssa-giants)),
  else a name from the id's bits [?] (community descriptions): high nibble = year of the release (1 = 2012 Giants ...
  5 = 2016), low byte = decoration: 0x01/0x05 a new pose ("Series N", N = year + 1), 0x02 another paint job, 0x03
  Legendary, 0x06 LightCore, 0x10 Eon's Elite. `Catalogue::Describe` returns `known = false` then. [V] for Giants: bit 11
  = repose (Series 2), bit 10 = alternate deco (matched by the low byte), bit 9 = LightCore.

**Sources and completeness.** The Spyro's Adventure, Giants and Swap Force toys and variants follow the games
themselves: SSA's accepted toy types are its code ([figures.md](figures.md#toys-the-title-recognises)), and every toy and
variant Giants and Swap Force know comes from their discs' toy tables ([below](#the-games-own-toy-tables)) [V]. The later games' toys, and the
variants of the older toys that only later games released, are compiled from public knowledge (the retail names on the
packages, the fan wikis' figure lists, the community descriptions of the tag's fields): **from public lists, not checked
against a disc**. The file is our own format; no table was copied from GPL code (RPCS3 and Dolphin were not used for
this file). A real dump that disagrees is corrected through `catalogue.json` (its toy type and variant are in block 1,
readable with `ArkeyanFigureTest <dump>`).

| Game | Toys | Variants | Source |
|---|---|---|---|
| Spyro's Adventure | 50: 33 characters (Dark Spyro included), 4 Legendary, 8 magic items, 5 adventure packs | 90 on its toys: 24 Series 2 and 4 LightCore, Polar Whirlwind, Royal Double Trouble and 3 Legendary decos, all as Giants defines them [V]; the later releases and chase figures from public lists (below) | the title's code [V]; the Giants disc [V]; public lists [?] |
| Giants | 26: 16 characters (8 Giants), 8 sidekicks (505/514/519/526 accepted by SSA too; Giants' own 540-543: Barkley, Thumpling, Mini-Jini, Eye-Small), 2 magic items; 52 hats | 66: 4 LightCore, 3 Legendary, Granite, Jade, Molten, Punch, Scarlet, Gnarly, Golden Dragonfire Cannon [V]; the later releases, chase and event figures and the plain retail ids from public lists (below) | the Giants disc [V]; public lists [?]; hat names [?] |
| Swap Force (catalogue id `swapforce`) | 57: 16 swappers (top 2000-2015 and bottom 1000-1015: 32 toys), 16 core, 5 magic items, 2 adventure packs and 2 battle arenas | 88: 30 its data defines (5 LightCore, 6 Legendary, 5 Dark, 13 other alternate decos, VVind-Up), Hog Wild Fryno (`0x3801`, Trap Team re-release), and 57 plain retail ids (`base`: `0x2000`, the two arenas `0x2206`) [V] | the Swap Force disc [V] |
| Trap Team | 63: 18 Trap Masters, 18 core, 8 minis, 11 trap types, 4 items, 4 adventure packs | 76: the trap shapes (its own table) and 15 alternate decos (its table's names, SuperChargers' and Imaginators' ids) [V] | the Trap Team, SuperChargers and Imaginators discs [V]; public lists |
| SuperChargers | 71: 20 SuperChargers, 20 vehicles, 4 racing trophies (3500 Sky, 3501 Land, 3502 Sea, 3503 Kaos), 13 racing villains and their 14 vehicles (its table) | 42: 8 Instant from its table; Dark, Legendary, Power Blue, Nitro, holiday, chase and E3 decos | the owner's dumps (types, variant ids, names) [V]; the disc's 469 badges name every one |
| Imaginators | 44: 31 Senseis, 10 creation crystals (680-689, one per element), the Mystery Chest (235), 2 adventure packs (310, 311) | 58: 18 Sensei decos and Heartbreaker Buckshot (its table), 34 crystal shapes and Legendary shapes, 5 chests | the owner's dumps [V]; Senseis' types from public lists, matched by the dumps |

420 named variants in all (97 before 2026-10-26, 129 after the Giants disc's tables, 200 after the public list, 293
after the Swap Force disc's tables, 394 with Trap Team's and the owner's SuperChargers and Imaginators dumps, 420 with SuperChargers' and Imaginators' tables on 2026-11-10). `ArkeyanCollectionTest` prints the
counts and checks that the 49 variants Giants reads (the 47 its disc defines, and the public Rare LightCore Prism
Break and Love Potion Pop Fizz, which Giants reads as LightCore and as Punch) read in Giants as listed.

### Issue #25 audit status (2026-11-11)

The current file contains **311 toy types and 420 named variant rows**. Of the variants, 417 have an id and three
are `same_tag` editions. Counts above include variants of older toys released by later games and plain `base` ids;
they group rows by the toy's first game. SSA has 90 variant rows, Swap Force 88; those counts were understated above.
SuperChargers has 20 retail vehicles and 14 racing-villain vehicles, not 15.

`collection_test.cpp` now requires every id in `catalogue_ids.h`: 311 toy ids and 417 exact toy/variant pairs. This is
an explicit checklist of the existing catalogue baseline, not an independently extracted list of everything each
game knows. It also checks all three Trap Team Dark characters, the corrected Sensei elements, and the Nitro Magna
Charge portrait key. Trap Team's misnumbered Nitro image filename is retained, mapped to the figure's `0x2402` id.

The extracted `private/<game>/toy_tables.json` files are absent in this workshop; `private/` contains only compiled
libraries. A fresh six-game comparison of ids, names, first games, elements and variant flags is therefore pending.
No missing toy or variant counts can be established from those tables here, and no speculative rows were added.
Pit Boss (undead), Hood Sickle (dark) and Grave Clobber (water) were corrected using the recorded Imaginators table
comparison below. The existing Dark Wildfire, Dark Snap Shot and Dark Food Fight entries are preserved.

### What each game reads (compatibility, 2026-10-28)

`Catalogue::Accepts(game, type)` answers for a toy; `Catalogue::Read(game, toy, variant)` for a figure: `works`, the
variant `seen` (the game's own entry, null for the plain toy), its `kind` and `exact` (the game tells this very variant
apart). `VersionsFor(game, toy)` is the list of versions a UI offers for that game: only the `exact` ones (and the same-tag
editions, a physical choice no game sees). `KindIn` and `NameIn` give a figure's kind and name as the game sees it. The Toy Box
of a game **hides** the figures it does not read (no "Not for this game" fold in game) and its Add a figure lists only that
game's toys and versions; Arkeyan Launcher keeps the whole collection, its Filters row **Works in** keeps the toys a game reads, and its
sheet says "In <game>: plays as ..." when the game does not tell the variant apart.

| Game | Toys it reads | Variants it tells apart (the rest play as the plain toy) | Source |
|---|---|---|---|
| Spyro's Adventure | 50: 33 characters (Dark Spyro), 4 Legendary, 8 items, 5 adventure packs | **none**: the toy type only, every Series 2, LightCore or Polar figure plays as its character [V] | the title's code [V] |
| Giants | 76 (the disc's 77 minus a debug toy): all of the above, 16 characters (8 Giants), 8 sidekicks, 2 items | the 49 its tables define: Series 2 (the repose bit, 24 toys), LightCore (8 toys), 15 alternate decos and Legendaries. Any other id: a **later repose** (bit 11: Series 3, Eon's Elite 0x3810, Volcanic) shows as **Series 2** when the toy has one; a LightCore bit on a toy not in its list, or a chase figure, plays plain [V rule; the Eon's Elite and chase ids are from public lists, not seen on a figure: [?]] | Giants disc `Init_Setup.bld` [V], [figures.md](figures.md#the-variant-id-what-each-game-reads-ssa-giants) |
| Swap Force | 133 (the disc's 136 minus three debug templates): SSA's and Giants' toys, 16 swappers (top and bottom), 16 cores, items 3200-3204, adventures 3300-3303 | the 2013 decorations its `CVariantIdentifier` list defines (the same flags, year codes 0-2); a later year plays as above [V list, V rule applied the same way [?]] | Swap Force disc `toydata.pak` [V] |
| Trap Team, SuperChargers, Imaginators | the older toys and their own (`accepts_games`); whether older magic items and adventure packs work is not checked [?] | every variant of the older years, taken as known [?] | public lists [?] |

What the games' own tables show (`tools/toy_tables.py`, run 2026-10-28): the Giants disc defines every toy type of
Spyro's Adventure (including items 200-207 and packs 300-304) and its own, so Giants reads them all; the Swap Force disc
defines every toy type of both earlier games and no Trap Team or later toy. Nothing in either disc rejects a type by its
variant: an unknown variant is ignored, never refused, so a variant never makes a figure unreadable, it only changes what is shown.
Public sources, facts only: Giants "is compatible with all Spyro's Adventure figures" and the original adventure packs
([Time Extension](https://www.timeextension.com/games/wii/skylanders_giants), [Giant Bomb](https://www.giantbomb.com/games/3030-37417/));
Swap Force "is backward compatible with all Spyro's Adventure and Giants figures" ([Nintendo](https://www.nintendo.com/en-gb/Games/Wii/Skylanders-SWAP-Force-820251.html)).
Per-version cases the rule produces: the Trap Team re-release of a sidekick (`0x3000`, "Terrabite (Trap Team mini)") is a
version only from Trap Team on; in Giants and Swap Force it is the plain sidekick. Re-run the table with a real figure by
`ArkeyanFigureTest` and the emulated portal if a figure disagrees, then correct the catalogue (`catalogue.json`).

**The public list** (2026-10-26): [Texthead1/Skylander-IDs](https://github.com/Texthead1/Skylander-IDs) (no licence:
used as facts, toy type and variant id per figure, names rewritten in the catalogue's style), one compiled list of
real figures' ids; one source only, so every id it alone gives is **from public lists, not checked against a disc**
[?]. It agrees with every id the Giants disc lets us check (the 24 Series 2 `0x1801`, LightCore `0x1206`, Legendary
Jet-Vac `0x1403`, Legendary Chill `0x1603`, Jade, Molten, Punch `0x1402`, Polar, Royal `0x1C02`, Granite, Scarlet,
Golden Dragonfire `0x1602`, Legendary Ignitor, Slam Bam, Stealth Elf `0x1C03`). From it (unreleased figures left out):
- **ids corrected**: LightCore Warnado and Wham-Shell `0x2206` (were `0x1206`), Legendary Bouncer `0x1603` (was
  `0x1C03`), Gnarly Tree Rex `0x1602` (was `0x1402`), White Flocked Stump Smash `0x1814` (was a same-tag edition),
  Platinum Hidden Treasure `0x0002` and never released;
- **added (71)**: **LightCore Flashwing `0x2206`** (Swap Force; neither disc defines it: Giants shows it as a plain
  Flashwing), Rare LightCore Prism Break `0x1214`; the Swap Force and Trap Team reposes of Giants toys (Turbo and Full
  Blast Jet-Vac, Fire Bone Hot Dog, Blizzard Chill, Super Gulp and Fizzy Frenzy Pop Fizz, Heavy Duty Sprocket, Sure Shot
  Shroomboom), Love Potion Pop Fizz `0x3C02`, Dark Mega Ram Spyro, 6 more Eon's Elite (Trigger Happy, Boomer, Zook,
  Stealth Elf, Ghost Roaster), the chase and event figures (Glow in the Dark, Sparkle, Iridescent, Stone, White Flocked,
  Metallic, Golden, Color Shift, Frito-Lay, Pumpkin, Halloween, Heavy Metal), Gnarly Barkley, the Trap Team minis of the
  sidekicks (`0x3000`), and the plain retail ids (`base`, above).

### The games' own toy tables

**Audit of 2026-11-11 (issue #25) [V]**: the six discs' tables read again (`tools/toy_tables.py`, a French and Dutch
Trap Team edition included: its tables are the same, with that language's names) against the catalogue: every toy and
variant the six games define is in it (`tools/catalogue_tables.py --check` and `tools/trapteam_catalogue.py` change
nothing; Swap Force's 136 toys and their variants all present; Giants exactly the disc's; Spyro's Adventure's accepted
types all present). Left out on purpose, as before: the games' templates and internal bodies. A figure still missing
can only be one no game lists (a promotional repaint the games read as its base variant): it needs its name from the
player. Trap Team's own picture of Nitro Magna Charge (top) is named `2010_9128` for the variant 9218 (0x2402): the
tool maps it to 0x2402.

The catalogue is checked against the tables the games carry, not guessed. `tools/toy_tables.py <disc.iso>` reads a
player's disc (read only) and writes them to `private/<game>/toy_tables.json` (game data: never commit it). Giants
takes about 1 s.

**Giants [Alchemy] [Giants] [V]: `character/Init_Setup.bld`.** Its `level.bld` (IGZ v6, LZMA in IGA v8) holds the
script `Portal - Set Up Object Level Loads`, one macro call per definition. A call is an `OpUseMacro` followed, in the
script's `OpCodeList`, by `OpMacroParameter` objects (word 7: the parameter's name, word 12: its value, a
`ValueRHSVariant` literal or an `RHSReferenceStack` string reference; word 14: the macro; word 6 bit 31 set = the
macro's own declaration, not a call). The macros:

| Macro | Parameters | What |
|---|---|---|
| `Portal - Define Object Level Load` | `level load`, `name`, `item code` (the toy type), `element` (0 magic, 1 water, 2 air, 3 undead, 4 tech, 5 fire, 6 earth, 7 life), `is magic item`, `is sky1 character` (default 1), `has known repose`, `is giant` | every toy: 76 and a DEBUG one (999) |
| `Portal - Define Legendary Alt Level Load` | `level load`, `check against deco id`, `subtype display string` (default "Legendary") | an alternate deco of the toy defined just before: 15 |
| `Portal - Define Alt Character` | `normal toy load`, `alt toy load`, `alt prefix` (default "Legendary") | toy types that are another's alternate: 404, 416, 419, 430 (Legendary) and 28 (Dark) |
| (`set [loads temp] to ...`, then a loop into `display as lightcore`) | 8 level loads | the LightCore toys |

Strings are external references (namespace `0x2694BE8E`), numbered by `permanent/global.bld`'s string groups in this
order, each taking one number and then one per string: `Item_Loads`, `ItemLoads_Legendary`, `Location_Loads`,
`Item_Names`, `ItemName_Locations`, `Minion_Loads`, `Minion_Loads_Legendary`, `Minion_Names`, `Toy Type Strings`
(global.bld keeps some merged: its `Item_Loads` also lists the legendary and location loads; a group is a set minus the
sets named after it). The tool checks the numbering against the toy types (`MinionLoad_108_Alchemist` is toy 108). The
English text is `global.bld`'s `ENGLISH.pak`: one `LanguagePackInfo` whose block lists every string (UTF-16) in the
order of the file's `StringInfo` objects. The variant rules the game applies to these tables: [figures.md](figures.md#the-variant-id-what-each-game-reads-ssa-giants).

**Swap Force [Alchemy] [SwapForce] [V]: `archives/toydata.pak`.** An IGA v10 archive (big-endian, [game_art.md](game_art.md#the-versions-of-the-alchemy-formats))
of 394 entries: 131 text files `toydata\<family>_toydata.igx` (Alchemy's XML object format, `<igx><object refname type>
<var name value|ref>...`), each with `_en.igx` and `_fr.igx` (`igLocalizedString`: `_string`, the `_object` and the
`_field` it replaces, `CToyData::_toyName` or `CVariantIdentifier::_toyName`). One object per toy:

| Class | Fields | What (count on the disc) |
|---|---|---|
| `CFullCharacterToyData` | `_toyId`, `_elementType`, `_toyName`, `_seriesName`, `_characterName`, `_isGiant`, `_variants` | characters: 37 SSA (Dark Spyro and the 4 Legendaries included), 16 Giants (8 giants), 16 Swap Force cores |
| `CShapeshifterPartToyData` | `_partName` (`Body_Aviator`, `Legs_Aviator`), `_partType` (`eSPT_Body` = top; `eSPT_FirstPart` = bottom), `_turboType` (bottoms) | 16 tops, 16 bottoms; the movement type: Flyer, Twister, Digger, Bouncer, Sneaker, Teleporter, Speeder, Climber (2 each) |
| `CMagicItemToyData`, `CBattlePieceToyData`, `CExpansionLevelToyData` | `_itemName`, `_unlockedZone` | 14 magic items (SSA's 8, Volcanic Vault 304, 3200-3204); battle pieces 208, 209 and the arenas 3302, 3303; adventures 300-303, 3300, 3301 |
| `CPetToyData` | `_characterName` | the 8 sidekicks |
| `CVariantIdentifier` (in a `CVariantIdentifierList`) | `_decoId`, `_yearCode`, `_wowPowFlag`, `_fullAltDecoFlag`, `_lightCoreFlag`, `_variantText` ("Series 2", "Series 3", "LightCore", "Special", "Rare"), pictures | 123 variants (below) |

Plus three debug toys, `Shapeshifter_Leg_Template` 1999, `_Body_Template` 2999 and `Character_Template` 3999
("Template Legacy"), kept out of the catalogue. Toy types and decos are written as the title's enum names
(`kTfbSpyroTag_ToyType_Character_Zook`, `kTfbSpyroTag_DecoID_Repose2`); the numbers are only in the executable: an
`igMetaEnum` table (the enum's name, then u32 pointers to its member names, then, 8-aligned, their s32 values), which
the tool reads from the loaded image `tools/xexdump` writes (`private/swapforce/image.bin`). `kTfbSpyroTag_ToyType`
has 159 members (0-32 and 100-115 characters, 200-209 items, 300-304 adventures, 404-430 Legendaries, 505-543 sidekicks,
`Shapeshifter_Legs_*` 1000-1015, `Shapeshifter_Body_*` 2000-2015, `Character_2013` 3000-3015, items 3200-3204,
`Expansion_2013` 3300-3303); the disc defines a toy for every named one. `kTfbSpyroTag_DecoID`: Normal 0, Repose1 1,
AlternateDeco 2, Legendary 3, Event 4, Repose2 5, LightDirect 6, LightStored 7, LightEnhanced 8, Gemstone 9, Gold 10,
Silver 11, Bronze 12, Evil 13, Fuzzy 14, Pearl 15, Purple 16, GlowDark 17, StoneStatue 18, GlitterSpectrum 19,
TreasureHunt2012 20, TreasureHunt2013 21, Holiday 22. How a variant becomes a figure's id: [figures.md](figures.md#the-variant-id-what-each-game-reads-ssa-giants).
The tool takes about 0.1 s.

The 123 variants: 24 Series 2 (Repose1, year 1) and 16 Series 3 or Swap Force reposes (Repose2, year 2); 16 LightCore
(8 Giants-era, 8 Swap Force: Warnado, Wham-Shell, Flashwing and five Swap Force cores); 16 Legendary (the 4 SSA
Legendary toys' own `0x0403`, 6 Giants-era, 6 Swap Force); 34 alternate decos (Dark Spyro's own `0x0402`, Platinum Treasure Chest `0x0002`, Polar,
Royal, Granite..., the Swap Force Dark, Nitro, Jade, Enchanted, Quickdraw, Jolly, Kickoff decos, Volcanic Eruptor,
Springtime Trigger Happy, Dark Stealth Elf); 17 others (Stone, Glow in the Dark, Sparkle, Iridescent and the other
"Rare" decos already in the catalogue, VVind-Up).
Checked against 133 real Swap Force figures [V] (`ArkeyanFigureTest`-style read of block 1): every in-game variant among
them (54 figures: reposes, LightCores, Dark, Legendary, Nitro..., VVind-Up) carries exactly the id the tool computes,
the plain ones `0x2000` (the debug Templates 0); not in the data: the 19 chase figures (Gold, Green, Metallic, Bronze &
Silver, Frito-Lay, Heavy Metal, Employee Edition: decos 0x04, 0x16, 0x18, 0x19, 0x1A-0x1C) and the arenas' `0x2206`.

**What changed in the catalogue with it** (2026-10-26): the game id `swap_force` became `swapforce` (the build's
`ARKEYAN_GAME` and Arkeyan Launcher's row id); **the swapper halves were the wrong way round** (the public list had tops at
1000-1015): tops (bodies) are 2000-2015 and bottoms (legs) 1000-1015, by the game's enums and data and by the real
figures; the halves are named from the game's half names (Boom + Jet: "Boom Jet (top)"); added 87 variants on the
Swap Force toys (above) and 6 on older toys that only Swap Force defines: the plain ids of Dark Spyro (`0x0402`) and
the four Legendary toys (`0x0403`), and a Flameslinger alternate deco `0x1002` with no name of its own ("Special
Flameslinger" [?]: maybe the public list's Golden Flameslinger `0x1802`). Names as the game writes them, except two
case fixes ("Kickoff CountDown", "VVind-up" become Kickoff Countdown and VVind-Up) and Volcanic Eruptor's trailing space;
the game's "Quickdraw" is kept. Not changed, though Swap Force names them otherwise (SSA's names stay): Time Twist
Hourglass (Time Twister), Sky Iron Shield, Sparx the Dragonfly, Pirate Ship (Pirate Seas), Eye Small, Platinum
Treasure Chest, and its "Dark Spyro" for `16:0x2C02` (the catalogue's Dark Mega Ram Spyro).

**SuperChargers and Imaginators [Alchemy] [SuperChargers] [Imaginators] [V]: `archives/permanent.pak`.** One binary IGZ v9
file per toy family, `ToyData/<model>_ToyData.igz` (IGA v11 big-endian; Imaginators' chunks raw deflate,
[game_art.md](game_art.md#the-versions-of-the-alchemy-formats)), with Swap Force's classes as objects
(`CFullCharacterToyData`, `CVehicleToyData`, `CShapeshifterPartToyData`, `CTrapToyData`, `CMagicItemToyData`,
`CExpansionLevelToyData`, `CBattlePieceToyData`, Imaginators' `CLootChestToyData`): SuperChargers 268 toys, Imaginators
323, every older game's included. A toy: word 2 its type, word 3 its element (0 none, 1 earth, 2 water, 3 air, 4 fire,
5 life, 6 undead, 7 magic, 8 tech, 9 light, 10 dark, 11 Kaos), word 4 its name; its `CVariantIdentifierList` holds
`CVariantIdentifier`s: deco id, year code (0 = 2011 ... 5 = 2016; -1 any year), four flag bytes (LightCore, alternate
deco, Wow Pow, and one Imaginators nearly always sets [?]), the variant text and the collection picture. The id is Swap
Force's composition (year << 12, Wow Pow bit 11, alternate deco 10, LightCore 9, deco). The tables never set bit 8,
which the retail SuperChargers carry (the owner's dumps: Dark Spitfire `0x4502`, the table `0x4402`): a variant
matches the catalogue with or without it. `tools/catalogue_tables.py` adds what the tables define and the catalogue
lacked, names from Trap Team's own variant table where it has them (each other name's source is in the script), and
`--check` says whether the catalogue is up to date.

**What they added** (2026-11-10, 26 variants and 27 toys): Trap Team's alternate decos, whose ids no Trap Team table
gives (its table names them; these give the ids): Dark Wildfire, Snap Shot and Food Fight, Legendary Blades, Deja Vu,
Jawbreaker, Bushwhack and Hand of Fate (`0x3403`), Nitro Head Rush and Krypt King, King Cobra Cadabra, Power Punch Pet
Vac, Eggsellent Weeruptor, Winterfest Lob-Star (`0x3402`; Lob-Star's is "any year": 2014 from Trap Team's table); Hog
Wild Fryno `0x3801`, Heartbreaker Buckshot `0x540C`; two deco-4 ("Event") figures of 2014, Eruptor `0x3804` and Snap
Shot `0x3004` (which event is not known) [?]; the "Instant" deco 15 (`0x440F`) of 5 vehicles and 3 SuperChargers (Trap Team's table:
"Virtual, Instant"; tablet editions [?]); SuperChargers' 13 racing villains (3440-3467, category `villain`) and their 15
vehicles (3470-3497), which only SuperChargers and Imaginators define (never sold) [?]. **Corrected**: Sure Shot
Shroomboom `0x3801` and Tidal Wave Gill Grunt `0x3809` (were `0x3805`, public list): both tables give the series by
the deco (Series 2 deco 1, 3 deco 5, 4 deco 9). **Left out**: the templates and creation bodies (`Template*`, `CYOS_*`,
the classes 4523-4534, `MediumGeneric_Base`, `BlueFalcon_Base`) and four toys with no picture nor series (Fusion,
Synergy, Unity 4500-4502, Undead Card 4517). **Not in either table**: the trophies, the Wii-only toys (Donkey Kong,
Bowser and their vehicles), Air Creation Crystal (682; the other nine crystals are), the Trap Team "Instant" Snap Shot
and Food Fight (deco 15, flags unknown), Knockout Terrafin's 2013 deco. Element differences corrected for issue #25: Imaginators' table makes Pit Boss undead, Hood Sickle dark
and Grave Clobber water (the recorded comparison; raw tables are unavailable for a fresh check).
**Pictures**: the same script maps each catalogue toy and variant that SuperChargers' or Imaginators' `portraits` lacked
to the tables' collection picture (lower case, as the badges are named; never a variant's plain `_Normal` one): 103 and
126 entries (the new toys, the Trap Team decos and traps' shapes, the Eon's Elite), each on its disc [V]
(`ArkeyanArtTest`), but Threat Rocket's, which neither disc has.
**In the games** (2026-11-10, the in-game Toy Box, emulated portal, figures as Add a figure makes them): Trap Team took
Dark Wildfire `458:0x3402`, Imaginators Heartbreaker Buckshot `618:0x540C` (its own badge on the sheet), SuperChargers
Instant Spitfire `3412:0x440F`, the villain vehicles Toaster Bomber and Steam Roller on Vehicle 1 and the Sky Trophy [V].
Its intro then wants a land vehicle: Toaster Bomber (sky) gets "Incorrect VEHICLE type", Steam Roller (land) does not. A
racing villain (Chef Pepper Jack, seated through a test catalogue whose Player 1 takes `villain`) is read too; whether
the game plays it as a character is not known [?], so no seat takes `villain` yet: the Collection lists the villains,
the Toy Box does not seat them.

**Spyro's Adventure [Lux] [SSA]: no table in data.** Its accepted toy types are code ([figures.md](figures.md#toys-the-title-recognises));
the dump lists the disc's toy pictures (`uigameimage.str`: 74 `toy_<name>_hud` and `<name>_vs`) and character models
(112 in `models/characters/minionsmonsters/`, the Legendaries and Dark Spyro among them, no variant).

**What the games knew that the catalogue lacked** (added 2026-10-26, 32 variants): Series 2 (`0x1801`) of the 24 SSA
characters Giants gives a repose (Whirlwind, Sonic Boom, Lightning Rod, Bash, Terrafin, Prism Break, Eruptor, Ignitor,
Flameslinger, Zap, Gill Grunt, Slam Bam, Spyro, Double Trouble, Trigger Happy, Drobot, Drill Sergeant, Wrecking Ball,
Zook, Stealth Elf, Stump Smash, Hex, Chop Chop, Cynder); LightCore Chill and LightCore Shroomboom (`0x1206`); Legendary
Jet-Vac (`0x1403`) and Legendary Chill (`0x1603`: with the LightCore bit, the game's only picture of it is
`icevalkyrielegendarylightcore`); Jade Flashwing, Molten Hot Dog, Punch Pop Fizz, Gnarly Tree Rex (`0x1402`). Names as
the game composes them ("Series 2 Eruptor"); Mini Jini and Eye Small are written Mini-Jini and Eye-Small as in the game.
The ids carry the bits Giants reads and year 1 (2012); a retail figure with another year nibble is found by the 12-bit
match above (2026-10-26, later: the public list gives these same ids). Giants' own short labels for two items ("Cannon", "Catapult") are not used: the retail names stay.
Giants also defines Volcanic Vault (304, its PvP bonus level `ItemLoad_PvP_BonusLevel`, element fire); the catalogue
keeps SSA's element (none).

**What the catalogue has that no game we have knows** (kept, from public lists, not checked against a disc): the later
releases of SSA toys: Series 3 and 4 reposes (Horn Blast Whirlwind, Knockout Terrafin, Hyper Beam Prism Break, Lava
Barf Eruptor, Anchors Away and Tidal Wave Gill Grunt, Mega Ram Spyro, Big Bang Trigger Happy, Thorn Horn Camo, Ninja
Stealth Elf, Twin Blade Chop Chop, Phantom Cynder: Giants reads their repose bit and shows them as Series 2), 9 Eon's
Elite, Volcanic Eruptor, Springtime Trigger Happy, Dark Stealth Elf (Giants shows them plain or Series 2), LightCore
Warnado and Wham-Shell (Giants shows them plain), Platinum Hidden Treasure; the 4 same-tag chase editions; the Trap
Team trap shapes. LightCore Flashwing is in neither game's tables: it is a Swap Force figure, added from the public
list (above).

## Upgrades

What each upgrade bit of a figure means, with the game's own names: `collection::Upgrades(toy, game, language)`
(`host/collection/upgrades.h`, shared with Arkeyan Launcher) returns the toy's list `{ bit, path, price, name, description }`
in the game's order (4 basic, the first path's 3, the second path's 3, the Soul Gem, the Wow Pow for Giants' Series 2
characters), the two paths' names and descriptions, and `fromDisc`. Constants: `UpgradePathChosenBit` (0),
`UpgradePathSecondBit` (1), `UpgradeSoulGemBit` (9), `UpgradeWowPowBit` (10: the Wow Pow's bit in the editors' working
bits only; the figure keeps it in its extra area, `FigureStats::wowPow`). The field itself:
[figures.md](figures.md#upgrades-ssa-giants).
- **Where the texts come from**: `art::Extract` (part `art::Upgrades`, in `art::All`; `host/art/game_text.cpp`) reads
  them from the player's disc as the game's manifest says (`games/<id>/art.json`, `"upgrades"`) and writes
  `<data>/<Game>/upgrades.json` once (game text: never shared; delete it to read the disc again). Arkeyan Launcher does it
  while it checks the disc (Giants: about 6 s, SSA: under 1 s); the in-game Toy Box reads the file if it is there.
  English and French (`"en"`, `"fr"`); a missing text falls back to English.
- **Fallback**: no file, or a toy it does not list: a plain list ("Upgrade 1"... / "Amélioration 1"..., "Path 1
  upgrade 1", "Soul Gem"), the same bits and prices, `fromDisc = false`. A toy type defined as another's alternate
  (Legendary Bash, Spyro, Trigger Happy, Chop Chop; Dark Spyro) gets its base toy's list (Giants' archives of those give
  the base toy's names [V]).
- **Counts** (2026-10-26): SSA 32 Skylanders (every character but Dark Spyro, which takes Spyro's), Giants 53 (every
  Skylander of both games, the Legendaries and Dark Spyro included); 11 upgrades each, 12 with a Wow Pow entry in
  Giants (the 24 Series 2 characters, their Legendaries and Dark Spyro, and Ghost Roaster, whose list has one, "Crazy
  Skull Speed Increase", though Giants gives him no repose: the game shows it only for a repose figure).
- **Edit rules** (shared by the Toy Box and Arkeyan Launcher's Edit, `upgrades.h`): `UpgradeBought` / `UpgradeGreyed` (a
  path upgrade counts only on the chosen path; the other path is greyed), `ToggleUpgrade` (a click on the other path
  switches to it, the bought path upgrades move with it, and buys the clicked one), `AllUpgrades` (the chosen path, else
  the first), `NoUpgrades` (bits 0-11), `BoughtCount`; `UpgradeName` / `UpgradeLine` clean the game's texts for display
  (its button marks `{`, `[`, `&ICON(...)` become "the button", the description is cut to its first sentence). The
  rules work on `EditorBits(field, wowPow)`, the field with bit 10 = the Wow Pow; `StoreEditorBits` puts the Wow Pow in
  `FigureStats::wowPow` (the extra area's +0x06 bit 0 [V]) and keeps the field's own bits 10-11 (another Giants field).
- **Tests**: `ArkeyanCollectionTest` (a made-up file on a scratch data folder: languages, the alternate types, the
  fallback), `ArkeyanArtTest` with the discs (Spyro's list in both games, both languages, every Skylander listed).

## Icons

- **From the player's disc** (cached as PNG by the game in `<game data>/portraits`, never shipped): the 37
  `toy_*_hud` portraits (characters, legendaries; sidekicks use their counterpart's), and now four magic items whose
  pictures the UI archive has: Healing Elixir (`potion`), Ghost Pirate Swords (`crossed_swords`), Sky-Iron Shield
  (`shield`), Winged Boots (`speed_boots`) [V] (looked at the decoded images; 64x64). The disc has no picture of the
  other four items or of the adventure packs (it has level banners, 512x128, but which level is which pack was not
  established), and none of the variants except Dark Spyro and the four Legendaries.
- **Drawn by our code** for everything else (`collection/icons.cpp`): a shape per category (shield for Skylanders,
  bigger for Giants, smaller for minis, half discs for swappers, octagon Trap Master, bolt SuperCharger, star Sensei,
  crystal trap, wheel vehicle, sparkle magic item, folded map tile adventure, cup trophy) in the element's colour.
- **Giants' artworks** (`<data>/Giants/portraits/artworks/`, 99 pictures of about 1000x960: the versus screen's art of
  every Skylander and its decos, [game_art.md](game_art.md)): `BestPicturePath(game, toy, variant)` (`icons.h`) gives
  the best picture of a toy or variant: the variant's own (artwork, then portrait) before the plain toy's, the largest
  first. `PortraitPath` stays the square picture of icons and cards.
- **A badge** for the variant kind in the corner, on disc pictures too: star (Legendary, Eon's Elite, event, gold),
  crescent (Dark), diamond (Crystal, Metallic, Silver, Chrome), ring (Glow, LightCore), dots (Series, Flocked).
- Nothing is downloaded. `ArkeyanCollectionTest --icons <folder>` writes a contact sheet of all shapes and badges.

## Search and sort (`search.h`, shared with Arkeyan Launcher) (2026-10-27)

`SearchMatches(query, card, game)` is the one search of the Toy Box's field and the launcher's: the query's words must all
match the card (both halves of a swapper): a name word (label, nickname, name, version, edition, file), an element, a
game, a toy type or a version in English or French, "level N" / "niveau N" / "lvl N" / "lv N", with accents and case
folded (`Fold`) and one typo tolerated in a word of 5 letters or more. `Shelf(list, Sort, game)` is `Group`'s shelf in the
player's sort (`Sort`: Recent = favourites, then what the game took, then element; Name, Level, Element, Game ignore
favourites). `SortLabel(sort, french)` and `FigureCount(n, french)` hold the words both apps show. What each app puts
around them (the field, Filters, the tags): [toybox_ux.md](toybox_ux.md) "Search, filters and sort". Unit test:
`ArkeyanCollectionTest` (`TestSearch`). [V]

## Toy Box (in the game)

Swap Force's Toy Box shows a swapper as one card and puts both tags on the portal: [toybox_ux.md](toybox_ux.md) section 4,
"Swappers".

Same look; the content changed: tabs **Skylanders / Specials / Sidekicks / Magic Items / Adventure Packs** (a tab with
no card is hidden: the later games' categories never show for SSA). Specials holds the special toy types (the four
Legendaries, Dark Spyro) and one card per variant dump of the collection (a variant id, or an edition picked in the
launcher), with chips per variant group (All, Legendary, Dark, Series...) where Skylanders has element chips. Cards
show the disc picture or the drawn icon, with the badge. Variant cards are bound to their file; the plain cards keep
choosing a dump as before (the one on the portal, else the starter file).

## Variables for tests

| Variable | Effect |
|---|---|
| `SKY_COLLECTION` | the collection folder |
| `SKY_MIGRATE_FROM` | the old folder to migrate (only this one; nothing is migrated without it when `SKY_COLLECTION` is set) |
| `SKY_PORTRAITS` | the disc pictures' cache (else `<game data>/portraits`) |
| `SKY_LAUNCHER_INI` | the settings file to read instead of the player's `launcher.ini`; empty: none (defaults) |

`launcher.ini` is read from the player's data folder even with `SKY_SAVE_DIR`, so a player's `fps_limit=60` changed
test runs; tests should set `SKY_LAUNCHER_INI=` (or their own file) from now on.
