// The pointer in the script menus (script_menu.h): the reader on guest memory and the thread that drives the focus.
#include "script_menu.h"
#include "host/input.h"
#include "host/mouse.h"
#include "memory.h"
#include "menu_pointer.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <span>
#include <thread>

namespace
{
    constexpr uint32_t NameOffset = 8, ValueOffset = 0x1C, StructMembers = 0x18, StructType = 0x1C, ArrayCount = 8,
                       ArrayItems = 0x14, ListOffset = 0x18, NumberOffset = 0x20, SpriteOffset = 0x1C;
    constexpr uint32_t SpriteX = 0x3C, SpriteY = 0x44;
    constexpr float ArtScale = 720.0f / 480.0f;
    constexpr uint32_t MaxMembers = 64, MaxItems = 32;

    bool Ok(uint32_t address, uint32_t size)
    {
        return address >= 0x10000 && (address & 3) == 0 && uint64_t(address) + size <= 0xE0000000ull;
    }

    uint32_t Word(uint32_t address) { return g_memory.Read<uint32_t>(address); }

    // The name at the pointer stored at `address` (script names are short).
    std::string_view NameAt(uint32_t address)
    {
        const uint32_t text = Word(address) & ~1u;
        if (text < 0x10000 || uint64_t(text) + 64 > 0xE0000000ull)
            return {};
        const char* chars = static_cast<const char*>(g_memory.Translate(text));
        const size_t length = strnlen(chars, 64);
        return length < 64 ? std::string_view(chars, length) : std::string_view();
    }

    struct Menu
    {
        uint32_t page = 0;
        alchemymenu::Page pointerPage;
        std::array<alchemymenu::Item, MaxItems> items;
    };

    class Reader
    {
    public:
        explicit Reader(const scriptmenu::Layout& layout) : m_layout(layout) {}

        // The page on screen, false when none.
        bool Read(Menu& menu)
        {
            using namespace std::chrono;
            const auto now = steady_clock::now();
            if (!IsVariable(m_current, "current menu"))
            {
                m_current = 0;
                if (now - m_lastScan < seconds(2))
                    return false;
                m_lastScan = now;
                ForEachVariable("current menu", [&](uint32_t variable) { m_current = variable; });
                if (m_current == 0)
                    return false;
                fprintf(stderr, "[menu] the script's current menu at %08X\n", m_current);
            }
            const uint32_t page = StructOf(Word(m_current + ValueOffset), m_layout.pageType);
            if (page == 0)
                return false;

            uint32_t array = 0;
            const uint32_t count = ListOf(Member(page, "items"), array);
            size_t boxes = 0;
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t item = StructOf(Word(array + i * 4), m_layout.itemType);
                if (item != 0 && Box(item, menu.items[boxes]))
                    boxes++;
            }
            menu.page = page;
            menu.pointerPage.handle = page;
            menu.pointerPage.items = std::span<const alchemymenu::Item>(menu.items.data(), boxes);
            menu.pointerPage.scrolls = Scrolls(page);
            // Items with no box on screen are 3D objects in a ring (Trap Team's pause menu).
            menu.pointerPage.turns = count >= 2 && boxes == 0;
            menu.pointerPage.focused = Focused(menu, now);
            return true;
        }

    private:
        bool IsVariable(uint32_t variable, std::string_view name) const
        {
            return Ok(variable, 0x20) && Word(variable) == m_layout.reference && NameAt(variable + NameOffset) == name;
        }

        // The script structure at `object` if it is one of type `type`, else 0.
        uint32_t StructOf(uint32_t object, std::string_view type) const
        {
            if (!Ok(object, 0x20) || Word(object) != m_layout.structure)
                return 0;
            const uint32_t info = Word(object + StructType);
            return Ok(info, 0x10) && NameAt(info + NameOffset) == type ? object : 0;
        }

        // The member of a structure by its name, 0 if it has none.
        static uint32_t Member(uint32_t object, std::string_view name)
        {
            const uint32_t members = Word(object + StructMembers);
            if (!Ok(members, 0x18))
                return 0;
            const uint32_t count = Word(members + ArrayCount), array = Word(members + ArrayItems);
            if (count > MaxMembers || !Ok(array, count * 4))
                return 0;
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t member = Word(array + i * 4);
                if (Ok(member, 0x24) && NameAt(member + NameOffset) == name)
                    return member;
            }
            return 0;
        }

        // A list member's elements: how many, and where the array is.
        uint32_t ListOf(uint32_t member, uint32_t& array) const
        {
            array = 0;
            if (member == 0 || Word(member) != m_layout.list)
                return 0;
            const uint32_t list = Word(member + ListOffset);
            if (!Ok(list, 0x18))
                return 0;
            const uint32_t count = Word(list + ArrayCount);
            array = Word(list + ArrayItems);
            return count <= MaxItems && Ok(array, count * 4) ? count : 0;
        }

        bool Scrolls(uint32_t page) const
        {
            uint32_t array = 0;
            const uint32_t count = ListOf(Member(page, "extra data"), array);
            for (uint32_t i = 0; i < count; ++i)
                if (StructOf(Word(array + i * 4), "Menu - Scrolling - Page Scroll Info") != 0)
                    return true;
            return false;
        }

        // An item's box on the canvas from its container sprite; false for an item the focus cannot take.
        bool Box(uint32_t item, alchemymenu::Item& box) const
        {
            const uint32_t unselectable = Member(item, "unselectable");
            if (unselectable != 0 && Word(unselectable) == m_layout.number && Word(unselectable + NumberOffset) != 0)
                return false;
            const uint32_t variable = Member(item, "container sprite");
            if (variable == 0 || Word(variable) != m_layout.spriteVariable)
                return false;
            const uint32_t sprite = Word(variable + SpriteOffset);
            if (!Ok(sprite, 0x100) || Word(sprite) != m_layout.sprite)
                return false;
            box.id = item;
            box.left = g_memory.Read<float>(sprite + SpriteX);
            box.top = g_memory.Read<float>(sprite + SpriteY);
            box.width = g_memory.Read<float>(sprite + m_layout.spriteSize) * ArtScale;
            box.height = g_memory.Read<float>(sprite + m_layout.spriteSize + 4) * ArtScale;
            // Items waiting off the canvas (a carousel's ends) are not targets.
            return box.width > 0 && box.height > 0 && box.left + box.width > 0 && box.left < 1280 &&
                   box.top + box.height > 0 && box.top < 720;
        }

        // The variables, found by a scan of the physical heap's allocations (the 0xA0000000 window, where the script
        // keeps them; a few tens of ms) and checked again on every read.
        template <typename Match>
        void ForEachVariable(std::string_view name, const Match& match) const
        {
            g_physicalHeap.ForEachAllocation([&](uint32_t base, uint32_t size) {
                const uint32_t begin = 0xA0000000u | base;
                for (uint32_t offset = 0; offset + 0x20 <= size; offset += 4)
                {
                    if (Word(begin + offset) == m_layout.reference && IsVariable(begin + offset, name))
                        match(begin + offset);
                }
            });
        }

        uint32_t ItemIn(const Menu& menu, uint32_t variable) const
        {
            if (!IsVariable(variable, "selection cur"))
                return 0;
            const uint32_t item = Word(variable + ValueOffset);
            for (const alchemymenu::Item& box : menu.pointerPage.items)
                if (box.id == item)
                    return item;
            return 0;
        }

        // The focused item: the "selection cur" that holds one of the page's items. Each menu has its own, so a new
        // page may need a new scan (at most every 300 ms).
        uint32_t Focused(const Menu& menu, std::chrono::steady_clock::time_point now)
        {
            if (menu.pointerPage.items.empty())
                return 0;
            if (uint32_t item = ItemIn(menu, m_selection))
                return item;
            if (now - m_lastSelectionScan < std::chrono::milliseconds(300))
                return 0;
            m_lastSelectionScan = now;
            m_selection = 0;
            ForEachVariable("selection cur", [&](uint32_t variable) {
                if (m_selection == 0 && ItemIn(menu, variable) != 0)
                    m_selection = variable;
            });
            return ItemIn(menu, m_selection);
        }

        scriptmenu::Layout m_layout;
        uint32_t m_current = 0, m_selection = 0;
        std::chrono::steady_clock::time_point m_lastScan{}, m_lastSelectionScan{};
    };

    // A page counts as open for a moment after it goes, so the pointer does not blink between two pages.
    constexpr int64_t MenuGone = 300; // ms
    std::atomic<int64_t> g_menuSeen{ -MenuGone };

    int64_t Now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    void Watch(scriptmenu::Layout layout)
    {
        Reader reader(layout);
        alchemymenu::Driver driver;
        static Menu menu;
        uint32_t shownPage = 0;
        size_t shownItems = 0;
        for (;;)
        {
            if (g_memory.base != nullptr)
            {
                const bool open = reader.Read(menu);
                if (open)
                    g_menuSeen.store(Now(), std::memory_order_relaxed);
                const uint32_t page = open ? menu.page : 0;
                if (page != shownPage || (open && menu.pointerPage.items.size() != shownItems))
                {
                    shownPage = page;
                    shownItems = open ? menu.pointerPage.items.size() : 0;
                    fprintf(stderr, "[menu] page %08X: %zu items%s\n", page, shownItems,
                        !open                      ? ""
                        : menu.pointerPage.turns   ? ", a carousel turned by its sides"
                        : menu.pointerPage.scrolls ? ", a carousel"
                                                   : "");
                    for (size_t i = 0; i < shownItems; ++i)
                    {
                        const alchemymenu::Item& item = menu.pointerPage.items[i];
                        fprintf(stderr, "[menu]   item %08X at %.0f/%.0f, %.0fx%.0f\n", item.id, item.left, item.top,
                            item.width, item.height);
                    }
                }
                const mouse::MenuPointer::Read read = mouse::Menu().Take();
                alchemymenu::Driver::Pointer pointer;
                pointer.active = read.active;
                pointer.inside = read.inside;
                pointer.x = read.x;
                pointer.y = read.y;
                pointer.clicks = read.clicks;
                if (const uint16_t buttons = driver.Step(pointer, open ? &menu.pointerPage : nullptr, Now()))
                {
                    PressButtons(buttons);
                    if (buttons == Button::A)
                        fprintf(stderr, "[menu] accept\n");
                    else if (menu.pointerPage.turns)
                        fprintf(stderr, "[menu] turn: d-pad %04X\n", buttons);
                    else
                        fprintf(stderr, "[menu] focus: d-pad %04X from %08X\n", buttons, menu.pointerPage.focused);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
}

void scriptmenu::Start(const Layout& layout) { std::thread(Watch, layout).detach(); }

bool scriptmenu::MenuOpen() { return Now() - g_menuSeen.load(std::memory_order_relaxed) < MenuGone; }
