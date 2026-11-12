# SSA: public research

> **Purpose:** what public sources say about SSA and how far we trust them · **Status:** current (2026-10-26) · **Tags:** [SSA]

## Sources
- The other ports and tools (facts only, never their code): [prior_art.md](../../../docs/porting/prior_art.md).
- The franchise, cross-game tables (title IDs, engines, portals): [franchise.md](../../../docs/porting/franchise.md).
- Our own analysis of the disc, which outranks every public source:
  [engine_notes.md](../../../docs/engines/lux/engine_notes.md) §1 and the [Lux pages](../../../docs/engines/lux/README.md).

## Facts
- 2011, Toys for Bob (the disc also has an XPEC logo movie); Xbox 360, PS3, Wii, 3DS ([franchise.md](../../../docs/porting/franchise.md)).
- Title ID `415608AB`, media ID `53CE7B97` (our disc). Engine **Lux + NGL** (our binary analysis).
- Portal: the original Portal of Power (HID portals `1430:0150`; the Xbox 360 USB portal `1430:1F17`)
  ([portal.md](../../../docs/runtime/host/portal.md)).
- Upgrades [V] (2026-10-26): the figure's upgrade field (bits 0-1 the path, 2-5 basic, 6-8 the path's, 9 the Soul
  Gem) and every Skylander's upgrade names, descriptions and prices are in `global.str` (0x54-byte records) and
  `text/<language>/global.lxb` (CRC-32 keys); the variant id is not read ([figures.md](../../../docs/runtime/host/figures.md#upgrades-ssa-giants)).

## Confidence
Everything SSA-specific here is **[V]** from our own disc and runs; the public pages are leads for the other games.
