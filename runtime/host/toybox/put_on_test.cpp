// Which refusal a seat gives a card (host/toybox/put_on.h): no files, device, window or game needed.
#include "host/toybox/put_on.h"
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
}

int main()
{
    using namespace toybox::puton;
    const Half fits{};
    const Half one[] = { fits };
    CHECK(Check(false, true, false, one) == Refusal::None);

    // A magic item on Player 1: the game reads it, that seat does not take it.
    const Half item[] = { { false, true, false } };
    CHECK(Check(false, true, false, item) == Refusal::WrongSeat);

    // A toy the game reads nowhere is "cannot read", whatever the seat says.
    const Half foreign[] = { { false, false, false } };
    CHECK(Check(false, true, false, foreign) == Refusal::CannotRead);
    const Half foreignFits[] = { { false, false, true } };
    CHECK(Check(false, true, false, foreignFits) == Refusal::CannotRead);

    // A swapper: either half refuses it, the first refusal found wins.
    const Half swapperBottomBusy[] = { fits, { true, true, true } };
    CHECK(Check(false, true, false, swapperBottomBusy) == Refusal::Busy);
    const Half swapperBottomWrong[] = { fits, { false, true, false } };
    CHECK(Check(false, true, false, swapperBottomWrong) == Refusal::WrongSeat);
    const Half busyAndWrong[] = { { true, true, false } };
    CHECK(Check(false, true, false, busyAndWrong) == Refusal::Busy);

    // The portal and the seat come before the card.
    CHECK(Check(true, false, true, item) == Refusal::RealPortal);
    CHECK(Check(false, false, true, item) == Refusal::Unknown);
    CHECK(Check(false, true, true, item) == Refusal::RealFigure);

    printf("%d checks, %d failures\n", g_checks, g_failures);
    puts(
        g_failures ? "FAIL: put on refusals" : "PASS: put on refusals: wrong seat, cannot read, swapper halves, order");
    return g_failures ? 1 : 0;
}
