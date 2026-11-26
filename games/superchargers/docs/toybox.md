# SuperChargers' Toy Box: vehicles, seats, badges

> **Purpose:** what the in-game Toy Box and the collection show for SuperChargers (its vehicles, seats and pictures), how it is proved, what is still open · **Status:** current (2026-11-05) · **Tags:** [Alchemy] [SuperChargers] [V] [?]

The same screens and code as the other games (`runtime/host/toybox/`, [toybox_ux.md](../../../docs/runtime/host/toybox_ux.md));
`runtime/` has no SuperChargers case. What makes it SuperChargers' is data: its catalogue entry (seats, badges) and the vehicle
fields of the tag ([figures.md](../../../docs/runtime/host/figures.md#superchargers-vehicles-alchemy-superchargers)).

| Part | What [V] |
|---|---|
| Seats | Player 1, **Vehicle 1**, Player 2, **Vehicle 2**, Magic item / trophy, **Trap**; a vehicle seat takes a supported vehicle only, never a racing villain's (3470-3497, issue #135: the game crashes after one arrives in the Academy), Trap takes traps only (host tests; the trap's in-game reward remains to check) |
| Vehicles | 20 types (3220-3241), 13 decos from the owner's dumps (Dark, Legendary, Power Blue, Nitro, Golden, E3, holiday, chase); a blank one made by the Toy Box or `new:<type>` is read and driven (`undeadcar.pak` opens when the game reads Crypt Crusher, 181 s into a new game) |
| A vehicle's card and sheet | the card "Shield 2 · Weapon 1" (the garage's upgrades, [figures.md](../../../docs/runtime/host/figures.md#superchargers-vehicles-alchemy-superchargers)), the sheet "Gearbits: N · shield 2/5 · weapon 1/5 · driven <time>" once the game used it, "New: never driven" before; gearbits change through `collection::SetGearbits` (the launcher's Edit) |
| Trophies | 3500 Sky, 3501 Land, 3502 Sea, 3503 Kaos (the dumps' tags; the public list had two swapped); the item seat; the villains a racing trophy captured, a row each on its Figure page (A flips it) and a switch each in the launcher's Edit ([Racing villains](#racing-villains-issue-165)) |
| Pictures | the disc's 469 round badges (88 px, `archives/toycollectionmaterials.pak`, [art.md](art.md)): 363 toys and variants mapped in the catalogue's `portraits` (SuperChargers' 44 and their decos, every older toy Swap Force and Trap Team name the same way). Drawn icons left: Smash Hit (no badge of its own on this disc) and the UFO Hat (only its variant's). `SUPERCHARGERS_ISO=<disc> ArkeyanArtTest` checks them |

## Traps (issue #101)

[V] The game's local toy table lists traps as well as vehicles. Its entries specify toy families and variants,
without a seat or hardware-slot requirement. The shared catalogue already accepted Trap Team's traps; the missing
Trap seat now exposes them in the Toy Box and the launcher's seat picker. Traps use ordinary emulated portal slots
and their stored tag data, as in Trap Team. Clown Cruiser and Barrel Blaster are absent from the Xbox tables and
excluded from this game's vehicle choices. See [the shared rule](../../../docs/runtime/host/collection.md#later-game-accessories-issue-101).

[?] Player check, not run here: keep a Skylander on Player 1 and a supported vehicle on Vehicle 1, then choose a
trap on **Trap**, not Adventure or Vehicle. Check that it is read and the expected elemental vehicle attack/ammo
becomes available; with a captured villain, check the corresponding Skystones Overdrive reward where applicable.
Lift and replace it, then switch element, checking the game's feedback and that the player and vehicle stay seated.
The tables establish recognition, not those rewards or their repeat-use conditions. Also verify Vehicle 2 and the
six-seat layout through the in-game Toy Box and the separate Toy Box window.

**Internal names** (the badges' `collection_<name>_<deco>`): Fiesta `souljam`, High Volt `lightbulb`, Splat `inkwell`, Stormblade
`peacock`, Spitfire `nitro`, Dive-Clops `airlock`, Astroblast `lightdriver`, Nightfall `darkdriver`, Thrillipede `lifebug`, the six
reposed cores `driver<name>`; vehicles `vehicle<name>`: Jet Stream `airplane`, Tomb Buggy `doombuggy`, Reef Ripper `watersub`,
Burn-Cycle `firebike`, Hot Streak `firecar`, Shark Tank `earthtank`, Thump Truck `earthtruck`, Crypt Crusher `undeadcar`, Stealth
Stinger `lifecopter` (its plain badge is named `plane`), Dive Bomber `watertorpedosub`, Sky Slicer `sparhawk`, Clown Cruiser
`clownplane`, Gold Rusher `techbike`, Shield Striker `techtruck`, Sun Runner `lightplane`, Sea Shadow `darkmantasub`, Splatter
Splasher `magicsub`, Soda Skimmer `magicfloat`, Barrel Blaster `dkcart`, Buzz Wing `lifeplane`; trophies `racepack_<sea, land, air,
kaos>`. Matched by looking at each badge and by the decos the dumps hold (a Power Blue badge for the toy that has a Power Blue
figure); Gold Rusher and Shield Striker by that rule only [?].

## Racing villains (issue #165)

[V] (2026-11-25, Europe disc, the 0.9.10b2 kit, emulated portal; runs `race1`-`race11`, `acad1`-`acad4`.) The 13 racing
villains (3440-3467) and their 14 vehicles (3470-3497) are in the game's toy tables but are no toys: the game plays
them from a trophy.
- **In Racing** (Single Player, Single Race): the driver select ("ASSOCIE UN PILOTE") shows, under the drivers, the four
  villains of the trophy for the track's type, padlocked until the trophy captured them. A captured one is chosen there
  and races with its own vehicle (Wolfgang on Sub Woofer, Cloud Factory, 60 s, no fault). The Kaos Trophy adds Kaos to the
  drivers, unlocked. Where the trophy keeps them: [figures.md](../../../docs/runtime/host/figures.md#superchargers-trophies-alchemy-superchargers-2026-11-25-issue-165).
- **A villain as a toy** (Chef Pepper Jack, 3440, alone on the portal in the Academy): the portal's entry plays and names
  him, then he stays in the tunnel about a minute and the Academy shows his name on the HUD with no character to play
  (nothing moves for 30 s of presses): not playable.
- **A villain's vehicle as a toy** in the Academy: Steam Roller (land, 3480) and Wave Singer (sea, 3490) load the
  villain's vehicle and driver banks (`vehicleglumshanks.pak`, `vehiclemesmeralda.pak`), then the main thread faults at
  guest `0x2C` in `sub_82DDE2C0` 3-4 s later: launcher issue #263's crash, on this Mac. Toaster Bomber (sky) is not
  taken there (the Academy wants a land vehicle).
- **So**: no seat takes either (`Seat::Takes`), Add a figure never offers them and the shelf never shows a dump of one
  (`Toy::FromTrophy`); a racing trophy's Figure page has a row per villain, captured or not (A flips it: `Model::PutCaptured`,
  [toybox_ux.md](../../../docs/runtime/host/toybox_ux.md)), and the launcher's Edit a switch each.
- **Not run** [?]: Boss Pursuit (the game's own way to capture), a villain in Cup or Time Trial, two players, a Land or
  Sea villain racing (their bits were only seen unlocking them).

## Proof
- `tools/regress/superchargers_toybox.sh` (`SUPERCHARGERS_ISO`, `build-superchargers/`, about 4 min, through `../gamelock.sh`): a
  scratch collection of Fiesta, Crypt Crusher and the Sky Trophy, the game on an empty portal; the Toy Box (F6) seats Fiesta on
  Player 1, Crypt Crusher on Vehicle 1 and the trophy, then the menus' presses run to the intro: the log must show both seated and
  taken by the game; seven pictures.
- The tag fields: [figures.md](../../../docs/runtime/host/figures.md#superchargers-vehicles-alchemy-superchargers).

## The portal prompt (2026-11-04) [V]
The Toy Box opens by itself when the game asks for a figure, **and for a vehicle** ("Please place a LAND VEHICLE on the Portal of
Power"), on that seat, and closes when the prompt is gone. Code: `games/superchargers/superchargers_portal_prompt.*` and the
adapter's `ReadPortalPrompt` (four guest reads a frame); `runtime/` only learnt a `vehicle` flag on the prompt (seat `vehicle<N>`).
- **The object** is Swap Force's `CMagicMoment` singleton. Found by its class: the string `CMagicMoment` (`0x820B8374`) is
  registered by `sub_82B8CA78` with its descriptor `0x83765EA0`; the 3-instruction getter of that descriptor, `sub_82B39BC8` (Swap
  Force's `sub_8285FEA8`), is slot 1 of the class's vtable (`0x8209EF14`, found by its address in the image); slots 6 and 7,
  `sub_82B49390` and `sub_82B493A0`, store and clear the instance at **`0x83764ED0`** (Swap Force's slots 6 and 7 do the same with
  `0x83131BE0`).
- **The fields**, measured in runs `mmB` and `mmC` (a dump of the object at each state change, empty portal, A every 4 s):
  the state at **`+0x2C`** (the enum's names at `0x820976A0` are in value order: Swap Force's with `FirstVehicleSequence` added at 7,
  so Idle 5, IdleOutro 8, Dead 14, Error 17), the reason at **`+0x168`** (values listed at `0x820974B0`: 13 VehicleRequired as the
  vehicle prompt comes up; 12, VehicleAdded by the names' order, both after a vehicle came and while the prompt stays up with
  none, run `scP3`), the player asked for a Skylander at **`+0x1FC`** (0 for player 1; -1 while the vehicle is asked for). So a
  vehicle prompt is Idle with nobody named and the reason 13 or 12. A new game asks for the vehicle first (states 1, 2, 3, 18, 5 with reason 13), then, after the vehicle's
  intro (7, 8, 9, 11), for the Skylander (5 with player 0).
- **The run** `scP1` (empty portal, a scratch collection of Fiesta and Crypt Crusher, A every 4 s to 160 s, then Return every 5 s):
  `[toybox] prompt for player 1's vehicle`, `V1: Crypt Crusher on slot 2`, `the prompt is gone`, `V1: the game took Crypt Crusher`
  (`undeadcar.pak` at 182 s), then `prompt for player 1`, `P1: Fiesta on slot 0`, `P1: the game took Fiesta` (`souljam.pak` at
  201 s). `ArkeyanPortalTest` checks the classification (`ClassifySuperChargers`).
- **A prompt counts after 2.5 s.** With the Skylander already on the portal, the vehicle's intro still passes through Idle for
  its driver (state 5, player 0) and the game reads the figure about a second later: the first build opened the Toy Box there
  and marked Fiesta out of health ("Fiesta is out of health: pick another one.", run `scV1`). The reader now reports a prompt
  once it has held for 2.5 s: run `scV2` (both figures placed from the start) goes through the two Idle passes with no prompt,
  `undeadcar.pak` at 182 s and `souljam.pak` at 196 s.
- **After both fixes** (runs `scP4`, `scP5`, 2026-11-04): on an empty portal the Toy Box opens on "Choose for Vehicle 1" by itself
  while the game shows "Please place a LAND VEHICLE" (shot `private/runs/scP4-shots_prompt.png`), Return seats Crypt Crusher, the
  game takes it, the Toy Box closes, `undeadcar.pak` opens, then it opens by itself on Player 1; with the vehicle placed from the
  start only the Skylander prompt opens it.

## Player 2 (2026-11-05) [V]
Run `scL2` (a continue to the Academy from a saved game, Fiesta and Crypt Crusher on the portal, Eruptor and High Volt in the
collection; presses written while it runs, `SKY_PRESS_FILE`, and the Toy Box's own script pressing Return every 4 s): a second
pad's A shows "JOIN" and joins; the CMagicMoment object goes 2, 3, 18, 5 **with player 1 at `+0x1FC`**, `[toybox] prompt for
player 2`, the Toy Box opens on "Choose for Player 2", `P2: Eruptor on slot 3`, `the game took Eruptor`, the prompt goes and the
Toy Box closes; Eruptor plays beside Fiesta with his own HUD. No vehicle was asked for player 2 in the Academy (a player joining
on foot). (Run `scL1`: a pad that shows up only at its first press needs a second one; the Toy Box takes keys and its script, not
the scripted pad.)

## The Dead state, met (2026-11-05) [V]
Runs `gar1`, `gar2`, `gar3` (the lobby test's save, Chapter 1 "The Rift to Skylands" from Flynn's map, `intro1/intro1`): Fiesta, level 1 and
then a level 20 made by `ArkeyanFigureTest --make <file> 3400 xp=96500 sfxp=150000 upgrades=0fff`, dies in the first arena; the log says
`Fiesta is out of health: pick another one.`, `player 1's figure is out of health (the game says its Skylander died)`, `prompt for
player 1`, the Toy Box opens by itself with Fiesta crossed out, Return seats the next card, `the prompt is gone`, `P1: the game took
Eruptor`. Out of health is per tag file: a second Fiesta tag is not crossed out. Y also skips the chapter's opening movie (`[movie]
skipped fmv02 at frame 822 of 1596`).

## The garage (2026-11-05): found on the disc, not reached in a run
- **What it is** [V] (read from the disc): the Academy's **Upgrade Vehicle Station**, Sharpfin's pit crew (`Academy.pak`: the script
  `common_Archetypes_Levels_UpgradeVechicle_script_vs.vvl`, the GUI project `VehicleCustomization`, the quest steps `STO_VUPGRADE_01`
  and `STO_SKY03_01`). Its two lines: "Only a SuperCharger Skylander can customize vehicles." and "Come back later to upgrade and
  customize your SuperCharger!". Persephone's quest `STO_UPGRADE_01` hands over to Sharpfin ("Vehicles can be upgraded and also, but
  not with fairy magic").
- **Why the lobby test's save does not get there** [V]: that save enters the Academy through `SKY_LEVEL=academy`, past Chapter 1;
  Cali says "Incorrect Hub State. Don't Debug Here." and Flynn only offers Chapter 1. The station needs a save made by the story:
  Chapter 1 finished at least, which quest opens it not known [?].
- **The pause menu does not buy** [V] (run `gar2`): Crypt Crusher with 5000 gearbits (written by `ArkeyanFigureTest --edit <file> -
  sfxp=5000`, the game shows 5000) gets the Upgrades screen with Back only.
- **Chapter 1 by remote steering stalled** (runs `gar1`-`gar3`, `SKY_PRESS_FILE` and `SKY_TOYBOX_SCRIPT_FILE`, a frame every 3 s):
  past the first arena and the cage tops, a level 20 High Volt is held in place by a spearman in the second arena. A player, or a
  route recorded once by hand, is the way to the station.

## Open [?]
- Vehicle 2: no prompt for player 2's vehicle met (the Academy is on foot); the seat works in the Toy Box. The error screens (17)
  were not reached.
- A vehicle's mods (performance, specialty, cosmetic, horn) are not mapped: the garage (above) was not reached in a run
  ([figures.md](../../../docs/runtime/host/figures.md#superchargers-vehicles-alchemy-superchargers)).
