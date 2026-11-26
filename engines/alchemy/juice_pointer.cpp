// The pointer in the Juice menus (juice_menu.h; docs/runtime/xbox360/input.md, "Juice menus"): the reader on guest
// memory, the lists found by a scan of the physical heap, and the focus moved by d-pad presses on player 1's
// controller (host/input.h, PressButtons), as in Giants (games/giants/menus.cpp).
#include "game.h"
#include "host/input.h"
#include "host/mouse.h"
#include "juice_menu.h"
#include "memory.h"
#include "menu_pointer.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    constexpr uint32_t ImageBegin = 0x82000000, ImageEnd = 0x83000000;

    class GuestMemory final : public juicemenu::Memory
    {
    public:
        uint32_t Word(uint32_t address) const override
        {
            return Ok(address, 4) && (address & 3) == 0 ? g_memory.Read<uint32_t>(address) : 0;
        }
        std::string_view Text(uint32_t address) const override
        {
            if (!Ok(address, 64))
                return {};
            const char* chars = static_cast<const char*>(g_memory.Translate(address));
            const size_t length = strnlen(chars, 64);
            return length < 64 ? std::string_view(chars, length) : std::string_view();
        }

    private:
        static bool Ok(uint32_t address, uint32_t size)
        {
            return address >= 0x10000 && uint64_t(address) + size <= 0xE0000000ull;
        }
    };

    // The lists (igJuiceList objects) in the physical heap, where the game keeps its Juice objects.
    std::vector<uint32_t> FindLists(const std::vector<uint32_t>& vtables)
    {
        std::vector<uint32_t> lists;
        g_physicalHeap.ForEachAllocation([&](uint32_t base, uint32_t size) {
            const uint32_t begin = 0xA0000000u | base;
            for (uint32_t offset = 0; offset + 4 <= size; offset += 4)
            {
                const uint32_t word = g_memory.Read<uint32_t>(begin + offset);
                for (const uint32_t vtable : vtables)
                    if (word == vtable)
                        lists.push_back(begin + offset);
            }
        });
        return lists;
    }

    int64_t Now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    // A page counts as open for a moment after it goes, so the pointer does not blink between two pages.
    constexpr int64_t MenuGone = 300; // ms
    // A new page may be a list not seen yet: the heap is scanned again (~0.1 s) while none is on screen, this often on
    // the front end, where pages come and go, and less in play, where the game reuses its lists (the pause menu takes
    // the main menu's).
    constexpr int64_t ScanGap = 2000, PlayScanGap = 10000; // ms
    std::atomic<int64_t> g_menuSeen{ -MenuGone };

    void Watch()
    {
        GuestMemory memory;
        juicemenu::Reader reader(memory);
        alchemymenu::Driver driver;
        static juicemenu::Menu menu;
        std::vector<uint32_t> vtables, lists;
        int64_t lastScan = -PlayScanGap;
        uint32_t shownList = 0;
        size_t shownItems = 0;
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            if (g_memory.base == nullptr)
                continue;
            const int64_t now = Now();
            bool open = !lists.empty() && reader.Read(lists, menu);
            if (!open && now - lastScan >= (game::InMenuLevel() || lists.empty() ? ScanGap : PlayScanGap))
            {
                lastScan = now;
                if (vtables.empty())
                    vtables = reader.ListVtables(ImageBegin, ImageEnd);
                if (!vtables.empty())
                    lists = FindLists(vtables);
                open = !lists.empty() && reader.Read(lists, menu);
                const int64_t took = Now() - now;
                static size_t loggedLists = 0;
                if (lists.size() != loggedLists)
                {
                    loggedLists = lists.size();
                    fprintf(stderr, "[menu] %zu Juice lists (%lld ms)\n", lists.size(), static_cast<long long>(took));
                }
            }
            if (open)
                g_menuSeen.store(now, std::memory_order_relaxed);
            const uint32_t list = open ? menu.list : 0;
            if (list != shownList || (open && menu.page.items.size() != shownItems))
            {
                shownList = list;
                shownItems = open ? menu.page.items.size() : 0;
                fprintf(stderr, "[menu] page %08X: %zu items%s\n", list, shownItems,
                    open && menu.page.scrolls ? ", a carousel" : "");
                for (size_t i = 0; i < shownItems; ++i)
                {
                    const alchemymenu::Item& item = menu.page.items[i];
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
            if (const uint16_t buttons = driver.Step(pointer, open ? &menu.page : nullptr, now))
            {
                PressButtons(buttons);
                if (buttons == Button::A)
                    fprintf(stderr, "[menu] accept\n");
                else
                    fprintf(stderr, "[menu] focus: d-pad %04X from %08X\n", buttons, menu.page.focused);
            }
        }
    }
}

void juicemenu::Start()
{
    static std::once_flag once;
    std::call_once(once, [] { std::thread(Watch).detach(); });
}

bool juicemenu::MenuOpen() { return Now() - g_menuSeen.load(std::memory_order_relaxed) < MenuGone; }
