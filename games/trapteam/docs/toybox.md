# Trap Team's Toy Box

> **Purpose:** what the in-game Toy Box shows for Trap Team, how it is proved, and what is still a placeholder · **Status:** current (2026-10-31) · **Tags:** [TrapTeam] [V] [?]

The same screens and code as the other games (`runtime/host/toybox/`, [toybox_ux.md](../../../docs/runtime/host/toybox_ux.md)); `runtime/` has no Trap Team case. What makes it
Trap Team's is data: its catalogue entry ([art.md](art.md)), its pictures from the player's disc, and one sheet block.

| Screen | What it shows [V] (`tools/regress/trapteam_toybox.sh`, 2026-10-30) |
|---|---|
| Seats | Player 1, Player 2 ("needs a 2nd controller"), **Trap**, Magic item, Adventure (the catalogue's 5 seats); figure cards with the game's own pictures |
| A core on Player 1 | Fling Kong: `P1: Fling Kong on slot 0`, the game takes it |
| A trap on the Trap seat | Biter's Bane (an empty Magic Trap, shape 2): its own slot (4), the game takes it; its picture is the trap-screen picture of that shape |
| A trap's sheet | picture, name = the shape, "Magic Trap · Shape · Magic", **"Can hold 3 villains"**: the villains of the trap's element from the game's table, their vault pictures and names |
| Sheets | a Trap Master (Gusto), an Eon's Elite (Eruptor, badge), a mini (Trigger Snappy) |
| Add a figure | by element (Magic ... Dark), then "Minis and sidekicks" and "Traps" (the 11 traps, their 61 shapes and decorations as versions); 180 toys read by the game; every portrait is the game's |

**The trap seat** takes a trap (category `trap`) and nothing else; a trap is no figure of Player 1 (its list offers none). **A trap with a villain** shows it on its sheet: "Holds The Gulper", " · Evolved" when its flag is set, the other villains of
its element shown as escaped ([V] `figures::TrapVillainVerified` is true since the game played a crafted Gulper trap, normal and
evolved: [figures.md](../../../docs/runtime/host/figures.md#acceptance-in-the-game-2026-10-31-v)). The Toy Box only
shows it; the launcher's Edit chooses the villain (a list) and the Evolved switch, with the trap off the portal.
[V] Seen in the game (2026-10-31, a crafted evolved Gulper trap in the collection, the trap seat's sheet): "Holds The Gulper · Evolved" over the element's six villains. `tools/regress/trapteam_toybox.sh` does not check this row yet.

## Placeholders [V]
28 toys draw a generated icon from this disc alone (full list in `ArkeyanArtTest`'s output with `TRAPTEAM_ISO`): 8 Giants' figures (only a later repose has a picture here), Dragonfire Cannon and Scorpion
Striker Catapult, Fiery Forge, Arkeyan Crossbow, the 16 swappers' bottoms (a swapper shows its top: Trap Team's Toy Box has no swapper card yet; Swap Force's halves come from its own disc).
The Toy Box does not tell a Trap
Master from a core on its sheet (both read "Air · from Trap Team"). Shape names other than the catalogue's older ones are "Shape N" ([art.md](art.md)).

## Proof
`TRAPTEAM_ISO=<disc> tools/regress/trapteam_toybox.sh [<run>]` (about 2 min, through `../gamelock.sh` when it exists; scratch collection of six new dumps, offscreen video, disk audio):
13 pictures `private/runs/<run>/shots_*.png` and 6 log checks. The Toy Box opens with F6 at 35 s (the portal prompt is not hooked in the adapter yet, so it does not open by itself).
