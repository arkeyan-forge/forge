#pragma once

// The pointer in Spyro's Adventure's own menus (docs/runtime/xbox360/input.md, "Mouse"): which item is under it and what
// to ask the game for, the focus or Accept. Plain C++ (no guest memory): ArkeyanSsaMenuTest runs it; games/ssa/menus.cpp
// reads the menus from the game and applies the answers.
#include <cstddef>
#include <cstdint>
#include <span>

namespace ssamenu
{
    // The game lays its menus out on a 480-unit-high screen, y up, centred: 1.5 canvas pixels a unit.
    constexpr float CanvasPerUnit = 720.0f / 480.0f;
    void ToUnits(float canvasX, float canvasY, float& x, float& y);

    // An item the focus can reach: its id and its box (centre and half sizes, in units).
    struct Item
    {
        uint32_t id = 0;
        float x = 0, y = 0, halfWidth = 0, halfHeight = 0;
    };
    // The item under the point, the nearest centre for its box's size when boxes overlap; 0 when none.
    uint32_t Hit(std::span<const Item> items, float x, float y);

    // One of the game's pages that holds the focus: what the game says this frame.
    struct Page
    {
        uint32_t handle = 0;  // the page (its guest address)
        uint32_t focused = 0; // the focused item's id, 0 for a page without items
        bool moving = false;  // a focus move the game has not made yet
        std::span<const Item> items; // none: a notice, whose OK is the page's own
    };

    // What to do this frame: move the focus of a page to an item, and/or press Accept on it.
    struct Action
    {
        uint32_t page = 0;
        uint32_t focus = 0;  // 0: no move
        bool accept = false; // Accept on the page's focused item
    };

    // Frame by frame, from the pointer (mouse::MenuPointer::Read's fields) and the pages: a pointer moved onto an item
    // gives it the focus; a click on the focused item is Accept, on another item the focus first, then Accept once the
    // game has moved it; on a page with a single item or none, a click anywhere is Accept. A pointer that does not move never
    // takes the focus from the keys or the pad.
    class Driver
    {
    public:
        struct Pointer
        {
            bool active = false, inside = false;
            float x = 0, y = 0; // canvas pixels
            uint32_t clicks = 0;
        };
        Action Step(const Pointer& pointer, std::span<const Page> pages);

    private:
        bool m_known = false; // m_x, m_y hold the pointer's last place
        float m_x = 0, m_y = 0;
        uint32_t m_acceptPage = 0, m_acceptItem = 0; // a click waiting for the focus
        int m_acceptFrames = 0;
    };
}
