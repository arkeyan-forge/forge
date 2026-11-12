// The game's TLSF heaps (live_blocks.h) [SuperChargers]: tlsf_add_pool, malloc and memalign mark which blocks belong to
// each pool as it is now; a free of a block from an earlier pool of the same memory (the level heap is made again at
// every zone start, and an object from the menus can be released after the race's zone has made it) is dropped
// instead of merging a stale header into the new pool's free lists, which faulted in sub_82505C10 (an unlink through
// a null link) as races started from the menus (games/superchargers/docs/status.md). A game whose addresses.h does not
// name the four functions keeps its allocator as it is.
#include "game_hooks.h"
#include "live_blocks.h"
#include "memory.h"
#include "platform/platform.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#if defined(GAME_HOOK_TlsfAddPool) && defined(GAME_HOOK_TlsfMalloc) && defined(GAME_HOOK_TlsfMemalign) &&              \
    defined(GAME_HOOK_TlsfFree)
namespace
{
    constexpr uint64_t Covered =
        0x100000000ull; // the whole guest address space: 128 MB reserved, committed where touched

    alchemy::LiveBlocks& Blocks()
    {
        static alchemy::LiveBlocks blocks(
            reinterpret_cast<std::atomic<uint64_t>*>(PlatformReserveMemory(size_t(Covered >> 2) / 8)), Covered);
        return blocks;
    }

    std::atomic<uint32_t> g_dropped{ 0 };
}

PPC_FUNC_IMPL(GAME_IMP(TlsfAddPool));
// tlsf_add_pool(control, memory, bytes)
PPC_FUNC(GAME_HOOK(TlsfAddPool))
{
    Blocks().PoolMade(ctx.r4.u32, ctx.r5.u32);
    GAME_IMP(TlsfAddPool)(ctx, base);
}

PPC_FUNC_IMPL(GAME_IMP(TlsfMalloc));
// tlsf_malloc(control, bytes): the user pointer or 0
PPC_FUNC(GAME_HOOK(TlsfMalloc))
{
    GAME_IMP(TlsfMalloc)(ctx, base);
    Blocks().Allocated(ctx.r3.u32);
}

PPC_FUNC_IMPL(GAME_IMP(TlsfMemalign));
// tlsf_memalign(control, alignment, bytes)
PPC_FUNC(GAME_HOOK(TlsfMemalign))
{
    GAME_IMP(TlsfMemalign)(ctx, base);
    Blocks().Allocated(ctx.r3.u32);
}

PPC_FUNC_IMPL(GAME_IMP(TlsfFree));
// tlsf_free(control, pointer); tlsf_realloc's moves come here too.
PPC_FUNC(GAME_HOOK(TlsfFree))
{
    if (ctx.r4.u32 != 0 && !Blocks().Freed(ctx.r4.u32))
    {
        uint32_t count = g_dropped.fetch_add(1) + 1;
        // The stale header: with bit 1 (previous block free) the free would have merged through a dead pointer.
        if (count <= 8 || count % 100 == 0)
            fprintf(stderr,
                "[heap] free of %08X (heap %08X, lr %08X, header %08X) dropped: its pool was made again since (%u)\n",
                ctx.r4.u32, ctx.r3.u32, uint32_t(ctx.lr), g_memory.Read<uint32_t>(ctx.r4.u32 - 4), count);
        return;
    }
    GAME_IMP(TlsfFree)(ctx, base);
}
#endif
