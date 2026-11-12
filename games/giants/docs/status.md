# Skylanders: Giants (giants)

> **Purpose:** which disc Giants needs, where the port stands, where to start, and the dated news of the port · **Status:** current (2026-10-28) · **Tags:** [Giants] [Alchemy]

## Disc
| | |
|---|---|
| Release | 2012, Toys for Bob; Xbox 360 only (no Xbox One version); Redump "Skylanders - Giants (USA, Europe) (En,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi)", XGD3, 8.74 GB |
| Title ID, media ID, version | `415608DA`, `52FFE94B`, 2 (`games/giants/info.cpp`) |
| `default.xex` SHA-256 | `12d1e29e...` (full value: `games/giants/game.txt`, `games/giants/info.cpp`) |
| Engine | **Alchemy** (not Lux: proven by the boot, 2026-10-25), XDK 2.0.21076 ([docs/engines/alchemy/README.md](../../../docs/engines/alchemy/README.md)) |

The user's disc stays outside git (`private/discs/` or the path in `GIANTS_ISO`); everything derived from it goes to
`private/giants/`.

## Status
Giants plays from the title to Chapter 1 and through the level tour (78 of 91 levels play to the end; the other 13 are
challenge levels the tour starts without their set-up). Every toy type and variant the disc defines works (level 15,
hats, LightCore, the smart portal, Player 2 with scripted presses). Bink 2 movies are drawn; ultrawide (Hor+), fps
limit 30/60 and HD texture packs work; shaders come from the disc (Arkeyan-Builder); the baseline is 2363 pipelines. It
installs through Arkeyan Launcher (macOS tested). The dated entries, with the numbers: [history.md](history.md).
Feature by feature against SSA: [parity.md](parity.md); what is left: [100.md](100.md).

## Where to start
- The engine: [docs/engines/alchemy/README.md](../../../docs/engines/alchemy/README.md); the boot findings:
  [boot.md](boot.md); every problem and its fix: [problem_catalogue.md](../../../docs/porting/problem_catalogue.md).
- How the port was started (the worked example of a new game): [day1.md](../../../docs/porting/day1.md),
  and its step-by-step form.
- Runs: `tools/test_all.sh --only giants` ([testing.md](../../../docs/getting-started/testing.md), "The regression
  harness"); the tour: `tools/pipelines/tour_giants.sh`.

## Open problems
- The tour cannot start a challenge level the way the game does (with its base mesh and challenge session), so it does
  not test them; reaching them needs a save with the Heroic Challenges bought ([history.md](history.md), "Challenge levels in the tour").
- Giants on a real Windows PC and a real Linux PC (the kits build and start it in tests: Wine, a container); the Windows and Linux icons are implemented but need a real desktop check ([100.md](100.md)).
- Player 2 works with scripted presses; a real second pad is untried ([figures.md](../../../docs/runtime/host/figures.md), "Player 2").
- `XMsgStartIORequest(0xFB, 0x000B0006)` is refused, about 2 per second; the `SKY_D3D_CAPTURE` log prints
  Resolve/DrawIndexed arguments with SSA's layout ([boot.md](boot.md)).
- Real portals and a real LightCore with Giants; a full playthrough by people ([playtest.md](playtest.md)).

## Difficulty (2026-10-28) [Giants] [V]
A new game's difficulty screen has three choices (Easy, "Cyclops Crusher (Medium)" the default, "Heavy Hitter (Hard)"), all open on a
new save: Giants has no locked difficulty and no Nightmare, so `nightmare_from_start` does nothing here (the runtime says so in the
log). Swap Force's Nightmare is the one that is locked: [Swap Force history](../../swapforce/docs/history.md), "Nightmare difficulty".
