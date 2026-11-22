#include "netplay/scheduler.h"
#include "netplay/events.h"
#include "netplay/session_runtime.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace netplay
{
    const bool g_cooperative = [] {
        const char* schedule = std::getenv("SKY_NETPLAY_SCHEDULE"); // NOLINT(concurrency-mt-unsafe)
        if (!schedule || std::string_view(schedule) != "cooperative")
            return false;
        const char* deterministic = std::getenv("SKY_NETPLAY_DETERMINISTIC"); // NOLINT(concurrency-mt-unsafe)
        const char* delivery = std::getenv("SKY_NETPLAY_DELIVERY"); // NOLINT(concurrency-mt-unsafe)
        if (!deterministic || std::string_view(deterministic) != "1" || !delivery ||
            std::string_view(delivery) != "frame")
        {
            if (Online())
                return false;
            fprintf(stderr, "[netplay] refused: cooperative scheduling requires deterministic=1 and delivery=frame\n");
            std::_Exit(1);
        }
        return true;
    }();

    struct Scheduler::Context
    {
        enum class State
        {
            Suspended,
            Runnable,
            Running,
            Blocked,
            Exited
        };
        State state = State::Suspended;
        Id next = None;
        uint64_t deadline = UINT64_MAX;
        uint32_t reads = 0;
        std::function<bool()> ready;
        std::function<void()> location, capture;
    };

    thread_local Scheduler* Scheduler::t_scheduler = nullptr;
    thread_local Scheduler::Id Scheduler::t_context = None;

    Scheduler::Scheduler(Clock& clock, bool enabled)
        : m_clock(clock), m_enabled(enabled), m_handover(std::chrono::steady_clock::now())
    {
        if (enabled)
            m_watchdog = std::jthread([this](const std::stop_token& stop) {
                std::unique_lock lock(m_mutex);
                while (!stop.stop_requested())
                {
                    m_changed.wait_for(lock, std::chrono::milliseconds(100));
                    if (m_networkWait || m_contexts.empty() ||
                        std::chrono::steady_clock::now() - m_handover < std::chrono::seconds(2))
                        continue;
                    fprintf(stderr, "[netplay] scheduler watchdog: holder %u, no handover for 2 s\n", m_holder);
                    if (m_holder != None && m_contexts[m_holder - 1]->location)
                        m_contexts[m_holder - 1]->location();
                    constexpr const char* States[] = { "suspended", "runnable", "running", "blocked", "exited" };
                    for (size_t i = 0; i < m_contexts.size(); ++i)
                        fprintf(stderr, "[netplay] context %zu: %s deadline %llu\n", i + 1,
                            States[unsigned(m_contexts[i]->state)],
                            static_cast<unsigned long long>(m_contexts[i]->deadline));
                    m_handover = std::chrono::steady_clock::now();
                }
            });
    }

    Scheduler::~Scheduler()
    {
        m_watchdog.request_stop();
        m_changed.notify_all();
    }

    void Scheduler::NetworkWait(bool waiting)
    {
        std::lock_guard lock(m_mutex);
        // The network deliberately holds the guest baton; a missing peer is diagnosed by the session timeout.
        m_networkWait = waiting;
        m_handover = std::chrono::steady_clock::now();
    }

    bool Scheduler::Active() const { return m_enabled && t_scheduler == this; }

    void Scheduler::Queue(Id id)
    {
        auto& context = *m_contexts[id - 1];
        context.state = Context::State::Runnable;
        context.next = None;
        if (m_tail != None)
            m_contexts[m_tail - 1]->next = id;
        else
            m_head = id;
        m_tail = id;
    }

    Scheduler::Id Scheduler::Register(bool runnable)
    {
        if (!m_enabled)
            return None;
        std::lock_guard lock(m_mutex);
        m_contexts.push_back(std::make_unique<Context>());
        const Id id = Id(m_contexts.size());
        if (runnable)
            Queue(id);
        if (m_holder == None)
            Dispatch();
        return id;
    }

    void Scheduler::Wake()
    {
        // Reserve signals under the signaller's baton; resetting an event cannot undo a completed wait.
        for (Id id = 1; id <= m_contexts.size(); ++id)
        {
            auto& context = *m_contexts[id - 1];
            if (context.state != Context::State::Blocked)
                continue;
            if ((context.ready && context.ready()) || context.deadline <= m_clock.Elapsed())
            {
                context.ready = {};
                context.deadline = UINT64_MAX;
                Queue(id);
            }
        }
    }

    void Scheduler::Signal()
    {
        if (!Active())
            return;
        std::lock_guard lock(m_mutex);
        Wake();
    }

    void Scheduler::Dispatch(Id yielding)
    {
        const Id previousHolder = m_holder;
        Wake();
        if (yielding != None)
            Queue(yielding);
        if (m_head == None)
        {
            uint64_t deadline = UINT64_MAX;
            for (const auto& context : m_contexts)
                if (context->state == Context::State::Blocked)
                    deadline = std::min(deadline, context->deadline);
            if (deadline != UINT64_MAX)
            {
                if (g_events.load(std::memory_order_acquire))
                {
                    const uint64_t now = m_clock.Elapsed();
                    NoteEvent("idle", std::to_string(deadline > now ? deadline - now : 0));
                }
                m_clock.AdvanceTo(deadline);
                Wake();
            }
        }
        m_holder = m_head;
        if (m_holder != None)
        {
            auto& context = *m_contexts[m_holder - 1];
            m_head = context.next;
            if (m_head == None)
                m_tail = None;
            context.state = Context::State::Running;
            context.reads = 0;
        }
        if (previousHolder != None || m_holder != None)
            m_handover = std::chrono::steady_clock::now();
        m_changed.notify_all();
    }

    void Scheduler::Await(std::unique_lock<std::mutex>& lock, Id id)
    {
        // Only an idle scheduler polls host completion. No running guest can race with these predicates.
        while (m_holder != id)
        {
            if (m_holder == None)
                Dispatch();
            if (m_holder != id)
                m_changed.wait_for(lock, std::chrono::milliseconds(1));
        }
    }

    void Scheduler::Enter(Id id)
    {
        if (!m_enabled)
            return;
        std::unique_lock lock(m_mutex);
        t_scheduler = this;
        t_context = id;
        Await(lock, id);
    }

    void Scheduler::Resume(Id id)
    {
        if (!m_enabled || id == None)
            return;
        std::lock_guard lock(m_mutex);
        if (m_contexts[id - 1]->state == Context::State::Suspended)
            Queue(id);
        if (m_holder == None)
            Dispatch();
    }

    void Scheduler::Yield()
    {
        if (!Active())
            return;
        std::unique_lock lock(m_mutex);
        auto& context = *m_contexts[t_context - 1];
        if (context.capture)
            context.capture();
        // Signals join ahead of the yielding signaller, behind contexts already runnable.
        context.state = Context::State::Blocked;
        context.deadline = UINT64_MAX;
        Dispatch(t_context);
        Await(lock, t_context);
    }

    void Scheduler::Block(std::function<bool()> ready, uint64_t deadline)
    {
        if (!Active())
            return;
        std::unique_lock lock(m_mutex);
        auto& context = *m_contexts[t_context - 1];
        if (context.capture)
            context.capture();
        context.state = Context::State::Blocked;
        context.ready = std::move(ready);
        context.deadline = deadline;
        Dispatch();
        Await(lock, t_context);
    }

    void Scheduler::Exit()
    {
        if (!Active())
            return;
        std::lock_guard lock(m_mutex);
        m_contexts[t_context - 1]->state = Context::State::Exited;
        Dispatch();
        t_context = None;
        t_scheduler = nullptr;
    }

    void Scheduler::Poll()
    {
        // The budget is reset only at a handover: a call that returns at once is part of the loop that polls.
        if (Active() && ++m_contexts[t_context - 1]->reads >= PollQuantum)
            Yield();
    }

    void Scheduler::Diagnose(const std::function<void()>& location, const std::function<void()>& capture)
    {
        if (!Active())
            return;
        std::lock_guard lock(m_mutex);
        auto& context = *m_contexts[t_context - 1];
        context.location = location;
        context.capture = capture;
        capture();
    }

    Scheduler& GuestScheduler()
    {
        static Scheduler scheduler(g_clock, Cooperative());
        return scheduler;
    }
}
