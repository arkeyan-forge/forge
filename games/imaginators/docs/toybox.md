# Imaginators' Toy Box: crystals, Senseis, badges

> **Purpose:** what the in-game Toy Box and the collection show for Imaginators (its creation crystals, Senseis and pictures), how it is proved in the game, what is still open · **Status:** current (2026-11-05) · **Tags:** [Alchemy] [Imaginators] [V] [?]

The same screens and code as the other games (`runtime/host/toybox/`); `runtime/` has no Imaginators case. The tag formats:
[figures.md](../../../docs/runtime/host/figures.md#imaginators-creation-crystals-alchemy-imaginators).

| Part | What [V] |
|---|---|
| Seats | Player 1, Player 2 (a Sensei, a creation crystal or any older figure), Magic item, Adventure (the catalogue's) |
| Creation crystals | 10 types (680-689, one per element), 31 shapes and 3 Legendary shapes as versions; a card shows the class and level ("Ninja · Level 14", French names too), a blank one "Ready to create"; the sheet the name, experience bar on the game's 80 levels, gold, play time, and in Details the parts (body, weapons, hat, powers by the game's names) |
| Senseis | 31 (601-631), 18 decos (Dark, Legendary, holiday, chase) from the owner's dumps; characters with the older games' progress |
| Other toys | the Mystery Chest (235: Bronze, Silver, Gold, two Blue chests), two adventure packs (310, 311) |
| Pictures | the disc's 565 badges (172 px): 422 toys and variants mapped (every Sensei and its decos, each element's crystal and the three Legendary ones, the packs, every older toy); drawn icons left: Smash Hit, the UFO Hat, the Mystery Chest. `IMAGINATORS_ISO=<disc> ArkeyanArtTest` checks them |
| Signature | a crystal made here has none; the port takes it as signed (the hook `GAME_HOOK_Portal_TagCheck`, `tag_signature.cpp`), the card says "Virtual · ..." and the sheet "Virtual: a real game would refuse this crystal" ([figures.md](../../../docs/runtime/host/figures.md#imaginators-creation-crystals-alchemy-imaginators), "Signature") |
| Creation editor | Figure → Edit on a crystal: name (the keyboard; capitals as the game writes them), class (its primary and tertiary powers follow), element (a virtual crystal only: the same file, a new tag of the other type and its first shape), level (1-80 on the game's table), then the 74 parameters as the game stores them (parts, gear, powers, voices and phrases **by the game's own names**, in the game's text language or the disc's first: [figures.md](../../../docs/runtime/host/figures.md#the-parts-names-alchemy-imaginators-v); "none", sliders 0-8, palette colours 0-127); Left / Right, LB / RB or Shift by 10, the mouse wheel and arrows; Save writes it (`collection::WriteCrystal`; a blank crystal gets its first creation), lifting and placing it again when it is on the portal |

**Internal names** (the badges' `collection_<name>_<deco>`): King Pen `bladepenguin`, Tri-Tip `macetriceratops`, Chopscotch
`axegirl`, Boom Bloom `whipgirl`, Pit Boss `staffserpent`, Barbella `doublerearthgirl`, Air Strike `petfalconguy`, Ember
`doublersamurai`, Ambush `swordlifeguy`, Mysticat `staffsphinx`, Starcast `thrownfourarms`, Buckshot `archergoat`, Aurora
`twinbladeslightgirl`, Flare Wolf `bazookafirefox`, Ro-Bow `archerrobot`, Chain Reaction `twinbladeschainsaw`, Wild Storm
`swordarmor`, Tidepool `pistolsquids`; the villains by name (`drkrankcase`, `kaossensei`, `graveclobberer`...); crystals
`cyos_<element>_normal` (undead `death`), `cyos_<element>_leg` (Legendary), `cyos_<element>_sculpt_1..3` (shapes, not mapped [?]).
Matched by looking at each badge and by the decos the dumps hold.

## Proof in the game (2026-11-04, `private/research/run_img.sh <run> <s> <dump>`, `SKY_LEVEL=hub_2016`)
- A Sensei: King Pen (the owner's dump) plays in the hub, HUD "KING PEN", level 1; `bladepenguin.pak` and its banks open.
- The owner's made crystal (Air Ninja) plays in the hub (HUD "NINJA DE L'AIR", level 14, 7759 gold: the values the codec reads).
- The same creation written by our codec onto the owner's blank signed crystal (`ArkeyanFigureTest --make-crystal <out> <blank dump>
  <made dump>`): plays the same; first use as a new crystal (owner written, name and dates cleared).
- A crystal made without a signature, before the hook: refused ("Ton Skylander est peut-être corrompu"). With the hook
  (`GAME_HOOK_Portal_TagSignatureVerify`, 2026-11-05) and the 2016 sector-0 trailer (`FF 07 80 69`, key byte `0x51`: without it the
  game never even checks): the same crystal with Air Ninja's creation plays at level 14 with its 7759 gold (`imgh9`); a blank one
  opens the creator's class choice ("MARTELEUR DE L'AIR", the class previews, the game's writes: `imgh8`); the owner's signed Air
  Ninja plays as before and the hook is never asked (`imgh10`).
- The creation editor (`imge3`): a virtual blank Air crystal edited in the Toy Box (F6, its card, F2, Edit: Knight -> Ninja, level
  1 -> 14, Save), then "Put on Player 1": the game takes it and plays the creation, HUD "NINJA DE L'AIR", level 14.
- The plain crystal (variant 0: the catalogue has no plain one, every row but the first is a shape), 2026-11-05, run `crys*`, a
  blank Undead crystal on the portal, `SKY_LEVEL=hub_2016`: the one the 0.9.0 launcher's Collection made (no 2016 sector-0 trailer)
  was refused ("Ton Skylander est peut-être corrompu", `cyos_vessel_undead.pak` reopened every 4 s, the hook never asked); the same
  tag with only that trailer, a new one from the fixed library, and the old one as it is (mended at placement, `[portal] ...: the 2016
  sector keys of a virtual toy set`, the game's write kept it) open the creator ("CRÉER : MARTELEUR DES MORTS-VIVANTS"); one made
  in the in-game Toy Box (Add a figure, Creation Crystals, Undead Creation Crystal, Create, "Put it on Player 1 now") opens the
  class choice too ([figures.md](../../../docs/runtime/host/figures.md#imaginators-creation-crystals-alchemy-imaginators), "Signature").
- The owner's blank signed crystal (Air Angel 1): the game's creator opens ("CRÉER : MARTELEUR DE L'AIR ... Es-tu prêt à créer :
  Marteleur ? C'est une décision définitive."), the class previews (`cyos_class_preview_*.bk2`) and `cyos_vessel_air.pak` open.

## The portal prompt (2026-11-05) [V]
The Toy Box opens by itself on the seat the game asks for, and closes when the prompt is gone: `games/imaginators/
imaginators_portal_prompt.h` and the adapter's `ReadPortalPrompt` (three guest reads a frame, a prompt counts after 2.5 s as
SuperChargers'), classified by Swap Force's `ClassifySwapForce` (the same state numbers).
- **The object** is SuperChargers' `CMagicMoment` singleton, found by its class as there: the string `CMagicMoment` (`0x820624D4`)
  is registered by `sub_82CF0AC0` with its descriptor `0x837F7410`; the 3-instruction getter of that descriptor, `sub_82CB4538`, is
  slot 1 of the vtable `0x8216099C` (its address found in the image); slots **7 and 8** (SuperChargers': 6 and 7), `sub_82CC2118`
  and `sub_82CC2128`, store and clear the instance at **`0x837F649C`**. (Day 1 looked for slot 6's setter, hence "not found".)
- **The states**: the enum's names (`0x82157FA0`, in value order) are Swap Force's list (Idle 5, IdleOutro 7, Dead 13, Error 16,
  then the online ones); SuperChargers' `FirstVehicleSequence` is gone. **The fields**, from the readers of the instance (`+0x40`
  compared with 0 in most) and run `imgA1` (a dump of the object's changed words each frame, `SKY_LEVEL=hub_2016`, empty portal):
  the state at **`+0x40`** went 0, 1, 2, 3, 17, 5 (SuperChargers': 1, 2, 3, 18, 5, at `+0x2C`); the player at **`+0x210`**
  (SuperChargers' `+0x1FC` moved by the same 0x14) read 0 at the prompt. Player 2 and the -1 "nobody" value are not seen [?].
- **Runs**: `imgA1` (empty portal, empty collection): `[toybox] prompt for player 1`, the Toy Box open on Player 1.
  `imgC1` (empty portal, a scratch collection with a generated Eruptor, Return every 5 s from 95 s): `prompt for player 1`, `open (1
  figures)`, `the game took Eruptor (slot 0, 0.1 s after the change)`, `the prompt is gone`, `closed`, then Eruptor plays in the hub.

## Open [?]
- Player 2's prompts, the Dead state (13) and the error screens (16) were not run.
- The crystal's 0x60-0x6F, Sensei training, the secondary power's numbering.
