// The Juice menus as the pointer reads them (engines/alchemy/juice_menu.h), on a fake guest memory laid out as Swap
// Force's: the class metadata and getMeta, a page's list, items and placeables, a covered page, a transparent one, a
// scaled one, the newest page winning, a scrolling list. No game data: ./ArkeyanJuiceMenuTest
#include "juice_menu.h"
#include <bit>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace juicemenu;

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

    bool Near(float a, float b) { return std::fabs(a - b) < 0.05f; }

    class FakeMemory final : public Memory
    {
    public:
        uint32_t Word(uint32_t address) const override
        {
            const auto it = m_words.find(address);
            return it == m_words.end() ? 0 : it->second;
        }
        std::string_view Text(uint32_t address) const override
        {
            const auto it = m_texts.find(address);
            return it == m_texts.end() ? std::string_view() : std::string_view(it->second);
        }

        void Set(uint32_t address, uint32_t value) { m_words[address] = value; }
        void SetFloat(uint32_t address, float value) { Set(address, std::bit_cast<uint32_t>(value)); }
        void SetByte(uint32_t address, uint8_t value)
        {
            const uint32_t word = address & ~3u, shift = 24 - 8 * (address & 3);
            Set(word, (Word(word) & ~(0xFFu << shift)) | (uint32_t(value) << shift));
        }
        uint32_t Alloc(uint32_t bytes)
        {
            const uint32_t address = m_next;
            m_next += (bytes + 15) & ~15u;
            return address;
        }
        uint32_t String(const std::string& text)
        {
            const uint32_t address = Alloc(uint32_t(text.size()) + 1);
            m_texts[address] = text;
            return address;
        }

        // A class: its metadata (name at +8, the field list's data at +0x14, its size in bytes just before), the
        // global holding it, getMeta in the code ("lis r11; lwz r3,lo(r11); blr") and a vtable whose first entry is it.
        struct FieldSpec
        {
            const char* name;
            uint32_t offset, shift;
        };
        uint32_t Class(const char* name, std::vector<FieldSpec> fields)
        {
            const uint32_t meta = Alloc(0x60);
            Set(meta + 8, String(name));
            const uint32_t data = Alloc(4 + 4 * uint32_t(fields.size())) + 4;
            Set(data - 4, 4 * uint32_t(fields.size()));
            Set(meta + 0x14, data);
            for (size_t i = 0; i < fields.size(); ++i)
            {
                const uint32_t field = Alloc(0x30);
                Set(field + 0x10, fields[i].offset << 16);
                Set(field + 0x1C, String(fields[i].name));
                Set(field + 0x28, fields[i].shift);
                Set(data + 4 * uint32_t(i), field);
            }
            const uint32_t global = 0x83120000 + 4 * m_classes;
            Set(global, meta);
            const uint32_t code = 0x82800000 + 16 * m_classes;
            const uint32_t high = (global + 0x8000) >> 16;
            const int16_t low = int16_t(global & 0xFFFF);
            Set(code, 0x3D600000 | high);                         // lis r11,high
            Set(code + 4, 0x806B0000 | uint16_t(low));            // lwz r3,low(r11)
            Set(code + 8, 0x4E800020);                            // blr
            const uint32_t vtable = 0x82070000 + 0x40 * m_classes; // in the image's data
            Set(vtable, code);
            m_classes++;
            return vtable;
        }
        uint32_t Object(uint32_t vtable, uint32_t bytes)
        {
            const uint32_t object = Alloc(bytes);
            Set(object, vtable);
            return object;
        }

    private:
        std::map<uint32_t, uint32_t> m_words;
        std::map<uint32_t, std::string> m_texts;
        uint32_t m_next = 0xA6000000;
        uint32_t m_classes = 0;
    };

    // Swap Force's offsets (read from its metadata); the reader finds them by name, so any other set works too.
    struct World
    {
        FakeMemory memory;
        uint32_t listClass, arrayClass, itemClass, placeableClass, otherClass;

        World()
        {
            listClass = memory.Class("igJuiceList",
                { { "_items", 0x8C, 0 }, { "_visibleItems", 0x90, 0 }, { "_focusItem", 0xB4, 0 },
                    { "_hasFocus", 0xD4, 0 }, { "_rootPlaceable", 0x84, 0 }, { "_scrollPlaceable", 0x88, 0 } });
            arrayClass = memory.Class("igJuiceListItemList", { { "_count", 8, 0 }, { "_data", 0x10, 0 } });
            itemClass = memory.Class("CJuiceListItemPauseMenuOption",
                { { "_visible", 0xA, 0 }, { "_placeable", 0x10, 0 }, { "_min", 0x14, 0 }, { "_max", 0x1C, 0 } });
            placeableClass = memory.Class("igJuicePlaceable",
                { { "_parent", 0x20, 0 }, { "_position", 0x30, 0 }, { "_pivot", 0x3C, 0 }, { "_scale", 0x54, 0 },
                    { "_extent", 0x60, 0 }, { "_alpha", 0xA0, 0 }, { "_hidden", 8, 5 }, { "_finalHidden", 8, 8 } });
            otherClass = memory.Class("igJuiceProject", {});
        }

        uint32_t Placeable(uint32_t parent, float x, float y, float width = 0, float height = 0, float pivotX = 0,
            float pivotY = 0, float scale = 1)
        {
            const uint32_t p = memory.Object(placeableClass, 0xB4);
            memory.Set(p + 0x20, parent);
            memory.SetFloat(p + 0x30, x);
            memory.SetFloat(p + 0x34, y);
            memory.SetFloat(p + 0x3C, pivotX);
            memory.SetFloat(p + 0x40, pivotY);
            memory.SetFloat(p + 0x54, scale);
            memory.SetFloat(p + 0x58, scale);
            memory.SetFloat(p + 0x60, width);
            memory.SetFloat(p + 0x64, height);
            memory.SetFloat(p + 0xA0, 1);
            memory.Set(p + 8, 0x10);
            return p;
        }

        uint32_t Items(const std::vector<uint32_t>& items)
        {
            const uint32_t array = memory.Object(arrayClass, 0x18);
            const uint32_t data = memory.Alloc(4 * uint32_t(items.size()) + 4);
            for (size_t i = 0; i < items.size(); ++i)
                memory.Set(data + 4 * uint32_t(i), items[i]);
            memory.Set(array + 8, uint32_t(items.size()));
            memory.Set(array + 0x10, 4 * uint32_t(items.size())); // the memory reference: its size, then the data
            memory.Set(array + 0x14, data);
            return array;
        }

        uint32_t Item(uint32_t scroll, float left, float top, float right, float bottom)
        {
            const uint32_t item = memory.Object(itemClass, 0x3C);
            memory.Set(item + 0x10, Placeable(scroll, left, top, right - left, bottom - top));
            memory.SetFloat(item + 0x14, left);
            memory.SetFloat(item + 0x18, top);
            memory.SetFloat(item + 0x1C, right);
            memory.SetFloat(item + 0x20, bottom);
            memory.SetByte(item + 0xA, 1);
            return item;
        }

        struct Page
        {
            uint32_t list, root, scroll;
            std::vector<uint32_t> items;
        };

        // A focused page: the screen, a panel at `panelX/Y` (pivot, scale), the list root and its scroll placeable,
        // and `rows` items of 450x30 every 35 pixels, the first focused.
        Page MakePage(float panelX, float panelY, int rows, float pivot = 0, float scale = 1, int visible = -1)
        {
            const uint32_t screen = Placeable(0, 0, 0, 1280, 720);
            const uint32_t panel = Placeable(screen, panelX, panelY, 1280, 720, pivot, pivot, scale);
            Page page;
            page.root = Placeable(panel, 0, 0, 550, 500);
            page.scroll = Placeable(page.root, 0, 0);
            for (int i = 0; i < rows; ++i)
                page.items.push_back(Item(page.scroll, 0, 5 + 35.0f * float(i), 450, 35 + 35.0f * float(i)));
            page.list = memory.Object(listClass, 0xE0);
            memory.Set(page.list + 0x84, page.root);
            memory.Set(page.list + 0x88, page.scroll);
            memory.Set(page.list + 0x8C, Items(page.items));
            const std::vector<uint32_t> shown(
                page.items.begin(), page.items.begin() + (visible < 0 ? rows : std::min(visible, rows)));
            memory.Set(page.list + 0x90, Items(shown));
            memory.Set(page.list + 0xB4, page.items.empty() ? 0 : page.items[0]);
            memory.SetByte(page.list + 0xD4, 1);
            return page;
        }
    };

    void FindsTheListVtable()
    {
        World world;
        const std::vector<uint32_t> vtables = Reader(world.memory).ListVtables(0x82000000, 0x83000000);
        EXPECT(vtables.size() == 1);
        EXPECT(!vtables.empty() && vtables[0] == world.listClass);

        // Before the game registers its classes, the metadata slot is empty: nothing yet.
        FakeMemory empty;
        EXPECT(Reader(empty).ListVtables(0x82000000, 0x83000000).empty());
    }

    void ReadsAPage()
    {
        // Swap Force's DIFFICULTY: the panel at 181.9/165 in a frame at -88.2/93.6.
        World world;
        const uint32_t screen = world.Placeable(0, 0, 0, 1280, 720);
        const uint32_t frame = world.Placeable(screen, -88.2f, 93.6f, 650, 576);
        const uint32_t panel = world.Placeable(frame, 181.9f, 165, 550, 500);
        const uint32_t root = world.Placeable(panel, 0, 0, 550, 500);
        const uint32_t scroll = world.Placeable(root, 0, 0);
        std::vector<uint32_t> items(4);
        for (size_t i = 0; i < items.size(); ++i)
            items[i] = world.Item(scroll, 0, 5 + 35.0f * float(i), 450, 35 + 35.0f * float(i));
        const uint32_t list = world.memory.Object(world.listClass, 0xE0);
        world.memory.Set(list + 0x84, root);
        world.memory.Set(list + 0x88, scroll);
        world.memory.Set(list + 0x8C, world.Items(items));
        world.memory.Set(list + 0x90, world.Items(items));
        world.memory.Set(list + 0xB4, items[1]);
        world.memory.SetByte(list + 0xD4, 1);

        Reader reader(world.memory);
        Menu menu;
        const uint32_t lists[] = { list };
        EXPECT(reader.Read(lists, menu));
        EXPECT(menu.list == list);
        EXPECT(menu.page.handle == list);
        EXPECT(menu.page.focused == items[1]);
        EXPECT(!menu.page.scrolls);
        EXPECT(menu.page.items.size() == 4);
        if (menu.page.items.size() == 4)
        {
            const alchemymenu::Item& first = menu.page.items[0];
            EXPECT(first.id == items[0]);
            EXPECT(Near(first.left, 93.7f) && Near(first.top, 263.6f));
            EXPECT(Near(first.width, 450) && Near(first.height, 30));
            EXPECT(Near(menu.page.items[3].top, 368.6f));
        }
    }

    void ScaledPanel()
    {
        // The save slots: a panel scaled 1.3 about its centre (pivot 0.5 of 1280x720 at 640/380), the list at
        // 296/209.7 in it.
        World world;
        const uint32_t screen = world.Placeable(0, 0, 0, 1280, 720);
        const uint32_t frame = world.Placeable(screen, 640, 380, 1280, 720, 0.5f, 0.5f, 1.3f);
        const uint32_t panel = world.Placeable(frame, 296, 209.7f, 700, 320);
        const uint32_t root = world.Placeable(panel, 0, 0, 910, 416);
        const uint32_t scroll = world.Placeable(root, 0, 0);
        const std::vector<uint32_t> items = { world.Item(scroll, 0, 0, 700, 120),
            world.Item(scroll, 0, 104, 700, 224) };
        const uint32_t list = world.memory.Object(world.listClass, 0xE0);
        world.memory.Set(list + 0x84, root);
        world.memory.Set(list + 0x88, scroll);
        world.memory.Set(list + 0x8C, world.Items(items));
        world.memory.Set(list + 0x90, world.Items(items));
        world.memory.Set(list + 0xB4, items[0]);
        world.memory.SetByte(list + 0xD4, 1);

        Reader reader(world.memory);
        Menu menu;
        const uint32_t lists[] = { list };
        EXPECT(reader.Read(lists, menu));
        EXPECT(menu.page.items.size() == 2);
        if (menu.page.items.size() == 2)
        {
            const alchemymenu::Item& first = menu.page.items[0];
            EXPECT(Near(first.left, 192.8f) && Near(first.top, 184.6f));
            EXPECT(Near(first.width, 910) && Near(first.height, 156));
            EXPECT(Near(menu.page.items[1].top, 319.8f));
        }
    }

    void CoveredPagesAreNotOnScreen()
    {
        World world;
        const World::Page main = world.MakePage(100, 200, 3);
        const World::Page slots = world.MakePage(300, 200, 3);
        // The main menu under the save slots: still focused, but its list root is hidden (_finalHidden).
        world.memory.Set(main.root + 8, 0x150);
        Reader reader(world.memory);
        Menu menu;
        const uint32_t lists[] = { main.list, slots.list };
        EXPECT(reader.Read(lists, menu));
        EXPECT(menu.list == slots.list);

        // Hidden by a transparent parent instead.
        world.memory.Set(main.root + 8, 0x10);
        world.memory.SetFloat(world.memory.Word(main.root + 0x20) + 0xA0, 0);
        Reader other(world.memory);
        EXPECT(other.Read(lists, menu));
        EXPECT(menu.list == slots.list);

        // _hidden alone (bit 5) is not what the engine shows: _finalHidden is.
        world.memory.Set(slots.root + 8, 0x30);
        EXPECT(other.Read(lists, menu));
        EXPECT(menu.list == slots.list);
    }

    void NewestPageWins()
    {
        World world;
        const World::Page first = world.MakePage(100, 200, 3);
        const World::Page second = world.MakePage(300, 200, 2);
        Reader reader(world.memory);
        Menu menu;
        const uint32_t one[] = { first.list, second.list };
        world.memory.SetByte(second.list + 0xD4, 0); // not focused yet
        EXPECT(reader.Read(one, menu));
        EXPECT(menu.list == first.list);
        world.memory.SetByte(second.list + 0xD4, 1); // a dialog over it, the first one not hidden
        EXPECT(reader.Read(one, menu));
        EXPECT(menu.list == second.list);
        EXPECT(reader.Read(one, menu)); // and it stays
        EXPECT(menu.list == second.list);
        world.memory.SetByte(second.list + 0xD4, 0); // the dialog goes
        EXPECT(reader.Read(one, menu));
        EXPECT(menu.list == first.list);
    }

    void NotAPage()
    {
        World world;
        Reader reader(world.memory);
        Menu menu;
        // Not focused.
        const World::Page idle = world.MakePage(100, 200, 3);
        world.memory.SetByte(idle.list + 0xD4, 0);
        // Focused but empty.
        const World::Page empty = world.MakePage(100, 200, 0);
        // Freed: its words are something else now.
        const World::Page freed = world.MakePage(100, 200, 3);
        world.memory.Set(freed.list, world.otherClass);
        const uint32_t lists[] = { idle.list, empty.list, freed.list, 0, 0x12345678 };
        EXPECT(!reader.Read(lists, menu));
        EXPECT(!reader.Read({}, menu));
    }

    void ItemsOffTheList()
    {
        World world;
        const World::Page page = world.MakePage(100, 200, 4);
        world.memory.SetByte(page.items[1] + 0xA, 0);            // not visible
        world.memory.SetFloat(page.items[2] + 0x14, 2000);        // off the canvas
        world.memory.SetFloat(page.items[2] + 0x1C, 2450);
        Reader reader(world.memory);
        Menu menu;
        const uint32_t lists[] = { page.list };
        EXPECT(reader.Read(lists, menu));
        EXPECT(menu.page.items.size() == 2);
        EXPECT(menu.page.items.size() == 2 && menu.page.items[0].id == page.items[0] &&
               menu.page.items[1].id == page.items[3]);
    }

    void ScrollingList()
    {
        World world;
        const World::Page page = world.MakePage(100, 200, 6, 0, 1, 4); // 6 items, 4 shown at a time
        Reader reader(world.memory);
        Menu menu;
        const uint32_t lists[] = { page.list };
        EXPECT(reader.Read(lists, menu));
        EXPECT(menu.page.scrolls);
    }
}

int main()
{
    FindsTheListVtable();
    ReadsAPage();
    ScaledPanel();
    CoveredPagesAreNotOnScreen();
    NewestPageWins();
    NotAPage();
    ItemsOffTheList();
    ScrollingList();
    printf("%d of %d checks passed\n", g_checks - g_failures, g_checks);
    return g_failures == 0 ? 0 : 1;
}
