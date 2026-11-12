// The virtual portal's swap boundary: no files, device, window or game needed.
#include "host/portal_slots.h"
#include "host/portal_prompt.h"
#include <atomic>
#include <cstdio>
#include <thread>

namespace
{
    int g_checks, g_failures;

    void Check(bool condition, const char* what, int line)
    {
        g_checks++;
        if (!condition)
        {
            g_failures++;
            fprintf(stderr, "FAIL line %d: %s\n", line, what);
        }
    }
#define CHECK(condition) Check((condition), #condition, __LINE__)

    // Read status and the synthetic markers under the same lock as a portal command. A pair must never be torn.
    class Slots final : public PortalSlots
    {
    public:
        bool CompletePair()
        {
            std::lock_guard lock(m_mutex);
            return (StatusBits() & 5) == 5 && m_slots[0].data && m_slots[1].data &&
                   (*m_slots[0].data)[0] == (*m_slots[1].data)[0];
        }
    };

    FigureData Marker(uint8_t marker)
    {
        FigureData data{};
        data[0] = marker;
        return data;
    }

    void SwapChecks()
    {
        PortalSlots slots;
        CHECK(slots.Place(Marker(1), "old", 0) == 0);
        CHECK(slots.TakeStatus() == 3);
        CHECK(slots.Replace({ { 0, Marker(2), "new" } }));
        CHECK(slots.TakeStatus() == 3); // present and changed: the title must reread, without an empty slot
        CHECK(slots.TakeStatus() == 1);
        CHECK(slots.SlotPath(0) == "new");

        // Single figure to swapper, swapper to swapper, then swapper to single figure.
        CHECK(slots.Replace({ { 0, Marker(3), "top" }, { 1, Marker(3), "bottom" } }));
        CHECK(slots.TakeStatus() == 15);
        CHECK(slots.Replace({ { 0, Marker(4), "next-top" }, { 1, Marker(4), "next-bottom" } }));
        CHECK(slots.TakeStatus() == 15);
        CHECK(slots.Replace({ { 0, Marker(5), "single" }, { 1, std::nullopt, {} } }));
        CHECK(slots.TakeStatus() == 11); // slot 0 remains present while the unused bottom leaves

        // Player swap with a swapper: all three tags move together and keep their player slots.
        CHECK(slots.Replace({ { 2, Marker(6), "p2-top" }, { 3, Marker(6), "p2-bottom" } }));
        slots.TakeStatus();
        CHECK(slots.Replace({ { 0, Marker(6), "p2-top" }, { 1, Marker(6), "p2-bottom" }, { 2, Marker(5), "single" },
            { 3, std::nullopt, {} } }));
        CHECK(slots.TakeStatus() == 191);
        CHECK(slots.SlotPath(0) == "p2-top" && slots.SlotPath(1) == "p2-bottom");
        CHECK(slots.SlotPath(2) == "single" && !slots.Holds(3));

        // A full portal still permits a replacement in place.
        for (int i = 3; i < PortalSlots::SlotCount; i++)
            CHECK(slots.Place(Marker(7), {}, i) == i);
        CHECK(slots.Place(Marker(8), {}) == -1);
        CHECK(slots.Replace({ { 0, Marker(8), "full-swap" } }));
        CHECK((slots.TakeStatus() & 0x55555555) == 0x55555555);

        // Reserved (real), invalid and repeated slots reject the whole batch, keeping every old figure.
        slots.SetReserved(1u << 15);
        for (int invalid : { -1, 15, 16, 0 })
        {
            CHECK(!slots.Replace({ { 0, Marker(9), "refused" }, { invalid, Marker(9), "refused" } }));
            CHECK(slots.SlotPath(0) == "full-swap");
            CHECK(slots.TakeStatus() == 0x55555555);
        }
    }

    void ConcurrentChecks()
    {
        Slots slots;
        CHECK(slots.Replace({ { 0, Marker(1), {} }, { 1, Marker(1), {} } }));
        std::atomic<bool> stop{ false }, started{ false }, complete{ true };
        std::thread reader([&]() {
            do
            {
                if (!slots.CompletePair())
                    complete = false;
                started = true;
            } while (!stop.load());
        });
        while (!started.load())
            std::this_thread::yield();
        bool replaced = true;
        for (int i = 0; i < 10000; i++)
        {
            uint8_t marker = uint8_t(i);
            replaced &= slots.Replace({ { 0, Marker(marker), {} }, { 1, Marker(marker), {} } });
        }
        stop = true;
        reader.join();
        CHECK(replaced);
        CHECK(complete.load());
    }

    void RemovalChecks()
    {
        PortalSlots slots;
        portalprompt::Tracker tracker;
        portalprompt::Prompt appeared;
        CHECK(slots.Place(Marker(1), {}, 0) == 0);
        slots.TakeStatus();
        slots.Remove(0);
        CHECK(slots.TakeStatus() == 2); // explicit removal still reports absence
        for (int i = 0; i < 3; i++)
        {
            auto event = tracker.Update({ portalprompt::Kind::Player, 1 }, appeared);
            CHECK(event == (i == 2 ? portalprompt::Tracker::Event::Appeared : portalprompt::Tracker::Event::None));
        }
        CHECK(appeared.player == 1);
    }
}

int main()
{
    SwapChecks();
    ConcurrentChecks();
    RemovalChecks();
    printf("Toy Box swap: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
