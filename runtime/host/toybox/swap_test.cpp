// The virtual portal's swap boundary: no files, device, window or game needed.
#include "host/portal_slots.h"
#include "host/portal_prompt.h"
#include "host/toybox/arrival.h"
#include "host/toybox/retake.h"
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

    // Read status and the synthetic markers under the same lock as a portal command. A pair must never be torn: both
    // halves leave and arrive in the same report.
    class Slots final : public PortalSlots
    {
    public:
        bool CompletePair()
        {
            std::lock_guard lock(m_mutex);
            uint32_t present = StatusBits() & 5;
            if (present == 0)
                return true;
            const auto& top = m_slots[0].data;
            const auto& bottom = m_slots[1].data;
            return present == 5 && top && bottom && top.value()[0] == bottom.value()[0];
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
        // A real toy is lifted before the next one is placed: one report without it, then the arrival.
        CHECK(slots.SlotPath(0) == "new" && slots.Holds(0));
        CHECK(slots.TakeStatus() == 2);
        CHECK(slots.TakeStatus() == 3);
        CHECK(slots.TakeStatus() == 1);
        CHECK(slots.SlotPath(0) == "new");

        // Swapped again before the removal was reported: still one removal, then the last figure.
        CHECK(slots.Replace({ { 0, Marker(9), "first" } }));
        CHECK(slots.Replace({ { 0, Marker(2), "new" } }));
        CHECK(slots.TakeStatus() == 2);
        CHECK(slots.TakeStatus() == 3);
        CHECK(slots.SlotPath(0) == "new");

        // Taken off during the removal: the waiting figure never arrives.
        CHECK(slots.Replace({ { 0, Marker(9), "gone" } }));
        slots.Remove(0);
        CHECK(!slots.Holds(0));
        CHECK(slots.TakeStatus() == 2);
        CHECK(slots.TakeStatus() == 0);
        CHECK(slots.Place(Marker(2), "new", 0) == 0);
        CHECK(slots.TakeStatus() == 3);

        // Single figure to swapper, swapper to swapper, then swapper to single figure.
        CHECK(slots.Replace({ { 0, Marker(3), "top" }, { 1, Marker(3), "bottom" } }));
        CHECK(slots.TakeStatus() == 2); // the top arrives with its bottom, after the old figure left
        CHECK(slots.TakeStatus() == 15);
        CHECK(slots.Replace({ { 0, Marker(4), "next-top" }, { 1, Marker(4), "next-bottom" } }));
        CHECK(slots.TakeStatus() == 10);
        CHECK(slots.TakeStatus() == 15);
        CHECK(slots.Replace({ { 0, Marker(5), "single" }, { 1, std::nullopt, {} } }));
        CHECK(slots.TakeStatus() == 10);
        CHECK(slots.TakeStatus() == 3);

        // Player swap with a swapper: all three tags move together and keep their player slots.
        CHECK(slots.Replace({ { 2, Marker(6), "p2-top" }, { 3, Marker(6), "p2-bottom" } }));
        slots.TakeStatus();
        CHECK(slots.Replace({ { 0, Marker(6), "p2-top" }, { 1, Marker(6), "p2-bottom" }, { 2, Marker(5), "single" },
            { 3, std::nullopt, {} } }));
        CHECK(slots.TakeStatus() == 162); // slot 1 was empty: nothing to report until the arrival
        CHECK(slots.TakeStatus() == 63);
        CHECK(slots.SlotPath(0) == "p2-top" && slots.SlotPath(1) == "p2-bottom");
        CHECK(slots.SlotPath(2) == "single" && !slots.Holds(3));

        // A full portal still permits a replacement in place, and its slot cannot be taken during the removal.
        for (int i = 3; i < PortalSlots::SlotCount; i++)
            CHECK(slots.Place(Marker(7), {}, i) == i);
        CHECK(slots.Place(Marker(8), {}) == -1);
        CHECK(slots.Replace({ { 0, Marker(8), "full-swap" } }));
        CHECK(slots.Place(Marker(8), {}) == -1);
        CHECK((slots.TakeStatus() & 0x55555555) == 0x55555554);
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

    void RetakeChecks()
    {
        using toybox::retake::Due;
        constexpr uint64_t Ms = 1'000'000;
        CHECK(!Due(0, 900 * Ms) && Due(0, 1100 * Ms)); // 1 s after the player's swap
        CHECK(!Due(1, 1900 * Ms) && Due(1, 2100 * Ms)); // then 2 s
        CHECK(!Due(2, 3900 * Ms) && Due(2, 4100 * Ms)); // then 4 s
        CHECK(!Due(3, 60'000 * Ms));                    // then never: the 20 s "not taken yet" toast is left
        // The game asks for the seat its unread figure is on (Swap Force's prompt after a wheel swap): sooner.
        CHECK(!Due(0, 250 * Ms, true) && Due(0, 350 * Ms, true));
        CHECK(!Due(1, 1900 * Ms, true) && Due(1, 2100 * Ms, true)); // later tries keep their wait
        CHECK(!Due(3, 60'000 * Ms, true));

        using toybox::retake::Taken;
        const uint64_t placed = 5'000 * Ms;
        CHECK(!Taken(0, placed));                // only a later block of the previous toy was read since
        CHECK(!Taken(placed - 10 * Ms, placed)); // block 0 read before the swap: the previous toy
        CHECK(Taken(placed + 100 * Ms, placed));
    }

    // The game's prompt for a seat whose figure the Toy Box just put on, not read yet (launcher #97): no Toy Box over it.
    void PromptChecks()
    {
        using toybox::retake::Holds;
        using toybox::retake::PromptGate;
        constexpr uint64_t Ms = 1'000'000;
        CHECK(Holds(true, 0) && Holds(true, 2)); // unread, a re-place still to come
        CHECK(!Holds(true, 3));                  // the last re-place done: the player may need the Toy Box
        CHECK(!Holds(false, 0));                 // taken, or nothing put on

        PromptGate gate;
        CHECK(gate.Appeared(false)); // a free seat, or its figure taken: opens at once
        CHECK(!gate.Update(false, 5'000 * Ms));
        gate.Gone();

        // A wheel swap: P1's new figure unread 0.5 s after, the prompt up. The take answers it.
        CHECK(!gate.Appeared(true));
        CHECK(!gate.Update(true, 600 * Ms));
        CHECK(!gate.Update(false, 700 * Ms)); // taken: the prompt goes away in a few polls
        CHECK(!gate.Update(false, 900 * Ms));
        gate.Gone();
        CHECK(!gate.Update(false, 9'000 * Ms));

        // Taken, yet the game keeps asking: it opens once, a moment after.
        CHECK(!gate.Appeared(true));
        CHECK(!gate.Update(false, 10'100 * Ms));
        CHECK(!gate.Update(false, 10'900 * Ms));
        CHECK(gate.Update(false, 11'200 * Ms));
        CHECK(!gate.Update(false, 12'000 * Ms));
        gate.Gone();

        // Never read: held through the re-places, then it opens.
        CHECK(!gate.Appeared(true));
        CHECK(!gate.Update(true, 26'000 * Ms));
        CHECK(!gate.Update(false, 26'500 * Ms));
        CHECK(gate.Update(false, 27'600 * Ms));
        gate.Gone();
    }

    void HoldChecks()
    {
        using namespace toybox::arrival;
        constexpr uint64_t Ms = 1'000'000;
        CHECK(Locked(true, UINT64_MAX));          // the game brings a figure in
        CHECK(Locked(false, 300 * Ms));           // a figure just put on, not taken yet: its arrival comes next
        CHECK(!Locked(false, 1500 * Ms));         // still not taken after that: the re-place's business
        CHECK(!Locked(false, UINT64_MAX));        // nothing pending
        CHECK(!Release(true, 4900 * Ms) && Release(false, 10 * Ms));
        CHECK(Release(true, 5000 * Ms));          // a game that never says it is done still gets the figure
    }
}

int main()
{
    SwapChecks();
    ConcurrentChecks();
    RemovalChecks();
    RetakeChecks();
    PromptChecks();
    HoldChecks();
    printf("Toy Box swap: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
