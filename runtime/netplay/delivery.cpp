#include "netplay/delivery.h"
#include "netplay/clock.h"
#include <cstdlib>
#include <string_view>

namespace netplay
{
    std::atomic<bool> g_flipping{ false };
    Deliveries g_vblankDeliveries(60, 1);
    Deliveries g_audioDeliveries(48000, 256); // one audio period is 256 samples at 48 kHz

    bool FrameDelivery()
    {
        static const bool frame = [] {
            const char* value = std::getenv("SKY_NETPLAY_DELIVERY"); // NOLINT(concurrency-mt-unsafe)
            return Enabled() && value && std::string_view(value) == "frame";
        }();
        return frame;
    }

    void Deliveries::Start()
    {
        {
            std::lock_guard lock(m_mutex);
            m_started = true;
            m_taken = m_done = m_granted;
        }
        m_doneSignal.notify_all();
    }

    uint64_t Deliveries::GrantSteps(uint64_t steps)
    {
        std::lock_guard lock(m_mutex);
        m_remainder += steps * m_numerator;
        const uint64_t unit = 60 * m_divisor;
        m_granted += m_remainder / unit;
        m_remainder %= unit;
        return m_granted;
    }

    uint64_t Deliveries::Grant(uint64_t count)
    {
        std::lock_guard lock(m_mutex);
        m_granted += count;
        return m_granted;
    }

    bool Deliveries::TryTake()
    {
        std::lock_guard lock(m_mutex);
        if (m_taken == m_granted)
            return false;
        ++m_taken;
        return true;
    }

    void Deliveries::Done()
    {
        {
            std::lock_guard lock(m_mutex);
            ++m_done;
        }
        m_doneSignal.notify_all();
    }

    bool Deliveries::Complete(uint64_t granted)
    {
        std::lock_guard lock(m_mutex);
        return !m_started || m_done >= granted;
    }

    bool Deliveries::WaitDone(uint64_t granted, std::chrono::nanoseconds timeout)
    {
        std::unique_lock lock(m_mutex);
        return m_doneSignal.wait_for(lock, timeout, [&] { return !m_started || m_done >= granted; });
    }
}
