// Imaginators' chest kinds: the ids the game gave each kind in a run (games/imaginators/docs/research.md, "The chest
// pile") decode to that kind, and the loot roll gets what the game's purchased-chest path gives it.
#include "chest_kinds.h"
#include <cstdio>

namespace
{
    int g_checks, g_failures;

    void Check(bool condition, const char* what, int line)
    {
        g_checks++;
        if (!condition)
        {
            g_failures++;
            fprintf(stderr, "FAIL line %d: %s\n", line, what);
        }
    }
#define CHECK(condition) Check((condition), #condition, __LINE__)

    int Kind(std::string_view name)
    {
        for (size_t i = 0; i < imaginators::ChestKinds.size(); i++)
            if (imaginators::ChestKinds[i].name == name)
                return int(i);
        return -2;
    }
}

int main()
{
    using namespace imaginators;
    // The ids seen in the pile, one per kind.
    CHECK(ChestKindOf(0x80EC000000000000ull) == Kind("imaginite"));
    CHECK(ChestKindOf(0x80ED500400000000ull) == Kind("copper"));
    CHECK(ChestKindOf(0x80ED500700000000ull) == Kind("bronze"));
    CHECK(ChestKindOf(0x80ED500800000000ull) == Kind("silver"));
    CHECK(ChestKindOf(0x80ED500900000000ull) == Kind("gold"));
    CHECK(ChestKindOf(0x80ED500600000000ull) == Kind("platinum"));
    CHECK(ChestKindOf(0x80EB500100000000ull) == Kind("toy_bronze"));
    CHECK(ChestKindOf(0x80EB500200000000ull) == Kind("toy_silver"));
    CHECK(ChestKindOf(0x80EB500300000000ull) == Kind("toy_gold"));
    // A serial in the low bits, another in-game variant: the same kinds. Another toy, an unknown tier: none.
    CHECK(ChestKindOf(0x80EB50030000ABCDull) == Kind("toy_gold"));
    CHECK(ChestKindOf(0x80EC500000000001ull) == Kind("imaginite"));
    CHECK(ChestKindOf(0x80ED500500000000ull) == -1);
    CHECK(ChestKindOf(0x8001000000000000ull) == -1);

    // The purchased chest's path: variant 0x5007 -> (237, 7, 5, 0, 0, 0).
    const LootRoll bronze = LootRollOf(ChestKinds[size_t(Kind("bronze"))]);
    CHECK(bronze.type == 237 && bronze.tier == 7 && bronze.group == 5 && bronze.bit9 == 0 && bronze.bit10 == 0 &&
          bronze.bit11 == 0);
    const LootRoll toy = LootRollOf(ChestKinds[size_t(Kind("toy_silver"))]);
    CHECK(toy.type == 235 && toy.tier == 2 && toy.group == 5);
    CHECK(ChestKindNames[0] == "imaginite" && ChestKindNames.size() == 9);

    printf("IMGChestKindsTest: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
