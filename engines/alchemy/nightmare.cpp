// "Nightmare from the start" [Alchemy] [SwapForce] [Imaginators] [TrapTeam] (games/swapforce/docs/history.md, "Nightmare
// difficulty"; games/imaginators/docs/status.md and games/trapteam/docs/status.md, "Nightmare").
//
// The difficulty menu offers Easy, Medium and Hard always and Nightmare once the story is beaten. Two kinds of lock:
// - Swap Force, Imaginators: the slot's property "nightmare_difficulty_unlocked", set at the story's end (a script function:
//   Swap Force's tryUnlockNightmareDifficultyLevel_Internal, Imaginators' unlockNightmareDifficultyLevel).
//   GAME_HOOK_NightmareUnlocked reads that property and answers whether it is 1: it is the check's only source, so
//   answering 1 while the option is on opens the row and leaves the save as it is (nothing writes the property: the
//   answer is not stored anywhere), and with the option off the game's own answer comes back.
// - Trap Team: no native check; the menu's script copies the save header's system flag 3 into its variable "allow
//   nightmare" (nightmare_gate.h). GAME_HOOK_ScriptVariable_Get resolves every script variable; when it is that one, the
//   option gives it 1 just before the read. The flag itself is never written.
// A game whose addresses.h names neither has no locked Nightmare.
#include "game_hooks.h"
#include "nightmare_gate.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    std::atomic<bool> g_fromStart{ false };
    // The value the option last gave 1 (ScriptVariable_Get): read again, it is still the option's answer, not the save's.
    [[maybe_unused]] std::atomic<uint32_t> g_offeredValue{ 0 };

    // Once for each of the three cases: locked, unlocked by the save, unlocked by the option.
    [[maybe_unused]] void LogAnswer(bool saved, bool offered)
    {
        static std::atomic<int> logged{ 0 };
        const int kind = saved ? 1 : offered ? 2 : 0;
        if ((logged.fetch_or(1 << kind) & (1 << kind)) == 0)
            fprintf(stderr, "[nightmare] the menu asked: the save says %s, the option is %s: %s\n",
                saved ? "unlocked" : "locked", g_fromStart ? "on" : "off", offered ? "offered" : "locked");
    }
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
    LogAnswer(saved, offered);
    ctx.r3.u64 = offered ? 1 : 0;
}
#endif

#ifdef GAME_HOOK_ScriptVariable_Get
// r3: where the reference to the variable's record is; the original puts the variable's value there and calls the
// value's getter. Every script variable read passes here: the name's first character turns the others away at once.
PPC_FUNC_IMPL(GAME_IMP(ScriptVariable_Get));
PPC_FUNC(GAME_HOOK(ScriptVariable_Get))
{
    using namespace alchemy;
    const uint32_t record = PPC_LOAD_U32(ctx.r3.u32);
    const uint32_t name = record != 0 ? PPC_LOAD_U32(record + ScriptVariableName) : 0;
    if (name != 0 && PPC_LOAD_U8(name - 1) == NightmareGateVariable[0])
    {
        const char* text = reinterpret_cast<const char*>(base + name - 1);
        const int32_t slot = int32_t(PPC_LOAD_U32(record + ScriptVariableSlot));
        const uint32_t script = PPC_LOAD_U32(game::ScriptContext);
        if (slot >= 0 && script != 0 && IsNightmareGate(std::string_view(text, strnlen(text, 32))))
        {
            const uint32_t values = PPC_LOAD_U32(script + ScriptContextValues);
            const uint32_t value = values != 0 ? PPC_LOAD_U32(values + 4 * uint32_t(slot)) : 0;
            if (value != 0)
            {
                const uint32_t saved = PPC_LOAD_U32(value + ScriptValueWord);
                const NightmareGate gate = ReadNightmareGate(g_offeredValue == value ? 0 : saved, g_fromStart);
                if (gate.offer)
                {
                    PPC_STORE_U32(value + ScriptValueWord, 1);
                    g_offeredValue = value;
                }
                else if (gate.answer != NightmareGate::Offered)
                    g_offeredValue = 0;
                LogAnswer(gate.answer == NightmareGate::UnlockedBySave, gate.answer != NightmareGate::Locked);
            }
        }
    }
    GAME_IMP(ScriptVariable_Get)(ctx, base);
}
#endif
