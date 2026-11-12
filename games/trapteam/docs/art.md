# Trap Team's pictures and tables from the disc: logo, portraits, traps, villains

> **Purpose:** where Trap Team keeps its logo, background, toy, trap and villain pictures and its own toy and villain tables, and how the Toy Box and the launcher read them · **Status:** current (2026-10-30) · **Tags:** [Alchemy] [TrapTeam] [V]

The provider and the rule are the same as for the other games ([game_art.md](../../../docs/runtime/host/game_art.md)); this page is what is Trap Team's. Read from the
player's own disc at run time and cached in `<data>/TrapTeam/`; nothing is shipped. Manifest: [art.json](../art.json). The archives are IGA v11, their IGZ v8 (the
padded names: [game_art.md](../../../docs/runtime/host/game_art.md#the-versions-of-the-alchemy-formats)).

## Where the pictures are [V]

| Picture | Archive | Entries (`C:/tfb/build/360/ui_sky4/...`) | Size |
|---|---|---|---|
| Logo | `permanent/global.arc` | `titlelogo/titlelogo_001.png` | 683x341, DXT5 on transparency (the logo, the crystal between "TRAP" and "TEAM", the eight element medallions) |
| Background | `permanent/global.arc` | `screens/loadingscreen/textures/levelload_313.png` | 1280x720 |
| Toys | `misc/UI_Collection_Champions.arc` | `collection_images/skylander_toys/<year>/<year>_skylanders[_regular]` and `<year>_items`, `<type>_<variant>_<name>.png`: 288 | 128 |
| Traps | `misc/ui_collection_traps.arc` | `collection_images/traps/square/<type>_<variant>.png`: 60 (a picture per shape) | 128 |
| Villains | `misc/ui_villainvault_stream.arc` | `collection_images/villains/captured/<id>_villaincaptured.png` and `.../escaped/<id>_villainescaped.png`: 46 and 46 | 171 |

`Title.bld` holds only Giants' logo and a sprite sheet left over from it (not Trap Team's logo, which is `global.arc`'s); the collection screen's own scene
(`UI_Collection_Champions.bld`, 7374 objects) holds 7 `igImage2` of the screen's furniture, no portrait: the portraits are the streamed `.arc` entries.
**The toys' file names carry the figure's identity**: the toy type and the variant id a tag holds (`474_13315_bushwhack_legendary` is type 474, variant 0x3403), so
`tools/trapteam_catalogue.py` maps them without a table of ours. "Every toy it knows" (Trap Masters, cores, minis, Eon's Elite, older games' toys, items, adventure
packs) is the list of that arc: 180 toy and trap types of the four games (348 pictures with the traps); the 16 Swap Force bottoms, 8 Giants' plain figures
(their only picture here is a later repose), the Giants' Dragonfire Cannon and Scorpion Striker Catapult, Fiery Forge and Arkeyan Crossbow have no plain picture:
**28 toys show a drawn icon from this disc alone** (their own game's disc gives them one, by the rule in game_art.md; `ArkeyanArtTest` with `TRAPTEAM_ISO` counts them).

## The game's own tables [V]

`tools/toy_tables.py <disc>` writes `private/trapteam/toy_tables.json` from `character/Init_Setup.bld`'s `ENGLISH.pak` (an IGZ whose text is UTF-16 big-endian rows):

| Table | Row | Count |
|---|---|---|
| villains | `1001,life,ChompyMage,Chompy Mage,5,quest=0` (id, element, model, name, vault index, quest) | 46 |
| toys | `450,air,Typhoon,Gusto,ranger` (type, element, model, name, flags `giant`/`ranger`: a Trap Master) | 116 (SSA, Giants, Swap Force cores, Trap Team's 36 and 16 minis) |
| debug toys | `9990,dark,Sky_Debug_Minion_Core,DEBUG Core`, `409,fire,Sam,Sam Test`... | 18 (not in the catalogue) |
| items, packs | `230,Item_HelpingHand,Hand of Fate`, `305,Item_Location_Mirror,level=327` (the adventure packs are named by their level) | 32 |
| traps | `210,magic,Magic Trap,2,8,11,14,18,21`: the shapes, **variant id = 0x3000 + shape** | 11 |
| variants by year | `474,Legendary,Legendary`, `211,Legendary,Legendary,deco=6` (the deco is the shape it redecorates: picture 0x3400 or deco; Kaos's Ultimate is 0x351F) | 74 |
| Swap Force bots | `1000,air,BoomJet,Jet` (bottoms 1000-1015) and tops 2000-2015 | 32 |

`tools/trapteam_catalogue.py` writes the catalogue's Trap Team part from it: the portraits (358 keys), the 46 villains (`Game::villains`), each trap's shapes. The catalogue's
toys already matched the game's table by type, name and element (5 names differ in spelling: items the game names by their long name; "Deja Vu" the game writes with
accents). **Differences found:** the game lists **61 trap shapes and decorations against the 51** of the public lists the catalogue was made from (15 added, 5 dropped), three numbers
differ by one (Magic Trap 0x3015 not 0x3014; Air 0x300E not 0x300D; Tech 0x3001 not 0x3000), so the catalogue's shape names are [?]: a name stays only where the number matches, the other
shapes are "Shape N"; the game's table gives no shape names. The villains' elements: `death` is Undead, `kaos` (Kaos alone) has no element.

## What is not known [?]
- Where a trap holds its villain: `figures::TrapVillainOffset`, packed `0x10`, settled by a capture in play ([boot.md](boot.md#the-first-capture-2026-11-02-v)). The Toy Box lists the villains a trap can hold by its element and
  shows "Holds <name>" only when `figures::TrapVillainVerified` is true.
- The shapes' names; the villains' display names in other languages (the table is the English text); which of a trap's shapes were ever sold.
- The collection screen's artworks: Trap Team shows no large picture of a toy (`Game::artworks` is empty).

## The launcher's Collection
Arkeyan Launcher reads the same `art.json` and catalogue (`tools/sync_skylauncher.sh`), so for Trap Team it needs: the new `art.json` and `catalogue.json`; the **provider files**
(`alchemy_archive.cpp`: the v8 layout and the name padding; `game_art.cpp`: `streams`; `icons.{h,cpp}`: `VillainPicturePath`; `catalogue.{h,cpp}`: `Game::villains`);
the catalogue id **`trapteam`** (it was `trap_team`; the folder is `TrapTeam`, `datadir.cpp`); a Trap Team disc check at start-up (its XEX hash is in `games/trapteam/game.txt`);
and a sheet for a trap and a villain list the way the Toy Box draws it (`DrawTrapVillains`). Not changed here.
