# Spyro's Adventure: dated history of the port

> **Purpose:** the dated news and session entries of the Spyro's Adventure port, moved out of [status.md](status.md) on 2026-10-29; the current state is in status.md · **Status:** archive (history, not maintained) · **Tags:** [SSA] [V]



## Story tour (2026-10-28) [SSA] [V]
Every story level of the disc, in chapter order (`text\<lang>\global.lxb`, the `Level_<n>_Chapter` and `Level_<n>`
keys): the 22 chapters, Lair of Kaos' second part (`level_010b`, loaded at the transition; its mission names the ending
movie and the credits) and the 4 adventure packs (chapters 23-26). `tools/pipelines/tour_ssa_story.sh <iso> <work dir>
[<level>...]` (~2 min 30 s a level, ~70 min for the 27; one instance, isolated, headless, silent): a new game started in
the level with `SKY_LEVEL` (chapter 1 is also the real flow: the harness's SSA run reaches it from the menus), the
figure `new_14.sky` on the emulated portal, B through the opening cutscene, then from 62 s walking, attacking, jumping
and the pause menu, stopped at 141 s. Pass: the level's `mission.str` opened (`SKY_TRACE_FILES`), no fault or `[guard]`
line, frames to the end, at least 60 s in the level (from its first frame), last shot not black
(`tools/pipelines/story_summary.py`). Result: **27 of 27 ok** (evidence: `private/ssa100/story/`, `summary.tsv`, a last
shot per level). **What it proves:** each level loads and runs 2 minutes without a fault. **What it does not:** play;
outside chapter 1 the `SKY_LEVEL` redirect creates no Skylander (no hero, no HUD in the end shots; Kaos' arena shows his
health bar but no hero), so the presses act on nothing ([100.md](100.md), "Modes").

| Chapter | Level | Name | Result | Boss (from the level's own text) |
|---|---|---|---|---|
| 1 | `level_027` | Shattered Island | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 2 | `level_021` | Perilous Pastures | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 3 | `level_018` | Sky Schooner Docks | ok: 120 s in the level, 0 faults, 0 `[guard]` | the Elite Airship (to shoot down) |
| 4 | `level_026` | Stormy Stronghold | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 5 | `level_014` | Oilspill Island | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 6 | `level_037` | Dark Water Cove | ok: 119 s in the level, 0 faults, 0 `[guard]` | - |
| 7 | `level_006` | Leviathan Lagoon | ok: 120 s in the level, 0 faults, 0 `[guard]` | Kaos and his water minions |
| 8 | `level_001` | Crystal Eye Castle | ok: 119 s in the level, 0 faults, 0 `[guard]` | - |
| 9 | `level_019` | Stonetown | ok: 120 s in the level, 0 faults, 0 `[guard]` | the Stone Golem |
| 10 | `level_038` | Treetop Terrace | ok: 119 s in the level, 0 faults, 0 `[guard]` | - |
| 11 | `level_017` | Falling Forest | ok: 120 s in the level, 0 faults, 0 `[guard]` | Kaos' life spell |
| 12 | `level_009` | Troll Warehouse | ok: 99 s in the level, 0 faults, 0 `[guard]` | - |
| 13 | `level_040` | Goo Factory | ok: 119 s in the level, 0 faults, 0 `[guard]` | - |
| 14 | `level_034` | Battlefield | ok: 121 s in the level, 0 faults, 0 `[guard]` | the Trolls' secret weapon |
| 15 | `level_023` | Crawling Catacombs | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 16 | `level_002` | Cadaverous Crypt | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 17 | `level_032` | Creepy Citadel | ok: 121 s in the level, 0 faults, 0 `[guard]` | Kaos' undead spell |
| 18 | `level_000` | Molekin Mine | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 19 | `level_024` | Lava Lakes Railway | ok: 121 s in the level, 0 faults, 0 `[guard]` | Kaos (fire) |
| 20 | `level_039` | Quicksilver Vault | ok: 121 s in the level, 0 faults, 0 `[guard]` | - |
| 21 | `level_025` | Arkeyan Armory | ok: 114 s in the level, 0 faults, 0 `[guard]` | - |
| 22 | `level_010` | Lair of Kaos | ok: 120 s in the level, 0 faults, 0 `[guard]` | Kaos' minion rounds |
| 22b | `level_010b` | Lair of Kaos (part 2) | ok: 121 s in the level, 0 faults, 0 `[guard]` | **Kaos, the final fight: starts on arrival** (his dialog, then the arena with his health bar) |
| 23 | `level_003` | Empire of Ice | ok: 120 s in the level, 0 faults, 0 `[guard]` | - |
| 24 | `level_008` | Pirate Seas | ok: 121 s in the level, 0 faults, 0 `[guard]` | Captain Dreadbeard (cards) |
| 25 | `level_036` | Darklight Crypt | ok: 121 s in the level, 0 faults, 0 `[guard]` | King Occulous |
| 26 | `level_046` | Dragon's Peak | ok: 121 s in the level, 0 faults, 0 `[guard]` | Vathek |

- **With the hero (2026-10-28) [V].** The tour above used `SKY_LEVEL`, which reads level_027's files from another
  level's: each level loads and runs, but **no Skylander is created** outside level_027 (no hero, no HUD). The test
  switch `SKY_NEXT_LEVEL` makes the game itself go to the level (the opening movie's `nextLevel`,
  [str_levels_loading.md](../../../docs/engines/lux/str_levels_loading.md#which-level-a-new-game-starts-and-starting-another-one-lux-ssa-v)).
  `TOUR_MODE=hero TOUR_SECONDS=110 tools/pipelines/tour_ssa_story.sh <iso> <work dir> <levels...>`: **28 of 28 ok with
  the hero**, the 27 story levels and `level_hub` (the Ruins' first stage): each run 0 faults, 0 `[guard]`, ~90 s in
  the level, and the game wrote the figure back (its first-use write, made when the Skylander enters play; the
  summary's `hero` column), while with `SKY_LEVEL` the figure stays untouched outside level_027. A shot per level shows
  Gill Grunt's portrait, health bar and level in the corner (evidence: `private/ssa100/bosses/herotour/`,
  `hero_tour_sheet.png`).
- **Bosses.** Kaos' final fight starts on arrival (`level_010b`): reached and started [V]. The other bosses
  (column 5) wait at the end of their level, behind its puzzles: a script that walks and attacks blindly does not get
  there. The levels' own boss checkpoint (`checkBossFightCheckPoint`, `TeleportToBoss` in level_019) was found but
  editing it did not move the first spawn [?] (the leads: the engine page above). Left to a human
  ([playtest.md](playtest.md)) or that lead.
- **Heroic Challenges.** `SKY_NEXT_LEVEL=Challenge_Level_000` loads the challenge and runs it (its timer, "0 of 100"),
  but no Skylander appears: challenge levels hold no `Player` thing; the hero likely comes from the Ruins' challenge
  menu [?].
- **The ending and the credits.** `tools/pipelines/ssa_ending.sh <iso> <work dir>` (~11 min): a script cannot beat Kaos,
  so the test switch `SKY_MOVIE_SWAP=legal:sl_sc30,atvilogo:credit` ([debugging.md](../../../docs/porting/debugging.md))
  makes the two first start-up movies the ending (`sl_sc30.bik`, 5025 frames, 167 s) and the credits roll
  (`credit.bik`, 13804 frames, 460 s). Both play to their own end in the game's movie player: `[movie] legal ended after
  5026 updates`, `[movie] atvilogo ended after 13799 updates` (`engines/lux/movie.cpp`), then the game goes on to its
  title; no fault; shots at 90 s (Kaos in the ending), 300 s and 600 s (the credits) look right [V]. What triggers them
  in the real flow (Kaos beaten) is not reached.
