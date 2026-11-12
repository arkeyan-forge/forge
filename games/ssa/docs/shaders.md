# SSA's shaders: the disc's archives and the cache

> **Purpose:** where SSA's shaders come from and how the shader cache is built from the disc · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

## On the disc
- `shaders.lsa` (600 VS + 419 PS) and `effects.lsa` (17 VS + 57 PS) at the disc root, format `\0ASL` v2.
- 81 more containers embedded in `default.xex`.
- Bink 1's movie shaders are built at run time: not on the disc.
- Totals: 1174 containers in the archives and the XEX, 1119 once duplicates are removed, plus the run-time ones; **all 1122 translate**
  (with AIR, SPIR-V and DXIL).
- The 2 that Metal used to reject (`2EB16A1FF553A092`, `CF51652CDDD369A7`; with a third, `shaders.lsa` 975, 977 and 979) are **the grass vertex shaders**
  (`GrassInfo` constants): each samples a wind simulation texture (`SimSampler`) in the vertex stage, which the translator did not support. Their draws were skipped, so **the grass fringes were missing**
  (the islands' edges in the Heroic Challenges: 8 of the 32 challenge levels drew it in a 34-run scan, `challenge_level_018` shows the difference). Fixed by vertex texture fetch:
  [docs/runtime/gpu/shaders.md](../../../docs/runtime/gpu/shaders.md) ("Vertex texture fetch"). Giants has no such vertex shader.

Format, ABI and the translation: [docs/runtime/gpu/shaders.md](../../../docs/runtime/gpu/shaders.md).

## How we extract it
- The player kit: Arkeyan-Builder reads `shaders.lsa`, `effects.lsa` and the XEX image (`shaders=lsa` in
  `games/ssa/game.txt`), runs XenosRecomp, then a short headless run dumps the run-time shaders and the cache is rebuilt
  ([kit_contract.md](../../../docs/release/launcher/kit_contract.md)).
- A developer build: `tools/shaders/build_cache.sh <iso> [<dump dir>]` (`tools/shaders/lsa.py`, `xdvdfs.py`).
- Bink's run-time shaders: [bink.md](../../../docs/engines/lux/bink.md) (`SKY_DUMP_SHADERS`).

## Tests
For the explicit-LOD/cube translator update (2026-11-01), install the rebuilt kit and choose the launcher's game
update/rebuild so the shader cache is regenerated locally; developers rerun the cache recipe above, configure and
relink. Arkeyan-Builder invalidates `shaders/stamp.txt` by the new translator binary's SHA-256; changed translated text also
changes each compiled shader's cache key. It ships in 0.8.0, whose SSA 0.9.5 differs from
0.7.2's 0.9.4, so the launcher offers the update and the rebuild follows; caches stay on the player's machine. No raw
SSA containers were available for this lane's census: repeat the prior two-shader explicit-LOD finding on a complete
local extraction and check reflection mip selection plus the existing grass, shadow, movie and scale 1/2 passes.
See [translation proof and remaining gaps](../../../docs/runtime/gpu/shaders.md).

The game logs `[gpu] shader cache: <n> shaders` at start; `tools/test_all.sh` runs SSA to `level_027` with the cache and
checks the frames ([testing.md](../../../docs/getting-started/testing.md)). The pipeline baseline
(`games/ssa/pipelines/baseline.zst`) is compiled during the loading screens:
[pipeline_cache.md](../../../docs/runtime/gpu/pipeline_cache.md).

## Is it engine-generic?
The `.lsa` archive is **[Lux]**; the containers, the translation and the cache are **[Xbox]** (every game). Giants
(Alchemy) keeps its shaders in an IGA archive instead: [games/giants/docs/shaders.md](../../giants/docs/shaders.md).
