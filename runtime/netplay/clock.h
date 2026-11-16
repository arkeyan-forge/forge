#pragma once

#include <atomic>
#include <cstdint>

namespace netplay
{
    inline constexpr uint64_t TimeBaseFrequency = 49875000;
    inline constexpr uint64_t EpochFileTime = 134116992000000000;

    extern const bool g_enabled;
    inline bool Enabled() { return g_enabled; }

    struct ReadCursor
    {
        uint64_t frame = UINT64_MAX;
        uint64_t reads = 0;
        uint64_t last = 0;
        uint64_t Read(uint64_t steps);
    };

    class Clock
    {
    public:
        void Advance(uint32_t presentInterval);
        uint64_t Steps() const { return m_steps.load(std::memory_order_acquire); }
        uint64_t Elapsed() const;
        uint64_t SystemTime() const { return EpochFileTime + Elapsed(); }

    private:
        std::atomic<uint64_t> m_steps{ 0 };
    };

    extern Clock g_clock;
    uint64_t ReadTimeBase();
    void OnFrame(uint32_t presentInterval);
    void Finish();
    void SetTimeStampBundle(uint32_t address);
}
