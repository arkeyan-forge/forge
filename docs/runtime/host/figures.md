# Figures and the Toy Box

> **Purpose:** what is on a figure's tag (format, crypto, progress, levels, each game's areas), which toys each game accepts, and how the figure library and the Toy Box work · **Status:** current (2026-10-31) · **Tags:** [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [TrapTeam] [Xbox]

[V] = verified (test, the title's own code or a run), [?] = unverified. The portal side (protocol, backends, light,
identity, prompt watcher): [portal.md](portal.md). What the title's driver expects:
[portal_title_side.md](../../engines/lux/portal_title_side.md). The collection: [collection.md](collection.md).

## Code

| File | What |
|---|---|
| `runtime/host/figures/figure_crypto.*` | MD5 (RFC 1321), AES-128 (FIPS-197), the CRC-48 of the sector keys. Written from the specifications. |
| `runtime/host/figures/figure_tag.*` | Decrypt/encrypt a tag, read/write the progress area, levels, nicknames. |
| `runtime/host/figures/figure_catalog.*` | Every toy the title accepts: toy type, name, the title's internal name, element, kind; built from the shared catalogue (`resources/collection/catalogue.json`, [collection.md](collection.md)). |
| `runtime/host/figures/figure_data.*` | The 1 KB tag type, its checksum, a blank figure. |
| `runtime/host/collection/*` | The collection shared by every game and the launcher: catalogue, folder, locks, backups, migration, icons ([collection.md](collection.md), [collection_api.md](collection_api.md)). |
| `runtime/host/art/*` | Each game's pictures from the disc: inflate and the `strm` archive's UI image records (`lux_images`), Giants' IGA/IGZ (`alchemy_archive`), Xenos texture decoding, the PNG cache (`game_art`). Shared with Arkeyan Launcher: [game_art.md](game_art.md). Was `figures/figure_portraits.*`. |
| `runtime/host/figures/figure_test.cpp` | `ArkeyanFigureTest [<dump>...]`: crypto, areas, the cross-game round trips ("Data rules" below); `--make <dump> <type>[:<variant>] [xp= gold= hat= ssahat= sfxp= upgrades=<hex> wowpow=]` writes a figure for a run, `--edit <dump> - [the same fields]` changes them on an existing dump, `--same-extra <before> <after>` checks a game's write kept the extra areas, `--roundtrip <before> <after>` checks nothing outside the first pair changed (the portraits are `ArkeyanArtTest`'s), `--roundtrip-later <before> <after>` checks a Giants or Swap Force run kept the identity and both pairs valid while writing the extra areas |
| `runtime/host/toybox.*`, `runtime/host/toybox/{model,draw}.*` | The in-game menu (View + Y or F6; hold View or Tab = the wheel), Dear ImGui draw lists rasterised on the CPU into a 1280x720 overlay. |
| `runtime/host/portal.cpp` | Portal choice (`PortalInit`), place, remove and swap in a slot, `PortalSeen` (the traffic as the title sees it), slot queries for the menu, hotkeys **F7** (next figure of the folder) and **F8** (every figure off). |
| `runtime/gpu/gpu.cpp` `SetOverlay` | One extra blended draw in `gpu::Present` (`OverlayPS` in `present.metal`/`.hlsl`). |

Public sources: Dolphin's and RPCS3's Skylanders code were read (GPL, not copied) for the block layout and the
checksums; everything was checked against a figure the title itself wrote (below).

## Tag format (MIFARE Classic 1K dump, 64 blocks of 16 bytes)

- Block 0: serial (4 bytes), BCC (xor), SAK/ATQA `81 01 0F`. Block 1: toy type LE16 at 0x10, trading card id
  0x14..0x1B, variant LE16 at 0x1C, CRC-16/CCITT-FALSE of 0x00..0x1D at 0x1E (LE). [V]
- Sector trailers (blocks 3, 7, ... 63): key A (6 bytes), access bits `0F 0F 0F 69` (sector 0) or `7F 0F 08 69`,
  key B zero. Key A of sector 0 is `4B 0B 20 10 7C CB` [V] (matches the public constant); sectors 1-15 use the
  CRC-48 (poly 0x42F0E1EBA9EA3693, init 2·2·3·1103·12868356821) of serial + sector number [? not compared with a
  real dump; the title never sees key A: real portals authenticate, the emulated one serves the bytes].
- Blocks 8..63 except trailers: AES-128-ECB, key = MD5(block 0 ‖ block 1 ‖ block number ‖
  `" Copyright (C) 2010 Activision. All Rights Reserved. "`). An all-zero block is left as is (never written).
  [V: the constant is in the title at 0x8264398C (sub_82643958); the title's portal log (`SKY_PORTAL_LOG=1`) shows it
  decrypting blocks this code encrypted to the same plaintext.]
- Two copies of the progress ("areas"): blocks 8-16 and 36-44 (7 data blocks each, trailers in between). Byte 0x09 of
  the first block is a sequence number; the newer valid copy is read (A unless B = A + 1 mod 256). [V on the title's
  own writes: it wrote both areas, sequence E7/E8.]
- Area checksums, CRC-16/CCITT-FALSE, little-endian [V] (recomputed on the title-written figure):
  - +0x0E: the first block with `05 00` in place of the checksum;
  - +0x0C: +0x10..+0x2F and +0x40..+0x4F (the trailer is skipped);
  - +0x0A: +0x50..+0x6F and +0x80..+0x8F, then 0xE0 zero bytes.

## Progress fields (offsets from the area's first block)

| Offset | Field | Status |
|---|---|---|
| 0x00 (24 bits) | experience | [V] 9000 written by the menu: the HUD shows level 6, the title writes it back |
| 0x03 LE16 | gold (max 65000) | [V] 3000 written by the menu: the HUD shows 3000 |
| 0x05 LE32 | play time in seconds | [?] |
| 0x09 | area sequence | [V] |
| 0x10 LE16 | upgrades (bit field: the path, the bought upgrades, the Soul Gem; ["Upgrades"](#upgrades-ssa-giants)) | [V] (both games' menus) |
| 0x13 | platforms the figure was used on; the title wrote 02 (Xbox 360) | [V] (written by the title) |
| 0x14 LE16 | **Spyro's Adventure's hat** (`ssaHat`). SSA has hats too (its English text names about 40: Combat Hat, Napoleon Hat...), in the order of Giants' list below. Giants shows it when its extra area has no hat of its own | [?] (SSA writing it not observed); the library keeps it unless asked |
| 0x16 bit 0 | "the extra area's experience counts": the title sets it on a figure's first use in Giants | [V] seen set by the title on every figure it started |
| 0x18..0x1F | owner id, written by the title on first use (`ff37837e ffc3067f` here) | [V] (written by the title) |
| 0x20..0x2F, 0x40..0x4F | nickname, 16 UTF-16LE units | [V] "Ace" written by the menu: the HUD shows it |
| 0x50..0x55 | last placed: minute, hour, day, month, LE16 year | [V] (written by the title: 2c 08 17 0a ea07) |
| 0x56 LE32 | heroic challenges (one bit each; Giants' `pp_ChallengeLevelsComplete`, 32 bits here and 24 more in its extra area) | [V] the field (Giants' save table); which bit is which challenge [?] |
| 0x5A LE16 | hero points | [?] |
| 0x60..0x65 | last reset date | [V] (written by the title) |
| 0x5F | set by the title on a figure's first use (all 37 characters of the 2026-10-27 runs); meaning not known | [V] the write, [?] the meaning |
| 0x80..0x8F | unknown; kept as is | |

Levels (cumulative experience): 0, 1000, 2200, 3800, 6000, 9000, 13000, 18200, 24800, 33000 (level 10, Spyro's
Adventure's cap), then **Giants: 42700, 53900, 66600, 80800, 96500 (level 15)**, then **Swap Force: 113700, 132400,
152600, 174300, 197500 (level 20)**.
[V] 9000 = level 6 and 96500 = level 15 match the HUD; the disc says "Level up any Skylander to level 15" and "Amass
65,000 Treasure". [?] Spyro's Adventure's and Giants' steps in between are public values. [V] Swap Force's table is
the game's own: see "Swap Force's level cap" below.

`WriteStats` starts from the newest valid area (unknown bytes kept), writes the fields, and stores the result in both
areas with sequence numbers s+1 (A) and s+2 (B), checksums recomputed, then re-encrypts. A blank figure stays blank
until edited.

The title accepts such a figure [V] (run of 2026-10-23): a never-played Gill Grunt edited by the menu (9000 XP, 3000
gold, nickname, no owner) was read from area B, shown on the HUD with those values, and written back by the title to
area A with sequence 3, the same values, its owner id, platform byte 02 and the dates (last placed and last reset =
the time it was first used). After the Toy Box re-placed that figure, the HUD showed "Gill Grunt" instead of the
nickname although both areas held it [?] (unexplained).

## Upgrades [SSA] [Giants]

The field at 0x10 (16 bits; the save code's word 4 of the area's data, bits 0-15) [V], the same in both games: a figure
upgraded in one is upgraded in the other.

| Bit | What | Price |
|---|---|---|
| 0 | a path is chosen | |
| 1 | the second path (clear: the first) | |
| 2-5 | the four basic upgrades, in the game's order | 500, 700, 900, 1200 |
| 6-8 | the chosen path's three upgrades (both paths use these bits: bit 1 says which) | 1700, 2200, 3000 |
| 9 | the Soul Gem's ability | 4000 |
| 10-11 | Giants: a 2-bit field (limit 3), continued in the extra area (+0x06 bits 6-8); the game clears both on a figure's first use [V]; its meaning is not known [?]. **Not the Wow Pow**: that one is the extra area's +0x06 bit 0 (below) | |
| 12-15 | not used | |

How it was found:
- **Giants** [V]: `character/Init_Setup.bld` sets up the figure's save fields (`Save - Setup Portal Save Param`: value
  index = 32-bit word of the area's data with the trailers skipped, bit index, bit count, limit). Word 4 bits 0-9 are the
  set `pp_Flags1_UpgradeBits` in its order: `pp__UpgradePath`, `pp__UpgradePath_Choice`, `pp_Upgrade(1)` ...
  `pp_Upgrade(8)`; the macro `Minion - Get Upgrade Details` reads upgrades 1-4, then 5-7 of path A or B by
  `Save - Get/Set Minion Upgrade Path` (path chosen: bit 0; which: bit 1), then upgrade 8 (the Soul Gem) and, for a
  repose figure, the Wow Pow. The same table places every other field where this page has it (experience, gold, play
  time, hat at 0x14, owner, nickname, dates, challenges, hero points; in the extra area Giants' experience at +0x03 and
  hat at +0x05), which checks the reading.
- **Spyro's Adventure** [V]: `global.str` holds one record of 0x54 bytes per upgrade (index, name and description keys
  = CRC-32 of the text keys of `text/<language>/global.lxb`, price): per Skylander 4 basic (index 0-3), "Choose Your
  Path" (index 8, price 0), 3 + 3 path upgrades (index 4-6 twice), the Soul Gem (index 7), after a header with the
  toy's and the two paths' names.
- **Runs** (2026-10-26, isolated, emulated portal, `ArkeyanFigureTest --make <dump> 16 xp=33000 upgrades=<hex>`): a Spyro
  with `0x0004` shows Long Range Raze bought and the other basic upgrades not, in Giants (Back > Skylander Info >
  Upgrades) and in Spyro's Adventure (Back > Skylander Info > Powers); with `0x027F` SSA lists the four basic upgrades,
  **Stun Charge** (the second path's first upgrade) and Spyro's Earth Pound (the Soul Gem); a Series 2 Spyro with
  `0x067F` in Giants shows the second path's upgrade and the Soul Gem bought. Screenshots: `private/proof/` (local).
- **The Wow Pow** [V] (Giants, Series 2 figures, 5000 gold): **the extra area's +0x06 bit 0** (its word 1 bit 16 in
  `Init_Setup.bld`'s numbering), not the upgrade field. Proven by buying it (2026-10-27, isolated run, a Series 2 Spyro
  `16:1801` at level 15 with 60000 gold and every other upgrade, `SKY_LEVEL=level_hub_2_stage_1`, `SKY_PRESS=` A every 4 s from 8 to 44 s, `lup:50:2` (Auric's
  scene), `lup:66:3,lleft:70:2,lup:73:3` (into Persephone's cabin), `y:80,a:86,a:91,left:93.5,a:95` (talk, Upgrade, the
  Wow Pow, Yes)): the game wrote the figure back with
  55000 gold and, in the extra area, only +0x06 `00` -> `01` (and its sequence and checksum); the upgrade field stayed
  `03FF`. A figure made by `ArkeyanFigureTest --make <dump> 16:1801 xp=96500 upgrades=03FF wowpow=1` shows the Wow Pow
  bought in the Upgrades menu (Back > Skylander Info > Upgrades: its card in colour; with 0 it stays dark), on its first
  use too: the game keeps that bit. Screenshots and the before/after dump: `private/proof/gre/` (local).
  - Bits 10-11 of the upgrade field and the extra area's +0x06 bits 6-8 are another Giants field (2 + 3 bits, limit 3 in
    the first area): the game clears them on a figure's first use and keeps them afterwards; setting them (1, 2, 3; 1, 7)
    shows nothing in the menu (runs of 2026-10-26). The library keeps them as they are.
  - `FigureStats::wowPow` reads and writes +0x06 bit 0. The editors (the Toy Box, Arkeyan Launcher) work on
    `collection::EditorBits` (the field with bit 10 standing for the Wow Pow) and store with `StoreEditorBits`
    ([collection.md](collection.md#upgrades)); `ArkeyanFigureTest` and `ArkeyanCollectionTest` check both.
  `ArkeyanFigureTest --edit <dump> - [fields]` changes fields of a game-written dump for such runs.
- **Names, descriptions, prices**: per Skylander, read from the disc (Giants: each character archive's
  `Portal - Define Minion` call, `unlock names` / `unlock descriptions` / `upgrade path N name|desc`, and
  `global.bld`'s `Minion_UpgradeCosts`; SSA: the records above) by `tools/toy_tables.py` (a dump into `private/`) and,
  at install, `art::Extract` (`<data>/<Game>/upgrades.json`, read by `collection::Upgrades`:
  [collection.md](collection.md#upgrades)). English and French; SSA has no path descriptions in data (their key is not
  in any file [?]). The texts keep the game's button marks (`{`, `[`, `]` in Giants, `&ICON(0,ICON_ENTER)` in SSA).
  Spyro's Adventure writes "Longe Range Raze".
- **Heroic challenges** (0x56): not in these tables. SSA's titles are in `text/<language>/challenge_level_NNN.lxb`
  (one file per challenge), Giants' in `global.bld`'s `Challenge_Titles`; which bit is which challenge is not checked
  [?].

## Giants' additions: the extra areas, level 15, hats [Alchemy] [Giants]

Giants keeps its part of the progress in a **second pair of areas**, written next to the first pair. Spyro's Adventure
never reads or writes them, so a figure made for it stays what it was until Giants uses it. [V: the title wrote them on
every figure placed in it; a figure with an extra area made by our code was accepted and played at level 15 with its
hat, and the same figure with a wrong checksum was refused, see below.]

- **Where:** blocks 17, 18, 20, 21 (extra area A) and 45, 46, 48, 49 (extra area B): 4 data blocks each, the trailers
  (19, 47) between them, encrypted like the others (all four blocks, zero or not, once the area is in use). The first
  pair's last block (16, 44) sits in the same sector as the extra area's first two.
- **Kept as 64 bytes** (the data blocks one after the other):

| Offset | Field | Status |
|---|---|---|
| 0x00 LE16 | checksum: CRC-16/CCITT-FALSE of the 64 bytes with this field replaced by `06 01` | [V] the title wrote `5B54` (sequence 1) and `D730` (sequence 2) for an empty area, and refused a figure whose checksum was wrong |
| 0x02 | sequence number (A: s+1, B: s+2, the newer copy is read, like the first pair) | [V] |
| 0x03 LE16 | **Giants' experience**, 0..63500: levels 11-15; the first area keeps the first 33000 (total = both) | [V] 33000 + 63500 = level 15 on the HUD |
| 0x05 | **Giants hat** (`hat`; 0: none, the first area's shows) = the disc's `misc/StoreItem_Hat_NNN` + 1 | [V] 6 shows `Hat_005`, the pirate hat (run of 2026-10-26, with 0x14 = 0: the hat is read from here); the disc has 52 hats (000-045, 063, 071, 081, 082, 090, 092), named in the catalogue's `hats` [names ?] |
| 0x08 LE32 | **Swap Force's experience** (`swapForceExperience`, levels 16-20, 0..101000; 101000 = level 20): the part above Giants' 96500, written and counted by Swap Force, kept by the other two | [V] the disc's table (96500 + 101000 = 197500, "Swap Force's level cap" below) and Swap Force's own runs; first named in public (tsapZ/skylanders-portal, MIT: facts only); Spyro's Adventure and Giants keep it byte for byte |
| 0x0C, 0x0E | hat (Swap Force, Trap Team), hat (SuperChargers on, number minus 256) | [?] public, kept as they are |

- **Reading:** `ReadStats` adds the extra experience only when the first area's `0x16` bit 0 is set, takes the hat
  from the extra area when it is set (else the first area's, `ssaHat`), and `ReadExtraState` tells Blank (none, every
  figure Spyro's Adventure made), Valid or Damaged (a damaged extra area counts as none).
- **Writing:** `WriteStats` writes the extra pair only when the figure needs it (Giants' experience, a hat other than
  the first area's, Swap Force's experience) or already has it (it starts from the newest valid copy, so the bytes it
  does not know stay), and then sets `0x16` bit 0. `hat` goes to the extra area only and `ssaHat` to 0x14 only, so
  each game keeps its own hat (before 2026-10-26 the library wrote the Giants hat into 0x14 too).
  Experience is clamped to 33000 + 63500. **Spyro's Adventure is unchanged**: 1200 writes within its limits hash to the
  same bytes as the library gave before Giants (a regression test, `ArkeyanFigureTest`).
- **API:** `FigureStats::giantsExperience`, `ssaHat`, `hat`, `swapForceExperience`, `TotalExperience`,
  `SetTotalExperience` (splits at 33000), `ExperienceFor(stats, cap)` / `SetExperienceFor(stats, cap, xp)` (what a game
  with that cap counts and edits: with cap 10 only the first area's part), `LevelForExperience(xp, cap)`,
  `LevelCap(game)` (10 for `ssa`, 15 for `giants`, 20 for the others, "Trap Team's figures" below), `MaxLevelGiants`, `MaxLevelSwapForce`.
  `TotalExperience` is the three parts; `ExperienceFor(stats, cap)` is what a game counts (cap 10: the first part; 15:
  the first two; 20: all three) and `SetExperienceFor` edits that and nothing above it (Giants' editing keeps Swap
  Force's part; `SetTotalExperience` fills 33000, then 63500, then 101000). `WriteStats` clamps the parts to 33000,
  63500 and 101000. The Toy Box and the launcher's Edit stats use the game's cap (`Model::LevelCap`, the launcher's
  game selector); the collection shows a figure's real level (up to 20) whatever the game.

### Swap Force's level cap: 20 [Alchemy] [SwapForce] [V]
- **The game's table.** Every hero's character data on Swap Force's disc (the 81 `*_characterdata.igx` of the hero
  archives, in the toy archives' XML form; the text kept in `private/`, never in git) holds the same map from level to
  experience: 20 entries, level 1 = 0, 2 = 1000, 3 = 2200, 4 = 3800, 5 = 6000, 6 = 9000, 7 = 13000, 8 = 18200,
  9 = 24800, 10 = 33000, 11 = 42700, 12 = 53900, 13 = 66600, 14 = 80800, 15 = 96500, **16 = 113700, 17 = 132400,
  18 = 152600, 19 = 174300, 20 = 197500**. The first fifteen are Spyro's Adventure's and Giants' table, so the cap is
  **20**; the class that carries the map also has a `_maxLevel` field, set in none of the 81 files (so the map's last
  level is the cap). 197500 = 33000 + 63500 + 101000: the three parts of the figure.
- **The field, as the game writes it.** In a run the game took a figure made by our tools with 132400 experience (33000,
  63500, 35900 in the extra area's +0x08) and wrote it back after 87 s of play with **+0x08 = 35921** (the 21 it earned),
  the first two parts and the extra area's other bytes unchanged. So Swap Force adds what it earns to +0x08 after the
  first two parts are full, and the library writes it the same way (`SetTotalExperience`).
- **Seen on the HUD** (emulated portal, offscreen, a Scratch made with `ArkeyanFigureTest --make <dump> 3000 xp=<total>`):
  197500 shows **"MAX 20"** with the gold, 132400 shows **17**. Giants at 197500 shows **Level 15, MAX** and writes the
  figure with the extra area byte-identical; Spyro's Adventure plays the same figure (an Eruptor with 197500) and
  writes only the first pair, the extra area byte-identical (`--same-extra`).
- Not seen: the HUD of Swap Force for 20 reached by play, and the other levels' numbers (16, 18, 19) by eye; the
  thresholds come from the table, and 17 matched.
- **LightCore figures carry no extra data**: the same tag format, the base toy's type (Jet-Vac 100, Pop Fizz 108 and the
  SSA characters Warnado 2, Prism Break 7, Eruptor 9, Wham-Shell 13, Drobot 20, Hex 29) with **variant `0x1206`**;
  the catalogue names them and `CreateFigure(100, 0x1206)` makes a blank one. [V] Giants accepts a blank one and a
  leveled one and plays them (a different, glowing body, level and gold kept). The light is not data on the figure:
  [portal.md](portal.md#portal-light).

## Data rules: one figure, every game [Lux] [Alchemy]

**A toy's identity.** One dump = one physical figure: its serial (block 0), toy type and variant (block 1, under the
header checksum) say what it is, whichever game made or last played it; `collection.json` adds only what the tag
cannot hold (label, favourite, a same-tag edition). The games never write block 0 or 1, so a figure stays the same toy
across every game [V] (neither title wrote them in any run). Its progress is split by game:

| Area | Blocks | Who writes it | What |
|---|---|---|---|
| first pair (A, B) | 8-16, 36-44 | Spyro's Adventure and every later game | experience up to 33000 (level 10), gold, play time, upgrades, `ssaHat`, owner, nickname, dates, hero points, challenges; `0x16` bit 0 = "count the extra area" |
| extra pair (A, B) | 17-21, 45-49 | Giants and later; **never Spyro's Adventure** | Giants' experience (levels 11-15), Giants' hat, the Wow Pow (+0x06 bit 0); Swap Force's experience (101000, level 20) [V] and later games' hats [?] |
| block 6 (plain sector 1) | 6 | Swap Force [SwapForce] [V] (never SSA or Giants), SuperChargers, Imaginators | the toy's **owner**: `01` and four bytes, the same on both halves of a swapper written together [?] ([history.md](../../../games/swapforce/docs/history.md), "Figures of each type"; "Owner" below) |
| later games' areas | traps mapped below; vehicles and crystals not mapped | Trap Team, SuperChargers, Imaginators | [V] trap layout; [?] game acceptance, vehicles, crystals |

**How a later game keeps an older game's data.** Each area pair is two copies with a sequence number: a writer reads
the newer valid copy, changes only its own fields, keeps every byte it does not know, and writes the other copy with
the sequence + 1 and fresh checksums (three CRC-16 per first area, one per extra area; above). So:
- **Spyro's Adventure after Giants** [V] (runs of 2026-10-26): it reads only the first pair, sees at most 33000
  experience (**level 10 on its HUD**), and writes the first pair back with the next sequence number, keeping `0x16`
  bit 0; the extra areas are not touched (**byte-identical**), so the figure is still level 15 with its hat in Giants.
- **Giants after Spyro's Adventure**: it writes the first pair as SSA does (same fields, same rules) and adds the extra
  pair; a figure SSA made has none (Blank) until Giants first uses it [V].
- **Our library** writes like the games: `WriteStats` starts from the newest valid copies, and in Spyro's Adventure
  (cap 10) the Toy Box and the launcher change only the first area's experience (`SetExperienceFor`), never Giants'
  part. A figure made for SSA and edited within its limits stays byte for byte what SSA expects (the hash test above).

**Verified** [V]:
- `ArkeyanFigureTest` round trips (`TestRoundTrips`): create, SSA writes (the title's own write rules, coded separately from
  the library), Giants writes level 15 and a hat, SSA writes again: the extra areas are byte-identical, every checksum
  valid, SSA's level 10 and Giants' level 15 read back, both catalogues accept it; the same with a LightCore and with
  Swap Force's experience (level 20, 101000); the 20-level table, the caps, `ExperienceFor` and `SetExperienceFor` per
  game and the clamp of the third part; SSA's Toy Box edit keeps Giants' part; `ssaHat` and `hat` independent.
- **Real game, 2026-10-26** (fast recipe, emulated portal, `--make` figures): Eruptor at level 15 with Giants' hat 6, and
  Gill Grunt with also 101000 Swap Force experience (written before the field was read from Swap Force's disc). SSA showed **level 10**, the gold, played the figure (no hat: SSA
  has none on it), and wrote the figure (owner, dates, sequence 2 to 3); after it, `--same-extra` found the extra areas
  byte-identical and valid, and the library read level 15, hat 6 and the Swap Force experience back. Giants plays such a
  figure at level 15 with its hat (the run above). `tools/test_all.sh --long` repeats the Eruptor run.
- **Swap Force, 2026-10-27** [SwapForce]: it plays SSA, Giants and its own figures (cores, LightCores, swappers as two
  tags, magic items) and writes the first and extra pairs like Giants, plus block 6; `ArkeyanFigureTest` reads every
  figure it wrote as valid. Each half of a swapper is a figure of its own with its own progress
  ([history.md](../../../games/swapforce/docs/history.md), "Figures and the portal").
- Not seen yet: SSA writing gold or experience it earned in play on such a figure (the runs did not earn any; the same
  write path as the first-use write, [?]).

## Trap Team's figures [TrapTeam]

[V] `ToyKind::Trap` covers toy types 210-220. `ToyKind::TrapMaster` has character progress; traps use their own
progress format, even though `HasProgress` remains false for the character editor. Eon's Elite uses the base toy's
identity with a variant. The character codec and its existing byte-regression test remain unchanged.

[?] The character level cap is 20 for Trap Team and later titles; the existing 20-level table is verified from Swap
Force [V]. This trap work does not establish Trap Team's character experience rules.

### Trap progress layout

[V] Static local research establishes a single pair of copies, each with **21 data blocks** (336 packed bytes),
starting at physical block 8 (A) or 36 (B). There is no character/extra-area split. Trailers are skipped:

| Copy | Physical data blocks, decimal | Status |
|---|---|---|
| A | 8,9,10,12,13,14,16,17,18,20,21,22,24,25,26,28,29,30,32,33,34 | [V] |
| B | 36,37,38,40,41,42,44,45,46,48,49,50,52,53,54,56,57,58,60,61,62 | [V] |

[V] Each copy spans 432 physical bytes including six trailers. Packed offsets count only data bytes. The header
and records are defined by the crystal save schema and native packing/serializer routines; the evidence and
executable/script locations remain in the local, untracked research report.

| Field | Packed byte offset | Physical A / B | Status |
|---|---|---|---|
| Data format | `0x00`, 8 bits | `0x080` / `0x240` | [V] field; [?] accepted initialization values |
| Stored villain count | `0x01`, 8 bits | `0x081` / `0x241` | [V] six record slots |
| Special-look preload ID | `0x07`, 8 bits | `0x087` / `0x247` | [V] separate from evolution; [?] variant/deco restrictions |
| Sequence | `0x09`, 8 bits | `0x089` / `0x249` | [V] modulo-256 adjacent ordering |
| Selected villain | `0x10`, 8 bits | `0x090` / `0x250` | [V] aliases record 0's ID |

[V] Record `i`, 0 through 5, begins at packed `0x10 + 48*i` (value index `4 + 12*i`). Its 36 described bytes are:

| Record byte offset | Field | Status |
|---|---|---|
| `+0` | storage ID, 8 bits; 0 empty, ordinary IDs 1-46 correspond to resources 1001-1046 | [V] |
| `+1`, bit 0 | quest complete / evolved; same storage ID | [V] |
| `+2` | hat, 8 bits | [V] |
| `+3` | bauble, 8 bits | [V] |
| `+4..+35` | name, sixteen 16-bit units, little-endian | [V] |
| `+36..+47`, plus the final 32 packed bytes | unspecified payload, preserved by the codec | [?] meaning |

| Record | ID A / B | Quest bit A / B | Status |
|---:|---|---|---|
| 0 | `0x090` / `0x250` | `0x091` / `0x251`, bit 0 | [V] |
| 1 | `0x0D0` / `0x290` | `0x0D1` / `0x291`, bit 0 | [V] |
| 2 | `0x110` / `0x2D0` | `0x111` / `0x2D1`, bit 0 | [V] |
| 3 | `0x150` / `0x310` | `0x151` / `0x311`, bit 0 | [V] |
| 4 | `0x190` / `0x350` | `0x191` / `0x351`, bit 0 | [V] |
| 5 | `0x1D0` / `0x390` | `0x1D1` / `0x391`, bit 0 | [V] |

[V] Selecting a stored villain moves its full record into slot 0: quest, hat, bauble and name move with the ID.
The count records stored villains, while only the first record is selected/playable. The former guessed LE16
field in the character extra area was wrong and has been removed.

[V] Three little-endian CRC-16/CCITT-FALSE values use polynomial `0x1021`, initial `0xFFFF`, no final XOR:

| CRC at packed offset | Input | Status |
|---|---|---|
| `0x0A` | packed `0x40..0x14F`: seventeen actual data blocks (272 bytes) | [V] |
| `0x0C` | packed `0x10..0x3F`: the next three data blocks (48 bytes) | [V] |
| `0x0E` | first sixteen bytes, substituting `05 00` at `0x0E..0x0F` | [V] |

[V] Seal the two body CRCs before the header CRC. The character codec's 224 zero-padding bytes must instead be
actual trap payload. Every non-trailer block of a used copy is encrypted, including zero plaintext blocks, using
the existing identity/physical-block MD5 and AES primitives. Identity, sector keys and header CRC use the existing
figure rules. Copies at different physical blocks have different ciphertext.

[V] `ReadTrapStats` reads the newest checksum-valid copy (B when B sequence = A + 1 modulo 256, otherwise A).
`WriteTrapStats` preserves the newest payload's reserved bytes and high quest bits, the identity, and every trailer.
It validates count, unique nonzero IDs and the selected/record-0 alias, rejects a damaged pair, and prepares the
inactive copy first, then the former active copy, with sequences from the tag plus 1 and 2. The tag's actual
sequence takes precedence over the caller's snapshot. The API produces a complete offline image.

[V] **A capture in play** (2026-11-02, Sheep Creep into an empty Life Trap; [boot.md](../../../games/trapteam/docs/boot.md#the-first-capture-2026-11-02-v)):
the game wrote copy A only, sequence 2 over copy B's 1, data blocks in order and the header block (8) last: count 1,
`figures::TrapVillainOffset` (packed `0x10`, physical `0x090`) = 20, and packed `0x132` changed from `0x01` to `0xA3`
(meaning unknown [?]; the codec keeps the newest copy's byte). `ReadTrapStats` reads it as written; `TestTrapCapture`
rebuilds those bytes and checks the reader and an offline `SelectTrapVillain` + `WriteTrapStats` of the same capture.

[?] This does not prove physical portal write ordering or interrupted-write recovery. The guest alternates copies
transactionally; hardware-level write scheduling still requires a trace. Offline callers must remove/unlock a tag
before editing and remove/re-place it afterward: the emulated portal caches placed bytes [V].

[V] `CreateTrap(type, shape, villain, evolved)` uses this codec for populated traps and leaves an empty trap blank.
`ReadStats` exposes only a compatibility selected-ID view for traps; character `WriteStats` refuses them.
`TrapVillainVerified` is **true** since the game accepted a crafted trap (below).

[?] The new-image candidate is Water toy 211, variant `0x3002`, count 1, Gulper storage ID **9**, preload 0,
data format 0, sequences A=1/B=2. Format 0 is a candidate initialization value, not proved guest acceptance.
The evolved candidate changes the same record's quest bit, retaining ID 9. Special looks are distinct from
quest evolution [V]; no special-look authoring policy is established [?].

[V] From the clone root, this command exclusively creates two 1 KB dumps with the same identity, format and
sequences in the named scratch folder; it refuses existing output files. Dumps are ignored by git:

```sh
build/ArkeyanRuntime/ArkeyanFigureTest --make-trap-pair .scratch/trap-villain-candidates
```

[V] Files are `water-gulper-normal.sky` and `water-gulper-evolved.sky`. `ArkeyanFigureTest` checks normal/evolved and
six-record round trips, all three CRCs of both copies, full used-block encryption, reserved bytes, copy corruption
and fallback, sequence wrap in both directions, invalid edits, and character byte stability. These are host tests.

[V] Any trap, empty or with any villain: `ArkeyanFigureTest --make-trap <out> <type>:<shape hex> [villain=<1001-1046 or
1-46>] [evolved]` (`figures::CreateTrap`; e.g. `--make-trap .scratch/t.sky 211:3002 villain=1009`). The coverage runs
make every trap type and every villain (each in a trap of its element) this way ([testing.md](../../getting-started/testing.md),
"Coverage of every game").

[V] `SelectTrapVillain(stats, id)` makes a villain the selected one as the game does (its record to slot 0, the displaced one
to the slot it left, the other records kept; an unknown villain takes a free slot, or replaces slot 0 when all six are used).
`FigureStats::villainEvolved` is the read-only view of slot 0's flag. A catalogue villain's ID minus `VillainResourceBase`
(1000) is its storage ID. `collection::SetTrapVillain(path, id, evolved)` edits a dump (refused while the dump is in use).

### Acceptance in the game (2026-10-31) [V]

[V] Offscreen, scratch collection, `SKY_PORTAL_PLACE` with Gusto and `water-gulper-normal.sky` or `-evolved.sky`
(`ArkeyanFigureTest --make-trap-pair`), `level_313_tutorial`, a virtual pad (`SKY_GLYPHS_SCRIPT` `btn:lt`), snapshots by
`SKY_GPU_SNAPSHOT_SECONDS`. The game parsed both traps (Gulper played), so the layout, the three CRCs, the ciphertext and the storage ID 9 are accepted.
- **Normal trap:** the HUD stays on Gusto with the trap's picture; **LT** at 100 s: "The Gulper" plays (blue, with its
  own bar); **LT** again 12 s later: Gusto is back. No capture and no defeated villain.
- **Evolved trap:** the same, with the Gulper **green** instead of blue (the evolved material). The word "Evolved" was not
  seen: Back held 1.5 s opens the host's quick-swap wheel, Start the pause menu, and neither shows the tag's display
  string [?] (it may need the villain vault or the trap screen).
- **Not run:** reload after a game write, one corrupted copy, sequence wrap, negative cases (wrong element, count above six,
  unsupported format), and the combat effects of the evolved flag [?].

[?] Negative cases still need guest observation: wrong element, count above six, unsupported format, absent or
wrong-namespace ID, inconsistent record membership, and special-preload/variant combinations. Host validation
is not evidence the game repairs them. Ignored audio/light commands are separate from tag storage [V: host
backend]; whether a deployment waits on them remains unverified [?].

## SuperChargers' vehicles [Alchemy] [SuperChargers]

The owner's SuperChargers dumps are all blank (never played), so the vehicle's fields come from the game: two isolated runs
(2026-11-04, `private/research/run_sc.sh`, snapshots of the collection every 20 s): a new game with Fiesta and a blank Crypt
Crusher to the end of the rift tunnel ride, then the same with that Crypt Crusher edited to 500 gearbits and frames of the HUD.

- **The character layout** [V]: the first pair (8-16, 36-44) and the extra pair (17-21, 45-49), the same checksums and sequences;
  the library reads and writes a vehicle as a character (`ReadStats`, `WriteStats`). Block 6 gets Swap Force's `01` and four bytes.
- **First use** (181 s, when the game reads the vehicle): owner, platform `02`, the dates, `0x14 = 1`, `0x16` bit 0, and **0x4E =
  0x33** (past the nickname's place) in the first area [V] (its meaning [?]); the extra area with **+0x03 = 9** and **+0x06 = 9** [V] (meaning
  [?]). The play time (0x05) grows while it is driven. Fiesta, a SuperCharger, got the same first-area writes, no extra fields.
- **Gearbits: the extra area's +0x08** [V]: 0 after the first use, then 4, 13, 42, 109, 150 during the ride; with 500 written by our
  tools the game went on from it (518, 524, 539, 576, 630, 670) and the HUD showed **576** at 300 s, the value it wrote at 320 s.
  That is where a character keeps Swap Force's experience: `figures::Gearbits` / `SetGearbits` name it for a vehicle
  (`IsVehicleType`: 3200-3299; clamped to 65535 [?]); `collection::SetGearbits` refuses a vehicle the game never used.
- **`WriteStats` keeps the vehicle's bytes** [V]: the nickname's place (0x20-0x2F, 0x40-0x4F) is rewritten only when the nickname
  changes (before 2026-11-04 every write zeroed it, and with it the vehicle's 0x4E). `ArkeyanFigureTest <dumps>` checks, for every valid
  character dump, that a rewrite keeps each byte of the newer copy but the sequence and checksums.
- **The garage's upgrades: the first area's upgrade field (0x10)** [V] (2026-11-05, run `scL2`): bits 0-4 the shield's five
  levels, bits 5-9 the weapon's five, one bit a level. Crypt Crusher written by our tools with `0x0023` shows **Shield 2, Weapon 1**
  in the game's pause menu (My Team, the left stick to the vehicle, Upgrades: two gauges of five), and the game's own write of the
  tag (play time, dates, sequence 9) keeps `0x0023`. `figures::VehicleShieldLevel` / `VehicleWeaponLevel`; the Toy Box card says
  "Shield 2 · Weapon 1" (`collection::LevelText`), the sheet "Gearbits: N · shield 2/5 · weapon 1/5 · driven <time>" (run `scT2`: the Vehicle 1 list shows "Crypt Crusher, Shield 2 ·
  Weapon 1"; `ArkeyanCollectionTest`). The
  owner's 90 SuperChargers dumps are all blank (`ArkeyanFigureTest`): nothing on them to read.
- **Where the game changes them**: the garage of the Academy. The pause menu's My Team shows the vehicle (Blueprint: the mods
  owned per category, Upgrades: the two gauges, Ownership, Reset) but buys nothing there (its only hint is Back; A and a held A
  change no byte, `ArkeyanFigureTest --diff`). A save just after the intro (quest "Talk to Flynn") was used; the garage was not reached,
  so a purchase by the game itself is not seen [?]. The garage is the Academy's Upgrade Vehicle Station, opened by the story after
  Chapter 1, for a SuperCharger only ([toybox.md](../../../games/superchargers/docs/toybox.md#the-garage-2026-11-05-found-on-the-disc-not-reached-in-a-run)).
- **Not mapped** [?]: the mods (performance, specialty, cosmetic, horn: the Blueprint's four lists; owned per vehicle or per save
  not known), the vehicle's level (the HUD's "1"), 0x80+2 (a counter both figures advance at each write), +0x03 and +0x06.
- **Research tool**: `ArkeyanFigureTest --diff <before> <after>` lists every decrypted byte two tags differ in (trailers left out).

## Owner (block 6) [Alchemy] [SwapForce] [SuperChargers] [Imaginators] (2026-11-11, Arkeyan Launcher issue #28)

**What it is** [V]. A game that registers a toy writes its player's owner in block 6 (plain): a kind byte, then the id,
least significant byte first: kind `01` a 4-byte id the game made for an offline profile, kind `80` the Xbox Live account's
8-byte XUID (SuperChargers' writer `sub_82D81920`; another platform's kinds [?]). Imaginators writes kind `06` and its
4-byte id (run `imgown`: `[portal] slot 0's toy: owner kind 06, id 00000000E7780C73`). Trap Team has no owner (no such field in
its code). SuperChargers' tag states (`ePTS_OwnerIdLoad`, `OwnerIdReady`, `WriteOwnerId`) read it after the header: an
empty one gets the player's owner (`W` of block 6, then a `Q` to check it), another owner is left as it is.
`host/figures/tag_owner.h` decodes it; every portal kind logs it when the game reads it and it changed:
`[portal] slot 0's toy: owner offline 1F3AB27A` (`kernel/portal.cpp`; `ArkeyanFigureTest`, `TestTagOwner`).

**What it changes** [V] (SuperChargers, runs `live1`-`live3`, `fix2`). The game's collection (pause menu, Portal Master >
Collection > Skylanders or Vehicles: a count per element) lists the toys registered to the player. A toy owned by
someone else plays normally (its progress is written) but is not counted, and its page (pause menu, My Team) offers
**Ownership** ("Are you sure you want to take ownership of this toy?"): Yes writes the player's owner and the toy joins the
collection (High Volt with owner `01 11 22 33 44`: Tech 0, then 1). A toy keeps a place in the collection once counted.

**The bug, fixed in 0.9.6** [V]. The player's offline id (`offline_owner_id` in `SUPERCHARGERS_OPTIONS`) was made anew at
every start: the game fetches it once, at "press start" (`Owner_Fetch`, `sub_82D789B0`, from `sub_82EF6D00`), before the
options save is read, so the lookup misses, a random id is made, and the save's id is never read back (the save keeps its
first id; seeds `7AB23A1F`, then `724C5193`, `AA1E46F0` in the next sessions). Every toy registered in an earlier session was
someone else's: "Ownership" on the player's own toys, and the new owner written over them. Imaginators fetches its id from
its save's loading instead (`sub_82A08028` calls it), so it should keep it [?: run `imgown` of `imaginators_hub.sh`
registered a blank Eruptor in its new game only]. The fix (`engines/alchemy/owner_id.cpp`) fetches it
again once the options save is loaded, at the start flow's "most recent save slot" lookup (`sub_8299BF10`), and logs
`[owner] the player's owner id: 1F3AB27A (the save's)`. Proof: `tools/regress/superchargers_owner.sh` (a new game registers
Fiesta and Crypt Crusher, a second session on that save uses the same id and writes no owner); run `fix2`: Fiesta, registered
in the first session, has no "Ownership" in the second.

**Toys from elsewhere** [?]. A toy registered on a console, or in another game of the kit (Swap Force writes its own offline id,
Imaginators its own under kind `06`), is someone else's in SuperChargers until its "Ownership"; the port does not
take ownership by itself (that writes over the toy's owner, which its console would then count as someone else's). Not
tried with real figures; one shared owner for the kit's games is an open question (below).

## Imaginators' creation crystals [Alchemy] [Imaginators]

Read from the owner's dumps (`private/dumps/imaginators/`, never in git: 186 retail crystals, 132 blank of every element and
shape and 54 with a character made on them, named by battle class) [V]; the game itself is not at the portal yet (`../imgday1`).

**Identity.** Toy types **680-689**, one per element: Magic 680, Water 681, Air 682, Undead 683, Tech 684, Fire 685,
Earth 686, Life 687, Dark 688, Light 689. The variant's low byte is the crystal's **shape** (`0x52xx`: Angel, Lantern, Pyramid,
Reactor, Acorn, Rune, Fanged, Armor, Claw, Rocket, each with its own number per element, e.g. Air Angel `0x5202`, Fire Angel
`0x5201`); `0x56xx` is the Legendary deco of a shape. The catalogue lists the shapes the dumps hold as versions (kind `shape`),
read like a trap's (`Catalogue::Read`, `Describe`).

**Signature** [V]. Every Imaginators tag (crystals and Senseis, every retail dump of the owner's) holds 64 extra bytes in **blocks
2, 4, 34 and 62**, made at the factory. The game checks it and covers **only the toy's identity** (runs of 2026-11-04, offscreen,
`SKY_LEVEL=hub_2016`, `private/research/run_img.sh`):
- the owner's made crystal (Air Ninja) plays in the hub: HUD "NINJA DE L'AIR", level 14, 7759 gold, the class's archives
  (`cyos_class_thrown.pak`, the Ninja's) and its air power's open; the game rewrote a copy byte for byte the same;
- a crystal our tools make (`ArkeyanFigureTest --make-crystal`, no signature) with the same creation: "Ton Skylander est peut-être
  corrompu ... RÉESSAYER / RÉPARER", the class's archive reopened every 4 s;
- the same creation written by our tools onto the owner's **blank, signed** crystal (Air Angel 0): accepted and played like the
  first. Its first use is a new crystal's: the game writes the owner and platform (0x11-0x13, 0x4F, 0x60-0x64) and clears the name
  and dates; the creation, class, experience and gold stay;
- the owner's blank signed crystal: the game's creator opens ("CRÉER : MARTELEUR DE L'AIR").

**The owner's decision** (2026-11-04): **the port accepts unsigned toys.** The signature is checked by the portal library's
verifier `sub_831FD948(work, data, 96, key)` (`GAME_HOOK_Portal_TagSignatureVerify`, games/imaginators): `data` is blocks 2, 4, 34
and 62, then 0 and 1, the key picked by byte 63; 0 means verified. It has two callers: the tag check when a toy arrives
(`sub_831F27C0(result, tag image)`: for a toy of 2016, variant 0x5000 and up, bit 0x40000000 of result+8 "signature required",
0x80000000 "verified") and the check before a write (`sub_831F4238`); hooking only the first left an unsigned blank crystal looping
in the creator's opening (the second check refused every write). Found in a diagnostic build whose loads report the crystal's
signature words (`docs/porting/debugging.md`, the store watch's twin): `sub_831FC850` (a hash) read blocks 2 and 4, under the
verifier, under `sub_831F27C0`. The hook (`games/imaginators/tag_signature.cpp`) answers "verified" only when **blocks 2 and 4 are
all zero** (made here) and runs the game's own check otherwise: a signature that is there but wrong stays refused
(`IMGTagSignatureTest`). The library keeps blocks 34 and 62 of a 600-699 toy raw (never encrypted), so a tag made here holds
zeros there too. Such a crystal is **virtual**: its card says "Virtual · Ninja · Level 14", its sheet "Virtual: a real game would
refuse this crystal" (`collection::VirtualToyText`, en, fr, de, it, es, nl). A 2016 toy also needs its own sector-0 trailer
(access `FF 07 80 69`, last byte `0x51`, the key the game checks with; every retail dump of the owner's has it): without it the
game skips the check and refuses the toy, so `WriteSectorKeys` writes it for a variant of 0x5000 and up **or a toy type of 600-699**.
The type matters: the catalogue has no plain crystal (its variants are all shapes), so the launcher's Collection and the Toy Box make
the plain one with variant 0, which before 2026-11-05 got no such trailer and was refused (the owner's Undead crystal made in the
0.9.0 launcher). An older virtual toy is mended by `figures::MendVirtualToy` (its sector keys rewritten, nothing else; never a signed
toy): `collection::WriteCrystal` on a save, and the portal when the toy is placed (the game sees it mended, the file is when the game
first writes it; log `[portal] ...: the 2016 sector keys of a virtual toy set`). Proved in the game (2026-11-05): a virtual blank crystal opens the creator, a virtual crystal with a
creation plays it at its level, the owner's signed ones are unchanged ([toybox.md](../../../games/imaginators/docs/toybox.md)).

**Layout** [V]. The trap's pair of 21 data blocks (A from block 8, B from 36, packed without the trailers; the sequence picks the
newer), the trap's three CRC-16 except that **0x0A covers 16 blocks** (packed 0x40-0x13F): the 21st block is the signature's.

| Packed offset | Field | Status |
|---|---|---|
| 0x00, 24 bits | experience: **level** by the game's table (below; Air Ninja's 87565 is the HUD's level 14) | [V] |
| 0x03, 16 bits | gold | [V] |
| 0x05, 32 bits | play time, seconds | [V] |
| 0x09 | sequence (A and B adjacent, modulo 256) | [V] |
| 0x0A, 0x0C, 0x0E | checksums | [V] the 54 made crystals |
| 0x20-0x3F | the creation's name, 8-bit, upper case as the game writes it, zero ended (17 characters seen) | [V] |
| 0x40, 0x50 | last placed, created (the first area's date format) | [V] |
| 0x60-0x6F | 6 bytes that vary per crystal (upgrades bought? Sensei training?) | [?] kept |
| 0x70 | `30`, then zeros, in every made crystal | [?] kept; `CreateCrystal` writes it |
| 0x78, LE32 | `0x458D186A`: the hash of the disc's recipe format (`CCYOSRecipeFormat`) | [V] every made crystal; none on a blank one |
| 0x7C-0xC0 | **the creation**: 74 parameters, 547 bits, least significant bit first (below) | [V] |

**The creation (recipe)** [V]. The disc's `LooseData/GlobalCYOSParams.igz` (`archives/permanent_2016.pak`) holds the recipe
format and one settings object per parameter, in the order the tag stores them: 8 gear attachments (right and left weapon,
backpack or wings, hat, leg, arm and shoulder armour, ears; ids 10000-30999, 10 bits each), 6 shape sliders (lower and upper
body mass, height, muscles, head size, tail width; 0-8, 4 bits), 5 body parts (head 0-999, torso, arms, legs 1000-3999, tail
15000-15099), 43 colours (palette indexes 0-127, 7 bits: skin, aux, eyes, each gear's three), 12 choices (secondary and tertiary
power, **Class** 0-63 on 7 bits, aura, sound theme, eyes, two catch phrases, music, voice, voice effect, primary power). Each
value is the id minus its minimum; all ones is "none" (Air Ninja has no torso part: 1023). The class is the "Class" parameter
(bit 472, packed 0xB7: the field found first), 1 Knight, 2 Bowslinger, 3 Quickshot, 4 Ninja, 5 Brawler, 6 Smasher, 7 Sorcerer,
8 Swashbuckler, 9 Sentinel, 10 Bazooker; the powers are numbered by class (class x 50 + the choice). The widths: a slider's and a
colour's are their settings' own, the others the bit length of their count of values. Proof: all 54 made crystals decode with every
value in its range or "none", nothing set after bit 547, and rewrite byte for byte. The ids name parts on the disc (the class
archives `cyos_class_*.pak`, `materials:/CYOS/...`); their display names are not mapped [?].

**Levels** [V]. Every class's character data (`archives/cyos_class_<class>.pak`, `CYOS_Class_<Class>_CharacterData.igz`, its
`CCharacterProgressionComponentData`'s `igIntIntHashTable`) holds level -> experience for **80 levels**: 1-20 the older games' table
(197500 at 20), then 20000 more per level (220000 at 21, 1400000 at 80); Archer's and Mage's are the same. `figures::CrystalLevel`.
In the game Air Ninja (87565) shows level 14 on the HUD [V].

**Code.** `figures::ReadCrystalStats` / `WriteCrystalStats` / `CreateCrystal` / `CrystalParams` / `CrystalLevel` / `BattleClassName` (`figure_tag.h`); `ReadStats` and `WriteStats`
map a crystal's experience, gold, play time, name and dates onto the character fields, so the collection, search and Edit stats
treat it as one (the long name kept; `SetNickname` still cuts at 16). The writer starts from the newer copy, writes the inactive one
first, keeps each copy's signature block and every byte it does not know. A blank crystal ("Ready to create", the game's creator
fills it, or `collection::WriteCrystal` gives it a first creation) refuses `WriteCrystalStats`. The cards show the class and level
("Ninja · Level 14": `collection::LevelText`, `BattleClassText`, French names too); the Toy Box sheet the name, the experience bar
on the 80 levels, gold, play time and, in Details, the parts.

**Proof.** `ArkeyanFigureTest` `TestCrystal` (a crystal built as the dumps are, read, edited, damaged copies), `TestCrystalRecipe`
(the 547 bits, the levels, a creation made and edited) and, with the dumps as
arguments, every one of the owner's 432 retail dumps: decrypt then encrypt gives the dump's bytes, every crystal reads valid or
blank, a rewrite moves only the sequences and checksums and keeps the signature and identity byte for byte
(`find private/dumps -name '*.dump' ! -path '*Unreleased*' ! -path '*3D Printed*' -print0 | xargs -0 ArkeyanFigureTest`: 37354 checks, 0
failures). The "Unreleased" and "3D Printed" dumps are home-made (no signature, or blocks encrypted under another serial) and fail
on purpose.

### The parts' names [Alchemy] [Imaginators] [V]
A creation stores its parts as the game's ids (head 0-999, torso 1000-, weapons 30000-, powers 20000-22499: `CrystalParams`). The
names the game shows come from two files of `archives/permanent.pak` (2026-11-05):
- `LooseData/CollectibleTrackers.igz` (IGZ v9): a root `igObjectList` of 6827 objects and, in the file's `ONAM` record, an
  `igNameList` naming each of them **in the same order** (`{ name, hash }` pairs: `Bird_Head`, `Bird_Head_userData`...). The 1472
  `CCYOSLootCollectibleTracker` keep their part id at **word 36** (the id the recipe stores: `Bird_Head` 6, `Bird_Torso` 1006,
  `Bird_Arm_Armor` 10014, `Bird_Voice` 32818); the 1472 ids are distinct.
- `LooseData/CollectibleTrackers_<code>.lng` (fr and nl on the European disc): one `igLocalizedStringDataList` of 3828
  `igLocalizedStringData` { word 2 the text, word 3 the object, word 4 the field }. Object and field are named external
  handles, `0x80000000 | index`: the `EXNM` record's `{ file, name }` pairs (TSTR indices) whose file word has the top bit set are
  the objects (`CollectibleTrackers`, `Bird_Head`), the others the fields (`metafield`, `CCYOSLootCollectibleTracker::_name`), each
  list indexed on its own. A tracker's `_name` text is the part's name: `Bird_Head` "Tête d'oiseau" / "Veertastisch".
  Day 2 matched texts to trackers by order and failed past the fourth: the link is the name list, not the order.

All 1472 parts get a name in both languages. `art::ExtractPartNames` (game_text.cpp; the manifest's `"parts"`) writes
`<data>/Imaginators/parts.json` once, when the Toy Box first opens (`art::PartNames`); `collection::PartName(game, id, language)`
answers in the game's text language, else the disc's first one (an English game on this disc shows French). The Toy Box's Details
("Creation: head Capuche, arms Bras de Chevalier Doomlander, legs Jambes de raptor...") and the creation editor's rows show the names,
`#<id>` when one is missing. **Proof**: `IMAGINATORS_ISO=<disc> ArkeyanArtTest` (a head, a weapon and a power in both languages, English
falling back to French, an unknown id); runs `imgP1`, `imgP2` (the owner's Air Ninja crystal, a copy, in a scratch collection; F6,
its page, Details, Edit: shots `private/runs/imgP2/t_f.png`, `t_h.png`).

## Toys the title recognises

From the title's portal code: `sub_82643DA0` accepts 0-32 and the legendaries 404, 416, 419, 430; `sub_82643BE8`
accepts the magic items 200-207, the adventure packs 300-304 and 505, 514, 519, 526 (Giants-era sidekicks). The order
of 0-32 matches the internal character names built by `sub_829BC850` (AirDragon = Whirlwind ... Cynder) with Dark
Spyro (28, `DarkSpyro`) in its place; the `toy_<name>_hud` images name the legendaries `LegendaryRockDragon`,
`LegendarySpyro`, `LegendaryGoldenCheat`, `LegendaryPandoranGuard`. [V] Display names are the retail names.
Variants (block 1 0x1C): Spyro's Adventure reads the toy type only, a variant figure is its base character
(["The variant id"](#the-variant-id-what-each-game-reads-ssa-giants) below).

**Giants** [V] (runs of 2026-10-25) takes every SSA toy and adds its own: the 16 characters 100-115 (8 Giants: Swarm,
Crusher, Hot Head, Thumpback, Ninjini, Bouncer, Tree Rex, Eye-Brawl), LightCore variants, the magic items 208 and 209,
and the sidekicks 505, 514, 519, 526 and 540-543 (the disc has 8 pet items, `item/Item_Pet_00`-`07`: the four
Spyro's Adventure-era ones and Giants' own, Barkley, Thumpling, Mini Jini and Eye Small, the little Tree Rex, Thumpback,
Ninjini and Eye-Brawl; their portraits are on the disc [V]; toy types 540-543 [V] by the disc's toy definitions
(`character/Init_Setup.bld`: 540 Barkley `Item_Pet_04_TreeRex`, 541 Thumpling `Item_Pet_05_Whale`, 542 Mini-Jini
`Item_Pet_06_Djinni`, 543 Eye-Small `Item_Pet_07_UndeadGiant`; [collection.md](collection.md#the-games-own-toy-tables));
**540 Barkley played [V]** (run of 2026-10-26: Giants loads it as an item, "Magic Moment ... Load Item", and it follows Jet-Vac with its HUD ring; alone it asks for a Skylander, `TagTypeNotOnPortal`, like every sidekick)) and the adventure packs 300-304 (four `Item_Location_*`; 304 Volcanic Vault is the PvP bonus level `Item_PvP_BonusLevel` [V] the toy definitions). Each one
alone was accepted and played: a giant, a sidekick (the little creature and its HUD ring), a magic item, an adventure
pack, a LightCore, an SSA character. **A magic item and an adventure pack together are "too many toys" in Giants**:
they share one seat, which the catalogue's Giants seats now say (`item` accepts magic items, adventure packs and pets).

**The 8 pet items and the accepts list (2026-10-27) [Giants] [V].** Giants' own toy definitions (`character/Init_Setup.bld`,
`tools/toy_tables.py`) hold the 8 sidekicks as `Item_Pet_00`-`07`, all flagged "is magic item" like the items and packs:

| Toy type | Disc item | Name | Seen in play (HUD ring, follows the player) |
|---|---|---|---|
| 519 | `Item_Pet_00_TriggerHappy` | Trigger Snappy | yes |
| 526 | `Item_Pet_01_StealthElf` | Whisper Elf | yes |
| 514 | `Item_Pet_02_GillGrunt` | Gill Runt | yes |
| 505 | `Item_Pet_03_Terrafin` | Terrabite | yes |
| 540 | `Item_Pet_04_TreeRex` | Barkley | yes (2026-10-26) |
| 541 | `Item_Pet_05_Whale` | Thumpling | yes |
| 542 | `Item_Pet_06_Djinni` | Mini-Jini | yes |
| 543 | `Item_Pet_07_UndeadGiant` | Eye-Small | yes |

Each ran alone beside an SSA Gill Grunt from the title to the first level (isolated, offscreen, shots in the HUD). **Giants has
no toy-type check in its code (the PPC): the table above is the check.** `tools/toy_tables.py` now compares it with the
catalogue and fails on a difference: the 76 toy types of the games SSA and Giants in the catalogue are exactly the 76 the disc
defines (the 77th, 999, is a debug minion). **A pet takes the item seat**: a pet with a magic item, and two pets, are
"too many toys" (`103_PortalError_TooManyItems`, runs 2026-10-27), so the catalogue's Giants seats now accept `mini` in `item`
and no longer in `p1`/`p2` (SSA's item function `sub_82643BE8` lists 505/514/519/526 with the items: probably the same there [?]).

**Player 2 [Giants] [V] (2026-10-27).** The runtime has a second controller path (XAM user 1: a second SDL pad, or `SKY_PRESS2`,
[input.md](../xbox360/input.md)). With it Giants shows "Join Game"; a button of pad 2 makes the game ask for a second Skylander
(`Portal Control - Portal - 0 - Idle P2 Only`, `203_PortalError_TagTypeNotOnPortal`), the Toy Box opens on the Player 2 seat
(`[toybox] prompt for player 2`), the figure is placed (`P2: Eruptor`) and **both play together** (Gill Grunt and Eruptor, two
HUDs, level 1 of the tutorial: offscreen shots of runs 2026-10-27). **The "withdrawn after 1 s" of the first runs was the test, not
the game:** pad 2 only ever pressed Start (a script of Starts every 20 s), and the Toy Box's own script pressed Return on a clock that
rarely fell in the prompt's short window. The same run with pad 2 pressing A (or A, B, X, Y in turn) and the Toy Box pressing Return every 6 s
joins every time. Checked and **not** the cause: a XAM sign-in for user 1 (`XamUserGetSigninState` and the others were reverted and it
still works; the game asks `XamInputGetState` for users 0, 2, 3 all the time and for user 1 only once it exists), the pad's
flags (always 1). With one player, two figures on the portal are "too many Skylanders" (`102_PortalError_TooManyMinions`): the second
figure goes on the portal only after the game asks for it. A real second pad is untried.

**The catalogue** (`resources/collection/catalogue.json`: 284 toys, 6 games) gives each game the toys it accepts
(`accepts_games` per game, `accepts` per toy) and its seats (Giants: `p1`, `p2`, `item`); `figures::Catalog(game)` and
`FindToy(type, game)` (`figure_catalog.h`) read it. Trap Team's traps, Imaginators' creation crystals (above) and SuperChargers'
vehicles have their own layouts; the variants of SuperChargers' and Imaginators' toys, the crystals' shapes, Imaginators' Mystery
Chests and its two adventure packs come from the owner's dumps (block 1's variant id and the dump's retail name), and so do the
trophies' types (3500 Sky, 3501 Land, 3502 Sea, 3503 Kaos; the public list had Sky and Sea swapped). A plain figure of those
games carries the year and a flag in its variant id (`0x4000` vehicles and trophies, `0x4100` SuperChargers, `0x5000` Senseis):
`Catalogue::Read` and `Describe` take it as the toy itself. Portals and toy types per game:
[franchise.md](../../porting/franchise.md).

## A figure in a level (2026-10-28) [SSA]

`tools/regress/ssa_level_play.sh <iso> <work dir> [--level L] [--item <toy type>] [--walk <stick>] [--stand]`: a Gill Grunt (made at
`--xp`, `--gold`) and optionally a magic item on the emulated portal, one isolated run in a level with the level tour's presses (or a
held stick, or attacking in place), then `ArkeyanFigureTest --roundtrip` before and after and a frame every 25 s. **Proved:** the figure is
written at its first use in `level_027` and `level_001` (sequence 2 to 3), nothing outside the first pair changed, checksums valid.
**Not proved** (an item's effect never showed): an item's effect. **XP and gold earned in a level: proved later the same night**
(2026-10-28) [V]: with the hero spawned by `SKY_NEXT_LEVEL` (the hero tour), scripted walking and attacking earned gold in 11
levels and XP in 3 (`level_024`: XP 0 -> 12, gold 0 -> 64), written to the figure with only the first pair changed. Per
level: [100.md](../../../games/ssa/docs/100.md).

## Every SSA toy, type by type (2026-10-27) [SSA] [V]

`tools/regress/ssa_figures.sh <SSA disc> <work dir> [--variants] [--seconds N] [<type>[:<variant hex>] ...]` runs each toy the
catalogue's `ssa` list holds (54 types) in its own isolated run (the fast recipe to `level_027`, emulated portal, 80 s), then
`ArkeyanFigureTest --roundtrip <before> <after>` checks what the game did to each figure on the portal: **every block outside the
first pair of areas is byte for byte the same** (identity, toy type and variant, the extra areas, the trailers), the first pair
reads valid, and every byte the game changed in it is a known field. The run also needs the log's `[portal] placed ... (toy
type N)`, a block query of the toy's slot (`Q <n><slot>`), no fault line, and, for a character, the first-pair write. Characters
are made at 1000 XP and 100 gold; the 17 characters of odd toy type get a **Giants part** too (40000 XP, hat 6), so "Giants'
area untouched" is checked on real extra areas; sidekicks, magic items and adventure packs sit beside a Gill Grunt (the game
needs one). `tools/test_all.sh --long` runs one toy of each kind (14, 404, 201, 300, 505).

| Group | Types | Run result [V] |
|---|---|---|
| Characters | 0-32 (33, Dark Spyro 28 among them) | placed, read, played; **written once, on first use**: sequence +1, owner id (0x18..0x1F), platform byte (0x13), last placed (0x50..0x55) and last reset (0x60..0x65) dates, 0x5F (new: set on first use, meaning unknown), the three checksums; experience and gold as made |
| Legendary | 404, 416, 419, 430 | the same as characters |
| Sidekicks | 505, 514, 519, 526 | read, never written (a blank tag stays blank); 505 shows in the level as a small creature on the bridge |
| Magic items | 200-207 | read, never written; 202 (Healing Elixir) shows in the level as a pickup on the bridge |
| Adventure packs | 300-304 | read, never written; 300 shows an object in the level |
| Variants, one per catalogue kind (series, alt deco, Eon's Elite, LightCore, glow, flocked, legendary, gold, metallic, dark, event, other; 16 runs, e.g. `0:1801`, `7:1206`, `201:0002`, `505:3000`) | | play as their base toy; **block 1 (toy type and variant) is unchanged** after the game's write |

All 54 types and the 16 variants: 0 failures. The Giants parts of the 17 odd characters came back byte-identical and valid.
`ArkeyanFigureTest` also reads **the owner's own SSA dumps** (read only, never copied into git): 75 dumps (32 characters, 13 magic
items and adventure packs, 4 sidekicks, 5 in-game variants, 21 chase variants), whose toy types are **exactly the 54 of the catalogue**, all with
variant 0 (the chase and in-game variants of SSA share the base toy type; the variant ids above come from Giants-era toys).
73 are blank, one is a played figure with a Giants extra area, and one is a figure whose first pair our reader calls
**damaged** (area A's first block zero, area B's first block zero and the rest of B written, a valid extra area).
- The played one run in the game (80 s): the game wrote the first pair (sequence 2 to 3, the dates and the platform byte), the
  extra area came back byte-identical, valid.
- The damaged one run in the game: SSA read it, wrote 4 blocks of area B (36, 37, 41, 42) and the figure then reads as valid
  (sequence 104); the extra area is untouched. So SSA recovers a figure the library calls damaged: `ReadStats` returning
  Damaged there is cautious, not a crash [V], and the Toy Box and the library keep such a figure as it is until edited
  [?] (not tried). `ArkeyanFigureTest <dump>` fails on that dump by design (it asks for no damaged figure); keep it out of the
  dump arguments.
- **Not reached:** XP or gold earned in a level, or an item's use. In `level_027` (the spawn island has no enemy or coin on
  the way; 150 s of scripted walking and attacking, XP and gold unchanged, no write after the first-use one), `level_000`,
  `level_001` and `challenge_level_000` nothing was earned; with `SKY_LEVEL` set to `level_002`, `003`, `006`, `008` or `009`
  the game stays in `frontend` (no mission file opened, so no write at all; the cause is unknown [?]). So the game's
  write of earned values is the same first-use write path ([?], as before), and what an item does when used is unseen: only
  that the game loads it (the pickup in the level).

## Trap Team figure tour

`tools/regress/figures.sh trapteam <disc> <work dir>` uses the catalogue's `trapteam` accepts list, with earlier toys
included. The work directory must be below `private/`. Trap Masters and Trap Team minis run as
characters; older sidekicks and items get a generated Gill Grunt companion. Swappers run as matching pairs with their
catalogue base/variant tags. Known variants through Trap Team's year include its mini reissues and Eon's Elite;
later Elite editions are excluded. A type argument selects its known tags, while `type:hex` selects one tag.

The tour generator makes ordinary blank character tags with catalogue identities. Its trap cases remain disabled:
every known trap shape is listed as `SKIP` in `results.tsv`, with a reason, and no game is launched for it. Unknown
requests or generator failures are also listed as skips. The separate scratch command above makes trap candidates.
The later-game character roundtrip checker checks identity and character checksums; use `ReadTrapStats` for traps. Figure
acceptance, initialization and writes still need a game run. See [testing.md](../../getting-started/testing.md#trap-team-tours-scripts-prepared-game-runs-pending-trapteam)
for executable paths, isolation and checks that require no game.

## The variant id: what each game reads [SSA] [Giants]

Block 1, 0x1C (LE16): the figure's variant ("deco") id. Found in each game's own code and data (2026-10-26); the
games' toy tables themselves: [collection.md](collection.md#the-games-own-toy-tables).

**Spyro's Adventure [SSA]: the toy type only.** The portal driver keeps each slot's blocks 0-1 at slot + 0x164
(`sub_82645788` returns it). Its readers (`sub_82645278`, `sub_82645408`, `sub_8264A0D0`, `sub_8264A2D0`, and the
callers of `sub_82645788` and `sub_82645800`) take the toy type at + 0x174 (block 1, 0x10); none reads + 0x180 (the
variant) [V] (static: the readers listed). The disc has no variant data either: one model per toy type (`models/characters/minionsmonsters/`;
the Legendaries and Dark Spyro are toy types of their own, 404-430 and 28), one picture per toy type
(`uigameimage.str`). So any variant figure (Series 2, LightCore, Polar...) plays as its base character, and SSA never
writes block 1, so the variant stays on the tag. [V] `ArkeyanPortalTest` (the emulated portal hands block 1 over unchanged)
and the run below.

**Giants [Giants]: three flags and a deco id, checked against the toy's definition.** [V] The script macro
`Save - Get Tag Subtype Properties` (`permanent/global.bld`, `PortalControl.ai`) reads the tag's 16-bit "toy subtype"
with the save code's bit-field reader:

| Bits | Variable | Kept only if (the toy definitions of `character/Init_Setup.bld`) |
|---|---|---|
| 11 (0x0800) | `is repose`: Series 2 | the toy is in `sky1 characters with repose` ("has known repose"): 24 SSA characters |
| 10 (0x0400) | `is legendary`: an alternate deco | the toy has an alternate load (`Portal - Define Legendary Alt Level Load`) whose deco id is bits 0-7 |
| 9 (0x0200) | `is lightforce`: LightCore | the toy is in `display as lightcore`: Prism Break, Eruptor, Drobot, Hex, Jet-Vac, Chill, Pop Fizz, Shroomboom |
| 0-7 | `deco id` | (matched above) |

Bits 12-15 (the release year in public descriptions) are not read: 0x1801 and 0x2801 are the same Series 2 figure for
Giants. Anything that does not match a definition is ignored: a LightCore Warnado (Warnado is not on the list) plays
as a plain Warnado, a 0x2C02 Trigger Happy (no deco 2 for him) as a Series 2 Trigger Happy.

What the subtype changes [V] (the scripts; the runs below):
- **The model.** An alternate deco loads its own character archive instead of the toy's: `MinionLoad_108_AlchemistRed`
  = `character/108_Alchemist_Red.bld` (Punch Pop Fizz), `000_AirDragon_Alt` (Polar Whirlwind), `*_Legendary`. The
  15 alternate decos: Whirlwind 2 Polar, Ignitor 3 Legendary, Slam Bam 3 Legendary, Double Trouble 2 Royal, Stealth Elf 3
  Legendary, Jet-Vac 3 Legendary, Crusher 2 Granite, Flashwing 2 Jade, Hot Dog 2 Molten, Chill 3 Legendary, Pop Fizz 2
  Punch, Ninjini 2 Scarlet, Bouncer 3 Legendary, Tree Rex 2 Gnarly, Dragonfire Cannon 2 Golden Dragonfire. LightCore
  keeps the toy's model and adds the LightCore portal entry (`PortalEntry_LightForce.ai`) and HUD icon (`health display
  icon - lightcore`); Series 2 has no model of its own: its HUD icon (`health display icon - series 2`) and the Wow Pow
  upgrade (`pp_Upgrade_WowPow`, which the "fully upgraded" achievement checks next to `is repose`).
- **The name.** The toy's name and the subtype's string of the text table (`Toy Type Strings`): "Series 1" (an SSA
  character without the repose flag), "Series 2", "LightCore", "Legendary", "Special", and the decos' own names (Polar,
  Royal, Granite, Jade, Molten, Punch, Scarlet, Gnarly, "Golden Dragonfire"). Legendary Bash, Spyro, Trigger Happy and
  Chop Chop (404-430) and Dark Spyro (28) are separate toy types, defined as "alternate characters" of 4, 16, 19 and 30
  (`Portal - Define Alt Character`, prefixes Legendary and Dark).
- **The picture and the collection.** Each subtype has its portrait and versus art (`eruptor_series2`,
  `icevalkyrielegendarylightcore`, [game_art.md](game_art.md)); the save counts each one apart (`collected - series 1`,
  `series 2`, `light core`, `legendary`, `lightcore + legendary`, `special`).

**Swap Force [SwapForce]: the same bits, named by its data.** [V] (the disc's toy data, the title's enums, 133 real
figures) Each `CVariantIdentifier` of `archives/toydata.pak` ([collection.md](collection.md#the-games-own-toy-tables))
gives the fields a figure's id packs:

| Bits | Field | Values |
|---|---|---|
| 12-15 | `_yearCode` | `eSG_Spyro2011` 0, `eSG_Giants2012` 1, `eSG_Shapeshifters2013` 2 (the enum has no value table: the numbers are the real figures') |
| 11 (0x0800) | `_wowPowFlag` | set on the reposes (Series 2 and 3) and on alternate decos released as reposes (`0x2C02` Dark Stealth Elf, Volcanic Eruptor) |
| 10 (0x0400) | `_fullAltDecoFlag` | alternate decos and Legendaries |
| 9 (0x0200) | `_lightCoreFlag` | LightCore, also with an alternate deco (`0x2602` Enchanted Star Strike, `0x2603` Legendary Grim Creeper) |
| 0-7 | `_decoId` | `kTfbSpyroTag_DecoID`: 1 Repose1, 2 AlternateDeco, 3 Legendary, 4 Event, 5 Repose2, 6 LightDirect, ... 22 Holiday ([the list](collection.md#the-games-own-toy-tables)) |

So `0x2805` is a Swap Force repose (Series 3 Horn Blast Whirlwind), `0x2206` a Swap Force LightCore, `0x2402` a Swap
Force alternate deco (Dark Blast Zone: both halves carry it), `0x2403` a Legendary, `0x2000` a plain Swap Force figure.
Each of the 54 real in-game variant figures carries exactly the id its definition gives. How the title compares a
figure's id with the list (the whole 16 bits or some of them), and what it does with an id no definition has (the
chase figures' decos 0x16-0x1C), is not read yet [?]. Toy types: the swapper tops ("Body") are 2000-2015 and the
bottoms ("Legs") 1000-1015 [V] (enum, data, real figures).

**Our figures.** The catalogue gives every variant Giants knows an id Giants reads that way (47 for the SSA and Giants
toys; [collection.md](collection.md#the-catalogue-data)), and the Toy Box and Arkeyan Launcher write it as is
(`CreateFigure(type, variant)`). [V] `ArkeyanCollectionTest` creates each of the 47 through the collection library, reads it
back (toy type, variant, name, kind) and checks what Giants' rule above makes of it.

**Runs** (2026-10-26, isolated, emulated portal, `SKY_PORTAL_PLACE=new:<type>:<variant>`, `SKY_TRACE_FILES=1`):
- Giants, `level_203_the_machine`, a never-played **Punch Pop Fizz** (`new:108:1402`): the game opens
  `\character\108_alchemist_red.bld` (not `108_alchemist`) and plays the red Pop Fizz, its red portrait on the HUD.
- Giants, the same level, a never-played **Legendary Chill** (`new:106:1603`): it opens `\character\106_icesorc_legendary.bld`.
- Spyro's Adventure, the fast recipe, a never-played **Series 2 Eruptor** (`new:9:1801`): plain Eruptor in level_027
  (HUD "Eruptor"), and the dump still carries `01 18` at block 1 0x1C afterwards.
- One figure at a time in Giants: two Skylanders on the portal give "There are too many Skylanders on the Portal of
  Power" outside two-player play.

## Portraits

Each game's toy pictures come from its own disc and are cached as PNG in `<data>/<Game>/portraits/` (`SKY_PORTRAITS`
overrides it; `PortraitFolder` in `runtime/host/art/game_art.cpp`): **78 files for SSA** (the 37 `toy_<name>_hud`
128x128 DXT5 images and the 37 `<name>_vs` 512 px ones from `uigameimage.str`, plus 4 magic-item pictures of 64 px:
`potion`, `crossed_swords`, `shield`, `speed_boots`), **123 for Giants**. Toys without a picture get their family's picture from the disc (the three
128 px family icons `elementiconmagicitem`, `elementiconlocation`, `elementiconpet`), else a drawn icon
([collection.md](collection.md)). Provider, manifest, rule for a toy's picture: [game_art.md](game_art.md); the `strm`
and `APKF` format: [str_levels_loading.md](../../engines/lux/str_levels_loading.md).

## Toy Box menu

The design, the screens and what is left: [toybox_ux.md](toybox_ux.md) (sections 8 and 14). Code: `host/toybox/model.*` (logic),
`host/toybox/draw.*` (look: the first Toy Box's colours and fonts, kept plain for a reskin), `host/toybox.cpp` (input).
Texts: English, French when the game's text language is `fr` (`SKY_LANGUAGE`, launcher.ini), as `L("...", "...")`
pairs where they are used.

| Action | Keyboard | Gamepad |
|---|---|---|
| Quick swap wheel (in game) | hold Tab; arrows or 1-8 aim; release Tab or Enter swaps; Escape cancels | hold View; stick or d-pad aims; release View or A swaps; B cancels |
| Toy Box | F6, tap Tab | View + Y |
| Move / choose / back | arrows, Enter, Escape (also clears a search) | d-pad or left stick, A, B |
| The figure (Figure page), a seat's options | F2, Menu key, right click | X |
| Search (wheel: the seat's list) | F3, or type | Y |
| The other seat (Edit stats: by 10) | Page Up / Page Down (Edit stats: Shift + left/right) | LB / RB |
| Take off (a seat in the top row) | Delete | seat options |
| Reset, delete | hold Enter | hold A |

  Mouse: click to focus, double click (or a second click) to act, right click for options, wheel to scroll.
  While the Toy Box or the wheel is open the title gets no input from the pad (`ToyboxFilterGamepad`); buttons held
  when it closes are held back until released. View is held back while it may become the wheel and given back to the
  title as a tap when released before 0.25 s.
- Figure folder: the shared collection ([collection.md](collection.md)): `SKY_COLLECTION`, `SKY_FIGURES`, else
  `<user data>/Skylanders/Collection`; the old `<pref path>/figures` is copied into it once. The emulated portal no
  longer creates a dump of every character at start; the Toy Box's tidy moves the never-played ones older builds made
  to the trash once (F7 still cycles through the folder's dumps).
- The emulated portal starts as the last session left it: every place/remove rewrites `portal.txt` (`<slot> <path>`
  lines) in the save folder (`SKY_SAVE_DIR`, else the pref path; `LocalState` on the Xbox), read back at start; empty
  the first time. `SKY_PORTAL_PLACE` (tests, the launcher's `[portal] place`) replaces it with exactly its figures. No
  folder is ever placed whole. A real portal skips all of it.
- A Swap Force swapper is one card: both halves go on the portal (two slots) and come off together, "Change the
  bottom" / "Change the top" on its Figure page ([toybox_ux.md](toybox_ux.md#swappers-one-card-two-tags-swapforce-built-2026-10-27),
  [collection.md](collection.md#swappers-two-dumps-one-card-swapforce-2026-10-27)).
- Clear the portal (seat options, with a confirmation): every figure off, the same code as F8 (`PortalRemoveAllFigures`).
- Rendering: Dear ImGui draw lists (no ImGui windows or widgets; navigation is the menu's own) rasterised on the CPU
  by `host/imgui_software.cpp` into a 1280x720 RGBA image, uploaded when it changes and blended over the game by
  `gpu::SetOverlay`, on every platform including the Xbox (it replaced SDL's software renderer, which UWP lacks, and
  follows its rules). Fonts: Outfit Bold/Black (Bungee draws only the button glyphs, `glyphs.cpp`), built into the executable (`cmake/embed_fonts.cmake`).
  The renderer copies textured rectangles and fills rectangles but rasterises every other triangle pixel by pixel,
  so round shapes (portraits, discs, corners, the glow) are baked once into textures and opaque rectangles stretch
  a tiny opaque texture. A redraw costs a few ms; nothing runs while the menu is closed.
- Unattended checks: `SKY_TOYBOX_SCRIPT="F6:30,Right:31,Return:32,type=Ace:33,shot:34"` (key names as
  `SDL_GetKeyFromName` knows them, pressed and released; `shift+<key>`; `down=<key>` / `up=<key>`; `type=<text>`;
  `pad=back+lright*0.8` holds pad buttons as the Toy Box sees the pad (`*` seconds, else 0.15); `click=x/y`,
  `rclick=x/y` in canvas pixels; `hold=<scancode name>*0.5` holds a key for the game's input and logs what the game gets
  from it ([gamemenu.md](gamemenu.md), "Commands"); `shot` or `shot=<name>` = PNG of the last presented frame to
  `<SKY_TOYBOX_SHOTS>_<n or name>.png`; times in seconds since launch). `SKY_TOYBOX_SCRIPT_FILE=<file>`: lines
  appended to the file while the game runs are the same items without a time, run at once, or `+<s> <item>` that many
  seconds after the line is read (a live run steered from outside beside `SKY_PRESS_FILE`: a trap taken off and put
  back during play; a recorded route started when the log shows the level). `SKY_TOYBOX_CLOCK=frames`: the script's
  times and its `pad=` / `hold=` lengths count the frames the game presented, 30 a second, so a route keeps the game's
  pace when a busy machine drops it under 30 fps (`tools/regress/trapteam_capture.sh`). `SKY_TOYBOX_REAL_VIEW=1` shows the real
  portal's read-only view over the emulated portal's traffic.

## Tests

- `ArkeyanFigureTest [<dump>...]` (`runtime/host/figures/figure_test.cpp`): RFC 1321 and FIPS-197 vectors, the sector-0
  key, round trips, sequence wrap, damaged areas, the catalogue, the cross-game rules above; `--make` and
  `--same-extra` and `--roundtrip` for game runs (`--roundtrip <before> <after>`: everything outside the first pair identical, the
  first pair valid, only known fields changed; used by `tools/regress/ssa_figures.sh`, above). `ArkeyanPortalTest` (the emulated portal, identity, prompt classification),
  `ArkeyanPortalAsyncTest`, `ArkeyanCollectionTest`, `ArkeyanArtTest` (portraits). All run by `tools/test_all.sh`
  ([testing.md](../../getting-started/testing.md)).
- `tools/regress/swapper_toybox.sh` (Swap Force; `SWAPFORCE_ISO`, `build-swapforce/`, ~2 min, run by `tools/test_all.sh`):
  one linked swapper and a loose bottom in a scratch collection; the Toy Box at the portal prompt puts the swapper card on
  P1 (log: `P1: Blast Zone on slot 0 and slot 1 (bottom)`, both archives open, `the game took Blast Zone`), takes it off
  (`taken off`), puts it on again, then the Figure page's "Change the bottom" swaps the bottom (`Halves changed`, the new
  pair on P1, `collection.json` links checked). Screens of the Toy Box in `private/runs/swapper_toybox/shots_*.png`. With `SWAPPER_ART_DATA=<data folder where
  ArkeyanArtTest or the launcher already read the disc>` the run also checks that the card is drawn as its two halves stacked (the game
  itself only reads the portraits). `search_toybox.sh` takes the same variable.
- A run with a figure: `SKY_PORTAL=emulated SKY_COLLECTION=<scratch dir> SKY_PORTAL_PLACE=<dump>.sky`
  (+ `SKY_PORTAL_LOG=1`); `SKY_TOYBOX_SCRIPT` drives the menu.

## Open points [?]

Play time, heroic challenges and hero points unverified in game; items and adventure packs placed and seen in a level ("Every SSA toy") but their use not observed; XP and gold earned in play observed on 2026-10-28 (the hero tour); which figure becomes player 1 after lifts
and places is unchecked; Player 2 was run with scripted presses only (a real second pad is untried).

Owner (block 6, above): real figures registered on a console (kind `80` or another platform's) not tried; Swap Force's own
handling of a foreign owner and of its offline id across sessions not checked; one owner for the kit's three games (Swap
Force, SuperChargers, Imaginators: today each its own id, so a toy used in one is someone else's in the next) is a choice
to make, and changing an id makes the toys registered with the old one someone else's once.
