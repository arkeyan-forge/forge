# Giants: dated history of the port

> **Purpose:** the dated news and session entries of the Giants port, moved out of [status.md](status.md) on 2026-10-29; the current state is in status.md · **Status:** archive (history, not maintained) · **Tags:** [Giants] [Xbox] [Alchemy] [V]



## Status
- **Plays** from the title to Chapter 1 with music and voices (2026-10-25), and through the level tour: of 91 levels,
  **78 play** to the end of the run, **12 crash**, **1 stops**: all 13 are challenge levels started without their challenge
  set-up by the tour's redirect, not runtime bugs ("Challenge levels in the tour" below).
- **Figures done** (2026-10-26): every toy type Giants supports (SSA and Giants characters, giants, LightCore, sidekick,
  magic item, adventure pack), level 15 and hats; the smart portal (the prompt opens the Toy Box) works. Every variant
  the disc defines (Series 2, LightCore, Legendary, the alternate decos: 47 on SSA and Giants toys) is in the catalogue
  with the id the game reads, its portrait and versus artwork ([figures.md](../../../docs/runtime/host/figures.md#the-variant-id-what-each-game-reads-ssa-giants)).
- **Bink 2 movies are drawn** on the GPU, with the intro skip ([bink.md](../../../docs/engines/lux/bink.md), "Bink 2 in
  Giants"); button glyphs follow the device; ultrawide (Hor+); the fps limit 30/60; HD texture packs.
- **Voice language** (2026-10-26): the voices in any of the disc's 12 languages, whatever the text's (`voice_language`;
  [boot.md](boot.md#voice-language)).
- **Pipelines**: a 2363-pipeline baseline from tours with several figures (2026-10-27; was 1575 from one SSA figure), compiled during the loading screens.
- **Shaders** come from the disc, no dump: Arkeyan-Builder extracts all 4394 from `/permanent/global.bld`; a short run at
  install adds Bink 2's 50 ([shaders.md](shaders.md)).
- **Installs through Arkeyan Launcher** with kit 0.5.0 (macOS tested; "Installing Giants" below).
- Feature by feature against SSA: [parity.md](parity.md); what is left: [100.md](100.md).

## Performance (2026-10-26, 2026-10-27) [Giants] [Xbox] [V]
**Steady 30 fps and 60 fps at scale 2 on an M1 Pro** in Chapter 1, `level_203_the_machine`, `level_hub_2_stage_2` and
`arena_challenge_201_000`: frame gaps 33.3 +- 0.5 ms at the 30 limit (p99 33.4 ms, was 41-68 ms with spikes to 95-139 ms),
16.7 ms at the 60 limit (was 43-59 fps). The render thread works 7-9 ms a frame (was 15-24 ms). Two generic runtime fixes,
no game patch: the kernel woke every waiting thread at each signal (27 % of Giants' render thread in `NtReleaseSemaphore`;
[threads_sync.md](../../../docs/runtime/xbox360/threads_sync.md)), and macOS delivered the emulated vertical blanks up to
8 ms late ([video_vblank.md](../../../docs/runtime/xbox360/video_vblank.md)). Numbers per scenario, the profile and what
is left: [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md), "Giants". Measure with
`tools/perf/giants_run.sh` ([testing.md](../../../docs/getting-started/testing.md), "Frame times").

**2026-10-27** [V]: the render thread no longer hashes every big texture whole each frame (a 4 KB sample, the whole
texture every 8 frames and after a change: 21.9 -> 4.3-4.6 MB a frame in `level_203`; the texture hash 12-14 % -> 3-3.5 %
of the render thread; Chapter 1 at 60 fps work 13.8 -> 10.0 ms median, gap p99 27.4 -> 17.0 ms, quiet machine). The
**first-visit hitches** were pipelines the baseline lacked for figures other than the tour's SSA figure (a Giants figure
built 17-145 per level at 20-67 ms each, cold): the baseline now comes from tours with Tree Rex, Hot Dog and the other
Giants characters (1575 -> 2363); a figure no tour used builds 1-6 per level (was 23-102), and a new player's first
`level_203` with it has no frame over 40 ms after the first. Numbers: [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md),
"Giants (2026-10-27)".

**2026-10-28** [V]: the archives' **LZMA decoding runs as host code** (the LZMA SDK's `LzmaDec_DecodeReal`,
`sub_822BBA60`, [lzma.md](../../../docs/engines/alchemy/lzma.md); 131 072 calls checked against the translated one, 0
differences): 201 MB from start-up to Chapter 1 decode in 5.3-5.7 s of the workers' time instead of 9.5-11.0 s; the
level load from `level_213_tutotial.arc` to the level **1.7-2.3 -> 1.0-1.2 s**; in the level (the game streams) the
process's CPU per frame 17.7-18.1 -> 15.1-15.8 ms at 30 fps, work p99 17-18 -> 12-15 ms. The ~4 s before the `.arc`
(save, fade) are the game's own timing. Every optimization and which game has it: [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md),
"Optimizations per game".

## Figures, LightCore and the portal light (2026-10-26) [Giants] [V]
Giants plays with every toy type it supports, emulated (a real portal gets the same commands, untried with hardware).
Format and evidence: [figures.md](../../../docs/runtime/host/figures.md) (section "Giants' additions") and
[portal.md](../../../docs/runtime/host/portal.md) ("Portal light").
- **Format:** a second pair of areas (blocks 17-21 and 45-49: Giants' experience, the hat, its own CRC), `0x16` bit 0 of the
  first area, level table to 15 (96500 = 33000 + 63500). `figure_tag` reads and writes them; SSA figures stay byte for byte.
- **Toy types, each alone, run in Giants to Chapter 1 (offscreen shots, HUD):** an SSA character (Gill Grunt level 6, 3000
  gold), a Giants character (Swarm, level 1; Jet-Vac level 15, 40000 gold, hat 5), a LightCore (Jet-Vac `0x1206`, level 10,
  its glowing body), a sidekick (Terrabite, 505), a magic item (208), an adventure pack (301). Item + adventure pack
  together = "too many toys": one shared seat (the catalogue's Giants seats say so).
- **Catalogue and Toy Box:** the runtime now uses the build's game id (it was `"ssa"` everywhere: portal id, locks, Toy Box
  seats); the Toy Box in Giants lists the 72 toys Giants reads, creates blank ones (LightCore through "versions"), edits
  stats up to level 15 and shows the portal's colour.
- **Portal light:** only `C r g b` (faded all the time, 2081 commands in 200 s), no `J L M V`; the emulated portal keeps
  the colour, `[portal] light:` is logged, the Toy Box shows a swatch and a dot on a LightCore. A LightCore's own light
  is not figure data and not a command we saw [?].
- **The prompt adapter** (Giants' "Please put a Skylander" opens the Toy Box for the right player) works: see "Portal prompt" in
  [boot.md](boot.md). Run: no figure on the portal, `prompt for player 1`, Enter puts Jet-Vac, `the prompt is gone`, `closed`,
  `the game took Jet-Vac`, then the game's "Magic Moment" states and play.

Known leads: the disc has 8 pet items and 4 `Item_Location_*`; all 8 pets are in the catalogue, ran in play and share the item
seat (2026-10-27, [figures.md](../../../docs/runtime/host/figures.md)). Giants has no toy-type check in its code: its toy
definitions (`Init_Setup.bld`) are the check, and the catalogue matches them (`tools/toy_tables.py`). `private/` is git-ignored.

## Installing Giants through Arkeyan Launcher (2026-10-25, kit 0.5.0) [Giants]
A player drops the Giants disc, presses INSTALL, then PLAY, as for SSA. Design and contract: [kit contract](../../../docs/release/launcher/kit_contract.md) ("Several games in one kit").
- **Disc detection**: Arkeyan-Builder hashes `default.xex` (`12d1e29e...`) against `games/giants/game.txt` (the same hash as `info.cpp`; `make_kit.sh` checks that). Another disc: `@error` naming the SHA-256 and the supported games.
- **Code**: `games/giants/config/Giants.toml` now uses the kit layout (`../private/default.xex`, `../ppc`). XenonRecomp gives the same `ppc_context.h` as for SSA; `ppc_config.h` differs (memory layout), so the kit holds one runtime per game (`lib/giants/runtime`, `commands/giants/`), `kit.txt` has `ppc_config_sha256.giants`.
- **Shaders**: Arkeyan-Builder extracts them itself (`tools/arkeyan-builder/igz_shaders.cpp`: IGA v8, a small LZMA decoder, IGZ v6 scan; no Python): 4394 containers (264 vertex, 4130 pixel), the same count as `giants_extract.py`, in under a second. Then XenosRecomp, then the short headless run that dumps Bink 2's **50** shaders (9 s), the cache is rebuilt (4443 in the game: 4394 + 50 - 1 duplicate) and the game relinked.
- **Names**: `SGRecomp` (`SGRecomp.app`, `SGRecomp.exe`); the log stays `SSARecomp.log` in `Skylanders/Giants/`. The CMake target is still `SSARecomp`: a `-DSSA_GAME=giants` build gives `build-giants/SSARecomp/SGRecomp(.app)`.
- **Timing, M-series Mac, `--jobs 6`, kit 0.5.0** (Arkeyan-Builder's `timing` log lines): toolchain 0 s, disc 0 s (extraction included), recompile 2 s, shaders 445 s, compile 37 s, link 0 s, movies 9 s, shaders again 462 s, compile 28 s, link 0 s: **986 s (16.4 min)**. The shader translation was 95 % of it and ran twice (the second run re-translated all 4443 for 50 new ones).
  - **After the install-time work** (the install-time work, same Mac, same kit, the machine shared with other jobs, load average 10-70 on 8 cores, `--jobs 6`): before **977 s**, after **563 s**. Shaders 446 s then 450 s become 427 s then **56 s** (the second pass reloads 4443 translated shaders from `shaders/cache` and compiles the new ones). The first pass is bound by the Metal compiler (about 0.35 s of CPU per shader, 85 % of the translation), so it only drops on a quieter machine. Output: 4443 shaders compared, 0 differences (`tools/shaders/compare_cache.py`).
  - SSA, same conditions: 315 s before, 193 s after (shaders 100 s then 9 s).
  - The launcher shows `@progress` during both shader steps.
- **Check**: the built game reaches the title scene offscreen at 60 fps, `[gpu] shader cache: 4443 shaders`.
- **Windows and Linux kits** (2026-10-26): built and tested from the 0.5.0 kits (Windows cross build run under Wine, Linux in a container): both reach the tutorial level; the first runs faulted at start on x86 (XenonRecomp's 64-bit jump-table `switch`, fixed: [../../../docs/tools/toolchain/xenonrecomp.md](../../../docs/tools/toolchain/xenonrecomp.md)). [../../../docs/getting-started/platforms/windows.md](../../../docs/getting-started/platforms/windows.md), [linux.md](../../../docs/getting-started/platforms/linux.md).
- **App icon** (2026-10-27): on macOS the bundle's icon is the game's own, made from the disc at install ([game_art.md](../../../docs/runtime/host/game_art.md), "The app icon"); it was the neutral disc.
- **Game data copy** (2026-10-26, [game_data.md](../../../docs/runtime/host/game_data.md)): the whole disc copied and verified (988 files, 8.35 GB, 20 s on an SSD), then the game run isolated from the copy with **no disc image argument** for 60 s (3475 frames from the title, 30 fps limit).

## Level tour and pipeline baseline (2026-10-26) [Alchemy] [Giants]
`tools/pipelines/tour_giants.sh <iso> <work>` runs every `level\*.arc` of the disc (91 runs; the three arena challenges once; `Title`, `ChallengeVO` and mesh packs excluded) in an isolated run: new game, `SKY_LEVEL=<name>` makes the tutorial's `.arc`/`.bld` read from that level, an SSA figure, 80 s of walking, attacking and the pause menu. Result: **78 levels reached and played to the end of the run, 12 crash, 1 stops** (below). The baseline went from 459 to **1575 pipelines** (`games/giants/pipelines/baseline.zst`, 26 KB, hashes and states only). Draw-time builds (pipelines not in the old 459 baseline) summed over the 91 runs: 9817 (each run starts with an empty `pipelines.bin`). After: level_203_the_machine 215 -> 2, level_008_islandtown 135 -> 2, challenge_level_020 34 -> 3.

Loading wait: `OpenFile` waits for the pipeline list on every `\level\<name>.arc` open (the title and each level; `files.cpp`); the log has `[gpu] loading (<file>): ...` for each load.

| Level | Draw-time builds (before) | Median fps in level | Result |
|---|---|---|---|
| arena_challenge_200_000 | 51 | 41 | fault |
| arena_challenge_201_000 | 88 | 49 | ok |
| arena_challenge_215_000 | 60 | 60 | ok |
| challenge_level_000 | 52 | 29 | ok |
| challenge_level_001 | 46 | 0 | ok |
| challenge_level_002 | 58 | 35 | ok |
| challenge_level_003 | 38 | 0 | ok |
| challenge_level_004 | 23 | 0 | fault |
| challenge_level_005 | 46 | 0 | ok |
| challenge_level_006 | 46 | 0 | ok |
| challenge_level_008 | 40 | 0 | ok |
| challenge_level_009 | 56 | 0 | ok |
| challenge_level_010 | 76 | 0 | ok |
| challenge_level_011 | 52 | 28 | ok |
| challenge_level_013 | 66 | 59 | ok |
| challenge_level_014 | 41 | 0 | hang |
| challenge_level_015 | 48 | 0 | ok |
| challenge_level_016 | 38 | 0 | ok |
| challenge_level_017 | 54 | 0 | ok |
| challenge_level_018 | 31 | 0 | fault |
| challenge_level_019 | 68 | 0 | ok |
| challenge_level_020 | 34 | 0 | ok |
| challenge_level_021 | 40 | 0 | ok |
| challenge_level_023 | 68 | 53 | ok |
| challenge_level_024 | 46 | 0 | ok |
| challenge_level_026 | 70 | 0 | ok |
| challenge_level_028 | 43 | 0 | fault |
| challenge_level_029 | 64 | 30 | ok |
| challenge_level_030 | 48 | 0 | ok |
| challenge_level_031 | 74 | 0 | ok |
| challenge_level_032 | 45 | 0 | fault |
| challenge_level_033 | 65 | 57 | fault |
| challenge_level_034 | 53 | 67 | fault |
| challenge_level_035 | 66 | 0 | ok |
| challenge_level_036 | 70 | 60 | ok |
| challenge_level_037 | 44 | 26 | ok |
| challenge_level_038 | 73 | 60 | fault |
| challenge_level_039 | 44 | 51 | ok |
| challenge_level_046 | 86 | 0 | ok |
| challenge_level_047 | 48 | 0 | ok |
| challenge_level_048 | 21 | 0 | fault |
| challenge_level_049 | 72 | 42 | ok |
| challenge_level_050 | 56 | 0 | ok |
| challenge_level_051 | 55 | 58 | fault |
| challenge_level_052 | 48 | 21 | ok |
| challenge_level_053 | 67 | 60 | fault |
| challenge_level_054 | 64 | 58 | ok |
| challenge_level_055 | 23 | 0 | fault |
| credits_giants | 438 | 60 | ok |
| level_003_catapult | 207 | 60 | ok |
| level_008_islandtown | 135 | 60 | ok |
| level_036_haunted_expansion | 265 | 60 | ok |
| level_046_dragon_expansion | 175 | 60 | ok |
| level_200 | 221 | 56 | ok |
| level_201 | 181 | 59 | ok |
| level_202_arkeyancopter | 171 | 60 | ok |
| level_203_the_machine | 215 | 60 | ok |
| level_204_choices | 219 | 60 | ok |
| level_205_piratecards | 237 | 60 | ok |
| level_206 | 265 | 60 | ok |
| level_207_arkus_adventure | 215 | 59 | ok |
| level_208_ice | 373 | 59 | ok |
| level_209_giantrobot | 97 | 60 | ok |
| level_210_thevillage | 131 | 60 | ok |
| level_211_final_level | 177 | 60 | ok |
| level_212_defend_the_ship | 167 | 60 | ok |
| level_213_tutotial | 31 | 60 | ok |
| level_214_tutotial_2 | 381 | 59 | ok |
| level_215_enter_arkus | 185 | 60 | ok |
| level_hub_2_stage_1 | 409 | 60 | ok |
| level_hub_2_stage_2 | 487 | 60 | ok |
| level_hub_2_stage_3 | 401 | 60 | ok |
| level_hub_2_stage_4 | 453 | 60 | ok |
| pvp_level_001_cubedungeon | 59 | 66 | ok |
| pvp_level_002_cyndersexp | 63 | 61 | ok |
| pvp_level_003_darkforest | 71 | 0 | ok |
| pvp_level_004_ancientsewer | 45 | 0 | ok |
| pvp_level_006_troll | 53 | 60 | ok |
| pvp_level_007_spikes | 85 | 60 | ok |
| pvp_level_008_pirategrotto | 99 | 41 | ok |
| pvp_level_009_legendary_ancientsewer | 55 | 0 | ok |
| pvp_level_010_ice | 59 | 0 | ok |
| pvp_level_200_two_towers | 57 | 61 | ok |
| pvp_level_201_three_islands | 49 | 0 | ok |
| pvp_level_202_element_wheel | 47 | 60 | ok |
| pvp_level_203_powerup_pedestal | 55 | 59 | ok |
| pvp_level_204_cliff_jumping | 41 | 0 | ok |
| pvp_level_205_tech_chamber | 63 | 0 | ok |
| pvp_level_206_slippery_tunnel | 31 | 0 | ok |
| pvp_level_207_ice_bowl | 43 | 0 | ok |
| pvp_level_208_alternate_wheel | 41 | 0 | ok |

**Crashes (12):** `arena_challenge_200_000` and 11 `challenge_level_*` (004, 018, 028, 032, 033, 034, 038, 048, 051, 053, 055). **Stops (1):** `challenge_level_014`. Both explained below: redirect artefacts.

### Challenge levels in the tour (2026-10-26) [Alchemy] [Giants]
**Verdict: the 13 failures are artefacts of the `SKY_LEVEL` redirect, not runtime bugs.** A challenge level is not a
whole level: the game loads it on top of a shared **base mesh** (`level\challengeMesh_{000,002,006,034,ice,outdoor}.bld`)
inside a challenge session. The redirect swaps the tutorial's `.arc`/`.bld` for the challenge's and nothing else, so the
base mesh is never loaded (no floor is drawn: only the props, in black) and no challenge session exists.

**The crashes: no physics world.** [V]
- The fault: `sub_82246390`+0x30 (`lwz r11,120(r10)`) reads `0x78` of the global at `0x82673738`, which is null. That
  global is the Havok world (the level's `tfbPhysicsWorld`): made the first time such an object is activated
  (`sub_82242678`, a vtable slot, -> `sub_82243E90` -> `sub_822433F8` stores it), released by `sub_82246EC8` ->
  `sub_82244120`. Every other reader of it tests for null; `sub_82246390` does not. It is called by `sub_821AC838` (the
  actors' update) with the physics bodies of two actors (`+124` -> `+28`) and reads a pair table from the world (`+0x78`:
  10 doubles copied, `+0x80`: a table indexed by both bodies' materials, a callback at `+2736`): a contact query [?] for
  the exact meaning. So it runs at the first contact between two physical bodies (the player and a push block, a mine
  cart...), a few seconds after the load.
- Traced with temporary hooks (not kept) on `challenge_level_004`: the title's world is set, cleared when the title
  unloads, and never set again: `challenge_level_004.bld` is loaded alone, then the fault.
- On the disc, `tfbPhysicsWorld` is in every `challengeMesh_*.bld` and in 9 of the 45 challenge levels (006, 010, 011, 014,
  017, 019, 024, 026, 029) and in no arena: **none of the 12 that crash has one**; 8 of the 9 play, 014 stops (below).
  The 27 other levels without a world played the 80 s with no contact between two bodies; they may fault on another run, so the tour's
  allow-list (`private/test-refs/giants/tour.txt`) lists all 39 levels without a world and 014, each group with its reason.
- The game's own path loads the mesh: the challenge menu's script (`misc\UI_HeroicChallengeSelect.bld`) has
  `System - Get Challenge Base Mesh Load` ("return base mesh string"), `Game - Load Challenge Level`, `Load Level Stream`
  and `System - Set Game Mode` ("game mode - heroic challenge"). Which mesh goes with which challenge is decided there
  (no level references its mesh by name) [?].

**The stop of `challenge_level_014`: the challenge fails.** [V] for the trigger, [?] for the loop. This challenge forbids
hurting the Purple Chompies; the tour's attacks hurt one at ~44 s, Cali says "...endangered Purple Chompies. I warned
you about that." (offscreen shots), and the frames stop. The process lives: the main thread runs the level update
(`sub_821E93F8`) ~1100 times a second (60 before) and never presents. The end-of-challenge flow has no challenge session
to return to (restart or leave) in a redirected story game. Not every run hits a Chompy in time (one of four did not).
The level has its own physics world and does not fault.

**The real flow, read from the disc's scripts (2026-10-27) [V] unless marked.** A challenge is three things the game
loads together: the select UI (`misc\UI_HeroicChallengeSelect.arc/.bld`, `MiscLoad_ChallengeSelect`), the challenge's own
level (`Game - Load Challenge Level`, defined in that bld: "challenge number" -> `Challenge_Level_BaseString` + number) and
a **base mesh**, which `System - Get Challenge Base Mesh Load` (defined in `permanent/global.bld`) builds as
`Challenge_Level_Mesh_BaseString` + one of the six suffixes `Challenge_Mesh_Suffixes` = {000, 002, 006, 034, ice, outdoor}:
it loops over the suffixes and picks the one whose set ("challenge mesh sets", a structure `Heroic Challenge Mesh Sets` with
a member `challenges`) holds the challenge's title. **Which challenge uses which mesh is data I did not find** (not in
`global.bld`, the select UI, the hub or the challenge levels) [?]. The meshes' themes, from their own files: 000 mine,
002 spooky dungeon, 006 island, 034 level 34's art, ice icy alleys, outdoor grassy castle. The select UI is opened by a hub
actor's behaviour (`System - Show Heroic Challenge Select UI`, then `wait for heroic challenge ui` until the UI stream is
loaded; every `Level_Hub_2_Stage_*` has it) and by the challenge levels themselves (their end and failure screens), and
challenges "unlock through store" (`StoreItem_HeroicChallenges`, Auric's shop in the hub).

**The real flow was entered (2026-10-27, second pass) [V].** With the run steered from outside (`SKY_PRESS_FILE`,
[`tools/regress/live_run.sh`](../../../tools/regress/live_run.sh): presses decided by looking at the game's frames, not by the clock)
a new game started in `level_hub_2_stage_1` / `_2` (a figure with 65000 gold) reaches **Cali** on the upper deck (a Y icon over
them; Y talks, and her lines end in the question **"Play a Heroic Challenge?"**), YES loads `misc\ui_heroicchallengeselect`
("SELECT CHALLENGE": the first one offered is **Arachnid Antechamber**, "Defeat 8 giant spiders in this dungeon"), A on it loads
`level\challenge_level_016.arc/.bld`, then **`level\challengemesh_002.bld`** (the spooky dungeon mesh: base mesh and
challenge are matched by the game itself, as read in the scripts) and `challengevo`. The level starts (the 1:57 timer, "0 of 8",
the floor and the dungeon drawn) and was played for a minute and more with walking and attacks: **0 faults, 0 `[guard]`**; the
timer ran out, "Do you want to try again?" appeared, NO went back through `ship_part_*` to the hub. The pause menu of a challenge shows Restart
Challenge, Flynn's Ship (confirm) and Main Menu, as read. So the redirect's artefacts are now **confirmed for the one challenge
that can be reached this way: a real run of 016 does not fault** (the tour's 016 was already "ok"). **Only that challenge is
offered**: pressing right in the select UI does not move (the others are locked; the 65000 gold did not buy them, and the store
purchase was not done: Auric's shop was reached, the purchase screen was not). Per level: 016 (mesh 002) real run OK;
the other 44 challenge levels and the 3 arenas: not run for real (not unlocked); their tour results stand. The tour was not changed:
the real path needs the hub, Cali and the UI (about 2 minutes of presses per challenge) and the other levels are locked.
What would finish it: unlock all challenges (a save with the Heroic Challenges bought: Auric's store, or the unlock bits in a save), then
repeat per challenge with `live_run.sh` (one UI "right" per challenge).

**What was run in the first pass.** (1) Each of the six base meshes alone (`SKY_LEVEL=challengemesh_<suffix>`: the `.bld` opens, the missing `.arc`
is refused with 0xC0000034 and the game goes on; 034 has an `.arc` too): all six present frames for 90 s with **0 faults**,
so their data (shaders, physics world, scripts) runs in our runtime. (2) The hub (`level_hub_2_stage_1`) with a figure
holding 65000 gold: the deck, Auric's first-visit cutscene and dialogue and his shop's room were reached; the store, the
purchase and the select UI were **not**: blind navigation with timed presses (the game's speed under a loaded machine moves
every step) did not get there in that pass. (3) `challenge_level_014` again: frames stop about 45-49 s in, as before, all
threads idle or waiting after the last read of `permanent\global.arc` (a UI load) [?]: the end-of-challenge flow, not
a hang we can tell from a missing session.
(The result of that pass, before the real flow was entered: no runtime bug found.)

**How a player reaches them.** [?] (from the hub's scripts, not played) The Heroic Challenges are bought in the
Dreadnought's store (`StoreItem_HeroicChallenges`, "challenges unlock through store" in `Level_Hub_2_Stage_1.bld`), then
opened from the hub (`System - Show Heroic Challenge Select UI`); the pause menu of a challenge offers Restart Challenge,
Flynn's Ship and Main Menu. A save with them bought, or the select menu's own load request, would test the real path; the
tour does not have one. Starting the base mesh through another package request of the start-up (tried: the empty
`character\.bld` request) loads it during the title, and its world goes with the title's.
