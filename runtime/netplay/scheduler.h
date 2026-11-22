#pragma once

#include "netplay/clock.h"
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>

namespace netplay
{
    // Calls a guest poll loop makes without blocking (clock reads, uncontended critical sections) before it yields.
    inline constexpr uint32_t PollQuantum = 1024;
    extern const bool g_cooperative;
    inline bool Cooperative() { return g_cooperative; }

    class Scheduler
    {
    public:
        using Id = uint32_t;
        static constexpr Id None = 0;
        explicit Scheduler(Clock& clock, bool enabled = true);
        ~Scheduler();
        Id Register(bool runnable = true);
        void Enter(Id id);
        void Resume(Id id);
        void Signal();
        void Yield();
        void SpinHint() { Yield(); }
        void Block(std::function<bool()> ready, uint64_t deadline = UINT64_MAX);
        void Exit();
        void TimeBaseRead() { Poll(); }
        void Poll();
        void Diagnose(const std::function<void()>& location, const std::function<void()>& capture);
        bool Active() const;
        void NetworkWait(bool waiting);
        // Times a waiting context woke while another held the baton (the test of the handover's wake-up).
        uint64_t IdleWakes() const { return m_idleWakes.load(std::memory_order_relaxed); }

    private:
        struct Context;
        void Dispatch(Id yielding = None);
        void Wake();
        void Queue(Id id);
        void Await(std::unique_lock<std::mutex>& lock, Id id);
        Clock& m_clock;
        const bool m_enabled;
        std::mutex m_mutex;
        std::condition_variable m_changed;
        std::vector<std::unique_ptr<Context>> m_contexts;
        Id m_holder = None, m_head = None, m_tail = None;
        std::chrono::steady_clock::time_point m_handover;
        bool m_networkWait = false;
        std::atomic<uint64_t> m_idleWakes{ 0 };
        std::jthread m_watchdog;
        static thread_local Scheduler* t_scheduler;
        static thread_local Id t_context;
    };

    Scheduler& GuestScheduler();
}
