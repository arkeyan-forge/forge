// Swap Force's ultrawide and full-resolution scene hooks (d3d/ultrawide.h, docs/runtime/gpu/ultrawide.md "Swap Force",
// games/swapforce/docs/boot.md "The scene at full resolution"). The scene's size is the
// render-target pass's (the display's size times the pass's scales); the world camera takes the display's aspect from its
// viewport object. The scene's size is widened after the game's own code has run, only for the 16:9 values.
#include "d3d/ultrawide.h"
#include "game_hooks.h"
#include "memory.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Ultrawide_ScenePass
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_ScenePass));
// (r3: a render-target pass object): +32 and +36 its explicit width and height (-1: its parent's, or the display's),
// +40 and +44 its scales of that size, +88 and +92 its width and height once computed. The scene's chain starts at a pass
// with an explicit 1040x620; the others (half, quarter, the reductions) read their parent's size.
// Full resolution (the default: ultrawide::SetScene16x9 from game::Adapter::fullScene): that explicit size becomes the
// scene's (1280x720, or the wide one) before the original, so the whole chain and the game's own constants (1/size in
// the post effects) follow. Original resolution: a wide window widens the passes that come out 1040x620 instead.
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
    if (g_memory.Read<uint32_t>(pass + 88) != ultrawide::SceneWidth16x9 ||
        g_memory.Read<uint32_t>(pass + 92) != ultrawide::SceneHeight16x9)
        return;
    static std::atomic<bool> logged;
    if (!logged.exchange(true))
        fprintf(stderr, "[ultrawide] scene %ux%u -> %ux%u\n", ultrawide::SceneWidth16x9, ultrawide::SceneHeight16x9,
            ultrawide::SceneWidth(), ultrawide::SceneHeight());
    g_memory.Write<uint32_t>(pass + 88, ultrawide::SceneWidth());
    g_memory.Write<uint32_t>(pass + 92, ultrawide::SceneHeight());
}
#endif

#ifdef GAME_HOOK_Scene_Tiles
PPC_FUNC_IMPL(GAME_IMP(Scene_Tiles));
// (r3: a set of render targets: +0..+28 the colour surfaces, their count at +32, +36 the depth surface; a surface's +16 and
// +20 are its width and height). The original sums the targets' EDRAM tiles and, past 2048 (the console's 10 MB), splits
// the set into tiles for predicated tiling: the count at +40, rectangles {x0, y0, x1, y1} from +44 (16 bytes each), each
// surface's tile size at +108 and +112. The host's targets have no such limit and the runtime does not replay draws per
// tile, so at full resolution a split set is written back the way the original writes one that fits: one tile, the whole
// surface.
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
// fit in 2048 tiles, the game plans their EDRAM once (sub_82593870 copies a pass's size into its surface and calls this,
// returning to PlannerReturn): each surface gets a fixed base (at 1040x620 depth 0, colours 507, 1014, 1521) and some
// passes rely on what another left in those tiles. Otherwise every set of targets is laid out from tile 0 and the passes
// overwrite each other's leftovers (the HUD's masked area under the composite became the shadow mask's white, and the bloom
// made a halo of it). At full resolution the plan counts a surface of the scene's chain (a size that is the scene's times
// a fraction) at the original scene's size, so the plan and every base are the console's; the host's targets are keyed by
// base, format and size and keep their real size. The count must stay the original's afterwards too: setting the targets
// (sub_82514448) counts each planned surface again (byte +125 set), and with real counts the depth at 0 overlapped the
// colour at 507, so the game saved the depth to memory after every pass (84 resolves a frame instead of 46).
constexpr uint32_t PlannerReturn = 0x825938B0;
PPC_FUNC(GAME_HOOK(Scene_SurfaceTiles))
{
    const uint32_t surface = ctx.r3.u32;
    const uint32_t width = surface >= 0x10000 ? g_memory.Read<uint32_t>(surface + 108) : 0;
    const uint32_t height = surface >= 0x10000 ? g_memory.Read<uint32_t>(surface + 112) : 0;
    const uint32_t full = ultrawide::SceneWidth(), fullHeight = ultrawide::SceneHeight();
    const uint32_t original = ultrawide::SceneWidth16x9, originalHeight = ultrawide::SceneHeight16x9;
    // A size of the chain: both sides scale exactly from the scene to the original (1280 -> 1040 is 13/16, 720 -> 620 is
    // 31/36: the chain's halves, quarters and tenths do; the fixed 256x256 or 64x2 targets do not).
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

#ifdef GAME_HOOK_Ultrawide_WorldCamera
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_WorldCamera));
// (r3: the camera object, r4: its view): builds the world camera's projection. Its viewport object (r3 +44) holds the display's
// aspect at +8; the original reads it from there, so the window's aspect is put there for the call only (the composite quad
// and the HUD cameras read the same field and need the 16:9 one). Hor+, as SSA's and Giants': the vertical field of view stays.
PPC_FUNC(GAME_HOOK(Ultrawide_WorldCamera))
{
    const uint32_t viewport =
        ultrawide::Active() && ctx.r3.u32 >= 0x10000 ? g_memory.Read<uint32_t>(ctx.r3.u32 + 44) : 0;
    if (viewport < 0x10000)
    {
        GAME_IMP(Ultrawide_WorldCamera)(ctx, base);
        return;
    }
    const float original = g_memory.Read<float>(viewport + 8);
    static std::atomic<bool> logged;
    if (!logged.exchange(true))
        fprintf(stderr, "[ultrawide] camera aspect %.3f -> %.3f\n", original, ultrawide::Aspect());
    g_memory.Write<float>(viewport + 8, ultrawide::Aspect());
    GAME_IMP(Ultrawide_WorldCamera)(ctx, base);
    g_memory.Write<float>(viewport + 8, original);
}
#endif
