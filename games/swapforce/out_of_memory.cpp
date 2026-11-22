// Swap Force's out-of-memory screen: a memory pool that has no room calls the title's handler, which shows a green
// picture for ever (forge issue #84, players' logs in Wildwoods) [SwapForce]. Once per run, before the handler: which
// pool, the size asked and the pool's fields, so a player's log names the pool that runs out.
#include "game_hooks.h"
#include "memory.h"
#include "thread.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_OutOfMemory
PPC_FUNC_IMPL(GAME_IMP(OutOfMemory));
// (request): request = {pool, 0, bytes asked, the pool's allocation count, 1, -1 x 4}.
PPC_FUNC(GAME_HOOK(OutOfMemory))
{
    static std::atomic<bool> reported{ false };
    if (!reported.exchange(true))
    {
        const uint32_t request = ctx.r3.u32;
        const uint32_t pool = g_memory.Read<uint32_t>(request);
        // The pool: its index at +12, its memory at +16 (base) and +20 (bytes), its lowest free space so far at +80.
        fprintf(stderr,
            "[memory] out of memory: pool %u (0x%08X, class 0x%08X, %u bytes at 0x%08X, lowest free so far %u) has no "
            "room for %u bytes (its allocation %u); the game shows its green out-of-memory screen\n",
            g_memory.Read<uint32_t>(pool + 12), pool, g_memory.Read<uint32_t>(pool), g_memory.Read<uint32_t>(pool + 20),
            g_memory.Read<uint32_t>(pool + 16), g_memory.Read<uint32_t>(pool + 80),
            g_memory.Read<uint32_t>(request + 8), g_memory.Read<uint32_t>(request + 12));
        // The rest of the pool's object as words (its parent, its counts).
        char words[25 * 9 + 1] = {};
        for (uint32_t i = 0; i < 25; i++)
            snprintf(words + i * 9, 10, " %08X", g_memory.Read<uint32_t>(pool + i * 4));
        fprintf(stderr, "[memory] the pool's first 100 bytes:%s\n", words);
        if (g_memory.Read<uint32_t>(pool) == game::BlockPoolClass)
        {
            const uint32_t list = g_memory.Read<uint32_t>(pool + game::BlockPoolFreeListOffset);
            const uint32_t count = g_memory.Read<uint32_t>(list + 8), data = g_memory.Read<uint32_t>(list + 20);
            uint64_t total = 0;
            uint32_t largest = 0;
            for (uint32_t i = 0; i < count; i++)
            {
                const uint32_t bytes = g_memory.Read<uint32_t>(data + i * 8 + 4);
                total += bytes;
                largest = bytes > largest ? bytes : largest;
            }
            fprintf(stderr, "[memory] its free space: %llu bytes in %u blocks, the largest %u bytes\n",
                (unsigned long long)total, count, largest);
        }
        fprintf(stderr, "[memory] the allocation comes from %s\n", GuestCallChain(ctx).c_str());
    }
    GAME_IMP(OutOfMemory)(ctx, base);
}
#endif
