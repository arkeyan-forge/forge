// The pointer in Spyro's Adventure's menus (games/ssa/menu_pointer.h): the hit test, hovering, a click on the focused
// item and on another one, a pointer at rest, a click off the items, a page with a single item, a notice. No game data:
// ./ArkeyanSsaMenuTest
#include "menu_pointer.h"
#include <cstdio>

using namespace ssamenu;

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

    // The CHOOSE PLAY menu: Skyland Adventures over Battle Mode and Options.
    constexpr uint32_t Skyland = 0x998EAD37, Battle = 0x6DBACD8F, Options = 0x7A7A74FC, Choose = 0x43385FC0;
    constexpr Item Items[] = { { Skyland, 0.0f, 58.3f, 234.1f, 72.0f }, { Battle, -95.2f, -78.7f, 130.2f, 64.8f },
        { Options, 137.5f, -78.7f, 97.9f, 64.6f } };

    Driver::Pointer At(float x, float y, uint32_t clicks = 0)
    {
        Driver::Pointer pointer;
        pointer.active = pointer.inside = true;
        pointer.x = x;
        pointer.y = y;
        pointer.clicks = clicks;
        return pointer;
    }

    Page Menu(uint32_t focused, bool moving = false)
    {
        Page page;
        page.handle = Choose;
        page.focused = focused;
        page.moving = moving;
        page.items = Items;
        return page;
    }

    Action Step(Driver& driver, const Driver::Pointer& pointer, const Page& page)
    {
        return driver.Step(pointer, std::span<const Page>(&page, 1));
    }

    // Canvas pixels of a point in units.
    float CanvasX(float x) { return 640.0f + x * CanvasPerUnit; }
    float CanvasY(float y) { return 360.0f - y * CanvasPerUnit; }
}

int main()
{
    // Units: the canvas centre is 0/0, y up, 1.5 pixels a unit.
    float x = 1, y = 1;
    ToUnits(640, 360, x, y);
    EXPECT(x == 0 && y == 0);
    ToUnits(790, 210, x, y);
    EXPECT(x == 100 && y == 100);

    // The hit test: inside a box, between boxes, the nearest centre of two that overlap.
    EXPECT(Hit(Items, 0, 58) == Skyland);
    EXPECT(Hit(Items, -95, -78) == Battle);
    EXPECT(Hit(Items, 200, -100) == Options);
    EXPECT(Hit(Items, 300, 200) == 0);
    constexpr Item Overlap[] = { { 1, -70, 0, 50, 50 }, { 2, 30, 0, 60, 50 } };
    EXPECT(Hit(Overlap, -25, 0) == 1); // in both: 45/50 from the first's centre, 55/60 from the second's
    EXPECT(Hit(Overlap, -28, 0) == 1);
    EXPECT(Hit(Overlap, -15, 0) == 2);
    EXPECT(Hit(Overlap, 80, 0) == 2);

    // A pointer at rest when the menu opens takes nothing; moved onto another item, it gives it the focus.
    {
        Driver driver;
        Action action = Step(driver, At(CanvasX(-95), CanvasY(-78)), Menu(Skyland));
        EXPECT(action.focus == 0 && !action.accept);
        action = Step(driver, At(CanvasX(-95), CanvasY(-78)), Menu(Skyland));
        EXPECT(action.focus == 0 && !action.accept); // still at rest: the keys keep the focus
        action = Step(driver, At(CanvasX(-90), CanvasY(-78)), Menu(Skyland));
        EXPECT(action.page == Choose && action.focus == Battle && !action.accept);
        // Over the focused item: nothing to move.
        action = Step(driver, At(CanvasX(-80), CanvasY(-78)), Menu(Battle));
        EXPECT(action.focus == 0 && !action.accept);
        // Off every item: the focus stays.
        action = Step(driver, At(CanvasX(380), CanvasY(220)), Menu(Battle));
        EXPECT(action.focus == 0 && !action.accept);
    }

    // A click on the focused item is Accept at once.
    {
        Driver driver;
        Step(driver, At(CanvasX(0), CanvasY(58)), Menu(Skyland));
        Action action = Step(driver, At(CanvasX(0), CanvasY(58), 1), Menu(Skyland));
        EXPECT(action.page == Choose && action.accept && action.focus == 0);
    }

    // A click on another item: the focus first, Accept once the game has moved it.
    {
        Driver driver;
        Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Skyland));
        Action action = Step(driver, At(CanvasX(140), CanvasY(-80), 1), Menu(Skyland));
        EXPECT(action.focus == Options && !action.accept);
        action = Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Skyland, true));
        EXPECT(action.focus == 0 && !action.accept); // the move is not made yet
        action = Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Options));
        EXPECT(action.accept && action.focus == 0);
        action = Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Options));
        EXPECT(!action.accept); // once
    }

    // While the game moves the focus, a page may hold none for a frame: the click still waits for it.
    {
        Driver driver;
        Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Skyland));
        Action action = Step(driver, At(CanvasX(140), CanvasY(-80), 1), Menu(Skyland));
        EXPECT(action.focus == Options);
        action = driver.Step(At(CanvasX(140), CanvasY(-80)), {});
        EXPECT(action.focus == 0 && !action.accept);
        action = Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Options));
        EXPECT(action.accept);
    }

    // The game refuses the move (a greyed item): the click is dropped after a while, never pressed on another item.
    {
        Driver driver;
        Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Skyland));
        Step(driver, At(CanvasX(140), CanvasY(-80), 1), Menu(Skyland));
        bool accepted = false;
        for (int frame = 0; frame < 60; frame++)
            accepted |= Step(driver, At(CanvasX(140), CanvasY(-80)), Menu(Skyland)).accept;
        EXPECT(!accepted);
    }

    // A click off the items, on the black bars, or while the pointer is not the menu's does nothing.
    {
        Driver driver;
        Step(driver, At(CanvasX(380), CanvasY(220)), Menu(Skyland));
        Action action = Step(driver, At(CanvasX(380), CanvasY(220), 1), Menu(Skyland));
        EXPECT(action.focus == 0 && !action.accept);
        Driver::Pointer bars = At(CanvasX(0), CanvasY(58), 1);
        bars.inside = false;
        action = Step(driver, bars, Menu(Skyland));
        EXPECT(action.focus == 0 && !action.accept);
        Driver::Pointer off = At(CanvasX(0), CanvasY(58), 1);
        off.active = false;
        action = Step(driver, off, Menu(Skyland));
        EXPECT(action.focus == 0 && !action.accept);
    }

    // A page with a single item (the title): a click anywhere on the picture is Accept.
    {
        constexpr Item Start[] = { { 0xF77EB7C6, -12.1f, -110.4f, 22.4f, 22.4f } };
        Page title;
        title.handle = 0x425A12E0;
        title.focused = 0xF77EB7C6;
        title.items = Start;
        Driver driver;
        Step(driver, At(CanvasX(200), CanvasY(150)), title);
        Action action = Step(driver, At(CanvasX(200), CanvasY(150), 1), title);
        EXPECT(action.page == title.handle && action.accept && action.focus == 0);
    }

    // A notice (a page without items, its OK its own): a click anywhere is Accept, a move nothing.
    {
        Page notice;
        notice.handle = 0x42580170;
        Driver driver;
        Step(driver, At(CanvasX(10), CanvasY(10)), notice);
        Action action = Step(driver, At(CanvasX(20), CanvasY(10)), notice);
        EXPECT(action.focus == 0 && !action.accept);
        action = Step(driver, At(CanvasX(20), CanvasY(10), 1), notice);
        EXPECT(action.page == notice.handle && action.accept && action.focus == 0);
    }

    // No page holds the focus (play): nothing.
    {
        Driver driver;
        Action action = driver.Step(At(CanvasX(0), CanvasY(58), 1), {});
        EXPECT(action.page == 0 && action.focus == 0 && !action.accept);
    }

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
