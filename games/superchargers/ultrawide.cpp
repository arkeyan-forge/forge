// SuperChargers' ultrawide and full-resolution scene hooks (d3d/ultrawide.h, docs/runtime/gpu/ultrawide.md "SuperChargers"):
// Trap Team's (games/trapteam/ultrawide.cpp), re-addressed. The scene is the console's 1040x620 (the chain's root pass has that
// explicit size): at full resolution (the default) the frame's 1280x720, wider in a wide window; the targets are kept in one
// piece. No EDRAM plan was found calling the surfaces' tile count (Trap Team's Scene_SurfaceTiles hook): not hooked. The world
// camera's projection follows the window's aspect (Ultrawide_Projection).
#include "d3d/ultrawide.h"
#include "game_hooks.h"
#include "memory.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Ultrawide_ScenePass
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_ScenePass));
// (r3: a render-target pass object): +32 and +36 its explicit width and height (-1: its parent's, or the display's), +40 and
// +44 its scales of that size, +92 and +96 its width and height once computed. The scene's chain starts at a pass with an
// explicit 1040x620; the others (half, quarter, the reductions) read their parent's computed size.
// Full resolution (the default: game::Adapter::fullScene, ultrawide::SetScene16x9): that explicit size becomes the scene's
// (1280x720, or the wide one) before the original, so the whole chain and the game's own constants (1/size in the post
// effects, FXAA's +-0.5/size) follow. Original resolution: a wide window widens the sizes that come out 1040x620 instead.
PPC_FUNC(GAME_HOOK(Ultrawide_ScenePass))
{
    const uint32_t pass = ctx.r3.u32;
    const bool fullResolution = ultrawide::Scene16x9Width() != ultrawide::SceneWidth16x9;
    if (fullResolution && pass >= 0x10000 && g_memory.Read<uint32_t>(pass + 32) == ultrawide::SceneWidth16x9 &&
        g_memory.Read<uint32_t>(pass + 36) == ultrawide::SceneHeight16x9)
    {
        static std::atomic<bool> logged;
        if (!logged.exchange(true))
            fprintf(stderr, "[scene] full resolution: scene %ux%u -> %ux%u (original_resolution=1 keeps the game's)\n",
                ultrawide::SceneWidth16x9, ultrawide::SceneHeight16x9, ultrawide::SceneWidth(),
                ultrawide::SceneHeight());
        g_memory.Write<uint32_t>(pass + 32, ultrawide::SceneWidth());
        g_memory.Write<uint32_t>(pass + 36, ultrawide::SceneHeight());
    }
    GAME_IMP(Ultrawide_ScenePass)(ctx, base);
    if (fullResolution || !ultrawide::Active() || pass < 0x10000)
        return;
    if (g_memory.Read<uint32_t>(pass + 92) != ultrawide::SceneWidth16x9 ||
        g_memory.Read<uint32_t>(pass + 96) != ultrawide::SceneHeight16x9)
        return;
    static std::atomic<bool> logged;
    if (!logged.exchange(true))
        fprintf(stderr, "[ultrawide] scene %ux%u -> %ux%u\n", ultrawide::SceneWidth16x9, ultrawide::SceneHeight16x9,
            ultrawide::SceneWidth(), ultrawide::SceneHeight());
    g_memory.Write<uint32_t>(pass + 92, ultrawide::SceneWidth());
    g_memory.Write<uint32_t>(pass + 96, ultrawide::SceneHeight());
}
#endif

#ifdef GAME_HOOK_Scene_Tiles
PPC_FUNC_IMPL(GAME_IMP(Scene_Tiles));
// (r3: a set of render targets: +0..+28 the colour surfaces, their count at +32, +36 the depth surface; a surface's +16 and
// +20 are its width and height). The original sums the targets' EDRAM tiles and, past 2048 (the console's 10 MB), splits
// the set into tiles for predicated tiling: the count at +40, rectangles {x0, y0, x1, y1} from +44 (16 bytes each), each
// surface's tile size at +108 and +112. The runtime does not replay draws per tile, so at full resolution a split set is
// written back the way the original writes one that fits: one tile, the whole surface. No run has logged a split here yet
// (16:9, 21:9 and 32:9 into the Academy) [?].
PPC_FUNC(GAME_HOOK(Scene_Tiles))
{
    const uint32_t set = ctx.r3.u32;
    GAME_IMP(Scene_Tiles)(ctx, base);
    if (ultrawide::Scene16x9Width() == ultrawide::SceneWidth16x9 || set < 0x10000 ||
        g_memory.Read<uint32_t>(set + 40) <= 1)
        return;
    const uint32_t colors = g_memory.Read<uint32_t>(set + 32), depth = g_memory.Read<uint32_t>(set + 36);
    const uint32_t first = colors != 0 ? g_memory.Read<uint32_t>(set) : depth;
    if (first < 0x10000)
        return;
    const uint32_t width = g_memory.Read<uint32_t>(first + 16), height = g_memory.Read<uint32_t>(first + 20);
    static std::atomic<bool> logged;
    if (!logged.exchange(true))
        fprintf(stderr,
            "[scene] %ux%u targets kept in one piece (the game split them into %u tiles for the console's EDRAM)\n",
            width, height, g_memory.Read<uint32_t>(set + 40));
    g_memory.Write<uint32_t>(set + 40, 1);
    g_memory.Write<uint32_t>(set + 44, 0);
    g_memory.Write<uint32_t>(set + 48, 0);
    g_memory.Write<uint32_t>(set + 52, width);
    g_memory.Write<uint32_t>(set + 56, height);
    for (uint32_t i = 0; i < colors && i < 8; i++)
        if (uint32_t surface = g_memory.Read<uint32_t>(set + i * 4); surface >= 0x10000)
        {
            g_memory.Write<uint32_t>(surface + 108, width);
            g_memory.Write<uint32_t>(surface + 112, height);
        }
    if (depth >= 0x10000)
    {
        g_memory.Write<uint32_t>(depth + 108, width);
        g_memory.Write<uint32_t>(depth + 112, height);
    }
}
#endif

#ifdef GAME_HOOK_Ultrawide_Projection
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_Projection));
// (r3: the matrix, f1: the horizontal field of view in radians, or a negative number to derive it from f2, the vertical one;
// f3: the aspect; f4, f5: near and far). The world camera's set-up (sub_82B0BC00, Swap Force's sub_82B992A8) passes the
// display's aspect (its viewport object's +8, times the camera's +212) and a fixed horizontal field of view, so a wider
// aspect alone only crops the picture. Hor+ (the other games'): from that call the aspect is the window's and the horizontal
// field grows so that the vertical one stays what 16:9 gave. The HUD's 3D cameras and the shadow maps are left alone.
PPC_FUNC(GAME_HOOK(Ultrawide_Projection))
{
    constexpr double Wide = 16.0 / 9.0;
    if (!ultrawide::Active() || ctx.f1.f64 <= 0 || uint32_t(ctx.lr) != game::UltrawideWorldCameraCall ||
        std::fabs(ctx.f3.f64 - Wide) >= 0.01)
    {
        GAME_IMP(Ultrawide_Projection)(ctx, base);
        return;
    }
    const double aspect = ultrawide::Aspect();
    if (aspect > Wide * 1.01 && aspect < 5.0)
    {
        const double original = ctx.f1.f64;
        ctx.f1.f64 = double(float(2.0 * std::atan(std::tan(original / 2.0) * aspect / Wide)));
        ctx.f3.f64 = double(float(aspect));
        static std::atomic<bool> logged;
        if (!logged.exchange(true))
            fprintf(stderr, "[ultrawide] horizontal field of view %.1f -> %.1f degrees (aspect %.3f)\n",
                original * 180.0 / M_PI, double(ctx.f1.f64) * 180.0 / M_PI, aspect);
    }
    GAME_IMP(Ultrawide_Projection)(ctx, base);
}
#endif
