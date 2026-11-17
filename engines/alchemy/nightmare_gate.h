#pragma once

#include <cstdint>
#include <string_view>

// "Nightmare from the start" where the lock is a script variable [Alchemy] [TrapTeam] (games/trapteam/docs/status.md,
// "Nightmare"). The difficulty menu's macros copy the save header's system flag 3 (set when the story is beaten) into
// their own variable "allow nightmare" and offer the fourth button when it is 1. The script VM resolves a variable
// through one function (GAME_HOOK_ScriptVariable_Get): giving that variable 1 just before it is read offers Nightmare
// without writing the flag, so the save keeps what the game wrote.
namespace alchemy
{
    // The VM's variable record: +0x1C a pointer one character into the variable's name, +0x28 its slot among the running
    // script's values (negative: none). The running script (game::ScriptContext) keeps its values' list at +20; a value
    // holds its integer (a bool: 0 or 1) at +0x20. [TrapTeam] [V] (the menu's runs, 2026-11-17).
    constexpr uint32_t ScriptVariableName = 0x1C, ScriptVariableSlot = 0x28;
    constexpr uint32_t ScriptContextValues = 20, ScriptValueWord = 0x20;

    // The menu's variable, named exactly: the title's "allow nightmare difficulty" and the menu's parameter
    // "disallow nightmare setting" are other variables.
    constexpr std::string_view NightmareGateVariable = "allow nightmare";

    inline bool IsNightmareGate(std::string_view name) { return name == NightmareGateVariable; }

    // What the menu is about to read: `saved` is the value the script copied from the save; `offer` asks for 1 in its
    // place. The save's own 1 (the story beaten) is never touched.
    struct NightmareGate
    {
        enum Answer
        {
            Locked,
            UnlockedBySave,
            Offered,
        } answer;
        bool offer;
    };

    inline NightmareGate ReadNightmareGate(uint32_t saved, bool fromStart)
    {
        if (saved != 0)
            return { NightmareGate::UnlockedBySave, false };
        if (fromStart)
            return { NightmareGate::Offered, true };
        return { NightmareGate::Locked, false };
    }
}
