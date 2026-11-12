// "Nightmare from the start" [Alchemy] [SwapForce] [Imaginators] (games/swapforce/docs/history.md, "Nightmare difficulty";
// games/imaginators/docs/status.md, "Nightmare").
//
// The difficulty menu offers Easy, Medium and Hard always and Nightmare when the slot's property
// "nightmare_difficulty_unlocked" is 1; the story's end sets it (a script function: Swap Force's
// tryUnlockNightmareDifficultyLevel_Internal, Imaginators' unlockNightmareDifficultyLevel). GAME_HOOK_NightmareUnlocked
// reads that property and answers whether it is 1: it is the check's only source, so answering 1 while the option is on
// opens the row and leaves the save as it is (nothing writes the property: the answer is not stored anywhere), and with the
// option off the game's own answer comes back. A game whose addresses.h does not name it has no locked Nightmare.
#include "game_hooks.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    std::atomic<bool> g_fromStart{ false };
}

// [<game>] nightmare_from_start: the adapter's SetNightmareFromStart (declared in the game's own header).
void AlchemySetNightmareFromStart(bool on);

void AlchemySetNightmareFromStart(bool on)
{
    g_fromStart = on;
    if (on)
        fprintf(stderr, "[nightmare] offered from the start (the save is not changed)\n");
}

#ifdef GAME_HOOK_NightmareUnlocked
PPC_FUNC_IMPL(GAME_IMP(NightmareUnlocked));
PPC_FUNC(GAME_HOOK(NightmareUnlocked))
{
    GAME_IMP(NightmareUnlocked)(ctx, base);
    const bool saved = (ctx.r3.u32 & 0xFF) != 0, offered = g_fromStart || saved;
    static int logged = 0; // once for each of the three cases: locked, unlocked by the save, unlocked by the option
    const int kind = saved ? 1 : offered ? 2 : 0;
    if ((logged & (1 << kind)) == 0)
    {
        logged |= 1 << kind;
        fprintf(stderr, "[nightmare] the menu asked: the save says %s, the option is %s: %s\n",
            saved ? "unlocked" : "locked", g_fromStart ? "on" : "off", offered ? "offered" : "locked");
    }
    ctx.r3.u64 = offered ? 1 : 0;
}
#endif
