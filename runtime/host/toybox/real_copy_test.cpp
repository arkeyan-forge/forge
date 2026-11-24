// Checks the Toy Box's side of the copies of real toys (host/toybox/real_copy.h): the side the question focuses, one
// question at a time, a closed question back when the Toy Box opens again, one notice per failed save. usage:
// ArkeyanToyboxRealCopyTest
#include "host/toybox/real_copy.h"
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

    figures::FigureStats Placed(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute)
    {
        figures::FigureStats stats;
        stats.lastPlaced = { minute, hour, day, month, year };
        return stats;
    }

    void FocusChecks()
    {
        using namespace toybox::realcopy;
        constexpr auto Valid = figures::AreaState::Valid;
        const uint64_t morning = PlacedStamp(Valid, Placed(2026, 11, 3, 9, 59));
        const uint64_t evening = PlacedStamp(Valid, Placed(2026, 11, 3, 18, 0));
        const uint64_t lastYear = PlacedStamp(Valid, Placed(2025, 12, 31, 23, 59));
        CHECK(morning != 0 && morning < evening && lastYear < morning);
        CHECK(PlacedStamp(Valid, figures::FigureStats{}) == 0);
        CHECK(PlacedStamp(figures::AreaState::Damaged, Placed(2026, 11, 3, 9, 59)) == 0);
        CHECK(PlacedStamp(figures::AreaState::Blank, Placed(2026, 11, 3, 9, 59)) == 0);
        // The newest side; the toy when a date is missing.
        CHECK(ToyFirst(evening, morning) && !ToyFirst(morning, evening) && ToyFirst(morning, morning));
        CHECK(ToyFirst(0, evening) && ToyFirst(evening, 0) && ToyFirst(0, 0));
    }

    void QuestionChecks()
    {
        toybox::realcopy::Questions questions;
        CHECK(questions.Next({}, false).empty());
        // The first waiting toy, then nothing while its question is up.
        CHECK(questions.Next({ "a", "b" }, false) == "a");
        CHECK(questions.Next({ "a", "b" }, true).empty());
        // Answered: the next one.
        questions.Answered();
        CHECK(questions.Next({ "b" }, false) == "b");
        // Its question gone without an answer (the Toy Box closed): asked again.
        CHECK(questions.Next({ "b" }, false) == "b");
        // Chosen elsewhere: no longer waiting, its question is not kept.
        CHECK(questions.Next({ "c" }, true) == "c");
        // Closed with B: not asked again until the Toy Box opens again.
        questions.Closed("c");
        CHECK(questions.Next({ "c" }, false).empty());
        CHECK(questions.Next({ "c", "d" }, false) == "d");
        questions.Answered();
        questions.Reopened();
        CHECK(questions.Next({ "c" }, false) == "c");
    }

    void DifferenceChecks()
    {
        using namespace toybox::realcopy;
        figures::FigureStats toy, copy;
        toy.gold = 500;
        copy.gold = 900;
        copy.experience = 5000;
        CHECK(Differences(toy, copy) == 0); // level and gold are always shown
        copy.upgrades = 0x0007; // a path chosen (bits 0-1) and one basic upgrade
        CHECK(Differences(toy, copy) == Upgrades && UpgradeCount(copy) == 1 && UpgradeCount(toy) == 0);
        copy.upgrades = 0x03FC;
        copy.wowPow = 1;
        CHECK(UpgradeCount(copy) == 9);
        copy = toy;
        copy.heroPoints = 40;
        copy.heroicChallenges = 3;
        copy.hat = 7;
        copy.nickname = u"Spy";
        copy.playSeconds = 600;
        CHECK(Differences(toy, copy) == (HeroPoints | Challenges | Hat | Nickname | PlayTime));
        CHECK(Differences(copy, copy) == 0);
    }

    void FailureChecks()
    {
        toybox::realcopy::Failures failures;
        CHECK(!failures.New(0));
        CHECK(failures.New(1) && !failures.New(1));
        CHECK(failures.New(4) && !failures.New(4));
    }
}

int main()
{
    FocusChecks();
    QuestionChecks();
    DifferenceChecks();
    FailureChecks();
    if (g_failures == 0)
        printf("real copy: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
