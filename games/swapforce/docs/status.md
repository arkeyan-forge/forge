# Skylanders: Swap Force (swapforce)

> **Purpose:** which disc Swap Force needs, where the port stands, where to start, and the dated news of the port · **Status:** current (2026-10-28) · **Tags:** [Xbox] [XDK] [Alchemy] [SwapForce]

## Disc
| | |
|---|---|
| Release | 2013, Vicarious Visions (with Beenox); Redump "Skylanders - Swap Force (USA, Australia) (En,Fr)", 8738846720 bytes: the **reference disc** (decided 2026-10-26) |
| Title ID, media ID, version | `415608E9`, `22F1FB91`, `0x00000021` (base version the same; disc 1 of 1) |
| `default.xex` SHA-256 | `5d9a3bc3...` (full: `5d9a3bc3c2d85ed59684a9c5f94daf6bffcfdfedcef7650a866737edbf3f4ac3`), 18563072 bytes; the image is 0x014B0000 bytes at `0x82000000`, entry `0x82F422E8` |
| Engine | **Alchemy** (like Giants), XDK **2.0.21250** (Giants 21076, SSA 20500); D3D linked as **`D3D9`**, not `D3D9LTCG` |

The user's disc stays outside git (`private/discs/` or the path given on the command line); everything derived from it goes to
`private/swapforce/` (day 1: `private/swapforce/day1/`).

## Status
Swap Force is ported, merged and in the kit. It boots to the title and the tutorial and plays past the cutscenes; 1081 of
1081 shaders translate; the zone tour reaches 158 of 195 zones to the end (the 37 staging areas are black as expected),
with 0 faults. Every figure type works; the lava shimmer is fixed (polygon offset). The kit builds it (about 5 min).
The Nightmare difficulty is locked until beaten and can start unlocked (`nightmare_from_start`). The other Redump disc
(Europe, Es/It) runs with the same code. The dated entries, with the numbers: [history.md](history.md).
Feature by feature against SSA: [parity.md](parity.md); what is left: [100.md](100.md).

## Where to start
- The procedure: [the day-1 walkthrough](../../../docs/porting/day1.md); the engine:
  [docs/engines/alchemy/README.md](../../../docs/engines/alchemy/README.md); the previous Alchemy port:
  [games/giants/docs/boot.md](../../giants/docs/boot.md).
- Day 1 again: `tools/day1.sh swapforce <disc .iso>`, then Giants' inventory
  ([sigmatch.md](../../../docs/tools/toolchain/sigmatch.md), "Giants' inventory").
- **Run it** [V]: `-DARKEYAN_GAME=swapforce`, `nice ninja -C build-swapforce -j4 ArkeyanRuntime`, then the isolation block of
  [testing.md](../../../docs/getting-started/testing.md) with `"./build-swapforce/ArkeyanRuntime/Skylanders Swap Force"`,
  `SKY_SKIP_MOVIES=1 SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:9` (Eruptor) and an A every 2 s from 14 to 50 s
  (`SKY_PRESS=a:14,a:16,...,a:50`): the tutorial plays at ~25 s on a quiet machine, ~45-60 s under a heavy load
  ([testing.md](../../../docs/getting-started/testing.md#swap-force-to-the-tutorial-in-25-60-s-swapforce)). Another
  level: `SKY_LEVEL=barrens/brn1` (a zone of `zoneinfos.pak`, [debugging.md](../../../docs/porting/debugging.md)).
- **Test it**: `SWAPFORCE_ISO=<disc> tools/test_all.sh --only swapforce` (build, unit tests, one 75 s run to the
  tutorial with the fault/stub, expected-point, fps, frame and audio checks; `--record` once on a good build).

## Open problems
- Not compared with a console yet: a `k_16_16_16_16_FLOAT` resolve nothing drew, a purple HUD panel in some frames,
  the portal's magic moment ([boot.md](boot.md), "Still open").
- **Player 2 with a swapper remains untested**; a Player 1 swapper is placed through the Toy Box in the scripted test (`tools/regress/swapper_toybox.sh`).
- **The 37 staging areas are black when started alone** [V]: they are pieces of their Swap Zone, not playable zones
  ([history.md](history.md), "Zone tour"). Nothing to fix.

## The light buffer read before it is written (2026-11-10) [V]
The tutorial's frame at 80 s (`SKY_DUMP_FRAME=80 SKY_DUMP_FLOATS=1`): the RGBA16F 2560x1440 target that the deferred
lighting `F0A0A5A4AB9A1935` reads is resolved without a draw or a clear in the frame, and on the Mac it holds NaN in red,
blue and alpha (every texel) and 0 in green. Trap Team's same pass clears it first (`[dump] clear target 0 fmt 10`). The
Mac's picture is right, so the lighting does not keep those values there, but what such a target held was undefined
in D3D12 and Vulkan: another GPU could read other bits [?]. `TargetFor` now clears newly created colour targets to
zero before their first use ([EDRAM model](../../../docs/runtime/gpu/edram_resolve_tiling.md)); reused targets retain
their contents. This removes the undefined initial read. It has not been compared in a game run and is not proof of
the reported green-screen cause ([issue #26](https://github.com/mflr0/Arkeyan-Launcher/issues/26)).
