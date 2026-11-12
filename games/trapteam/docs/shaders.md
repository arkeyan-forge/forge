# Trap Team's shaders: the disc and the cache

> **Purpose:** where Trap Team's shaders are on the disc and how its shader cache is built · **Status:** current (2026-10-30) · **Tags:** [Xbox] [Alchemy] [TrapTeam] [V]

## On the disc
Xenos containers inside IGZ v8 files, themselves inside the `.arc`/`.bld` archives (IGA v11, little-endian; [game_art.md](../../../docs/runtime/host/game_art.md)).
A walk of all 1916 archives (lane F, 2026-10-30) finds **862 distinct containers**: 412 vertex and 450 pixel shaders.
The game also builds some at run time: of the 225 containers a run to the first level dumps (`SKY_DUMP_SHADERS`), **209 are byte for byte on the disc
and 16 are not** (compared by content: the names differ, the dump's are XXH3 of the container).

## How we extract it
1. Disc containers: Arkeyan-Builder reads every `.arc`/`.bld` archive through `tools/arkeyan-builder/igz_shaders.cpp` (IGA v11 / IGZ v8), using `shaders=igz:all` in `games/trapteam/game.txt`. The original measured extraction used `private/trapteam/tools/shx` (not committed).
2. Run-time containers: a run with `SKY_DUMP_SHADERS=<dir>` ([debugging.md](../../../docs/porting/debugging.md)).
3. Merge by content into one folder (the disc's files, plus a dump file only if no disc file has the same bytes: 862 + 16 = 878).
4. **The movie pass:** a run with the intro movies played (not `SKY_SKIP_MOVIES`) and `SKY_DUMP_SHADERS`, 40 s offscreen, dumps Bink's two YUV to RGB shaders; add them to the merged folder (880).
   Without them the logo and story movies are black (`[render] no translated vertex shader ... its draws are skipped`) [V].
5. `SKY_GAME=trapteam tools/shaders/build_cache.sh <disc> <merged folder>` writes `private/trapteam/ppc/shader_cache.cpp`; configure again and relink.

Result [V]: **878 of 878 translate (880 with the movie shaders), 0 without DXIL** (about 70 s). Before: 189 of the 225 dumped, 36 vertex shaders stopped on `TexCoord 8`.
`TexCoord 8` needs location 17 in XenosRecomp (`patches/xenosrecomp.patch`) and in `d3d/shaders.cpp` `InputLocation`; 16 is the point size.
The patch change makes the other games' caches stale too: they are not regenerated here.

## What it fixed in the first level (2026-10-30) [V]
The frame of `level_313_tutorial`'s intro, 31 s after the level opens (same presses, `SKY_DUMP_FRAME=90` / `91`; before: frame 2091 of a run
whose level opened at 58 s, after: frame 2241, level opened at 59.6 s): the left half of the picture was black and the right half dark, the
character lacked its hat, feather and binoculars, and the ground. After, the whole frame is drawn (sky, palms, rocks, fence, purple goo,
ground, shadows, the character complete). Nothing in the picture is worse.

## Is it engine-generic?
The cache recipe is Alchemy's (as Giants' and Swap Force's); the folder merge by content is new here.

## Player kit
The explicit-LOD/cube translator update (2026-11-01) affects 64 of the 880 local distinct containers; two also need
the cube operand fix. Update/rebuild the installed game with the rebuilt kit so its shaders are translated on the
player's machine. Arkeyan-Builder's `shaders/stamp.txt` hashes `bin/XenosRecomp` and the common header; the new binary
invalidates the stamp, and new HLSL changes the affected compiled cache keys. Developers rerun the merged-input cache
recipe above, configure and relink, retaining the movie inputs. It ships in 0.8.0 (Trap Team
0.0.4, 0.7.2 had 0.0.3), so the launcher offers the update and the rebuild follows. No player
cache ships in the kit. On hardware check reflection mip/roughness and cube direction as the camera moves, no sparkle
or sharp reflection regression, then shadows, 2D/post passes and movies at scales 1 and 2 on Metal, D3D12 and Vulkan.
[Audit, translation proof and remaining gaps](../../../docs/runtime/gpu/shaders.md).

Trap Team is in `tools/make_kit.sh`'s default games and uses the same SHA-256 identification and per-game checks as the
other games. Only `game.txt` needed a change: the disc hash stays the same, `config/` stays unchanged, and the new fields
supply the shader recipe and a best-effort movie pass. The kit contains no disc containers, run-time dumps or shader cache.

The movie pass requests an empty emulated portal and A every 4 s from 12 to 112 s (the cadence in [boot.md](boot.md)),
and stops at 18 new shaders or 120 s: the 16 the game builds at start come in the first second, Bink's two when the first logo movie plays.
A target of 16 ended the run before them, and every movie was black in a player's install (2026-10-30, the beta's clean install). The target
counts new cache hashes, not a prescribed list of containers.
A partial collection warns, then records `movies/done.txt`; remove that marker to retry a collection. For a complete cache,
use the manual content merge above (862 + 16 = 878 observed containers), keeping every dump and cache in `private/`.

Disc-free verification and regression checks for SSA, Giants and both Swap Force editions:
[Arkeyan-Builder checks](../../../docs/tools/arkeyan-builder.md#trap-team-and-disc-free-checks). This integration was checked without a
disc extraction, kit build or game run; the 862/878 counts above come from the earlier measured recipe.
## The post pass (2026-10-30) [V]
Tone map and FXAA 3.11 are one pixel shader, `6BD91E6572C6581C` (vertex `A6F60BE72C1BA743`), into a 1040x620 target; the frame draw (`661D0F94B904A567`) then stretches it to 1280x720.
`game::HostKernelPixelShader` names it: at scale N its constants c0, c1 (+-0.5, 8, -4 over the size) are divided by N so that the edge filter stays in guest pixels ([status.md](status.md) "The picture").
