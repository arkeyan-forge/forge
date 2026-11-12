// Ultrawide, the Alchemy engine's side (see d3d/ultrawide.h): the size of the 3D scene's targets. The camera's aspect is
// the game's own (games/giants/adapter.cpp).
//
// Found in Giants with a diagnostic build that logs the guest functions storing a value (docs/runtime/gpu/ultrawide.md):
// - sub_8218ED90 gives the scene's width: the back buffer's (1280), at most 1000; sub_8218EDD8 the height, at most 600.
//   Its three callers make the scene's targets and viewports from it: sub_82196600 keeps the chain 1000x600, 500x300,
//   250x150, 125x75 (each half the one before) in the render manager; sub_8219EC20 and sub_8219F328 set up the passes.
// - Two more callers cap the back buffer's size at 1000x600 inline (no call to hook) and pass it on as r4, r5:
//   sub_821939E0 to sub_82196150, sub_8219E0A8 to sub_8219EC20. Those two callees get the wide size.
// The scene keeps its pixel count (ultrawide::SceneHeight): wider and less tall, so that it still fits the EDRAM (a
// 1408x600 scene made the game split it into two tiles, and the left one came out black here). The sizes change only
// when they are the 16:9 ones and the window is wider than 16:9.
#include "d3d/ultrawide.h"
#include "game_hooks.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Ultrawide_SceneWidth
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_SceneWidth));
// () -> the scene's width in pixels.
PPC_FUNC(GAME_HOOK(Ultrawide_SceneWidth))
{
    GAME_IMP(Ultrawide_SceneWidth)(ctx, base);
    if (!ultrawide::Active() || ctx.r3.u32 != ultrawide::SceneWidth16x9)
        return;
    static std::atomic<bool> logged;
    if (!logged.exchange(true))
        fprintf(stderr, "[ultrawide] scene %ux%u -> %ux%u\n", ctx.r3.u32, ultrawide::SceneHeight16x9,
            ultrawide::SceneWidth(), ultrawide::SceneHeight());
    ctx.r3.u64 = ultrawide::SceneWidth();
}
#endif

#ifdef GAME_HOOK_Ultrawide_SceneHeight
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_SceneHeight));
// () -> the scene's height in pixels.
PPC_FUNC(GAME_HOOK(Ultrawide_SceneHeight))
{
    GAME_IMP(Ultrawide_SceneHeight)(ctx, base);
    if (ultrawide::Active() && ctx.r3.u32 == ultrawide::SceneHeight16x9)
        ctx.r3.u64 = ultrawide::SceneHeight();
}
#endif

#if defined(GAME_HOOK_Ultrawide_SceneTargets) || defined(GAME_HOOK_Ultrawide_ScenePasses)
// (r3 object, r4 width, r5 height, ...): the scene's size as its callers capped it.
static void WidenSceneArgument(PPCContext& ctx)
{
    if (ultrawide::Active() && ctx.r4.u32 == ultrawide::SceneWidth16x9 && ctx.r5.u32 == ultrawide::SceneHeight16x9)
    {
        ctx.r4.u64 = ultrawide::SceneWidth();
        ctx.r5.u64 = ultrawide::SceneHeight();
    }
}
#endif

#ifdef GAME_HOOK_Ultrawide_SceneTargets
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_SceneTargets));
PPC_FUNC(GAME_HOOK(Ultrawide_SceneTargets))
{
    WidenSceneArgument(ctx);
    GAME_IMP(Ultrawide_SceneTargets)(ctx, base);
}
#endif

#ifdef GAME_HOOK_Ultrawide_ScenePasses
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_ScenePasses));
PPC_FUNC(GAME_HOOK(Ultrawide_ScenePasses))
{
    WidenSceneArgument(ctx);
    GAME_IMP(Ultrawide_ScenePasses)(ctx, base);
}
#endif
