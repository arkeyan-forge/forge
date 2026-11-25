// The pointer in the Alchemy games' menus (engines/alchemy/menu_pointer.h): the hit test, hovering, a click on the
// focused item and on another one across a grid, a pointer at rest, a carousel, a page with a single item, no page, a
// focus the game never moves. No game data: ./ArkeyanAlchemyMenuTest
#include "menu_pointer.h"
#include <cstdio>

using namespace alchemymenu;

namespace
{
    int g_checks = 0, g_failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    constexpr uint16_t Up = 0x0001, Down = 0x0002, Left = 0x0004, Right = 0x0008, A = 0x1000;

    // Giants' CHOOSE PLAY: Story on the left, Battle Mode over Settings on the right.
    constexpr uint32_t Story = 1, Battle = 2, Settings = 3;
    constexpr Item Choose[] = { { Story, 269, 112, 348, 450 }, { Battle, 647, 135, 346, 304 },
        { Settings, 647, 451, 346, 133 } };
    // The pause menu: five tiles in a row, the middle one focused.
    constexpr Item Pause[] = { { 11, 20, 241, 238, 238 }, { 12, 270, 241, 238, 238 }, { 13, 521, 241, 238, 238 },
        { 14, 771, 241, 238, 238 }, { 15, 1022, 241, 238, 238 } };

    Driver::Pointer At(float x, float y, uint32_t clicks = 0)
    {
        Driver::Pointer pointer;
        pointer.active = pointer.inside = true;
        pointer.x = x;
        pointer.y = y;
        pointer.clicks = clicks;
        return pointer;
    }

    Page ChoosePage(uint32_t focused) { return { 0x100, focused, false, Choose }; }

    void HitTest()
    {
        EXPECT(Hit(Choose, 400, 300) == Story);
        EXPECT(Hit(Choose, 800, 250) == Battle);
        EXPECT(Hit(Choose, 800, 520) == Settings);
        EXPECT(Hit(Choose, 100, 100) == 0);
        EXPECT(Hit(Choose, 640, 600) == 0); // under Story's box, beside Settings'
        EXPECT(Hit(Pause, 640, 360) == 13);
    }

    void Hover()
    {
        Driver driver;
        Page page = ChoosePage(Story);
        EXPECT(driver.Step(At(400, 300), &page, 0) == 0); // the first place is no move
        EXPECT(driver.Step(At(800, 250), &page, 10) == Right);
        EXPECT(driver.Step(At(800, 251), &page, 20) == 0); // waits for the game between presses
        page.focused = Battle;
        EXPECT(driver.Step(At(800, 251), &page, 300) == 0); // there: nothing more, no A
        EXPECT(driver.Step(At(800, 251), &page, 600) == 0);
        EXPECT(driver.Step(At(400, 300), &page, 700) == Left); // and back
        page.focused = Story;
        EXPECT(driver.Step(At(400, 300), &page, 1000) == 0);

        // At rest the pointer never takes the focus back from the keys.
        page.focused = Settings;
        EXPECT(driver.Step(At(400, 300), &page, 1300) == 0);
        EXPECT(driver.Step(At(400, 300), &page, 1600) == 0);
    }

    void ClickAcrossTheGrid()
    {
        Driver driver;
        Page page = ChoosePage(Story);
        EXPECT(driver.Step(At(800, 520), &page, 0) == 0);
        // Story to Settings: right first (the larger distance), which lands on Battle Mode, then down, then A.
        EXPECT(driver.Step(At(800, 520, 1), &page, 10) == Right);
        page.focused = Battle;
        EXPECT(driver.Step(At(800, 520), &page, 100) == 0);
        EXPECT(driver.Step(At(800, 520), &page, 300) == Down);
        page.focused = Settings;
        EXPECT(driver.Step(At(800, 520), &page, 400) == 0);
        EXPECT(driver.Step(At(800, 520), &page, 600) == A);
        EXPECT(driver.Step(At(800, 520), &page, 900) == 0);
    }

    void ClickOnTheFocusedItem()
    {
        Driver driver;
        Page page = ChoosePage(Story);
        EXPECT(driver.Step(At(400, 300, 1), &page, 0) == A);
        EXPECT(driver.Step(At(400, 300), &page, 300) == 0);
        // Off the items, a click does nothing.
        EXPECT(driver.Step(At(100, 100, 1), &page, 600) == 0);
    }

    void Carousel()
    {
        Driver driver;
        Page page = { 0x200, 13, true, Pause };
        EXPECT(driver.Step(At(640, 360), &page, 0) == 0);
        EXPECT(driver.Step(At(890, 360), &page, 10) == 0); // hovering does not scroll it
        EXPECT(driver.Step(At(890, 360, 1), &page, 20) == Right);
        // The carousel scrolled: the picked item is in the middle now, another one under the pointer.
        page.focused = 14;
        EXPECT(driver.Step(At(890, 360), &page, 300) == A);
    }

    void NoticeAndNoPage()
    {
        Driver driver;
        const Item one[] = { { 7, 500, 300, 100, 50 } };
        Page notice = { 0x300, 7, false, one };
        EXPECT(driver.Step(At(100, 100, 1), &notice, 0) == A);
        Page none = { 0x301, 0, false, {} };
        EXPECT(driver.Step(At(100, 100, 1), &none, 300) == A);
        EXPECT(driver.Step(At(100, 100, 1), nullptr, 600) == A);
        EXPECT(driver.Step(At(100, 100), nullptr, 900) == 0);
        // Off the canvas (the black bars) nothing is pressed.
        Driver::Pointer bars = At(100, 100, 1);
        bars.inside = false;
        EXPECT(driver.Step(bars, nullptr, 1200) == 0);
    }

    void StuckFocus()
    {
        Driver driver;
        Page page = ChoosePage(Story);
        EXPECT(driver.Step(At(800, 250), &page, 0) == 0);
        EXPECT(driver.Step(At(800, 251, 1), &page, 10) == Right);
        // The game did not move it: the other axis once, then it gives up (no A on the wrong item).
        EXPECT(driver.Step(At(800, 251), &page, 300) == Up);
        EXPECT(driver.Step(At(800, 251), &page, 600) == 0);
        EXPECT(driver.Step(At(800, 251), &page, 900) == 0);
    }

    void UnknownFocusAndNewPage()
    {
        Driver driver;
        Page page = ChoosePage(0);
        EXPECT(driver.Step(At(400, 300), &page, 0) == 0);
        EXPECT(driver.Step(At(800, 250, 1), &page, 10) == 0); // no focus known: no guess
        // A click waiting for the focus is dropped when the page changes.
        page.focused = Story;
        EXPECT(driver.Step(At(800, 250, 1), &page, 300) == Right);
        Page next = { 0x400, 21, false, Choose };
        EXPECT(driver.Step(At(800, 250), &next, 600) == 0);
        EXPECT(driver.Step(At(800, 250), &next, 900) == 0);
        // Leaving the menu drops it too.
        Driver::Pointer off;
        EXPECT(driver.Step(off, &next, 1200) == 0);
    }
}

int main()
{
    HitTest();
    Hover();
    ClickAcrossTheGrid();
    ClickOnTheFocusedItem();
    Carousel();
    NoticeAndNoPage();
    StuckFocus();
    UnknownFocusAndNewPage();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
