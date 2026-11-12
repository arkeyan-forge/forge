// Fixes applied to the game's own code. Each patch says what it guards against and why it is safe.
#include "game_hooks.h"
#include "memory.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Heap_Free
namespace
{
    // The game's main heap (sub_828A25F8 free in SSA): a block is [previous block, size | in use (bit 0)] then the
    // payload; free blocks of up to 256 bytes sit in lists linked through payload words 0 and 1, larger ones in a
    // tree. heap+512 is the block kept apart from the lists, heap+544 the heap's critical section.
    constexpr uint32_t HeapUnlisted = 512;
    constexpr uint32_t HeapLock = 544;
    constexpr uint32_t HeapSmallLimit = 256;

    // A free neighbour that the free will unlink: its links must exist (unless it is the unlisted block).
    bool FreeNeighbourIntact(uint32_t heap, uint32_t block)
    {
        uint32_t head = g_memory.Read<uint32_t>(block + 4);
        if ((head & 1) || head == 0 || (head & ~3u) > HeapSmallLimit ||
            g_memory.Read<uint32_t>(heap + HeapUnlisted) == block)
            return true;
        return g_memory.Read<uint32_t>(block + 8) != 0 && g_memory.Read<uint32_t>(block + 12) != 0;
    }

    // Why freeing `block` would corrupt the heap further, or nullptr. Call with the heap's lock held.
    const char* HeapFreeProblem(uint32_t heap, uint32_t block)
    {
        uint32_t head = g_memory.Read<uint32_t>(block + 4);
        if (!(head & 1))
            return "block already free";

        uint32_t next = block + 8 + (head & ~3u);
        uint32_t nextHead = g_memory.Read<uint32_t>(next + 4);
        if (nextHead != 0 && (g_memory.Read<uint32_t>(next) != block || !FreeNeighbourIntact(heap, next)))
            return "next block corrupt";

        uint32_t previous = g_memory.Read<uint32_t>(block);
        if (previous != 0 && (previous + 8 + (g_memory.Read<uint32_t>(previous + 4) & ~3u) != block ||
                                 !FreeNeighbourIntact(heap, previous)))
            return "previous block corrupt";
        return nullptr;
    }
}
#endif

// NGL scene traversal (called by ListSendModule for the submitted frame, then recursively per scene).
// A frame submitted without any scene (seen while the first movie starts) leaves the list empty and the
// traversal dereferences it. An empty list has nothing to draw, so skip it.
#ifdef GAME_HOOK_Ngl_SceneTraversal
PPC_FUNC_IMPL(GAME_IMP(Ngl_SceneTraversal));
PPC_FUNC(GAME_HOOK(Ngl_SceneTraversal))
{
    if (g_memory.Read<uint32_t>(game::NglCurrentScene) == 0)
        return;

    GAME_IMP(Ngl_SceneTraversal)(ctx, base);
}
#endif

// NGL render list submission (called with a scene's render list, scene+0x348). nglListBeginScene leaves the list
// null until something is drawn into the scene, and the engine assumes every scene gets at least one item. Frames
// drawn while a movie has no picture ready break that assumption; an empty list has nothing to submit, so skip it.
#ifdef GAME_HOOK_Ngl_ListSubmit
PPC_FUNC_IMPL(GAME_IMP(Ngl_ListSubmit));
PPC_FUNC(GAME_HOOK(Ngl_ListSubmit))
{
    if (ctx.r3.u32 == 0)
        return;

    GAME_IMP(Ngl_ListSubmit)(ctx, base);
}
#endif

// Main heap free (r3 heap, r4 payload). Users crashed in the coalescing (sub_828A2360 -> sub_828A1EC8, write to
// address 0) while the frontend loads: a neighbour block marked free had null list links, i.e. the heap was already
// corrupt. The cause is not found (docs/engines/lux/boot_saves.md). A free that would merge with a corrupt neighbour,
// or a double free, is logged and skipped: the block leaks instead of the game crashing. The checks are done under the
// heap's own (recursive) critical section, so they see the same state as the free.
#ifdef GAME_HOOK_Heap_Free
PPC_EXTERN_FUNC(__imp__RtlEnterCriticalSection);
PPC_EXTERN_FUNC(__imp__RtlLeaveCriticalSection);
PPC_FUNC_IMPL(GAME_IMP(Heap_Free));
PPC_FUNC(GAME_HOOK(Heap_Free))
{
    static std::atomic<uint32_t> reports;
    uint32_t heap = ctx.r3.u32;
    uint32_t payload = ctx.r4.u32;
    uint32_t lr = uint32_t(ctx.lr);

    ctx.r3.u64 = heap + HeapLock;
    __imp__RtlEnterCriticalSection(ctx, base);
    const char* problem = HeapFreeProblem(heap, payload - 8);
    if (problem == nullptr)
    {
        ctx.r3.u64 = heap;
        ctx.r4.u64 = payload;
        GAME_IMP(Heap_Free)(ctx, base);
    }
    ctx.r3.u64 = heap + HeapLock;
    __imp__RtlLeaveCriticalSection(ctx, base);

    if (problem != nullptr && reports++ < 32)
        fprintf(stderr, "[heap] free of 0x%08X skipped (%s), lr 0x%08X\n", payload, problem, lr);
}
#endif
