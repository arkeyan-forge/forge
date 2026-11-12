// The player's offline owner id, the same from one session to the next (SuperChargers; docs/runtime/host/figures.md
// "Owner"). A toy the game registers gets the player's owner id; the in-game collection lists
// only toys owned by the player, and the toy's page offers "Ownership" for the others.
//
// The game fetches the id once, at "press start" (game::Owner_Fetch, from sub_82EF6D00), before its options save is
// read: the lookup misses, a new random id is made and the save's own id is never read back, so every session had
// another id and the toys registered before were someone else's. Imaginators fetches it from its save's loading
// instead. Here the fetch runs again once the options save is loaded (its "most recent save slot" lookup,
// game::Options_MostRecentSlot, which the start flow makes right after the load): the store then holds the save's id.
#include "game.h"
#include "game_hooks.h"
#include "memory.h"
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#if defined(GAME_HOOK_Owner_Fetch) && defined(GAME_HOOK_Options_MostRecentSlot)
PPC_FUNC_IMPL(GAME_IMP(Owner_Fetch));
// (owner object) -> 0 when the id came from the store, 1 when it was made (and stored); the id at game::OwnerIdOffset.
PPC_FUNC(GAME_HOOK(Owner_Fetch))
{
    const uint32_t owner = ctx.r3.u32;
    GAME_IMP(Owner_Fetch)(ctx, base);
    const uint32_t id = g_memory.Read<uint32_t>(owner + game::OwnerIdOffset);
    static uint32_t logged = 0;
    if (id != logged)
    {
        logged = id;
        fprintf(stderr, "[owner] the player's owner id: %08X (%s)\n", id, ctx.r3.u32 == 0 ? "the save's" : "new");
    }
}

PPC_FUNC_IMPL(GAME_IMP(Options_MostRecentSlot));
// (save manager, int* slot) -> 0 when the options save names one.
PPC_FUNC(GAME_HOOK(Options_MostRecentSlot))
{
    GAME_IMP(Options_MostRecentSlot)(ctx, base);
    const uint32_t owner = g_memory.Read<uint32_t>(game::OwnerObject);
    if (owner == 0)
        return;
    const uint64_t result = ctx.r3.u64;
    ctx.r3.u64 = owner;
    GAME_HOOK(Owner_Fetch)(ctx, base);
    ctx.r3.u64 = result;
}
#endif
