// The Lux engine's light tile classification (the sun's 32x32-pixel tiles): which resolve of the scene depth its job reads.
#include "d3d/depth_readback.h"
#include "game_hooks.h"
#include "kernel/yield_backoff.h"
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_LightTile_ClassifyJob
PPC_FUNC_IMPL(GAME_IMP(LightTile_ClassifyJob));
// Light tile classification job (worker thread): r4 = the first tile it reads (4 KB per tile) of the scene depth
// resolve, r8 tiles per row, r10 its first row. Records the resolve's physical address for WriteBackDepth.
PPC_FUNC(GAME_HOOK(LightTile_ClassifyJob))
{
    d3d::SetCpuReadDepth(ctx.r4.u32 - ctx.r10.u32 * ctx.r8.u32 * 4096);
    // The job calls Sleep(0) after each tile, a few microseconds of work apart: plain yields, as on the console, or the
    // back-off makes it ~115 ms long and the frame's tile classes several frames late (kernel/yield_backoff.h).
    kernel::WorkingThreadScope working;
    GAME_IMP(LightTile_ClassifyJob)(ctx, base);
}
#endif
