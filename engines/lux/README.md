# engines/lux/: the Lux engine's hooks (Spyro's Adventure)

> **Purpose:** what code is in this folder and where its documentation is · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

Hooks that belong to the engine, not to the Xbox and not to one game (class **b**,
[docs/architecture/hooks.md](../../docs/architecture/hooks.md)). Compiled into the runtime when `ARKEYAN_GAME=ssa`; the
addresses come from `games/ssa/addresses.h`.

| File | What |
|---|---|
| `patches.cpp` | `Ngl_SceneTraversal`, `Ngl_ListSubmit` (empty NGL lists), `Heap_Free` (a free that would corrupt the heap) |
| `movie.cpp` | `Movie_Update`: skipping movies (`SKY_SKIP_MOVIES`, `[video] skip_intro_movies`) |
| `light_tiles.cpp` | `LightTile_ClassifyJob`: which scene depth the light tile job reads |
| `ultrawide.cpp` | the 16:9 scene targets and the sun tile grid, widened for ultrawide (`runtime/d3d/ultrawide.h`) |

Documentation: [docs/engines/lux/README.md](../../docs/engines/lux/README.md) (the engine, its pages, the gotchas).
