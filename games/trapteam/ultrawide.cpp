// Trap Team's ultrawide hooks (d3d/ultrawide.h, docs/runtime/gpu/ultrawide.md; the same engine as Swap Force's
// games/swapforce/ultrawide.cpp, re-addressed by sigmatch and shape, with the pass object's computed size 4 bytes further).
// The scene is the console's 1040x620 (the chain's root pass has that explicit size): at full resolution (the default) the
// frame's 1280x720, with the tiling and the EDRAM plan of the console kept (the hooks of games/swapforce/ultrawide.cpp,
// re-addressed); with original_resolution, widened to the window's shape at the same pixel count. The world camera's
// projection follows the aspect (Ultrawide_Projection).
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
// written back the way the original writes one that fits: one tile, the whole surface.
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

#ifdef GAME_HOOK_Scene_SurfaceTiles
PPC_FUNC_IMPL(GAME_IMP(Scene_SurfaceTiles));
// (r3: a surface; +108 and +112 its tile size, +132 the EDRAM tiles it takes, computed here). When all the scene's passes
// fit in 2048 tiles, the game plans their EDRAM once (sub_82443DF8 copies a pass's computed size into its surface and calls
// this, returning to PlannerReturn): each surface gets a fixed base and some passes rely on what another left in those
// tiles. Otherwise every set of targets is laid out from tile 0 and the passes overwrite each other's leftovers. At full
// resolution the plan counts a surface of the scene's chain (a size that is the scene's times a fraction) at the original
// scene's size, so the plan and every base are the console's; the host's targets are keyed by base, format and size and
// keep their real size. The count must stay the original's afterwards too: setting the targets (sub_823EC8E0, byte +125 set
// on a planned surface) counts each planned surface again.
constexpr uint32_t PlannerReturn = 0x82443E44;
PPC_FUNC(GAME_HOOK(Scene_SurfaceTiles))
{
    const uint32_t surface = ctx.r3.u32;
    const uint32_t width = surface >= 0x10000 ? g_memory.Read<uint32_t>(surface + 108) : 0;
    const uint32_t height = surface >= 0x10000 ? g_memory.Read<uint32_t>(surface + 112) : 0;
    const uint32_t full = ultrawide::SceneWidth(), fullHeight = ultrawide::SceneHeight();
    const uint32_t original = ultrawide::SceneWidth16x9, originalHeight = ultrawide::SceneHeight16x9;
    // A size of the chain: both sides scale exactly from the scene to the original (1280 -> 1040 is 13/16, 720 -> 620 is
    // 31/36: the chain's halves, quarters and reductions do; the fixed 256x256 or 64x2 targets do not).
    const bool planned =
        uint32_t(ctx.lr) == PlannerReturn || (surface >= 0x10000 && g_memory.Read<uint8_t>(surface + 125) != 0);
    const bool chain = ultrawide::Scene16x9Width() != original && planned && width != 0 && height != 0 &&
                       width <= full && height <= fullHeight && width * original % full == 0 &&
                       height * originalHeight % fullHeight == 0;
    if (!chain)
    {
        GAME_IMP(Scene_SurfaceTiles)(ctx, base);
        return;
    }
    g_memory.Write<uint32_t>(surface + 108, width * original / full);
    g_memory.Write<uint32_t>(surface + 112, height * originalHeight / fullHeight);
    GAME_IMP(Scene_SurfaceTiles)(ctx, base);
    g_memory.Write<uint32_t>(surface + 108, width);
    g_memory.Write<uint32_t>(surface + 112, height);
}
#endif

#ifdef GAME_HOOK_Ultrawide_Projection
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_Projection));
// (r3: the matrix, f1: the horizontal field of view in radians, or a negative number to derive it from f2, the vertical one;
// f3: the aspect; f4, f5: near and far): the game gives its world camera a fixed horizontal field of view and derives the
// vertical one from the aspect, so a wider aspect alone only crops the picture. Hor+ (the other games'): for a wider aspect
// the horizontal field grows so that the vertical one stays what 16:9 gave. Two builders feed the world camera: sub_8243C700
// (the aspect of the scene's target, already wide) and sub_8230F3D8 (the display's aspect global, 16:9: two calls, from the
// return addresses below, get the window's aspect here). The global itself stays: the camera rig reads it and with 16:9 it
// follows the character as before, but with a wide one it pulls back and tilts away from him. The HUD's 3D cameras and the
// shadow maps come with 16:9 or a tall aspect and are left alone.
constexpr uint32_t WorldCameraCalls[] = { 0x8230F788, 0x8230F7F4 };
PPC_FUNC(GAME_HOOK(Ultrawide_Projection))
{
    constexpr double Wide = 16.0 / 9.0;
    if (!ultrawide::Active() || ctx.f1.f64 <= 0)
    {
        GAME_IMP(Ultrawide_Projection)(ctx, base);
        return;
    }
    double aspect = ctx.f3.f64;
    for (uint32_t call : WorldCameraCalls)
        if (uint32_t(ctx.lr) == call && std::fabs(aspect - Wide) < 0.01)
            aspect = ultrawide::Aspect();
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
