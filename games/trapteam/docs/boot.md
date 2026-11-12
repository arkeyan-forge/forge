# Trap Team boot: what broke on the way to the title and to a level

> **Purpose:** what stood between Trap Team's disc and the first level, the causes and the fixes · **Status:** current (2026-10-30) · **Tags:** [Xbox] [XDK] [Alchemy] [TrapTeam] [V]

## What broke on the way to the title (2026-10-30)
- **XenonRecomp**: no new instruction (the Swap Force patch is enough) and a clean log. `invalid_instructions` has the three
  handler entries re-found: `__CxxFrameHandler` `0x827ACE58` (237 sites), the `__C_specific_handler` import thunk
  `0x827E1F60` (60 SEH functions) and a custom handler `0x827BA230` before `0x827BA20C` [V]. The helper addresses
  (`savegprlr_14` `0x827ABF80` ...) are sigmatch's, all 1.0 and the same from SSA's and Giants' inventories.
- **Imports**: 232, five that no earlier game had: `NetDll_XNetQosServiceLookup`, `NetDll_XNetQosRelease`,
  `ObLookupThreadByThreadId`, `ObOpenObjectByPointer`, `XamGetLocaleEx` (`runtime/kernel/imports_trapteam.cpp`; outputs written,
  first call logged). Seen called in a run: `XamGetLocaleEx` at start-up (`r3=0x6F r4=0x2B r5=0x351C r6=0xD47`, signature
  unknown, answered with success) and `ObLookupThreadByThreadId` once, from the thread start-up `sub_8258AAEC` (answered "not
  found", the title goes on) [V]. `XexGetProcedureAddress` ordinals `0x24` and `0x50` are asked and not provided; harmless so far.
- **The portal thread starved the main thread** [V]. A guest thread named "Portal Audio" (`sub_821FC620`) reads the portal
  with `XamInputNonControllerGetRaw` **until a read returns nothing** (`sub_821FC708`), sleeps 2 ms, and repeats, all while
  holding a critical section the main thread (`sub_821FB2F8`) waits for. Our emulated portal answered every read of an active
  portal with a status report, so the loop never ended: nothing logged, no frame after `\permanent\global.arc` opened. Fix:
  `EmulatedPortal::SetStatusInterval`, one unprompted status per 8 ms (`runtime/host/portal.cpp`, test in `portal_test.cpp`).
  Found with `sample` and `tools/perf/sample_threads.py` on the stuck process. The other games poll once per round and are
  unaffected by the interval (12 ms and more); to be confirmed by the harness [?].
- **Without a shader cache** the game boots and every draw is skipped (`[render] no translated ...`): one run with
  `SKY_DUMP_SHADERS` gives the first containers (69 by the title, 225 by the tutorial's start), `build_cache.sh` takes about
  15 s, a relink 30 s. 189 of 225 translate; 36 vertex shaders stop on `TexCoord 8`.
- **Hooks**: D3D (11) and XMA (8) from Giants' inventory (27 strict), `LzmaDec_DecodeReal`, `BinkOpen`, `BinkClose` from
  an 80-instruction loose match of Swap Force's (`addresses.h`). `SKY_LZMA_CHECK=1` over a whole run to the tutorial: 0 calls differ,
  so the LZMA hook is right [V]. Every Alchemy engine hook of Swap Force (zone, language, string, font, ultrawide, save
  task) is **not found** by the same match: those functions changed (or moved), re-find them by hand.
- **Portal traffic** (`SKY_PORTAL_LOG=1`): `R` (answered `02 1B`, accepted), `A 00`/`A 01`, `S`, then `V 00 01 3F` (4 bytes, later
  `V 00 01 19`), `L 01`, `C`, and `J side r g b D0 07` (a 2 s fade, little endian) every few seconds on the title. `V`, `L`, `J` are
  ignored with one log line each; no `M` yet (no trap, no villain) [V].

## What broke on the way to a level
- **Title**: "Press A to begin" at 0.3 s of play for the legal screen, 15-37 s for the title (a 20 s wait on the legal screen
  in some runs, cause unknown), 30 fps with the default fps limit.
- **A on the title**: the autosave notice ("Skylanders Trap Team uses an Autosave feature..."), rendered correctly, then
  `Save_Header` and `Save_Slot_1..4` are created, `\movies\fmv1.mov` opens (the opening, 6941 frames, skipped), and
  `\level\level_313_tutorial.arc/.bld` (a first level) opens at about 59 s with an A every 4 s. The main menu was not
  captured; with a fresh profile the story starts at once, or an A passed it [?].
- **Fault in the first level: the video pool is full** [V] (2026-10-30). `Fault (signal 10 bus) at guest address 0x00000060` on the render
  thread, in `sub_8258B7E0` (a `memcpy`) called from `sub_823F0128`, which copies each vertex buffer of a mesh into memory it takes from a pool.
  Followed with `SKY_ALLOC_LOG=1` and temporary hooks on the game's own allocator (not committed):
  - **Nothing we provide returned 0**: 3063 `MmAllocatePhysicalMemoryEx` and 4 `NtAllocateVirtualMemory` calls, all non-zero; the five new imports
    are not involved ([kernel_exports.md](../../../docs/runtime/xbox360/kernel_exports.md)); a memory-limit "out of memory" line never appeared.
  - The null comes from the game's pool: `sub_823658D8` (allocate in pool `0xA003A278`, the 298 MB video pool of `alchemy.xml`'s `vramSize`)
    returns 0 for a 0x5064-byte vertex buffer, after 37714 successful ones; the pool's free list then has **10 blocks, 18 KB**. The mesh loader
    does not test the result, so `Lock` returns 0 and the `memcpy` faults.
  - **The pool is nearly full before the level**: at the first level's start 254 MB are live and 15 MB free. The boot allocates seven big blocks
    from it (`sub_82366070`: 18 MB, **160 MB**, 13 MB, four of 9 MB); the tutorial's vertex buffers need more than 15 MB.
  - The copy is the config option `Gfx/@enableVramCopy` (string at `0x821446F0`, read by `sub_823ED808` into three bytes at
    `0x8285D401`-`3`, default 1; `alchemy.xml` does not set it). Forced to 0, the tutorial loads, its intro stream (`313_intro_igc_stream`) opens and
    about 850 draws a frame render at 30 fps for the 130 s of the run, **no fault** (a frame: the 3D character is drawn, the left half of the picture is black [?]).
  - **Fix: a game hook** (`games/trapteam/vram_copy.cpp`, `GAME_HOOK_Gfx_ConfigInit`) sets the three bytes to 0 after the original. It is a workaround: **why the console
    has room for the copy is not proven** (a different size somewhere in the boot's big blocks, a block freed there that is not here, or a budget
    the game derives from a value we report). Ruled out: allocators and imports returning 0, the LZMA hook (checked), short reads. To do for the real cause: who
    asks for the 160 MB block (`sub_82366070`'s caller above `sub_823655A4`) and what sizes it. The store watch was not needed.

## Memory queries (2026-10-30) [V]
`SKY_ALLOC_LOG=1`, a run to the first level's load; the game's import table has these memory queries (`MmQueryStatistics` and the Win32-style
`GlobalMemoryStatus` are not in it; `XboxHardwareInfo` is a data export with no memory size):

| Import | Calls | What it returns here | What a 512 MB console returns | Does the 160 MB block follow? |
|---|---|---|---|---|
| `MmQueryAllocationSize` | 6092 | the allocation's size rounded to its page size: `0x10000` for 6088 calls (the game's small large-page physical allocations of 0x1C-0xB8 bytes), 4 others `0x00050000`, `0x00210000`, `0x00810000`, `0x09020000` (its real big ones) | the same: a large-page block is 64 KB granular [?] (not measured on a console) | no: the queried addresses are the allocations the game just made |
| `MmQueryAddressProtect` | 10298 | `0x04` (PAGE_READWRITE) every time | `0x04`, or `0x404` for write-combined memory such as the video pool [?] | no |
| `NtQueryVirtualMemory` | 0 (imported, not called) | n/a | n/a | n/a |
| `MmQueryStatistics` | not imported | n/a (ours: 512 MB of 4 KB pages, available = total minus allocated) | n/a | n/a |

The pool's size is `vramSize` in `alchemy.xml` (298000000, the 298 MB block `MmAllocatePhysicalMemoryEx(0x11C31E80)`), the same on every machine; the big blocks
inside it (160 MB and the others) are asked for by `sub_82366070`'s callers with fixed sizes. Nothing the game queries could change them. Open: who sizes the
160 MB, and whether the console's `MmAllocatePhysicalMemoryEx` of 28-byte large-page blocks costs 64 KB each as ours does (about 3000 of them were live at the peak) [?].

## Past the "Place a Skylander" prompt (2026-10-30) [V]
Figure: a blank **Trap Master**, Gusto (type 450, Air), made and placed by `SKY_PORTAL_PLACE=new:450` with `SKY_PORTAL=emulated` (`ArkeyanFigureTest` reads it as valid). The game
**accepts it**: no "wrong portal" screen, no "301" error. Portal log: `R` answered `02 1B`; then `Q 51 20` (the first read, answered with block 0), `Q 51 10 <block>` for 52 reads
and 23 `W` writes in a run (block 1 holds `C2 01`, the toy type 450 as a little-endian variant word); `J` (the light, 145 per run) and `V`/`L` stay unanswered, `M` never comes.
The prompt ("PLAYER ONE, PLACE A SKYLANDER ON THE TRAPTANIUM PORTAL") disappears and the game goes through a vortex loading effect into the level.

Frames of `level_313_tutorial` (level opened at 59.6 s, `SKY_DUMP_FRAME=<s>`, presses: A every 4 s, left stick up/right/down in turns from 106 s), 30 fps (frame median
33.3 ms, p95 33.4-33.7 ms, 1450-1740 draws), no `[guard]`, no fault, no missing file beyond the empty-name probes `\item\.arc`/`.bld` (as at the title):

| Run, second | Picture (`private/runs/<run>/shot.png`) | What is drawn |
|---|---|---|
| `f2`, 100 s | vortex | the blue space vortex with sparks, loading the level; no HUD |
| `f3`, 130 s | Gusto on the path | the whole tutorial scene: ground, grass, cliff, ropes, barrels, an NPC at the left, the purple goo; Gusto (blue, with his boomerang weapon); **HUD**: portrait, name "Gusto", health 400, level 1, coin counter 0 |
| `f4`, 160 s | Gusto in the air | the same place from another angle, Gusto jumping with motion blur on him; the same HUD; the camera has followed him |
| `f5`, 190 s | Gusto back on the path | like `f3`, camera and goo in other places; the same HUD |

Nothing missing or wrong that is visible: textures, shadows, the HUD's text and numbers are right; the cliff texture is a little blocky [?] (not compared with a console).
The player **moves**: the scripted stick input changes his place and the camera between the frames, and a jump is seen in `f4` [V]. Not tried: attacks, enemies, the tutorial's
text boxes, music and voices (disk audio only), a second Skylander, a trap. No trap was met in 190 s of play.

## The first trap: not reached (2026-10-30) [V]
Runs with `SKY_LEVEL` and a scripted play (A every 4 s, then the left stick in turns, X and Y; `private/runs/p1`-`p5/s<seconds>.png`), Gusto on the emulated portal:
- **Too many traps:** with Gusto and ten traps (one of each element, 10 `new:21x` figures) the game shows **"There are too many Traps on the Traptanium Portal."** over the vortex
  and stays there (`p1/s130.png`, `s220.png`); with Gusto and one trap (`new:210:3002`, a Magic Trap) it plays. The limit is below ten; its exact value is not measured [?].
- **The tutorial** (`level_313_tutorial`, 600 s, `p3`): Gusto walks and jumps along the path, coins 0 to 21, the HUD stays right (health 400, level 1); no enemy, no trap request in 600 s.
  A cave with coins (`p3/s350.png`, `s550.png`) is where the blind script stays.
- **`level_314_tutorial_2`** (`p4`, "Pompous Point", `p4/s110.png`): a zone to explore with an element gate icon at the top left; no enemy in 260 s.
- **`level_300_birdcage`** (`p5`, 480 s): Gusto in the Birdcage zone, one orange creature near him at 270-450 s (`p5/s270.png`, `s450.png`); no villain, no capture.
- Portal log of all five: only `R`, `A`, `S`, `Q`, `W`, `J`, `V`, `L`, `C`; **no `M`** (the speaker), and the trap tag is never written (`new_210_3002.sky` keeps its creation time; the game's writes go to Gusto's).
  The game loads the Trap Master's archive as `character\450_typhoon` while its HUD says "Gusto": the disc's folder name differs from the public one [?].
- Frame rate 30 fps throughout, no `[guard]`, no fault.

**First blocker:** a villain has to be met and beaten, then its capture screen answered; a scripted walk without a map does not get there. It needs a recorded input script
(a human run, `SKY_PRESS_FILE` or a stick recording) to the first villain, or the owner playing once with the trap on the portal. Both are settled by
[the first capture](#the-first-capture-2026-11-02-v): the villain field and `M`.

### Second attempt, played by hand (2026-11-01) [V]
A run steered live (`SKY_PRESS_FILE` lines, a frame every 2 s looked at between presses; Gusto and one empty Magic trap on the emulated portal, `level_313_tutorial`), 1 h of play over four starts. **No villain was met and no capture was reached.**
- **The portal takes one trap.** Gusto plus two traps (Magic and Water) already shows "There are too many Traps on the Traptanium Portal." (as with four and with ten); Gusto plus one plays [V]. Anything that needs a second element has to restart the game: there is no way to swap a trap live from the press file.
- **The tutorial is playable by hand** [V]: Buzz's dialogue (A), barrels (X), the plank bridges (jump, hold the stick and press A for 0.4 s), a cannon whose hint says "Press RT to fire cannon" (RT launches Gusto over the stone gate to a chest and a rope bridge, with turtle enemies on balloons further on), a hint that each chapter holds 3 chests. The orange path east of Buzz and the river below the chest are dead ends: Gusto falls off the island and returns to a checkpoint (a blue pad at the cave mouth), over and over. The coin cave the first attempt ended in is that river bed [?] (same look, not proved the same place).
- **Stick input is camera-relative and the camera turns by itself**, so blind steps drift; a press shorter than a game frame is lost: hold taps 0.3-0.4 s. At a load average of 28-50 (other lanes building) the game ran at about 0.6 game seconds per second, so every hold lasted that much less in game time: judge the pace from the frames, not the clock.
- **Game writes seen** [V]: Gusto's tag is rewritten by the game in a run (52 `W`, gold 12, "452 s played", valid, sequence 5, read back by `ArkeyanFigureTest <dump>`); the empty trap's file is not touched (sequence 1, count 0), as in the first attempt. No `M`.
- **Press script** (`runtime/host/input.cpp`, [input.md](../../../docs/runtime/xbox360/input.md)): `lb`, `rb`, `lt` and `rt` now exist in `SKY_PRESS` and `SKY_PRESS_FILE` (the cannon needed RT; the docs said the shoulders were missing).
- **Not done [?]:** the first villain, which element and which level it is in, the trap prompt, the capture write, the reload, the evolved villain, the lost-and-caught-again case. The next try needs either a recorded route to the first villain (a person's run through the chapter, as a `SKY_PRESS` script with times) or a way to see the player's place; with the 0.6 game speed under load, a route should be recorded on an idle machine.

## The first capture (2026-11-02) [V]
A live run (`SKY_PRESS_FILE`, a frame every 2 s looked at between presses) from a fresh save, Gusto and an **empty Life Trap** (`new:217:3003`, Oak Eagle) on
the emulated portal, after the speaker fix ([portal.md](../../../docs/runtime/host/portal.md#the-speaker-trapteam-v-2026-11-02)). Runs `v1`-`v4` under
`private/runs/`; frames `shots/`, Toy Box pictures `tbshots_*.png`, presses `presslog.txt`.
- **The first villain is Sheep Creep (Life, storage id 20)**, in `level_313_tutorial` itself: its intro cutscene (`313_sheepintro_igc_stream`) plays at
  "Soda Flats" and the fight follows on a round cliff past the turtle gate, about 20 min of play from the level's start. The Gulper is the chapter's boss
  later in the same level (`313_GulperCapture_IGC`; not reached). The earlier attempts carried a Magic or a Water trap.
- **The route** [V]: Buzz's lines (A), the cannon (RT), the brick gate is a **turtle stack to push** ("Push the Turtles to clear your path": hold the stick
  into it 4-5 s, twice), "Soda Flats", Sheep Creep's intro, its monsters, the bounce pad, then the fight (X and B).
- **The capture** [V]: the wanted poster is ticked, a vortex, then Sheep Creep's face in the trap's crest with "Continue"; A, then "Press the TAG button
  to play as your captured enemy...", A. Portal: `M 4D 01` / `M 4D 00` around the voice lines (answered), then **21 `W` to slot 1**, blocks 9, 10, 12-14, 16-18,
  20-22, 24-26, 28-30, 32-34, then 8: copy A, its header last. Copy B (written by the game when the empty trap first came, 21 `W`, sequence 1) is not
  touched. The trap reads back `valid, count 1, selected 20, evolved 0, seq 2` with our codec; the decrypted diff is three blocks: block 8 (count 1,
  sequence 2, the CRCs), block 9 byte 0 = `0x14` (the villain at packed `0x10`, `figures::TrapVillainOffset`) and block 33 (packed `0x130`
  `C2 FF A3 80 FF`, where copy B has `C2 FF 01 80 FF`: the byte `0x132` changes on a capture, meaning unknown [?]). Test: `ArkeyanFigureTest` `TestTrapCapture`.
- **Playing the villain** [V]: LT turns Gusto into Sheep Creep (HUD "Sheep Creep", purple bar), it moves and attacks; LT again gives Gusto back. The game
  opens `\character\1020_sheep.arc` and `ally_icon_1020` when it reads the full trap.
- **Off and on** [V]: through the Toy Box (`SKY_TOYBOX_SCRIPT_FILE`, live items): the trap's sheet says "Holds Sheep Creep"; "Take off Trap" while Gusto
  plays: the game goes on with Gusto, no prompt; "Put on Trap": `Trap: the game took Oak Eagle (slot 1, 0.1 s after the change)`, the villain's icon is
  back in the HUD and LT plays Sheep Creep again. Taken off while Sheep Creep plays: the game gives Gusto back at once, no prompt either. The file is not
  rewritten by these (identical to the capture).
- **Restart** [V] (`v3`, the same collection and save, no `SKY_PORTAL_PLACE`): `2 figure(s) back on the portal`, the game loads `1020_sheep.arc` while the
  level loads, Gusto keeps his 123 coins, the HUD shows Sheep Creep's icon, and LT plays Sheep Creep.
- **Repeat it:** `TRAPTEAM_ISO=<disc> tools/regress/trapteam_capture.sh [<run>]`: the route (`tools/regress/trapteam_capture.route`, presses timed
  from the moment the tutorial level opens) then the restart, with log and dump checks. The route is played as Toy Box script items on the game's
  frame clock (`SKY_TOYBOX_CLOCK=frames`, [figures.md](../../../docs/runtime/host/figures.md)): replayed on the wall clock (`v4`), it drifted at once
  because other lanes held the game at 3-18 fps for a minute (Buzz's first line still on screen at 440 s; v2 ran at 30). On the frame clock
  (`cap1`, 2026-11-02): **PASS**, 6 of 6 checks; Sheep Creep's intro at 764.8 s of the run (762.6 s in `v2`), the capture at 1170 s (1180 s), and
  both end pictures (`capture/toybox_end.png`, `restart/toybox_end.png`) show Sheep Creep played.
- **No trap request seen** [V]: with the right trap on the portal the game never asks for one, and taking the trap off (Gusto or the villain playing)
  shows no prompt either, so the Toy Box has nothing to follow here. The game has the texts "Please return the Trap to the Traptanium Portal." and
  "Press < to use this Trap, or choose another." (`global.bld`); where they show is not known [?].
- **Not done [?]:** the evolved villain (its quest), a second villain in the same trap, a capture with no trap of the villain's element on the portal
  (the likely trap request), the Gulper, a real portal's speaker (the sound blocks are still dropped).

## Language (2026-10-31) [V]
`[trapteam] language` (`--language`, `SKY_LANGUAGE`) picks the text and, by default, the voices. Independent `voice_language` is now wired through the adapter; implementation, static evidence and the required real-run checks: [LB and independent voices](#lb-and-independent-voices). Code: `games/trapteam/language.cpp`.
- **How the title picks.** Every archive (`<name>.bld`, 653 characters and the levels) holds one `<LANGUAGE>.pak` per language of the disc: the English and German disc `ENGLISH` and `GERMAN`, the French and Dutch disc `FRENCH` and `DUTCH` (the two discs have the same 2133 file names, only the sizes and these entries differ; the voice lines of a pack are the `<character>/german/vo_*.wav` entries of the `.arc`, English being the unmarked ones). The language chooser `sub_821D29B8` reads the **console language** (`XGetLanguage`, wrapped by `sub_8258A230`) and the locale (`XamGetLocaleEx`, a stub that answers 0), tests which `3d_font_msg_<language>` files and packs exist, and keeps German for console language 3 and Dutch for 16, French for 4; **any other console language gives English, or French on the French and Dutch disc** (so our English console language showed French there with `nl`, the first run of this work: the text, the chapter screen and the font `3d_font_msg_french`). `Language_Get` (`sub_823229A0`, 17 cases of the same console language) makes the IG_CORE_LANGUAGE value (English 6, French 8, German 9, Dutch 5...: the enum's names sorted, `ARABIC` 0 ... `SPANISH` 21, `SPANISH_MEXICAN` 22, `SWEDISH` 23, `TAIWANESE` 24, `TURKISH` 25) that the app object keeps.
- **Hooks.** `Console_Language` (`sub_8258A230`) answers the console language of the host's language (the XDK's numbers: de 3, fr 4, es 5, it 6, ja 2, ko 7, pt 9, zh 10, pl 11, ru 12, sv 13, tr 14, no 15, nl 16); `Language_Get` the value, which also gives the languages the console has no number for (da 4, fi 7, cs 3, hu 13, el 10 [?]). The console language of the kernel (`SetTextLanguage`, whose Dutch is English plus a country, as Lux reads it) stays as it is.
- **Proof** (2026-10-31, four isolated runs of 62 s, `SKY_LANGUAGE`, the first level's chapter screen, `snap` at about 40 s): French and Dutch disc `nl`: "HOOFDSTUK 1 / Frisdrankbronnen / Laden..."; `fr`: "CHAPITRE 1 / Source du soda / Chargement..."; English and German disc `de`: "KAPITEL 1 / Sprudelquell / Lädt ..."; `en`: "CHAPTER 1 / Soda Springs / Loading...". No fault in any. Without the console hook `nl` showed French and `de` showed German (the console had it).
- **Not proven [?].** The voices: the 2026-10-31 runs did not distinguish their streams and nobody listened. The independent pack and movie hooks added on 2026-11-01 still need the [real-run checks](#lb-and-independent-voices). Danish, Finnish, Czech, Hungarian, Greek, Spanish (Latin America), Polish, Russian, Japanese, Korean, Chinese and Turkish text selections are set by the same table and were not run on a disc; they are not offered as voice overrides.

## LB and independent voices

Static inspection of this title's translated code shows that keyboard `key_lb` (default Q) and the SDL left
shoulder reach the game as XInput `0x0100` (`runtime/main.cpp`, `runtime/kernel/input.cpp`). `sub_823E03E8`
decodes it into channel 10; `sub_82289360` copies it to controller +380 and merges it into Y (+364), just as
RB (+384) merges into B (+376). This explains the observed aliases at that controller layer; contextual trap
behavior and the unlocked third attack still need play checks. Issue #51 adds the missing LB row to
`games/trapteam/adapter.cpp`, so the runtime Commands page offers keyboard and pad binding, stored under
`[trapteam.input] key_lb` and `pad_lb`. The standalone launcher's corresponding `launcher/game_keys.cpp` is
outside this checkout and still needs the same LB row.

Text selection remains `Console_Language` / `Language_Get` and the app's language at +12 (`sub_821CF530`, global
`0x828940DC`). `sub_821CD058` chooses an archive's language pack from it; `sub_8223A780` sets a movie description's
+56 track to language +1. The adapter accepts `[trapteam] voice_language` / `--voice-language` /
`SKY_VOICE_LANGUAGE`: en/de and fr/nl on the respective regional discs. Unset voices follow text. Other codes
are rejected by the adapter; a supported code whose speech is absent logs a fallback to following text.

**Issue #51 (2026-11-11):** the previous preflight looked only for an uppercase language pack in
`permanent/global.bld`, and disabled the override when that pack was absent. A text pack is also insufficient
evidence that speech exists. `voice_disc.cpp` now uses the same `art::LanguagesOnDisc` detection as the launcher:
text packs in `character/Init_Setup.bld`, with `permanent/global.bld` as fallback, and localized speech names in
up to six character `.arc` indices. English lines have no language folder, and count as English only on an
English text disc; unmarked shared sounds on a French/Dutch disc do not imply English speech. Only index
metadata is read, after mounting and once per voice choice. Synthetic tests reproduce a valid voice choice
being rejected without a global pack and a text-only pack being accepted without speech.

`Language_LoadPack` (`sub_821CCFC0`, boolean success) still loads the voice pack before the text pack into the
same directory, only at `sub_821CD058`'s language-pack call (`LR=0x821CD4C4`, writable 2048-byte path). It restores
the path and complete caller context before the text load, including after a failed voice load. This follows
Giants' ordering; **Trap Team's first-sound/last-text precedence remains unverified in a real run**. `Movie_Open`
(`sub_824A2E40`) selects voice track en 7, de 10, fr 9 or nl 6 only for an ordinary language-selected audio
description (+32 from the worker, mode +40 = 0); silent and custom-track movies pass through.

`TTVoiceDiscTest` and `TTInputTest` exercise actual availability detection and adapter rows with synthetic
indices and host input only. Both failed with the previous logic and pass after the fix. `TTLanguageTest`
continues to cover pack ordering, all four codes, regional fallback, failed loads, caller restoration and
path bounds; its fixture now includes synthetic speech entries. No game or launcher was started for this change.

**A real run must prove:** using the [isolation block](../../../docs/getting-started/testing.md#isolation-every-game-run),
test en/de and de/en on one regional disc, fr/nl and nl/fr on the other. The log must show
`[language] voices de: <archive>.bld:/GERMAN.pak loaded before <archive>.bld:/ENGLISH.pak` (or the corresponding
language pair), then `[language] movie voice track 10 (de)` for a language-selected story movie. An absent
regional voice must instead say `[language] voices fr unavailable on this disc; following text` and leave
packs and movie tracks following text. Trace localized speech reads with `SKY_TRACE_FILES=1`; archive-offset
reads alone do not identify a spoken language. Capture unchanged text/subtitles and compare/listen to NPC,
character and story-movie speech in the requested language, including a level transition and character reload.
Compare unset and same-language voices with the original behavior. With the third attack available, compare Y
and LB on keyboard (default Q and rebound `key_lb`) and pad, plus B/RB. With `SKY_INPUT_LOG=1`, an isolated LB
press must give `buttons 0x0100`; the live binding log must name changes to `key_lb` or `pad_lb`. Check menu
navigation and both co-op controllers. Hook logs and synthetic checks alone do not prove audible language,
resource precedence or gameplay behavior.

## Still open
See [status.md](status.md) "Open problems".
