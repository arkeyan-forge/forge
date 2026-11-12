# Skylanders: Spyro's Adventure (ssa)

> **Purpose:** which disc SSA needs, where the port stands, and where to start · **Status:** current (2026-10-28) · **Tags:** [SSA]

## Disc
| | |
|---|---|
| Release | 2011, Toys for Bob; Redump "USA, Europe" XGD3; 1142 files, 8.10 GB |
| Title ID, media ID, version | `415608AB`, `53CE7B97`, 0.0.0.2 |
| `default.xex` SHA-256 | `e195f513...` (full value: `games/ssa/game.txt`, `games/ssa/info.cpp`) |
| Engine | Lux + NGL, Bink 1, Wwise, XDK 2.0.20500 ([docs/engines/lux/README.md](../../../docs/engines/lux/README.md)) |

The user's disc stays outside git (`private/discs/` or the path in `SSA_ISO`); everything derived from it goes to
`private/ssa/`.

## Status
SSA is the reference port. It plays well on macOS (Metal, 30 fps at scale 2 in levels, 60 fps opt-in), works on
Windows on a real PC, builds and runs on Linux (Docker; Linux kit since Arkeyan Launcher 0.3.1). Arkeyan Launcher installs it
from the player's disc through the player kit (kit 0.5.0 builds SSA and Giants). The Toy Box, the shared collection,
the smart portal and the portal identity work. The Xbox UWP port is paused. Feature by feature:
[parity.md](parity.md); what is left: [100.md](100.md).
The game also plays from a copy of the disc's data, with no disc image (2026-10-26,
[game_data.md](../../../docs/runtime/host/game_data.md): 1142 files, 8.10 GB copied and verified in 16 s, then 1150
frames at 30 fps from the title).

## Where to start
- The engine: [docs/engines/lux/README.md](../../../docs/engines/lux/README.md); every address:
  [ssa_addresses.md](../../../docs/engines/lux/ssa_addresses.md); the archive with the evidence:
  [engine_notes.md](../../../docs/engines/lux/engine_notes.md).
- How the boot was won: [boot.md](boot.md); the traps: [gotchas.md](../../../docs/porting/gotchas.md).
- Running it: [testing.md](../../../docs/getting-started/testing.md) (isolated runs to `level_027`, the
  `tools/test_all.sh` harness); a subsystem's history: [problem_catalogue.md](../../../docs/porting/problem_catalogue.md).
- Day-1 self-test: `tools/day1.sh ssa <iso>` (2026-10-24: 293/293 entries found, 291 at confidence 1.0, the hash matches).

## Open problems
- The real portal and performance on the tester's Windows PC (Arkeyan Launcher issues #11, #12).
- A full playthrough by hand has not been done ([100.md](100.md), [playtest.md](playtest.md)); every story level loads and plays
  2 minutes, Kaos' final fight starts, the ending and credits play ("Story tour" above); the 2-hour soak is done ([100.md](100.md)).

## Difficulty (2026-10-28) [SSA] [V]
Spyro's Adventure has no difficulty to choose: a new game starts at once and Options (General, Audio, Controls) hold none, so
`nightmare_from_start` does nothing here (the runtime says so in the log). Swap Force's Nightmare is the one that is locked:
[Swap Force history](../../swapforce/docs/history.md), "Nightmare difficulty".
