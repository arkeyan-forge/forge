#pragma once

#include <chrono>
#include <cstdint>
#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

// Nanoseconds from an arbitrary start, for durations only (the perf log's timers, several a draw on two threads). On
// Apple silicon the counter mach_absolute_time reads (CNTVCT_EL0), read directly: mach_absolute_time puts an
// instruction barrier before it, which drains the pipeline (2 % of SSA's render thread, 4 % of the GPU thread in
// `level_008`'s samples); steady_clock::now costs 20 ns. Its rate is CNTFRQ_EL0, not always mach_timebase_info's
// (an M5 Pro: 1 GHz against 24 MHz ticks).
inline uint64_t PlatformNowNs()
{
#if defined(__APPLE__) && defined(__aarch64__)
    static const double nsPerTick = [] {
        uint64_t frequency;
        asm volatile("mrs %0, CNTFRQ_EL0" : "=r"(frequency));
        return 1e9 / double(frequency);
    }();
    uint64_t ticks;
    asm volatile("mrs %0, CNTVCT_EL0" : "=r"(ticks));
    return uint64_t(double(ticks) * nsPerTick);
#elif defined(__APPLE__)
    static const double nsPerTick = [] {
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        return double(info.numer) / double(info.denom);
    }();
    return uint64_t(double(mach_absolute_time()) * nsPerTick);
#else
    return uint64_t(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
#endif
}
