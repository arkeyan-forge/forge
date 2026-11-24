#pragma once

// The Wii's one processor, shared by the game's threads: each guest thread runs on its own host thread, and only the
// one holding the processor runs guest code; the others wait in a hand-over. The operating system's own scheduler
// (its priorities, its queues, in guest memory) decides who runs next: the host only carries out its context switches
// (docs/runtime/wii/threads.md). Events (an IOS reply, an alarm), posted from any host thread, run on the holder at the
// guest's safe points, as interrupts would.
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace wii
{
    class Processor
    {
    public:
        struct Thread; // a host thread's place: who it runs as is the caller's business (an OSContext's address)
        using Event = std::function<void()>;

        Processor();
        ~Processor();
        Processor(const Processor&) = delete;
        Processor& operator=(const Processor&) = delete;

        // The calling host thread takes the processor (the first guest thread, at boot).
        Thread* Start();
        // The calling host thread's place, nullptr for a thread the processor does not know.
        Thread* Current() const;
        // A new host thread (with a `stackSize` stack) that runs `body` once the processor is handed to it.
        Thread* Spawn(size_t stackSize, std::function<void()> body);
        // The holder hands the processor to `target` and waits until it is handed back.
        void HandTo(Thread* target);

        // Queues an event for the holder; any host thread.
        void Post(Event event);
        // The holder, at a safe point: runs the queued events, in order. False when there was none.
        bool RunPending();
        // The holder, idle (no guest thread ready): waits for an event and runs the queued ones; false after `timeout`
        // with none (every thread waits on something nothing will send).
        bool WaitForEvent(std::chrono::milliseconds timeout);

    private:
        std::mutex m_mutex;
        std::condition_variable m_changed;
        Thread* m_holder = nullptr;
        std::vector<std::unique_ptr<Thread>> m_threads;
        std::deque<Event> m_events;
        void WaitTurn(Thread* self, std::unique_lock<std::mutex>& lock);
    };
}
