// The Lux engine's light tile classification (the sun's 32x32-pixel tiles): which resolve of the scene depth its job reads.
#include "d3d/depth_readback.h"
#include "game_hooks.h"
#include "kernel/yield_backoff.h"
#include <ppc_config.h>
#include <ppc_context.h>
#include "netplay/session_runtime.h"
#include <algorithm>
#include <array>

#ifdef GAME_HOOK_LightTile_ClassifyJob
namespace
{
    // Online, the job's depth comes from the host GPU, whose last bits differ between two GPUs (an AMD and an NVIDIA
    // card diverged in the first lit scene): its inputs and outputs only choose how the sun's tiles are drawn, so they
    // are left out of the state check. r3 the tiles' classes (a byte each), r4 their depth (4 KB each), r5 and r6 a
    // float each, r8 x r9 tiles; its stack frame and the caller's results below r1 + 0x100 hold what it computed.
    void MarkRenderOnly(const PPCContext& ctx)
    {
        static std::array<uint32_t, 4> marked{};
        const uint32_t tiles = ctx.r8.u32 * ctx.r9.u32;
        if (tiles == 0 || tiles > 4096 || std::find(marked.begin(), marked.end(), ctx.r4.u32) != marked.end())
            return;
        static size_t next = 0;
        marked[next++ % marked.size()] = ctx.r4.u32;
        netplay::SkipRenderOnly(ctx.r4.u32, ctx.r4.u32 + tiles * 4096);
        netplay::SkipRenderOnly(ctx.r3.u32, ctx.r3.u32 + tiles);
        netplay::SkipRenderOnly(ctx.r5.u32, ctx.r5.u32 + tiles * 4);
        netplay::SkipRenderOnly(ctx.r6.u32, ctx.r6.u32 + tiles * 4);
        netplay::SkipRenderOnly(ctx.r1.u32 - 0x4000, ctx.r1.u32 + 0x100);
    }
}

PPC_FUNC_IMPL(GAME_IMP(LightTile_ClassifyJob));
// Light tile classification job (worker thread): r4 = the first tile it reads (4 KB per tile) of the scene depth
// resolve, r8 tiles per row, r10 its first row. Records the resolve's physical address for WriteBackDepth.
PPC_FUNC(GAME_HOOK(LightTile_ClassifyJob))
{
    d3d::SetCpuReadDepth(ctx.r4.u32 - ctx.r10.u32 * ctx.r8.u32 * 4096);
    if (netplay::Online())
        MarkRenderOnly(ctx);
    // The job calls Sleep(0) after each tile, a few microseconds of work apart: plain yields, as on the console, or the
    // back-off makes it ~115 ms long and the frame's tile classes several frames late (kernel/yield_backoff.h).
    kernel::WorkingThreadScope working;
    GAME_IMP(LightTile_ClassifyJob)(ctx, base);
}
#endif
