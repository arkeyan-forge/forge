#pragma once

// The Alchemy games whose menus are run by the game's script (Menus.ai: Giants, Trap Team; docs/runtime/xbox360/input.md,
// "Script menus"): whether a page is on screen (game::Adapter::MenuOpen) and the pointer in it (menu_pointer.h). The
// pages are read from the script's variables, found by a scan of the physical heap, on a thread of their own; the focus
// moves by d-pad presses and A picks (host/input.h, PressButtons), the mouse being player 1's.
//
// What the script holds (the same in both games, only the vtables differ):
// - a variable holding an object (Layout::reference): +8 its name, +0x1C its value;
// - a script structure (Layout::structure): +0x18 its members (+8 how many, +0x14 the array), +0x1C its type (+8 its
//   name); a member is a typed variable: +8 its name, a list (Layout::list) at +0x18 (+8 how many, +0x14 the array), a
//   number (Layout::number) at +0x20, a sprite (Layout::spriteVariable) at +0x1C;
// - a name is a pointer to its text, its low bit a tag (set in Trap Team);
// - "current menu" holds the page on screen (a "Menu Page Info", Trap Team's "Menu - Page Info"), 0 in play; its
//   "items" are "Menu Item Info"s ("Menu - Item Info"), each with its "container sprite" and "unselectable"; its
//   "extra data" holds a "Menu - Scrolling - Page Scroll Info" in a carousel (Giants' pause menu); Trap Team's pause
//   menu is a ring of 3D objects whose items have no box (alchemymenu::Page::turns);
// - each menu's highlight keeps its own "selection cur": the focused item of its page;
// - a sprite (Layout::sprite): its top left on the 1280x720 canvas at +0x3C, +0x44; its width and height next to each
//   other (Layout::spriteSize), in the 480-line pixels the art was drawn in.
#include <cstdint>
#include <string_view>

namespace scriptmenu
{
    // The game's vtables and type names for the objects above.
    struct Layout
    {
        uint32_t reference = 0, structure = 0, list = 0, number = 0, spriteVariable = 0, sprite = 0;
        uint32_t spriteSize = 0; // the sprite's width, its height 4 bytes on
        std::string_view pageType, itemType;
    };

    // Reads the menus on a thread of its own and drives the focus from the pointer (alchemymenu::Driver). Called once,
    // at startup: on the title level game::InMenu does not ask MenuOpen, and the pointer is the menu's there too.
    void Start(const Layout& layout);
    // A page is on screen (kept a moment across a change of page).
    bool MenuOpen();
}
