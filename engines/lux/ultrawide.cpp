// Ultrawide, the Lux engine's side (see d3d/ultrawide.h): the sizes of the 3D scene's targets. The camera's aspect is
// the game's own (games/ssa/adapter.cpp).
//
// All of it is found from the 1120x704 scene of the Xbox 360 build and the 16:9 floats of its data, and checked against
// the other recompilation of this game (tsapZ/SSARecomp, MIT: THIRD_PARTY_NOTICES.md), which found the same places:
// - The scene's targets are textures made by sub_829611D8 (r5 width, r6 height; usage 0x80 makes the depth texture of
//   the same size): 1120x704 for the scene, 560x352 for the half-size passes, 280x176 and 288x176 for the quarter-size
//   ones, 144x96 for the eighth (the bloom and luminance chain, 288 and 144 being 280 and 140 rounded up to 32 and 16).
//   The header of one more 1120x704 texture goes through sub_823B38E0 from sub_82929EE8.
// - The sun's light tiles: sub_828924B8 keeps the scene size and a grid of 32x32-pixel cells (35 x 22) in the light
//   manager, which the tile classification job and the tile passes use.
// Every size is changed only when it is exactly the 16:9 one, and only when the window is wider than 16:9.
#include "d3d/ultrawide.h"
#include "game_hooks.h"
#include "memory.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    // The light manager's fields (sub_828924B8): scene width and height, grid columns and rows.
    constexpr uint32_t LightSceneWidth = 488;
    constexpr uint32_t LightColumns = 496;

    constexpr uint32_t Round(uint32_t value, uint32_t multiple) { return (value + multiple - 1) / multiple * multiple; }

    // The wide size of a 16:9 scene target (width, height), or false when it is not one of them.
    bool WideSize(uint32_t& width, uint32_t& height)
    {
        const uint32_t wide = ultrawide::SceneWidth();
        if (width == 1120 && height == 704)
            width = wide;
        else if (width == 560 && height == 352)
            width = wide / 2;
        else if (width == 280 && height == 176)
            width = wide / 4;
        else if (width == 288 && height == 176)
            width = Round(wide / 4, 32);
        else if (width == 144 && height == 96)
            width = Round(wide / 8, 16);
        else
            return false;
        return true;
    }
}

#ifdef GAME_HOOK_Ultrawide_CreateTexture
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_CreateTexture));
// Creates a texture (r3 usage, r4 format, r5 width, r6 height, r7 levels, r8 slices).
PPC_FUNC(GAME_HOOK(Ultrawide_CreateTexture))
{
    if (ultrawide::Active())
    {
        uint32_t width = ctx.r5.u32, height = ctx.r6.u32;
        if (WideSize(width, height))
        {
            static std::atomic<uint32_t> logged;
            if (logged.fetch_add(1) < 12)
                fprintf(stderr, "[ultrawide] texture %ux%u -> %ux%u\n", ctx.r5.u32, ctx.r6.u32, width, height);
            ctx.r5.u64 = width;
        }
    }
    GAME_IMP(Ultrawide_CreateTexture)(ctx, base);
}
#endif

#ifdef GAME_HOOK_Ultrawide_TextureHeader
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_TextureHeader));
// Texture header setup (r3 width, r4 height). The one 1120x704 header of sub_82929EE8 (its call returns to 0x82929FCC)
// is a scene texture too.
PPC_FUNC(GAME_HOOK(Ultrawide_TextureHeader))
{
    if (ultrawide::Active() && uint32_t(ctx.lr) == game::UltrawideSceneHeaderReturn && ctx.r3.u32 == 1120 &&
        ctx.r4.u32 == 704)
        ctx.r3.u64 = ultrawide::SceneWidth();
    GAME_IMP(Ultrawide_TextureHeader)(ctx, base);
}
#endif

#ifdef GAME_HOOK_Ultrawide_LightManagerSetup
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_LightManagerSetup));
// The light manager's setup stores the scene size and the grid of 32x32-pixel cells (35 x 22) and reads neither again
// (only the rows), so the wide ones are put in afterwards.
PPC_FUNC(GAME_HOOK(Ultrawide_LightManagerSetup))
{
    uint32_t manager = ctx.r3.u32;
    GAME_IMP(Ultrawide_LightManagerSetup)(ctx, base);
    if (!ultrawide::Active())
        return;
    g_memory.Write<uint32_t>(manager + LightSceneWidth, ultrawide::SceneWidth());
    g_memory.Write<uint32_t>(manager + LightColumns, (ultrawide::SceneWidth() + 31) / 32);
}
#endif
