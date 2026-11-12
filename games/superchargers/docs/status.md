# Skylanders: SuperChargers (superchargers)

> **Purpose:** which disc SuperChargers needs, where the port stands, and where to start · **Status:** current (2026-11-04) · **Tags:** [Xbox] [XDK] [Alchemy] [SuperChargers] [V] [?]

## At a glance

- **Played on:** macOS (the test suite, the owner); Windows (issue #19); Linux on a Steam Deck in Desktop Mode (issue #22).
- **Known limits:** The figure owner fix of 0.9.6 to confirm with a real portal (#28).

(The source of the README's game table: `python3 -I tools/game_status.py` writes it from this section and `game.txt`.)

## Disc
| | |
|---|---|
| Release | 2015, Vicarious Visions; Redump "Skylanders - SuperChargers (Europe)", 8738846720 bytes (the only disc tried) |
| Title ID, media ID, version | `4156091F`, `74144F53`, `0x00000007` [V] (the XEX execution info) |
| `default.xex` SHA-256 | `02ff6672...` (full value in `game.txt` and `info.cpp`), 24985600 bytes |
| Engine | **Alchemy**, Swap Force's line on the game side, Trap Team's XDK **2.0.21256** [V] ([research.md](research.md)) |

The disc stays outside git; everything derived from it is in `private/superchargers/` (day 1: `private/superchargers/day1/`).

## Status (day 4, 2026-11-04)
Codegen, build and boot work; the picture is right; **a new game reaches the Academy (the hub)**: the intro and the rift tunnel ride,
then `SKY_LEVEL=academy` (or, without it, `intro1` on foot), and **a continue from the save goes straight to the Academy**; 30 fps,
no fault. The automatic test is `tools/regress/superchargers_lobby.sh` ([boot.md](boot.md) "SKY_LEVEL and the test").

| Step | Result |
|---|---|
| Codegen | clean after three recompiler additions (`patches/xenonrecomp.patch`): `vadduhs`, `vpkuwum128`, `vpkd3d128` FLOAT16_2; 102087 functions, 0 trap left in the generated code |
| Build | `build-superchargers` links (`Skylanders SuperChargers`, bundle `SSCRecomp`); 0 warnings in `games/superchargers/` |
| Picture | right since two renderer fixes (the scissor with the test off, 16-bit pair positions; [boot.md](boot.md) "The black frame") |
| Story | the menus, `fmv01`, the intro, the tunnel ride with a vehicle, `intro1` on foot (1245 draws a frame, 30 fps), the Academy |
| Figures | Fiesta (`new:3400`) and Crypt Crusher (`new:3227`), generated blank tags, are read and played; the instant vehicle also works |
| Shaders | from the disc: IGA v11 big-endian, IGZ v9, 1207 containers ([shaders.md](shaders.md)) |
| The launcher's look | logo, background, 469 portraits, the menu theme, from the disc ([art.md](art.md)) |

## The details the earlier ports got wrong first (2026-11-04)
Each checked in SuperChargers with a log line or a shot (runs under `private/runs/`); [V] seen, [?] not checked.

| Detail | Result |
|---|---|
| Movies | the logos (`bumper_atvi`, `bumper_vvisions`; the adapter lists them for `skip_intro_movies`) and `fmv01` (3905 frames, its file read to the end) play [V]; at 21:9 with black bars [V]. **Y ends `fmv01`** (and any story movie) as Imaginators' Y: the game's own cutscene skip never comes, so the port cuts the movie at the frame it shows and the game goes on as at its end (run `skD`: the intro 100 s sooner); Start pauses it [V] ([boot.md](boot.md), "Movies: the skip") |
| Text and voices | this disc has English and German only (1146 `.lng` and 347 voice banks each, no French). `--language de`: every voice and text bank opened is `_de` (118), none `_en` [V]. **Voices in another language** (`voice_language`): Swap Force's hooks re-addressed, `voices en (6), text 9` [V]; a movie's voice track (5 English, 6 German) switched [?] ([boot.md](boot.md), "Text and voice language") |
| Sound | not silent: 20898 non-silent 1024-sample blocks in a 455 s run, 0.00 % repeated (`analyze.py audio`) [V] |
| HUD and prompts | the HUD (portrait, gearbits, health), the game's own Xbox 360 button prompts ("Hold RT to Accelerate!") [V]. **Glyphs**: the fonts' two button pictures redrawn in memory (Swap Force's way): the pause menu's hints follow PS5, Switch, the keyboard's keycaps and the 360 [V] ([art.md](art.md), "Button pictures"); the world's 3D prompts are a face and letters of geometry: the **RB** one follows the device (PS5 R1, a keycap, the 360's unchanged) [V], the others are not replaced yet ([art.md](art.md), "World prompts") |
| Save and continue | `SUPERCHARGERS_OPTIONS` and `SUPERCHARGERS_0` written; Continue reads them and loads the Academy [V]; Swap Force's save-task race is not in this game's code (no hook) [V] |
| Portal prompt | **the Toy Box opens by itself** on Vehicle 1 at "Please place a LAND VEHICLE", then on Player 1 for the Skylander; both seated and taken (run `scP1`); **player 2** joining in the Academy: the Toy Box opens on Player 2, Eruptor seated and taken (run `scL2`) [V] ([toybox.md](toybox.md), "The portal prompt", "Player 2") |
| Toy Box (F6) | opens over the Academy: 469 portraits read from the disc, the seats Player 1, Vehicle 1, Player 2, Vehicle 2, Magic item, Fiesta and Crypt Crusher seated [V]; their cards show drawn icons until the catalogue has SuperChargers' portrait keys |
| Outlines at scale 2 | the FXAA pass (`732669B14C8E3386`) is `HostKernelPixelShader`, so its kernel stays in guest pixels at scale 2, as Trap Team's. **Side by side** (the Academy from the same save and camera, the full-resolution scene, scale 1 run `scS4` against scale 2 run `scS1`, crops enlarged twice: `private/runs/outlines_scale1_vs_2.png`): the same outlines on Fiesta and the scenery, sharper at 2, none doubled or thinned [V] |
| Water and lava above 30 FPS | not measured: no water simulation pass found yet (`stepPassShaders` empty); the image names `_applyWaterSimulationNormals` and ripple settings, so one may exist in a water level [?] |
| Ultrawide and the scene | the 3D at full resolution (1040x620 -> 1280x720) and **ultrawide**: 21:9 (2560x1080: scene 1696x720, Hor+ 80 -> 96.4 degrees) and 32:9 (3840x1080: scene bounded to 2048x576, 118.4 degrees; a 2560x720 scene stopped the game in the Academy), HUD and menus at 16:9 [V] ([ultrawide.md](../../../docs/runtime/gpu/ultrawide.md), "SuperChargers") |
| 60 FPS | the game asks for interval 2 and paces on it (as Swap Force): `presentIntervalPacesGame`; at `--fps-limit 60` the menus and the intro run at 60.0 fps, `intro1` at 58.4-60 (1330 draws), the story at its pace (`intro1` at 321 s against 329 s at 30) [V]. The Academy (1280 draws) held 58.3 fps with the renderer at 11.2 ms a frame; with the 16-bit pair streams in the buffer cache and the swap spin made a wait (2026-11-04, [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md) "SuperChargers and Imaginators at 60"): renderer 6.0 ms, **59.9 fps held**, gap p99 19 ms, process CPU 38.6 -> 31.6 ms a frame; with the drop to 30 on, no drop: the Academy 60.0 fps, `intro1` 59.2, `sky1` 59.1 [V]. The other 12 zones (sky2-3, nat1-3, quest1-3, mag1, dark1, final1-2) all hold **59.9-60.0 fps with the drop on, 0 drops in their window** (2026-11-05; one drop to 30 in `nat1`'s load, then back; [renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md) "The tour"): `tools/perf/game_perf.sh superchargers <disc> <map> 60 <out>` with `SECONDS_TO_RUN=470` |

## Nightmare (2026-11-05) [SuperChargers] [V]
**Offered from the start**: a new game's "Choose your play style!" (the difficulty, after the title) has four cards, Adventurer
highlighted; the fourth, **Nightmare** ("Only for the most advanced players with a vast army of Skylanders! +30% Rewards"), has no
padlock on a new save, and A takes it: `SUPERCHARGERS_0` is written and `fmv01` starts (runs `nm-sc-explore`, `nm-sc-accept`). The
executable has no `nightmare_difficulty_unlocked` (Swap Force's and Imaginators' lock) and no other unlock name for it. So
`nightmare_from_start` does nothing here (no `SetNightmareFromStart`: the runtime logs that it is ignored), and the launcher shows no
toggle for this game.

## Coverage (2026-11-07) [SuperChargers] [V]
`tools/regress/coverage_all.sh` runs the whole catalogue (265 entries: 54 zones on the disc, the 40 with an archive less the
developers' rooms, and the 240 toy types of the accept list): a new game to the Academy once, its save then continued for
every zone with the Academy's request renamed (`SKY_LEVEL_FROM=academy/academy`, `engines/alchemy/zone_request.cpp`: ~25 s
to a zone instead of ~320 s), and `SKY_LEVEL` takes a full zone path. The 13 story zones, `intro`, `intro1` and the Academy
play 30 s with the hero, no fault. The 10 elemental zones **fault on load** (guest 0x10 in `sub_82B09528`): started alone
they lack the Academy's package (each zone's archive lists `academy_pkg.igz.dep`), a test path a player never takes; the
real entry is driving a vehicle of the zone's element through the Academy's Front Lawn Elemental Gate (Grizzo resets the gate
to the current vehicle's element), not scripted. Every figure is read by the game. The race tracks (2026-11-08) go through
the menus as a player starts them (`coverage_run.py` route `scrace`): the title, Racing (X), Single Player, Single Race, the
race map (the cursor moved from its centre by the stick; presses closer than 2.5 s are lost), the track, START, the driver
(Fiesta) and a vehicle of the track's handling type (Jet Stream, Crypt Crusher or Reef Ripper; the game offers only that
type). A pack's trophy shows its tracks: the six on the map from the start race with the trophy on the portal from boot (its
message closed at the title), the six behind clouds only when the trophy comes while the map is open and the map is opened
again; only one trophy at a time ("too many Legendary Treasures"). The map: Cloud Factory `apa1`, Calamity Canyon `apa4`,
Frozen Fossil Festival `aps1`, Mystical Vault `aps4`, Chompy Garden `apl1`, Dragon Spine `apl3`, Temple of Arkus `apl2`
(the moves: `SC_RACES` in `tools/regress/coverage_catalogue.py`). Results:
[STATUS.md](../../../docs/status/STATUS.md), what is left: [COVERAGE.md](../../../docs/status/COVERAGE.md).

## Where to start
- Build: `cmake -S . -B build-superchargers -G Ninja -DCMAKE_BUILD_TYPE=Release -DARKEYAN_GAME=superchargers`, `nice ninja -C build-superchargers -j4 ArkeyanRuntime`
  (the generated code: [research.md](research.md) "XenonRecomp"; the shader cache: `SKY_GAME=superchargers tools/shaders/build_cache.sh <disc> <dump dir>`
  from a `SKY_DUMP_SHADERS` run: 1214 of 1215 translate, `203D384226E0FD37.ps` is rejected by the Metal compiler).
- Run: the isolation block of [testing.md](../../../docs/getting-started/testing.md) plus `SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:3400`
  and an A every 4 s from 12 s: the menus to the intro in ~42 s; add `new:3227` (a vehicle) and RT for the tunnel ride
  ([boot.md](boot.md) "To the first level").

## Open problems
1. **Races started from the menus: fixed** (2026-11-08) [V]. They faulted as their zone started (guest 0xC in
   `sub_82505C10`, a TLSF free-list unlink with a null link). Cause, proven by a trace of every TLSF allocation and free:
   the game makes its level heap again in place at every zone start (`tlsf_add_pool`, control `A68EB438`, from
   `sub_828C512C`), and one 16-byte object of the menus' level heap (vtable `0x82284D00`, `CSkyboxBlendBundle`'s
   registration) is released after the race's zone has remade it (main thread, `sub_826E7658` ... `sub_8245BF40`): its
   stale header ("previous block free", previous pointer to the pool's first block, now the new pool's single free block)
   sends the merge through dead sizes. It happens at every race start from the menus, at 30 as at 60; it faults when
   the stale header has its "previous free" bit, which the menus' per-frame allocations decide (Cloud Factory `apa1`
   faulted at both rates in the 0.9.4 candidate). Why the game releases it that late is not known. Fix:
   `engines/alchemy/heap_guard.cpp` (`live_blocks.h`, `ArkeyanLiveBlocksTest`) drops a free of a block from before its pool
   was remade (`[heap] free of ... dropped`, one per race start). Proof (route `scrace`, the coverage seed save,
   each race played about 58 s after its zone starts; before the fix `apa1` faulted 3 of 3 at 60 and 1 of 1 at 30):
   **12 of 12 tracks at 30** and **the 6 map tracks at 60** (`apa1` twice) ran with no fault and no `[guard]`, one dropped
   free each; four pack tracks at 60 were not reached (`aps2`, `aps3` not run): the trophy that reveals them, put on the portal at the open map,
   does not take at 60 (a route step, no fault), so the coverage route keeps them at 30. Repeat starts: `apl1` at 30 and `apa4` at 60 once more each, ok (20 + 20 not run).
2. Vehicles: the owner's 90 dumps are blank; the garage's upgrades are mapped and shown (shield and weapon, [V]), the mods are
   not. The garage is the Academy's Upgrade Vehicle Station, opened by the story after Chapter 1, not by the `SKY_LEVEL` save
   (found on the disc, not reached in a run: [toybox.md](toybox.md), "The garage"). The Dead state is met [V]; Vehicle 2's prompt and the error state not.
3. Glyphs: the turbo pictures (`Turbo_A/X/Y`, not met) and the world's 3D prompts but RB (A/Y and B share a face; the others not met in a run, [art.md](art.md), "World prompts").
4. Two stubs on the boot path: `XamUserReadProfileSettings` and `XMsgCancelIORequest` (answered 0; nothing seen wrong).
