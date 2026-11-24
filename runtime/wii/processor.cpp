#include "wii/processor.h"
#include "platform/platform.h"

namespace wii
{
    struct Processor::Thread
    {};

    namespace
    {
        thread_local Processor::Thread* t_self = nullptr;
    }

    Processor::Processor() = default;
    Processor::~Processor() = default;

    Processor::Thread* Processor::Start()
    {
        std::lock_guard lock(m_mutex);
        m_threads.push_back(std::make_unique<Thread>());
        t_self = m_threads.back().get();
        m_holder = t_self;
        return t_self;
    }

    Processor::Thread* Processor::Current() const { return t_self; }

    Processor::Thread* Processor::Spawn(size_t stackSize, std::function<void()> body)
    {
        Thread* thread;
        {
            std::lock_guard lock(m_mutex);
            m_threads.push_back(std::make_unique<Thread>());
            thread = m_threads.back().get();
        }
        PlatformStartThread(stackSize, [this, thread, body = std::move(body)] {
            t_self = thread;
            {
                std::unique_lock lock(m_mutex);
                WaitTurn(thread, lock);
            }
            body();
        });
        return thread;
    }

    void Processor::HandTo(Thread* target)
    {
        Thread* self = t_self;
        std::unique_lock lock(m_mutex);
        m_holder = target;
        m_changed.notify_all();
        WaitTurn(self, lock);
    }

    void Processor::WaitTurn(Thread* self, std::unique_lock<std::mutex>& lock)
    {
        m_changed.wait(lock, [&] { return m_holder == self; });
    }

    void Processor::Post(Event event)
    {
        {
            std::lock_guard lock(m_mutex);
            m_events.push_back(std::move(event));
        }
        m_changed.notify_all();
    }

    bool Processor::RunPending()
    {
        bool ran = false;
        for (;;)
        {
            Event event;
            {
                std::lock_guard lock(m_mutex);
                if (m_events.empty())
                    return ran;
                event = std::move(m_events.front());
                m_events.pop_front();
            }
            event(); // unlocked: an event may post another, or hand the processor on
            ran = true;
        }
    }

    bool Processor::WaitForEvent(std::chrono::milliseconds timeout)
    {
        {
            std::unique_lock lock(m_mutex);
            if (!m_changed.wait_for(lock, timeout, [&] { return !m_events.empty(); }))
                return false;
        }
        return RunPending();
    }
}
