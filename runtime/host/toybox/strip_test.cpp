// The section strip's wrap (host/toybox/strip.h): no font, window or game needed.
#include "host/toybox/strip.h"
#include <cstdio>
#include <iterator>

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

    bool AllInside(const toybox::strip::Layout& layout, std::span<const float> widths, float width)
    {
        for (size_t i = 0; i < widths.size(); i++)
        {
            if (layout.chips[i].x < 0 || layout.chips[i].x + widths[i] > width)
                return false;
        }
        return true;
    }
}

int main()
{
    using namespace toybox::strip;
    const float gap = 6, rowHeight = 26;

    // Imaginators reads every earlier game's toys: about fourteen sections, far wider than Add a figure's 792 px.
    const float many[] = { 104, 84, 74, 141, 164, 119, 72, 129, 89, 148, 109, 141, 89, 151 };
    Layout layout = Wrap(many, 792, gap, rowHeight);
    CHECK(layout.chips.size() == std::size(many));
    CHECK(AllInside(layout, many, 792));
    CHECK(layout.rows >= 2);
    CHECK(layout.height == layout.rows * rowHeight + (layout.rows - 1) * gap);
    CHECK(layout.chips.back().y == (layout.rows - 1) * (rowHeight + gap));

    // Rows break like words: a chip moves down only when it does not fit after the previous one.
    for (size_t i = 1; i < std::size(many); i++)
    {
        if (layout.chips[i].y > layout.chips[i - 1].y)
        {
            CHECK(layout.chips[i].x == 0);
            CHECK(layout.chips[i - 1].x + many[i - 1] + gap + many[i] > 792);
        }
        else
            CHECK(layout.chips[i].x == layout.chips[i - 1].x + many[i - 1] + gap);
    }

    // The list behind the wheel is wider, still not wide enough.
    CHECK(AllInside(Wrap(many, 1004, gap, rowHeight), many, 1004));
    CHECK(Wrap(many, 1004, gap, rowHeight).rows >= 2);

    // A few sections stay on one row.
    const float few[] = { 104, 84, 141 };
    Layout one = Wrap(few, 792, gap, rowHeight);
    CHECK(one.rows == 1);
    CHECK(one.height == rowHeight);
    CHECK(one.chips[2].x == 104 + gap + 84 + gap);

    // A chip wider than the strip gets a row of its own; no section is no strip.
    const float wide[] = { 100, 900, 100 };
    Layout alone = Wrap(wide, 792, gap, rowHeight);
    CHECK(alone.rows == 3);
    CHECK(alone.chips[1].x == 0 && alone.chips[2].x == 0);
    CHECK(Wrap({}, 792, gap, rowHeight).height == 0);

    printf("%d checks, %d failures\n", g_checks, g_failures);
    puts(g_failures ? "FAIL: section strip wrap" : "PASS: section strip wrap: every chip inside, rows, height");
    return g_failures ? 1 : 0;
}
