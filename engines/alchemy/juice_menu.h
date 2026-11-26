#pragma once

// The pointer's view of the Juice menus (Swap Force on; docs/runtime/xbox360/input.md, "Juice menus"): the page on
// screen, its items' boxes on the 1280x720 canvas and the focused one. A Juice page is a list (igJuiceList) of items,
// each with a box in the list's scroll placeable, under a tree of placeables (position, pivot, scale, extent, alpha,
// hidden). Everything is found through the game's own class metadata, by class and field name, so no game address is
// needed. Plain C++ over a Memory: ArkeyanJuiceMenuTest runs it on a fake one.
#include "menu_pointer.h"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace juicemenu
{
    // Guest memory as the reader sees it.
    class Memory
    {
    public:
        virtual ~Memory() = default;
        // The big-endian word at `address`, 0 where there is no guest memory.
        virtual uint32_t Word(uint32_t address) const = 0;
        // The NUL-terminated text at `address` (at most 64 characters), empty where there is none.
        virtual std::string_view Text(uint32_t address) const = 0;
    };

    constexpr size_t MaxItems = 32;

    // The page on screen.
    struct Menu
    {
        uint32_t list = 0; // the igJuiceList
        alchemymenu::Page page;
        std::array<alchemymenu::Item, MaxItems> items;
    };

    class Reader
    {
    public:
        explicit Reader(const Memory& memory) : m_memory(memory) {}

        // The vtables of igJuiceList in the image's words [begin, end): its getMeta ("lis; lwz r3; blr", the class's
        // metadata slot) is the first entry of each. Empty until the game has registered its classes.
        std::vector<uint32_t> ListVtables(uint32_t begin, uint32_t end) const;

        // The page on screen among `lists` (igJuiceList objects), false when none. A list is a page while it has the
        // focus, holds items and no placeable above it is hidden or transparent; covered pages stay focused but are
        // hidden. When several are, the newest wins.
        bool Read(std::span<const uint32_t> lists, Menu& menu);

    private:
        struct Field
        {
            uint32_t offset = 0, shift = 0;
            bool found = false;
        };
        struct ListLayout
        {
            Field items, visibleItems, focusItem, hasFocus, rootPlaceable, scrollPlaceable;
        };
        struct ArrayLayout
        {
            Field count, data;
        };
        struct ItemLayout
        {
            Field placeable, min, max, visible;
        };
        struct PlaceableLayout
        {
            Field parent, position, pivot, scale, extent, alpha, finalHidden;
        };

        uint32_t MetaOf(uint32_t object) const;
        Field FieldOf(uint32_t meta, std::string_view name) const;
        float Float(uint32_t address) const;
        const ListLayout* List(uint32_t meta);
        const ArrayLayout* Array(uint32_t meta);
        const ItemLayout* Item(uint32_t meta);
        const PlaceableLayout* Placeable(uint32_t meta);
        uint32_t Count(uint32_t array, uint32_t& data);
        bool Shown(uint32_t placeable);
        // A point of the placeable's space on the canvas.
        void ToCanvas(uint32_t placeable, float& x, float& y);
        bool Page(uint32_t list, Menu& menu);

        const Memory& m_memory;
        std::unordered_map<uint32_t, ListLayout> m_lists;
        std::unordered_map<uint32_t, ArrayLayout> m_arrays;
        std::unordered_map<uint32_t, ItemLayout> m_items;
        std::unordered_map<uint32_t, PlaceableLayout> m_placeables;
        std::vector<uint32_t> m_shown; // the pages at the last read
        uint32_t m_current = 0;
    };
}

// The game's side (juice_pointer.cpp): a game with Juice menus calls Start once at startup and gives MenuOpen to its
// adapter (game::Adapter::MenuOpen).
namespace juicemenu
{
    // Reads the menus on a thread of its own and drives the focus from the pointer (alchemymenu::Driver).
    void Start();
    // A page is on screen (kept a moment across a change of page).
    bool MenuOpen();
}
