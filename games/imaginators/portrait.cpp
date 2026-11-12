// Imaginators: a created character's HUD portrait is a crop of a resolved frame that the game copies on the CPU; resolves
// stay on the host GPU, so the copy read zeros and the portrait was black (#47). The copy's source is written back to
// guest memory first (d3d/resolve_writeback.h; games/imaginators/docs/research.md, "The created character's portrait").
#include "d3d/resolve_writeback.h"
#include "game_hooks.h"
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Texture_CpuCopy
PPC_FUNC_IMPL(GAME_IMP(Texture_CpuCopy));

PPC_FUNC(GAME_HOOK(Texture_CpuCopy))
{
    if (ctx.r3.u32 != 0)
        d3d::WriteBackResolvedTexture(ctx.r3.u32 + 4); // the engine's texture object: its D3D texture at +4
    GAME_IMP(Texture_CpuCopy)(ctx, base);
}
#endif
