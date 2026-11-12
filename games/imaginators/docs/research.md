# Imaginators: research and day 1's numbers

> **Purpose:** what day 1 on the disc measured for Imaginators (identity, XDK, engine, sigmatch, codegen, imports), and the leads for Creation Crystals · **Status:** current (2026-11-11) · **Tags:** [Xbox] [XDK] [Alchemy] [Imaginators] [V] [?]

The public facts from before the disc are in [public background](#public-background-2026-10-25); every line
there is [?] unless this page says [V]. Nothing here is copied from another project.

## Day 1 (2026-11-04): the Europe disc [V]
What `tools/day1.sh`, `tools/sigmatch`, `tools/xmatch.py`, `tools/find_switch_tables.py` and XenonRecomp gave. All derived files are in
`private/imaginators/` (day 1: `private/imaginators/day1/`).

| Question | Result [V] |
|---|---|
| Identity | title ID **`41560930`** (the older page's guess was right), media ID `0133F901`, version `0x4`, disc 1 of 1; XEX SHA-256 `1c6d7e05...`, 25739264 bytes; image `0x01940000` bytes at `0x82000000`, entry `0x836366E8`, `.text` `0x82480000` + `0x012FD744`; 294 imports |
| XDK | every library 2.0.**21256** (D3D9LTCG, XAUDIO2, XMP, XAPILIB...): **Trap Team's and SuperChargers'**; LIBCMT/LINK 11886 |
| Engine | **Alchemy**, SuperChargers' line, not Lux. SSA's inventory (297 entries): **181 found, 52 strict** (SuperChargers 177 / 52, Trap Team 177 / 52): the XDK and CRT only; every Lux engine hook is missing (`inflate`, `Ngl_SceneTraversal`, `nglInit`, `nglPresent`, `LightTile_ClassifyJob`, `Movie_Update`, `NglCurrentScene`) |
| Sigmatch, SuperChargers' inventory (32) | **32 found, 27 strict**: D3D **11 of 11 strict**, `Resolve` included; XMA 5 strict + 3 at 0.9; LZMA 1.0; the ten helpers 1.0; `BinkOpen` / `BinkClose` 0.9 |
| Sigmatch, Trap Team's inventory (43) | **41 found, 29 strict**: D3D 10 of 11 strict (`Resolve` 0.62); `Console_Language`, `Scene_Tiles` 1.0; `Gfx_ConfigInit`, `Scene_SurfaceTiles` 0.9; `Language_Get` 0.61, `Movie_Open` 0.76, `Ultrawide_ScenePass` 0.53; none: `Language_LoadPack`, `Ultrawide_Projection` |
| Sigmatch, Swap Force's inventory (50) | **39 found, 23 strict**: D3D 5 strict (Swap Force's D3D build differs); engine hooks weak (`SaveTask_Poll` 0.77, `NightmareUnlocked` 0.79, `Language_Get` 0.62, `Font_CharMetrics` 0.61) or none (`Zone_IndexByName`, `SwapWait_Status`, `FallCheck`, `Language_SoundPak`, `Movie_Open`, `String_Intern`, both ultrawide hooks) |
| Reading | the XDK side is SuperChargers' build to the byte (`Resolve` 1.0 there, 0.62 against Trap Team); the game side follows SuperChargers too: the disc is `/archives/*.pak` (4995) with `/archives/shaders_xenon.pak` and 133 Bink 2 `.bk2` movies; the Alchemy components are labelled `2016-09-08 ... Activision_ToysForBob_SKY6`; the portal library is `[TFB Portal]` (13 log formats). Toys for Bob made it on Alchemy, as Giants and Trap Team |
| D3D device | `tools/xmatch.py .:trapteam imaginators offsets` over the ten D3D functions that are strict against Trap Team: the displacements that differ are global addresses (lo halves), no device field moved; Trap Team's device constants are kept, as SuperChargers does [?] |
| Shaders | `shaders_xenon.pak` (677776 bytes) is **IGA v11 big-endian**, as SuperChargers'; `TRAPTEAM_ARC=<it> ArkeyanArtTest` opens it as a verified v11, every entry named, unpacked to its size and an IGZ (237 checks, 0 failures) |
| XenonRecomp | **one new instruction**: `mulhd` (1 site, `0x83277E6C`; the signed 64-bit multiply high), added to `patches/xenonrecomp.patch` with two samples in `tools/xenonrecomp/` (`ArkeyanXenonRecompTest`: 89 checks, 0 failures). Then a clean log: **501 jump tables** (15 `bctr` unresolved: 12 at `0x8248A46C`-`0x8248A548`, one stride of 0x14, the tail-call table SuperChargers also has, and 3 at `0x82BC271C`-`0x82BC273C`) [?], 40071 explicit functions (pass 2 brings one, `0x835032A8`, already in the mapping and overlapping pass 1's `0x835032D0`: not added), **108887 functions** in the mapping (SuperChargers 102087), 429 files, 334 MB, 4 s |
| Tool fix | `find_switch_tables.py` wrote `size = 0x-4` for `0x83240984`, a data word pointing at the FuncInfo word of the handler pair before `0x83240988`; XenonRecomp's TOML parser aborts on it. Entries of no size are now dropped (no earlier game had one) |
| C++ EH | 830 `__CxxFrameHandler` sites (`0x836C9F08`), 61 `__C_specific_handler` (import thunk `0x8377C824`), a custom handler `0x836DCE70` before `0x836DCE4C`: found as the only words in `.text` that point into `.text` and sit 8 bytes before a `.pdata` start (sigmatch's helper inventory has none of them) |
| Imports | **17** beyond SSA's, Giants', Swap Force's and Trap Team's: SuperChargers' ten (its `imports.cpp`, compiled in through `games/imaginators/imports.cpp`) and seven new: `XamGetLanguage` (the same answer as `XGetLanguage`), `NetDll_XHttpSetOption`, `XamBackgroundDownloadSetMode`, `XamShowMarketplaceDownloadItemsUI`, `XamUserGetMembershipTierFromXUID`, `XamUserGetOnlineCountryFromXUID`, `XNetLogonGetTitleID` (offline answers, first call logged) |

## Creation Crystals and the toy types (leads, 2026-11-04) [?]
From the image's strings only (names, no code read yet):
- **The tag enum** (`kTfbSpyroTag_ToyType_*`, 415 names) lists every toy family since 2011 by name: characters by year, items, traps,
  expansions, legendaries, sidekicks, Swap Force halves, vehicles and villain drivers (SuperChargers), and a `2016` block:
  **`CYOS`** (Create Your Own Skylander: the crystals), `Print_2d`, `Print_3d`, the Senseis' generic bases (`Character_SmallGeneric_Base`...),
  `Character_CrashBandicoot`, 15 `Character_Prototype_NN`, and 13 **`DOOM_Class_*`** (Doomlanders, the villains' versions of the classes).
- **Crystal classes**: `kTfbSpyroTag_ToyType_CYOS_Class_{Base, Bazooka, Blades, Club, Doubler, Fist, Mage, Pet, Pistol, Sniper, Sword, Thrown, Whip, Max}`:
  ten playable classes plus two (`Doubler`, `Pet`) whose role is unknown; the in-game names (Bazooker, Brawler...) are not in these strings.
- **The created character's data**: part fields (`_cyosPartType`, `_cyosPartComponentNamesTable`, `_cyosPartTypeNamesTable`),
  stats (`_cyosArmor`, `_cyosAttack`, `_cyosSpeed`, `_cyosLuck`), `_cyosBlob`, `_cyosPortrait`, `ECYOSCharClassType`, `ECYOSPartType`; the
  editor (`CGuiBehaviorCyosPartViewer`, `_cyosEditorOpened`), the creation flow (`_creationFlowEnabled`, `_cyosCreationFlowPlayerId`,
  `_creationCrystalIntro`), loot chests (`kTfbSpyroTag_ToyType_Imaginite_Chest*`, `CGuiBehaviorCyosToyChests`). A crystal tag likely
  carries a blob of part choices and stats that the game writes back (`_cyosBlob`) [?].
- **Other tag families**: `kTfbSpyroTag_Sensei_*` (31), `kTfbSpyroTag_Villain_*` (49), `kTfbSpyroTag_Hat_*` (279), `kTfbSpyroTag_DecoID_*`
  (132), `kTfbSpyroTag_Creator_*` (11). Vehicles and traps are still read (`_vehicle*`, `_trapsOnPortal`, `_trapToy`).
- **Next**: the toy and variant tables from the disc (method in [collection.md](../../../docs/runtime/host/collection.md)) against the 31
  Sensei rows of the catalogue (601 to 631, public lists so far); then the crystal's data layout, read from the code that fills `_cyosBlob`,
  and a crystal tag the generator can make.

## The created character's portrait (#47, 2026-11-11) [V]
A created character's HUD portrait (the round picture at the bottom left) is made on the CPU when it arrives; a Sensei's is
a disc picture. Read in runs of the hub with a virtual crystal (Air Knight, level 14) and hooks that logged the calls:
1. `sub_82A328F8` (the job, on the "CLoadThread Render Thread"; strings `cachedScene`, `standard_renderer`) draws the
   character into the frame and resolves it (`D3DDevice_Resolve`, 2560x1440 at scale 2) into a 1280x720 k_8_8_8_8 texture;
2. it allocates two 480x480 RGBA images (`igImage2`), copies the frame's centre crop (400, 120, 480x480) into the second's
   texture on the CPU (`sub_8268AD50` -> `sub_8268CB80`, the engine texture objects with their D3D texture at +4), then
   reads that texture back into the first image (the context's slot 0xEC, `sub_8268C860`: a read-only lock,
   `sub_828799B0` -> the XDK's `sub_828754C0`, then the XG copy `sub_8364C140`);
3. the HUD component (`sub_82E480C8`) shrinks it to a 128x128 linear image (`sub_82642188(image, source, 128, 128, 1)`),
   gives it to the HUD's `igGraphicsMaterial` (`sub_826FB5E0`) and sends `CGuiEventCyosPortraitReady`.

Two steps read zeros, so the portrait was black on every backend, texture pack or not: the resolve existed only on the
host GPU, and the read-only lock returned the cached `0xC0000000` view, which does not alias the allocation here. Fixed by
writing the resolve back before step 2's copy (`games/imaginators/portrait.cpp`, `d3d::WriteBackResolvedTexture`) and by
mapping the lock's address to the allocation's window (`GAME_HOOK_D3D_LockResource`); both are in
[render_bugs.md](../../../docs/runtime/gpu/render_bugs.md). Proof: the hub's HUD shows the character's head on Metal, on
MoltenVK, and with a texture pack active (`[render] resolve at 0x... (1280x720) written back to guest memory` once in the
log). The write-back is one read-back of the frame (a few ms) each time a portrait is made.

## Public background (2026-10-25)

These source notes predate the port. They preserve release, hardware and toy references;
engine, import and runtime findings above take precedence. Confidence: [2] two sources, [1] one, [inf] inferred.

### Release
- Oct 13 2016 (AU), Oct 14 (EU), Oct 16 (NA): PS3, PS4, Wii U, **Xbox 360**, **Xbox One**; Switch Mar 3 2017. [1] ([W-IMG])
- Developer: **Toys for Bob**, Activision. [1] ([W-IMG])
- Xbox 360 version exists, full game. Last Skylanders game on the 360. [1]
- **Title ID `41560930`** (European disc, Spanish + Italian). [1] ([IMR]). Media ID, TUs, DLC, disc size: **not found**.

### Engine
- Wikipedia: Alchemy [1] ([W-IMG]) Toys for Bob developed it, and their Giants is Alchemy (our boot,
  2026-10-25), so **Alchemy is likely** [inf]. **Decide on the disc.**
- Fan port is scaffolded from the Giants/SSA/SwapForce ports; German text can be taken from the PS3 disc. [1] ([IMR])
  No middleware named.
- Changes: Imaginators (player-built characters), Senseis, Creation Crystals. No Kinect found.

### Portal hardware
- Uses SuperChargers-class portals, plus a modified SuperChargers portal "in some regions" (three runes cover the trap slot);
  older portals work with limits. [1] ([Wiki-Portal])
- **Creation Crystals** are tags in the portal like any toy. VID:PID same families as other games; Xbox 360 `1430:1F17`
  [1] ([Wilson-gist]). Xbox One PID not found.
- Protocol additions: none known. [inf]
- Switch version does **not** support older portals, traps or vehicles (vehicles scan once for a free chest). [1] ([W-IMG]) Irrelevant to the 360.

### Toys
- 31 Senseis (20 new, 11 villains from earlier games); Imaginators are stored in **Creation Crystals** that can move between
  consoles; Dark Edition adds 3 crystals. The game accepts 300+ earlier character toys. [2] ([W-IMG], [Wiki-IMG])
- Tag: MIFARE Classic 1K family [1]; **Creation Crystal data layout (class, parts, name, stats) not found** in sources
  read. A fan port shows the class on the portal menu. [1] ([IMR])
- Compat: all earlier toys work as characters; vehicles and traps in the 360/One version: not confirmed.

### Saves and online
- Creation sharing between consoles is done through the crystal tag. Any **online service for creations** (gallery,
  leaderboards): not found in the sources read; check imports on the disc. Local multiplayer exists. [1]

### Port risks
- Fan port: boot, menus, first level playable on emulated portal (Windows, saves in own folder). [1] ([IMR]) Real USB portal
  not tried with this game. Xenia: nothing found.
- The new creation UI is the biggest unknown for the portal emulation (crystal writes while the game builds a hero).

### Sources
[W-IMG] https://en.wikipedia.org/wiki/Skylanders:_Imaginators · [Wiki-IMG] https://skylanderswiki.com/wiki/Skylanders:_Imaginators ·
[IMR] https://github.com/tsapZ/ImaginatorsRecomp · [Wiki-Portal] https://skylanderswiki.com/wiki/Portal_of_Power ·
[Wilson-gist] https://gist.github.com/skylandersNFC/5c9fb3debc11c1ea194ed6ddc9fb8faf
