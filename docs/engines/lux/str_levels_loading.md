# Disc files, STR containers, level loading, WideLoad

> **Purpose:** which files are on SSA's disc, their formats, and how a level loads · **Status:** current (2026-10-28) · **Tags:** [Lux] [SSA]

Class **[Lux]** (formats), **[SSA]** (names, addresses). Detail: [engine_notes.md](engine_notes.md) §3.8-3.9, [renderer_threads.md](../../runtime/gpu/renderer_threads.md), [figures.md](../../runtime/host/figures.md), [input.md](../../runtime/xbox360/input.md).

## Files on the disc (SSA: 1142 files, 8.10 GB)

| File / format | What we know |
|---|---|
| `sku.lxb` | read by `sub_82808190`: `"languages"` bitmask `0x7CF4` (dut eng fre ger ita spa swe fin nor dan allowed), `"default"` `0x10` (eng) |
| `text\<lang>\*.lxb` (`global.lxb`, `level_<n>.lxb`...), `ui\<lang>\uituning.lxb` | localized text and UI |
| `movies\<lang>\legal.bik`, `movies\*.bik` | **Bink 1**; `sl_sc01.bik` has **15 audio tracks**: 0 English, 1-5 music/effects, 6 fre, 7 dan, 8 dut, 9 fin, 10 ger, 11 ita, 12 nor, 13 spa, 14 swe |
| `audio\init.bnk`, `global.bnk`, `audio\<lang>\voice.bnk`, `audio\<lang>\levels\*.bnk`, `audio\levels\level_<n>.bnk` | Wwise sound banks; BKHD language id `0x0C` en-US, `0x0F` fr-FR |
| `audio\streaming.pck` | Wwise file package: **2055 streamed voice lines per language** keyed by the language codes; also streamed music (`.wem` id `0x0C45FC64` = title music) |
| `levels\<name>\mission.str` (level_027: 43 MB -> 85 MB), frontend `mission.str`, `screenmask.apk`, hub `mission_stage<n>.str` | level/screen packages: **zlib streams inflated 16 KB at a time** |
| `uigameimage.str`, `startmenu.str`, `hudmenu.str`, `pausemenu.str`, `global.str`, `uiimage.str`, `ui\eng\*.str` | UI images (`APKF`) |
| `shaders.lsa`, `effects.lsa` | [../gpu/shaders.md](../../runtime/gpu/shaders.md) |
| `nxeart` | dashboard art (could give launcher visuals) [?] |
| XEX resource named after the title ID | XDBF/SPA: title name (string `0x8000`) "Skylanders", icon (image `0x8000`) 64x64 PNG |

Disc names used: `game:`, `d:`, `\Device\Cdrom0` and symbolic links (all the same disc); saves mounted as `lux:`. Levels list for tours: every `\levels\<name>\mission.str` except `frontend` and `mytest` (`tools/pipelines/tour.sh`).

## STR container (`.str`)

`strm` magic, then **BE32 file size, BE32 1, BE32 1, BE32 record count** (uigameimage.str: `0x2E1`), then **one zlib stream** (36 462 756 bytes unpacked). **Records**: BE32 type hash, BE32 name hash, BE32 0, BE32 size, body.
(The launcher's disc-art reader inflates the stream at offset `0x14` of `startmenu.str` and finds APKF packages by their original asset path.)

## `APKF` image packages

Chunks: `\0XET` (name 0x30 bytes in, then the **D3D texture header** with the fetch constant after `FFFF0000 FFFF0000`) and `SYHP` (data offset at `+8` = `0x1000`, size at `+16`). Image bodies: DXT1/DXT5 etc.
Portraits: the **37 `toy_*_hud` images are 128x128 DXT5 (format 20), tiled, 8-in-16 swapped**; `toy_<name>_hud` names the legendaries `LegendaryRockDragon`, `LegendarySpyro`, `LegendaryGoldenCheat`, `LegendaryPandoranGuard`. Four magic items have 64x64 pictures
(`potion`, `crossed_swords`, `shield`, `speed_boots`). Glyph textures (`3dbutton\a_mark.dds` etc.) live in `global.str`, `hudmenu.str`, `pausemenu.str`. Launcher art: `...\startmenu\skylanderslogo.dds` 1024x512, `...\pausemenu\godraysdark.dds` 512x512.
Decoding: untile (Xenia `Tiled2D`), swap endian, BC decode ([../gpu/textures_formats.md](../../runtime/gpu/textures_formats.md)); `runtime/host/art/lux_images.cpp` and `xenos_texture.cpp` do it self-contained, for the launcher too ([game_art.md](../../runtime/host/game_art.md)).

## Loading [Lux]

- The loader inflates `mission.str` and the other zlib streams **16 KB at a time on the game thread** with the statically linked zlib ([zlib.md](zlib.md)): translated, ~30 MB/s = **86% of the level_027 load** (~2.7 of ~3.4 s, 85 MB out), 0.7 s before the title. Host zlib: mission.str in 0.3-0.5 s, load to first full frame 4.5 -> 0.95 s.
- **WideLoad** (async loads): after the first level's mission.str loads, the title waits for **all its WideLoad completion events** with `WaitForMultipleObjects` (`sub_82846398`, callers `sub_828464E8`, `sub_828465C0`) using the count of jobs it queued, **which can be 0**. On NT a
  count of 0 fails with `STATUS_INVALID_PARAMETER_1` -> `WAIT_FAILED`, which the title ignores. Blocking there hung the main thread (holding the portal lock) (D6). Same loader, same trap in the next Lux game.
- Level start uploads ~2857 textures (25.6 MB) in the first frame. A level's `mission.str` open is also where we wait for the pipeline list.
- Loading screens: the level's mission.str loads **on the render thread** itself (one long frame or a frozen loading screen, then the level).

## Which level a new game starts, and starting another one [Lux] [SSA] [V]

- **The first level is data, not code.** `levels\frontend\mission.str` (9646 bytes inflated) holds the opening movie as a
  `MoviePlayer` thing (`OpeningMovie`, movie `sl_sc01`) whose `nextLevel` field is `"Level_027"`; the executable holds no
  level name in lower case (only the save's `Level_<n>` keys). A package names strings by their **CRC-32** (zlib's, on
  the exact text): the field's value is `0x2F2FA0CC` (CRC-32 of `Level_027`), and the string table maps that hash to the
  pooled string. **The game goes by the hash**: changing only the pooled string still loads `level_027`; changing the
  hash (field and table key) loads the level whose `gamedata.lxb` name (`SpyroLevels`, `Levels/<Name>/mission`) has
  that CRC-32: `Level_021`, `Level_010b`, `Level_Hub`, `Challenge_Level_000`... (2026-10-28, isolated runs).
- **A level started that way is the game's own start**: the Skylander on the portal is created with its HUD (portrait,
  health, level) in story levels, which `SKY_LEVEL` (level_027's *files* read from another level's) never gave: there
  the game still runs level_027's start, so no Skylander is created outside level_027 [V].
- Test switch **`SKY_NEXT_LEVEL=<Name>`** (`runtime/inflate.cpp`, `SwapNextLevel`; scripts only): when the host
  inflates a stream holding `nextLevel` and the pooled `Level_027`, every `0x2F2FA0CC` in it becomes `<Name>`'s CRC-32
  and the pooled string becomes `<Name>` when it fits (9 bytes; a longer name keeps the old text: only the hash
  counts). Log: `[level] the opening movie's nextLevel is <Name> (SKY_NEXT_LEVEL, 2 hashes)`. Needs the host inflate
  (zlib or miniz builds; a build with the game's own translated zlib ignores the switch).
- **Package records** (from level_019's `mission.str`, static reading, 2026-10-28) [?]: big-endian records
  `[type][name crc][0][size][body]`; a scene body is `[5][string-table offset][N]` then N `(name crc, type-def offset,
  data offset)` entries (offsets from the body's start); a named object is `[name crc][self-relative pointer to its
  type][fields]`; an `Instance` starts with `matrix` (a `Matrix43`: X, Y, Z axes, then the translation; Z is up). Every
  thing appears twice in the stream. A private reader: `private/ssa100/bosses/re/things.py <inflated level> [filter]`.
- **The boss checkpoint** [SSA] [?]: levels 006, 017, 019, 024, 032 and 036 have one `PlayerSpawnPoint` next to a
  `GameDataCondition` `checkBossFightCheckPoint` (fields: `id` +0, `condition` +4, its value holding the CRC-32 keys of
  `SpyroLevelCollectionGameDataSet` and `bossFightCheckPoint` at +8 and +0xC, then 7 and 1; `trigOnTrue` +0x3C,
  `trigOnFalse` +0x50, a `Trigger` whose +4 is a pointer relative to itself). In level_019 its `trigOnFalse` points at
  the `telboss` trigger, whose target is `TeleportToBoss` (~50 units in front of the Stone Golem's fight trigger box);
  `trigOnTrue` is empty. The flag is set by `setBossFightCheckPoint` on reaching the fight: the game's retry-at-the-boss
  path. **Tried, not working** (2026-10-28, a new game in level_006 and level_019 through `SKY_NEXT_LEVEL`): making the
  condition compare with 0, then pointing `trigOnTrue` at `telboss` too, in the inflated stream; the log confirmed each
  edit, yet the Skylander spawned at the level's start both times. So the condition is not what places the first spawn
  (likely evaluated on a respawn only, or the flag is read from the save). Next leads: a scratch save whose level
  collection data has `bossFightCheckPoint` set (the save layout of `SpyroLevelCollectionGameDataSet` is not mapped),
  then a death or a checkpoint respawn; or moving the `PlayerSpawnPoint`'s instance matrix (translation at +0x24 of an
  `Instance`; each thing is stored twice) to `TeleportToBoss`'s. Levels 003, 008, 010, 018, 034 and 046 have no
  such flag.
- **Heroic Challenges** started this way [?]: `Challenge_Level_000` loads and its challenge runs (timer, "0 of 100"
  counter), but no Skylander or hero HUD appears in 100 s. The real game enters a challenge from the Ruins' challenge
  menu with the chosen hero, and challenge levels hold no `Player` thing (story levels do): the Skylander likely comes
  from the hub (`level_hub`) it was entered from (lead, not traced).

## Language-dependent files

`text\<lang>\...`, `ui\<lang>\...`, `movies\<lang>\legal.bik`, `audio\<lang>\...`: the folder name is the language code from the table at `0x82AA5EBC` ([languages.md](languages.md)).
