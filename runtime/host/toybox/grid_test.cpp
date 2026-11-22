// The Toy Box grids' scroll (host/toybox/grid.h): no files, device, window or game needed.
#include "host/toybox/grid.h"
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

    // Four titled sections of two rows of four cards; rows 100 px, headers 40 px, 300 px in view.
    toybox::Grid Sections()
    {
        toybox::Grid grid;
        grid.rows = 8;
        grid.visibleHeight = 300;
        for (int s = 0; s < 4; s++)
        {
            toybox::ListSection section{};
            section.kind = collection::Section::Kind::Element;
            section.value = s;
            section.firstRow = s * 2;
            grid.sections.push_back(section);
            for (int c = 0; c < 8; c++)
                grid.cells.push_back({ toybox::Cell::Kind::Figure, c, s, s * 2 + c / 4, c % 4 });
        }
        return grid;
    }
}

int main()
{
    toybox::Grid grid = Sections();
    CHECK(grid.RowY(4) == 520 && grid.HeaderY(2) == 480);

    // Moving down: the least scroll, the cursor's row at the bottom of the view.
    grid.cursor = 20;
    grid.Fix();
    CHECK(grid.scroll == grid.RowY(5) + grid.rowHeight - grid.visibleHeight);
    // Back up to a section's first row: its header comes into view with it.
    grid.cursor = 8;
    grid.Fix();
    CHECK(grid.scroll == grid.HeaderY(1));

    // A section shown from above or from below: the cursor on its first card, its header at the top.
    grid = Sections();
    grid.ShowSection(2);
    CHECK(grid.cursor == 16 && grid.scroll == grid.HeaderY(2));
    grid.ShowSection(1);
    CHECK(grid.cursor == 8 && grid.scroll == grid.HeaderY(1));
    // The last section cannot reach the top: the list stops at its end.
    grid.ShowSection(3);
    CHECK(grid.cursor == 24 && grid.scroll == grid.RowY(7) + grid.rowHeight - grid.visibleHeight);

    // An empty grid: nothing to show.
    toybox::Grid empty;
    empty.cursor = 3;
    empty.scroll = 50;
    empty.Fix();
    CHECK(empty.cursor == 0 && empty.scroll == 0);

    printf("%d checks, %d failures\n", g_checks, g_failures);
    puts(g_failures ? "FAIL: toybox grid" : "PASS: toybox grid scroll, LB / RB and the section badges");
    return g_failures ? 1 : 0;
}
