#pragma once

// The pointer in the Alchemy games' own menus (docs/runtime/xbox360/input.md, "Mouse"): which item is under it and which
// controller buttons get the focus there or pick it. The menus are run by the game's scripts, which take no focus
// request from outside: the focus moves one d-pad press at a time towards the item, and A picks it. Plain C++ (no
// guest memory): ArkeyanAlchemyMenuTest runs it; each game reads its menus and presses the buttons.
#include <cstdint>
#include <span>

namespace alchemymenu
{
    // An item the focus can reach: an id the game gives it and its box on the 1280x720 canvas.
    struct Item
    {
        uint32_t id = 0;
        float left = 0, top = 0, width = 0, height = 0;
    };
    // The item under the point, the nearest centre for its box's size when boxes overlap; 0 when none.
    uint32_t Hit(std::span<const Item> items, float x, float y);

    // The page on screen.
    struct Page
    {
        uint32_t handle = 0;  // the page (its guest address)
        uint32_t focused = 0; // the focused item's id, 0 when not known
        bool scrolls = false; // a carousel: the focus scrolls the items, so a pointer at rest would chase them
        std::span<const Item> items;
        // A carousel whose items have no box on screen (Trap Team's pause menu): a click in the middle third picks the
        // centred item, one in a side third turns it one step that way.
        bool turns = false;
    };

    class Driver
    {
    public:
        struct Pointer
        {
            bool active = false, inside = false;
            float x = 0, y = 0; // canvas pixels
            uint32_t clicks = 0;
        };
        // The step between two presses: the game's script reads a press, runs the focus change, then waits for a
        // release before the next.
        static constexpr int64_t PressGap = 250; // ms

        // Frame by frame, from the pointer (mouse::MenuPointer::Read's fields), the page (nullptr: none, such as the
        // title's Press A), and a clock in ms: the buttons to press now (XINPUT bits), 0 for none. A pointer moved onto
        // an item of a page that does not scroll gives it the focus; a click on an item gives it the focus and then
        // picks it; on a page with a single item or none, or with no page, a click anywhere is A. A pointer at rest
        // never takes the focus from the keys or the pad.
        uint16_t Step(const Pointer& pointer, const Page* page, int64_t now);

    private:
        void Drop();

        bool m_known = false; // m_x, m_y hold the pointer's last place
        float m_x = 0, m_y = 0;
        uint32_t m_page = 0;
        uint32_t m_target = 0; // the item the focus moves to
        bool m_accept = false; // and picks once there
        int64_t m_next = 0; // no press before this time
        uint32_t m_pressedFrom = 0; // the focus when the last move was pressed
        bool m_otherAxis = false; // that press did not move it: the other axis is tried once
        int m_steps = 0;
    };
}
