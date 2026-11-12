// Trap Team's "VRAM copy" of vertex buffers [TrapTeam]. The graphics set-up reads the config's Gfx/@enableVramCopy (absent from
// alchemy.xml, so its default, 1) and, when it is set, the mesh loader (sub_823F0128) copies every vertex buffer of a level into
// the 298 MB video pool (alchemy.xml vramSize) before it is drawn. The boot's own blocks (a 160 MB one, 18, 13, four of 9 MB)
// leave about 15 MB free there, the first level needs more, the pool's allocator answers null and the copy faults at address 0
// (docs/porting/debugging.md SKY_ALLOC_LOG, games/trapteam/docs/boot.md). With the flag at 0 the buffers are read where the
// level's data was loaded, which is physical memory the same way, and the level loads and draws. Why the console has room
// for the copy is not known [?]; this keeps the first level loadable until it is.
#include "game_hooks.h"
#include "memory.h"
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Gfx_ConfigInit
PPC_FUNC_IMPL(GAME_IMP(Gfx_ConfigInit));
PPC_FUNC(GAME_HOOK(Gfx_ConfigInit))
{
    GAME_IMP(Gfx_ConfigInit)(ctx, base);
    for (uint32_t flag : game::VramCopyFlags)
        g_memory.Write<uint8_t>(flag, 0);
    fprintf(
        stderr, "[vram] enableVramCopy forced off (the video pool has no room for the first level's vertex buffers)\n");
}
#endif
