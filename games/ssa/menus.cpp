// Spyro's Adventure's own menus (docs/runtime/xbox360/input.md, "Mouse"): whether one is on screen (game::Adapter::
// MenuOpen) and the pointer in it (menu_pointer.h). Read on the game's thread, right after its UI has read the pads
// (menu_hook.cpp), where the menus cannot change under us; the main thread only reads the answer. This file needs no
// translated headers, so the code checks of a build without the game read it too.
//
// The UI (found by hand): a manager holds nine menus; each menu a tree of pages; a page that takes input a tree of items
// and the focused one. Up, down, left and right lead from an item to another by id. Moving the focus is the game's own
// request (an item id the page moves to on its next frame, running the item's own focus animations). Accept is A on
// player 1's controller (host/input.h, PressButtons), the mouse being player 1's: the title's Press START also starts
// the game's session from the pads, which a press in the UI's own copy of the buttons would skip.
#include "menu_pointer.h"
#include "host/input.h"
#include "host/mouse.h"
#include "memory.h"
#include "ssa.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr uint32_t UiManager = 0x82C590B0; // the UI manager (sub_8220CB68 returns it)
    // UiManager: +368 the nine menus.
    constexpr uint32_t Menus = 368, MenuCount = 9;
    // A menu: +6 on, +72 its pages.
    constexpr uint32_t MenuOn = 6, MenuPages = 72;
    // A page: +0 its items, +39 takes input, +124 the id its focus moves to next (with +1328 set), +132 the focused item
    // (the page's own +144 when none), +1256 its own actions on the pads' buttons (a list: empty, it points to itself at
    // +4), +1324 the pads it reads, a bit each (sub_8267FEC0).
    constexpr uint32_t PageItems = 0, PageInput = 39, PageFocusTo = 124, PageFocused = 132, PageNoFocus = 144,
                       PageActions = 1256, PagePads = 1324, PageFocusMove = 1328;
    // An item: +32 its model, +24 its id, +868 the ids up, down, left and right; its place now, a matrix at +768: the
    // scales at +768 and +788, the position at +816, +820 (in the units of menu_pointer.h, the camera looks straight on).
    // A model: its bounds' centre at +32, +36, the half sizes at +48, +52.
    constexpr uint32_t ItemModel = 32, ItemId = 24, ItemNext = 868, ItemScaleX = 768, ItemScaleY = 788, ItemX = 816,
                       ItemY = 820;
    constexpr uint32_t ModelX = 32, ModelY = 36, ModelHalfWidth = 48, ModelHalfHeight = 52;
    constexpr uint32_t NoItem = 0x12345678; // the focused id of a page that has none

    constexpr size_t MaxPages = 8, MaxItems = 48;
    constexpr int MaxDepth = 32;

    bool Valid(uint32_t address) { return address >= 0x10000 && address < 0x70000000 && (address & 3) == 0; }

    // The game's trees: children at +0 and +4, a leaf points to itself at +4; the value at +16.
    template <typename Visit>
    void Values(uint32_t node, const Visit& visit, int depth = 0)
    {
        if (depth > MaxDepth || !Valid(node) || g_memory.Read<uint32_t>(node + 4) == node)
            return;
        Values(g_memory.Read<uint32_t>(node), visit, depth + 1);
        visit(g_memory.Read<uint32_t>(node + 16));
        Values(g_memory.Read<uint32_t>(node + 4), visit, depth + 1);
    }

    struct Pages
    {
        std::array<ssamenu::Page, MaxPages> pages;
        std::array<std::array<ssamenu::Item, MaxItems>, MaxPages> items;
        size_t count = 0;
    };

    // The items the focus can reach from the focused one: those up, down, left and right of it, and so on.
    size_t Reachable(uint32_t page, uint32_t focused, std::array<ssamenu::Item, MaxItems>& out)
    {
        std::array<uint32_t, MaxItems> ids{}, items{};
        size_t all = 0;
        Values(g_memory.Read<uint32_t>(page + PageItems), [&](uint32_t item) {
            if (Valid(item) && all < MaxItems)
            {
                ids[all] = g_memory.Read<uint32_t>(item + ItemId);
                items[all++] = item;
            }
        });
        size_t count = 0;
        auto add = [&](uint32_t id) {
            if (id == 0 || count == MaxItems)
                return;
            for (size_t i = 0; i < count; i++)
                if (out[i].id == id)
                    return;
            for (size_t i = 0; i < all; i++)
                if (ids[i] == id)
                {
                    const uint32_t item = items[i];
                    const uint32_t model = g_memory.Read<uint32_t>(item + ItemModel);
                    if (!Valid(model))
                        return;
                    const float scaleX = std::fabs(g_memory.Read<float>(item + ItemScaleX));
                    const float scaleY = std::fabs(g_memory.Read<float>(item + ItemScaleY));
                    ssamenu::Item& entry = out[count];
                    entry.id = id;
                    entry.halfWidth = g_memory.Read<float>(model + ModelHalfWidth) * scaleX;
                    entry.halfHeight = g_memory.Read<float>(model + ModelHalfHeight) * scaleY;
                    entry.x = g_memory.Read<float>(item + ItemX) + g_memory.Read<float>(model + ModelX) * scaleX;
                    entry.y = g_memory.Read<float>(item + ItemY) + g_memory.Read<float>(model + ModelY) * scaleY;
                    if (std::isfinite(entry.x) && std::isfinite(entry.y) && entry.halfWidth > 0 &&
                        entry.halfHeight > 0 && entry.halfWidth < 1000 && entry.halfHeight < 1000)
                        count++;
                    return;
                }
        };
        add(focused);
        for (size_t at = 0; at < count; at++)
            for (uint32_t i = 0; i < 4; i++)
            {
                const uint32_t id = out[at].id;
                for (size_t j = 0; j < all; j++)
                    if (ids[j] == id)
                        add(g_memory.Read<uint32_t>(items[j] + ItemNext + i * 4));
            }
        return count;
    }

    // The pages that hold the focus now (none in play).
    void Read(Pages& found)
    {
        found.count = 0;
        for (uint32_t slot = 0; slot < MenuCount; slot++)
        {
            const uint32_t menu = g_memory.Read<uint32_t>(UiManager + Menus + slot * 4);
            if (!Valid(menu) || g_memory.Read<uint8_t>(menu + MenuOn) == 0)
                continue;
            Values(g_memory.Read<uint32_t>(menu + MenuPages), [&](uint32_t page) {
                if (found.count == MaxPages || !Valid(page) || g_memory.Read<uint8_t>(page + PageInput) == 0)
                    return;
                const uint32_t pads = g_memory.Read<uint32_t>(page + PagePads);
                if (pads == 0)
                    return;
                // A page that holds the focus is a menu; one without but with its own actions a notice (OK); the
                // others are idle (in play).
                const uint32_t item = g_memory.Read<uint32_t>(page + PageFocused);
                uint32_t focused =
                    Valid(item) && item != page + PageNoFocus ? g_memory.Read<uint32_t>(item + ItemId) : 0;
                if (focused == NoItem)
                    focused = 0;
                const size_t index = found.count;
                const size_t items = focused != 0 ? Reachable(page, focused, found.items[index]) : 0;
                const bool actions = g_memory.Read<uint32_t>(page + PageActions + 4) != page + PageActions;
                if (items == 0 && !actions)
                    return;
                ssamenu::Page& entry = found.pages[index];
                entry.handle = page;
                entry.focused = items != 0 ? focused : 0;
                entry.moving = g_memory.Read<uint32_t>(page + PageFocusTo) != 0;
                entry.items = std::span<const ssamenu::Item>(found.items[index].data(), items);
                found.count++;
            });
        }
    }

    // A page holds no focus for a frame while it moves it, and none for a moment while one screen replaces another;
    // the UI stops altogether while a level loads. A menu is gone once none has been seen for this long.
    constexpr int64_t MenuGone = 1000; // ms
    std::atomic<int64_t> g_menuSeen{ -MenuGone }; // steady clock, ms

    int64_t Now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
}

bool ssa::MenuOpen() { return Now() - g_menuSeen.load(std::memory_order_relaxed) < MenuGone; }

void ssa::MenuFrame()
{
    static Pages found;
    static ssamenu::Driver driver;
    Read(found);
    if (found.count > 0)
        g_menuSeen.store(Now(), std::memory_order_relaxed);

    const mouse::MenuPointer::Read read = mouse::Menu().Take();
    ssamenu::Driver::Pointer pointer;
    pointer.active = read.active;
    pointer.inside = read.inside;
    pointer.x = read.x;
    pointer.y = read.y;
    pointer.clicks = read.clicks;
    const ssamenu::Action action =
        driver.Step(pointer, std::span<const ssamenu::Page>(found.pages.data(), found.count));
    if (action.focus != 0 && g_memory.Read<uint32_t>(action.page + PageFocusTo) == 0)
    {
        g_memory.Write<uint32_t>(action.page + PageFocusTo, action.focus);
        g_memory.Write<uint8_t>(action.page + PageFocusMove, 1);
        fprintf(stderr, "[menu] focus %08X\n", action.focus);
    }
    if (action.accept)
    {
        PressButtons(Button::A);
        fprintf(stderr, "[menu] accept\n");
    }
}
