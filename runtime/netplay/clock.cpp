#include "netplay/clock.h"
#include "netplay/events.h"
#include "netplay/scheduler.h"
#include <algorithm>
#include <cstdlib>
#include <string_view>

namespace netplay
{
    Clock g_clock;

    const bool g_enabled = [] {
        // The mode is fixed at process launch, before any environment mutation on host threads.
        const char* value = std::getenv("SKY_NETPLAY_DETERMINISTIC"); // NOLINT(concurrency-mt-unsafe)
        return value && std::string_view(value) == "1";
    }();

    void Clock::Advance(uint32_t presentInterval)
    {
        const uint64_t interval = presentInterval == 0 || (presentInterval & 0x80000000u) ? 1 : presentInterval;
        m_steps.fetch_add(interval, std::memory_order_release);
    }

    uint64_t Clock::Elapsed() const
    {
        const uint64_t steps = Steps();
        return (steps / 60) * 10000000 + (steps % 60) * 10000000 / 60 + Idle();
    }

    void Clock::AdvanceTo(uint64_t elapsed)
    {
        const uint64_t now = Elapsed();
        if (elapsed > now)
        {
            m_idle.fetch_add(elapsed - now, std::memory_order_release);
            if (const auto hook = m_advanceHook.load(std::memory_order_acquire))
                hook();
        }
    }

    uint64_t ReadCursor::Read(uint64_t steps)
    {
        if (frame != steps)
        {
            frame = steps;
            reads = 0;
        }
        const uint64_t value = steps * (TimeBaseFrequency / 60) + ++reads;
        last = std::max(value, last + 1);
        return last;
    }

    uint64_t ReadTimeBase()
    {
        if (Cooperative())
            GuestScheduler().TimeBaseRead();
        thread_local ReadCursor cursor;
        CountEvent(Counter::TimeBase);
        return cursor.Read(g_clock.Steps()) + g_clock.Idle() / 10000000 * TimeBaseFrequency +
               g_clock.Idle() % 10000000 * TimeBaseFrequency / 10000000;
    }
}
