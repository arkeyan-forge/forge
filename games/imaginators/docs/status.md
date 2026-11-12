# Skylanders Imaginators (imaginators)

> **Purpose:** which disc Imaginators needs, where the port stands, and where to start · **Status:** current (2026-11-04) · **Tags:** [Xbox] [XDK] [Alchemy] [Imaginators]

## At a glance

- **Played on:** macOS (the test suite, the owner); Windows (issue #47).
- **Known limits:** Linux not tried yet; a created character's HUD portrait fixed in 0.9.6 (#47).

(The source of the README's game table: `python3 -I tools/game_status.py` writes it from this section and `game.txt`.)

## Disc
| | |
|---|---|
| Release | 2016, Toys for Bob; Redump "Skylanders - Imaginators (Europe) (Fr,Nl)" (the only disc tried) |
| Title ID, media ID, version | `41560930`, `0133F901`, `0x00000004` [V] |
| `default.xex` SHA-256 | `1c6d7e05...` (full value in `game.txt` and `info.cpp`), 25739264 bytes |
| Engine | **Alchemy**, SuperChargers' line, XDK **2.0.21256** [V] ([research.md](research.md)) |

The disc stays outside git; everything derived from it is in `private/imaginators/`.

## Status (day 2, 2026-11-05)
A new game goes from the boot to the **first level, on foot with Eruptor**, without a fault (14-minute run); 30 fps at the default
limit ([boot.md](boot.md)).

| Step | Result |
|---|---|
| Codegen | one new instruction (`mulhd`); 440 function entries the analyzer missed, added to the config ([boot.md](boot.md) 2) |
| Shaders | 1190 of 1190 translate; no missing shader in the menus, the opening and the first level |
| Story | the movies, the title, the licence (Y), the difficulty, the opening and its cutscenes, the portal's figure, the first level |
| Figures | the emulated portal's Eruptor (`new:9`) is read and played |
| SKY_LEVEL | `SKY_LEVEL=<map>` enters `2016/<map>/<map>` instead of the first level (`hub_2016`: the hub) [V] |
| Test | `tools/regress/imaginators_hub.sh`: a new game to the hub, then a continue from its save: PASS (no fault, 30 fps, sound, saves) [V] |

## The finishing details (day 2, 2026-11-05)
Each checked in a run (`private/runs/`); [V] seen, [?] not checked.

| Detail | Result |
|---|---|
| Glyphs | the fonts' two button pictures redrawn for the device in use (SuperChargers' way; the body one is new, 128x250, with a Back): the pause menu's hints as PS5, Switch, keyboard and 360 [V] ([art.md](art.md), "Button pictures") |
| Portal prompt | **the Toy Box opens by itself** on Player 1 at "place your Skylander", Return seats the figure, the game takes it, the Toy Box closes (run `imgC1`) [V]; player 2 and the Dead state not run [?] ([toybox.md](toybox.md), "The portal prompt") |
| Text and voices | French or Dutch from the settings (`language`, `voice_language`); voices follow the text or take the other; the movies' voice track follows [V] ([boot.md](boot.md) 5) |
| Ultrawide | 21:9 (scene 1696x720, Hor+ 96.4 degrees) and 32:9 (scene 2048x576, 118.4 degrees) in the hub [V] ([ultrawide.md](../../../docs/runtime/gpu/ultrawide.md#imaginators-alchemy-imaginators-v-2026-11-05)) |
| Movies | the logos in `skip_intro_movies`; `fmv01` skipped with Y after a first press shows the game's prompt [V] ([boot.md](boot.md) 7) |
| Save task | Swap Force's race is not in this game's code: no hook [V] ([boot.md](boot.md) 6) |
| 60 FPS, scale 2 | the first level 59.6, the hub 58.4 before the Metal lossless compression (GPU-bound with the full-resolution scene; 60.0 with `original_resolution=1`); **now the hub and all 12 levels (`level_500`-`510`, `512`) hold 59.8-60.0, no drop to 30** (2026-11-05) [V] ([renderer_threads.md](../../../docs/runtime/gpu/renderer_threads.md)) |

## Nightmare (2026-11-05) [Imaginators] [V]
The difficulty menu (a new game's first empty slot, after the licence) has four portraits, Medium highlighted; the fourth,
**Cauchemar** (Nightmare), carries a padlock on a new save, its text saying to beat Kaos on another difficulty to unlock it.
- **The lock**: `sub_829BE430` reads the slot's property `nightmare_difficulty_unlocked` and answers whether it is 1: Swap Force's
  `sub_829CC788` instruction for instruction (the slot's data at +44, not +40); the only reader of the property (its writer,
  `sub_829C4350`, is the script's `unlockNightmareDifficultyLevel`). Its two callers are the menu's (`sub_8317D680`,
  `sub_8317D888`); it is asked when the menu opens.
- **`[imaginators] nightmare_from_start=1`**: the shared hook `engines/alchemy/nightmare.cpp` (`GAME_HOOK_NightmareUnlocked`, Swap
  Force's) answers 1 while the option is on; the save is not written.
- **Proof** [V]: `IMAGINATORS_ISO=<disc> tools/regress/nightmare_menu.sh imaginators` PASS (`private/regress/nightmare/imaginators/`):
  option off, the padlock and `the save says locked, the option is off: locked`; option on, no padlock, the text "Tu ne peux pas
  changer la difficulté dans ce mode. +50% de récompenses", A takes it and the first level opens (`level_508.pak`). In `test_all.sh`.

## Coverage (2026-11-07) [Imaginators] [V]
`tools/regress/coverage_all.sh` runs the whole catalogue (313 entries: the 44 zones of `zoneinfos.pak` less the menu
background, and the 284 toy types of the accept list): the 10 story levels, the final battle, both adventure packs, the
Crash level, the 10 Sensei Realms, the hub, the overland map and the five `pu_*` zones each play 30 s with the hero from a
new game (`SKY_LEVEL`), no fault, no hang; every figure (Senseis, the 10 Creation Crystals blank, vehicles and traps too,
though no seat takes them) is read by the game. The 12 race tracks (`SKY_LEVEL` takes their full zone path) load and run
without a fault, but a race starts from the racing menus (not scripted), so they are not counted; the credits
(`credits.bk2`) follow the final battle won. Results: [STATUS.md](../../../docs/status/STATUS.md), what is left:
[COVERAGE.md](../../../docs/status/COVERAGE.md).

## Next
1. The hub test in `tools/test_all.sh`.
2. The hub at a locked 60 with the full-resolution scene (the GPU's cost, stream P); player 2's prompts; the parts' names in the
   creation editor ([toybox.md](toybox.md)).
