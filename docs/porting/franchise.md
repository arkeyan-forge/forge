# Franchise tables: games, engines, portals, toy types, compatibility

> **Purpose:** what is publicly known about the six Skylanders games on the Xbox 360 (engine, title ID, portal, toy types, cross-game compatibility) and shared port architecture · **Status:** current (2026-11-12; public source notes dated 2026-10-25) · **Tags:** [Lux] [Alchemy] [SSA] [Giants]

Per-game detail and sources: the [game index](../README.md#per-game-gamesiddocs) and each game's `research.md`. Confidence: [2] two sources, [1] one, [inf]
inferred, [V] verified on our disc.

## Games and engines
| Game | Year | Developer | 360 title ID | Engine | Also on |
|---|---|---|---|---|---|
| SSA | 2011 | Toys for Bob | `415608AB` [V] | **Lux + NGL** [V] (XDK 20500) | PS3, Wii, 3DS |
| Giants | 2012 | Toys for Bob | `415608DA` [V] | **Alchemy** [V] (our boot, 2026-10-25; XDK 21076) | + Wii U |
| Swap Force | 2013 | Vicarious Visions, Beenox | `415608E9` [V] | **Alchemy** [V] | + One, PS4 |
| Trap Team | 2014 | Toys for Bob, Beenox | `41560911` [V] | **Alchemy** [V] | + One, PS4, mobile |
| SuperChargers | 2015 | Vicarious Visions, Beenox | `4156091F` [V] | **Alchemy** [V] | + One, PS4 |
| Imaginators | 2016 | Toys for Bob | `41560930` [V] | **Alchemy** [V] | + One, PS4, Switch |

Disc identities and measured formats are recorded in each game's research page. The early public search found no Kinect use.
Sources: Wikipedia per game; fan recompile READMEs (tsapZ/*Recomp, TheBiemGamer/GiantsRecomp); Xenia Canary issue 1218.

## Portals by generation
| Portal | Games | USB (PC/Wii/PS) | Xbox 360 | Notes |
|---|---|---|---|---|
| Portal of Power | SSA, Giants | `1430:0150` HID [2] | `1430:1F17` wired, non-HID, 0x20 reports [1] | 4 figures |
| Swap Force | Swap Force | `1430:0150` [inf] | same family [inf] | 16 figures, flatter |
| Traptanium | Trap Team | `1430:0150` [2] | [inf] | speaker, two-sided ring, trap slot LED |
| SuperChargers | SuperChargers | not found | not found | larger, no lights |
| Imaginators | Imaginators | not found | not found | modified SuperChargers, 3 runes |

Xbox One portals use GIP; the runtime handles `1430:09AA`/`1430:09AB` and `24C6:FA00`
([portal backends](../runtime/host/portal.md)). Console portal families use different transports.
Sources: [Wiki-Portal](https://skylanderswiki.com/wiki/Portal_of_Power), [Wilson gist](https://gist.github.com/skylandersNFC/5c9fb3debc11c1ea194ed6ddc9fb8faf), [Kneppers](https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/).

## Protocol additions (over SSA's `R S Q W A C`)
| Command | Meaning | First seen |
|---|---|---|
| `S` with 16 entries | status array of 16 figures (was 4) | Swap Force [1] |
| `J` | fade one ring side (colour + time), reply needed | Trap Team [1] |
| `L` | ring side colour or trap-slot brightness, no reply | Trap Team [1] |
| `M` | speaker on/off, reply; then 8 kHz 16-bit mono audio (byte pairs swapped) on endpoint 1 | Trap Team [1] |
Notes: documented on Wii/PC portals; Xbox 360 framing of `J L M` not found. `SKY_PORTAL_LOG=1` on each disc decides.

## Toy types by game
| Game | New types | Tag nibble |
|---|---|---|
| SSA | core figures, items | 0 |
| Giants | Giants, LightCore | 1 |
| Swap Force | Swappers (two tags) | 2 |
| Trap Team | Traps, Trap Masters, Minis, Light/Dark, Eon's Elite | 3 |
| SuperChargers | Vehicles, SuperChargers, trophies | 4 |
| Imaginators | Creation Crystals, Senseis | not found |
Tag: MIFARE Classic 1K, 64 blocks, block 1 holds figure ID, web code, variant ID with the game nibble [1].

## Compatibility
Rule [2]: a toy works in its release game and every later one. Portals: Xbox 360 portals only on 360.
| Toy from \ Game | SSA | Giants | SF | TT | SC | IMG |
|---|---|---|---|---|---|---|
| SSA | yes | yes | yes | yes | yes | yes |
| Giants | no | yes | yes | yes | yes | yes |
| Swap Force | no | no | yes | yes | yes | yes |
| Trap Team | no | no | no | yes | yes | yes |
| SuperChargers | no | no | no | no | yes | yes (no on Switch, 360: not confirmed) |
| Imaginators | no | no | no | no | no | yes |
Traps work in SuperChargers [1]. Not confirmed: vehicles and traps in Imaginators on 360.

## Shared port architecture

All six games have ports. SSA uses Lux; the other five use Alchemy. The shared kernel, D3D, XMA and Bink layers
serve both engines. Alchemy hooks live in [engines/alchemy/](../engines/alchemy/README.md), with per-game addresses
and adapters under `games/<id>/`. Re-find addresses and verify archive versions for each binary.
Current versions, tested platforms and remaining limits are in the [project README](../../README.md).

## Prior art (read-only, public)
Fan recompiles of all six games exist (tsapZ/*Recomp, MIT, ReXGlue/Xenia based) and a shared portal library
(TheBiemGamer/skylanders-portal). Useful as facts (engine, Bink 2, EDRAM issues); these are references for facts only; no outside code is copied or adapted.

SSA's Wii version uses Alchemy; the Xbox 360 version uses Lux. The 30/60 fps measurements are in
[frame_pacing.md](../engines/lux/frame_pacing.md).
