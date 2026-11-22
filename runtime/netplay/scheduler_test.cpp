#include "netplay/scheduler.h"
#include "netplay/delivery.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            // At once: the other contexts may be waiting for a baton this thread will never pass.
            std::_Exit(1);
        }
    }

    std::vector<unsigned> Replay()
    {
        netplay::Clock clock;
        netplay::Scheduler scheduler(clock);
        std::array<netplay::Scheduler::Id, 4> ids{};
        for (auto& id : ids)
            id = scheduler.Register();
        Check(ids[0] != netplay::Scheduler::None, "contexts must register before their host threads start");
        std::vector<unsigned> order;
        bool signaled = false;
        std::array<std::thread, 4> threads;
        // Host arrival order must not choose the initial holder.
        for (unsigned i = 4; i-- > 0;)
            threads[i] = std::thread([&, i] {
                scheduler.Enter(ids[i]);
                order.push_back(i);
                if (i == 0)
                    scheduler.Block([&] { return signaled; });
                else if (i == 1)
                    scheduler.Block({}, 900);
                else if (i == 2)
                {
                    signaled = true;
                    scheduler.Signal();
                    signaled = false;
                    scheduler.SpinHint();
                }
                else
                    for (unsigned read = 0; read < netplay::PollQuantum; ++read)
                        scheduler.TimeBaseRead();
                order.push_back(10 + i);
                if (i == 1)
                    Check(clock.Elapsed() == 900, "timeout advances virtual time exactly to its deadline");
                scheduler.Exit();
            });
        for (auto& thread : threads)
            thread.join();
        return order;
    }

    void ResumeAndTimeouts()
    {
        netplay::Clock clock;
        netplay::Scheduler scheduler(clock);
        const auto parent = scheduler.Register();
        const auto child = scheduler.Register(false);
        const auto worker = scheduler.Register();
        std::vector<unsigned> order;
        std::thread second([&] {
            scheduler.Enter(child);
            order.push_back(2);
            scheduler.Block({}, 40);
            order.push_back(4);
            Check(clock.Elapsed() == 40, "equal deadlines use the virtual clock");
            scheduler.Exit();
        });
        std::thread third([&] {
            scheduler.Enter(worker);
            order.push_back(1);
            scheduler.Block({}, 40);
            order.push_back(3);
            scheduler.Exit();
        });
        scheduler.Enter(parent);
        scheduler.Resume(child);
        scheduler.SpinHint();
        Check(clock.Elapsed() == 0, "runnable parent prevents time from advancing");
        order.push_back(0);
        scheduler.Exit();
        second.join();
        third.join();
        Check(order == std::vector<unsigned>({ 1, 2, 0, 4, 3 }), "resume FIFO and equal deadlines in creation order");
    }

    void PollBudget()
    {
        netplay::Clock clock;
        netplay::Scheduler scheduler(clock);
        const auto poller = scheduler.Register();
        const auto worker = scheduler.Register();
        bool ran = false;
        std::thread thread([&] {
            scheduler.Enter(worker);
            ran = true;
            scheduler.Exit();
        });
        scheduler.Enter(poller);
        // A poll loop may mix clock reads with calls that never block (an uncontended critical section): both count.
        for (unsigned poll = 1; poll < netplay::PollQuantum; ++poll)
            poll % 2 ? scheduler.Poll() : scheduler.TimeBaseRead();
        Check(!ran, "polls short of a quantum keep the baton");
        scheduler.Poll();
        Check(ran, "a complete poll quantum hands over to a runnable competitor");
        scheduler.Exit();
        thread.join();
    }

    std::vector<unsigned> FrameCallbacks()
    {
        netplay::Clock clock;
        netplay::Scheduler scheduler(clock);
        netplay::Deliveries blanks(60, 1), audio(48000, 256);
        blanks.Start();
        audio.Start();
        const auto producer = scheduler.Register();
        const auto interrupt = scheduler.Register();
        const auto driver = scheduler.Register();
        std::vector<unsigned> callbacks;
        bool finished = false;
        auto run = [&](netplay::Scheduler::Id id, netplay::Deliveries& deliveries, unsigned tag) {
            scheduler.Enter(id);
            while (true)
            {
                scheduler.Block([&] { return finished || deliveries.TryTake(); });
                if (finished)
                    break;
                callbacks.push_back(tag);
                scheduler.SpinHint();
                deliveries.Done();
            }
            scheduler.Exit();
        };
        std::thread a([&] { run(interrupt, blanks, 1); });
        std::thread b([&] { run(driver, audio, 2); });
        scheduler.Enter(producer);
        for (unsigned frame = 0; frame < 8; ++frame)
        {
            const auto blankTarget = blanks.Grant(1);
            const auto audioTarget = audio.GrantSteps(1);
            scheduler.Block([&] { return blanks.Complete(blankTarget) && audio.Complete(audioTarget); });
            Check(callbacks.size() == frame + 1 + (frame + 1) * 25 / 8, "flip waits for every callback to return");
        }
        finished = true;
        scheduler.Exit();
        a.join();
        b.join();
        return callbacks;
    }

    // A handover wakes the new holder only. Waking every waiting context at each handover made SSA's forty threads
    // queue on the scheduler's mutex: online, it ran at 19 FPS with its threads mostly asleep.
    void WakesOnlyTheHolder()
    {
        netplay::Clock clock;
        netplay::Scheduler scheduler(clock);
        constexpr int Rounds = 2000;
        const auto first = scheduler.Register();
        const auto second = scheduler.Register();
        std::array<netplay::Scheduler::Id, 16> parked{};
        for (auto& id : parked)
            id = scheduler.Register();
        std::atomic<bool> finished{ false };
        std::vector<std::thread> threads;
        threads.reserve(parked.size() + 1);
        for (const auto id : parked)
            threads.emplace_back([&, id] {
                scheduler.Enter(id);
                scheduler.Block([&] { return finished.load(); });
                scheduler.Exit();
            });
        threads.emplace_back([&] {
            scheduler.Enter(second);
            for (int i = 0; i < Rounds; ++i)
                scheduler.Yield();
            scheduler.Exit();
        });
        scheduler.Enter(first);
        for (int i = 0; i < Rounds; ++i)
            scheduler.Yield();
        const uint64_t idle = scheduler.IdleWakes();
        finished = true;
        scheduler.Signal();
        scheduler.Exit();
        for (auto& thread : threads)
            thread.join();
        Check(idle < 100, "a handover wakes the new holder only, not every waiting context");
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "--mode")
    {
        printf("cooperative=%d\n", netplay::Cooperative());
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--watchdog")
    {
        netplay::Clock clock;
        netplay::Scheduler scheduler(clock);
        const auto holder = scheduler.Register();
        scheduler.Register(false);
        scheduler.Enter(holder);
        std::atomic<bool> reported{ false };
        scheduler.Diagnose([&] { reported.store(true); }, [] {});
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!reported.load() && std::chrono::steady_clock::now() < until)
            std::this_thread::yield();
        Check(reported.load(), "watchdog reports a holder with no handover");
        scheduler.Exit();
        return 0;
    }
    WakesOnlyTheHolder();
    const auto expected = Replay();
    const auto frameOrder = FrameCallbacks();
    Check(expected == std::vector<unsigned>({ 0, 1, 2, 3, 10, 12, 13, 11 }), "FIFO signals and spin quantum");
    for (unsigned run = 0; run < 200; ++run)
    {
        if (run != 0)
            Check(Replay() == expected, "200 replays give the same switch order");
        ResumeAndTimeouts();
        PollBudget();
        Check(FrameCallbacks() == frameOrder, "frame callback switch order repeats in 200 runs");
    }
    netplay::Clock clock;
    netplay::Scheduler disabled(clock, false);
    Check(disabled.Register() == netplay::Scheduler::None, "disabled registration is a pass-through");
    disabled.Enter(0);
    disabled.Block(
        [] {
            Check(false, "disabled mode must not evaluate predicates");
            return true;
        },
        50);
    disabled.Yield();
    disabled.Exit();
    Check(clock.Elapsed() == 0 && !disabled.Active(), "disabled mode never advances the clock or takes a baton");
    puts("scheduler_test: PASS (200 replays)");
}
