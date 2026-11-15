// Checks that PlatformNowNs counts nanoseconds: over a sleep, its duration matches steady_clock's within 2 % (on Apple
// silicon the counter it reads does not always tick at mach_absolute_time's rate: 1 GHz on an M5 Pro).
// usage: ArkeyanClockTest
#include "platform/clock.h"
#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
    const auto steadyStart = std::chrono::steady_clock::now();
    const uint64_t start = PlatformNowNs();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const uint64_t end = PlatformNowNs();
    const auto steadyEnd = std::chrono::steady_clock::now();
    const double steady = std::chrono::duration<double, std::nano>(steadyEnd - steadyStart).count();
    const double measured = double(end - start);
    if (measured < 0.98 * steady || measured > 1.02 * steady)
    {
        fprintf(stderr, "FAIL: PlatformNowNs measured %.0f ns over a sleep steady_clock measured as %.0f ns\n",
            measured, steady);
        return 1;
    }
    printf("ArkeyanClockTest: all passed\n");
    return 0;
}
