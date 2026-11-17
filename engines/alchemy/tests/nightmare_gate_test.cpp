// Checks the difficulty menu's Nightmare answer for the games that lock it in a script variable (engines/alchemy/
// nightmare_gate.h): the variable named exactly, what the save says, what the option adds, the save left alone.
// No game data. usage: ArkeyanNightmareGateTest
#include "nightmare_gate.h"
#include <cstdio>

namespace
{
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)
}

int main()
{
    using alchemy::NightmareGate;
    using alchemy::ReadNightmareGate;

    // Only the menu's own variable: the title's and the parameters' names differ.
    CHECK(alchemy::IsNightmareGate("allow nightmare"));
    CHECK(!alchemy::IsNightmareGate("allow nightmare difficulty"));
    CHECK(!alchemy::IsNightmareGate("disallow nightmare setting"));
    CHECK(!alchemy::IsNightmareGate("do not allow difficulty change if in nightmare"));
    CHECK(!alchemy::IsNightmareGate("Allow Nightmare"));
    CHECK(!alchemy::IsNightmareGate("allow nightmar"));
    CHECK(!alchemy::IsNightmareGate(""));

    // A new save: locked, offered once the option is on (the value the menu reads becomes 1).
    NightmareGate locked = ReadNightmareGate(0, false);
    CHECK(locked.answer == NightmareGate::Locked && !locked.offer);
    NightmareGate offered = ReadNightmareGate(0, true);
    CHECK(offered.answer == NightmareGate::Offered && offered.offer);

    // The story beaten: the save's own 1, nothing to change, option or not.
    for (bool fromStart : { false, true })
    {
        NightmareGate saved = ReadNightmareGate(1, fromStart);
        CHECK(saved.answer == NightmareGate::UnlockedBySave && !saved.offer);
    }
    // Any other value the script left there is the save's own answer too.
    CHECK(ReadNightmareGate(7, true).answer == NightmareGate::UnlockedBySave);

    if (g_failures == 0)
        printf("PASS: ArkeyanNightmareGateTest\n");
    return g_failures == 0 ? 0 : 1;
}
