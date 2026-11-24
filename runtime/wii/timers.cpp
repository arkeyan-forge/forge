// The Wii's timers on the host (docs/runtime/wii/threads.md): the decrementer (the operating system's alarms set it
// with PPCMtdec; when it runs out, its exception handler fires the alarms due and sets it again) and the video
// interface's retrace interrupt, 59.94 times a second (__VIRetraceHandler: the retrace callbacks, the threads waiting
// in VIWaitForRetrace). One host thread waits for the next of the two and posts it as an event for the processor.
#include "wii/boot_info.h"
#include "wii/os.h"
#include "ppc_recomp_shared.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>

namespace wii::os
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        constexpr uint32_t DisplayInterrupt0 = 0xCC002030; // VI's first display interrupt: the retrace
        constexpr uint32_t DecrementerException = 8;
        constexpr uint32_t RetraceInterrupt = 24; // the processor interface's VI interrupt
        constexpr auto RetracePeriod = std::chrono::nanoseconds(1000000000000ll / 59940);

        struct Timers
        {
            std::mutex mutex;
            std::condition_variable changed;
            std::optional<Clock::time_point> decrementer;
            std::optional<Clock::time_point> retrace;
            std::atomic<bool> retracePending{ false };
            uint8_t* base = nullptr;
            bool started = false;
        };

        Timers& TheTimers()
        {
            static Timers& timers = *new Timers; // its thread never ends
            return timers;
        }

        void RunTimers()
        {
            Timers& timers = TheTimers();
            std::unique_lock lock(timers.mutex);
            for (;;)
            {
                std::optional<Clock::time_point> next = timers.decrementer;
                if (timers.retrace && (!next || *timers.retrace < *next))
                    next = timers.retrace;
                if (!next)
                {
                    timers.changed.wait(lock);
                    continue;
                }
                if (timers.changed.wait_until(lock, *next) == std::cv_status::no_timeout)
                    continue; // changed meanwhile: look again
                const Clock::time_point now = Clock::now();
                uint8_t* base = timers.base;
                if (timers.decrementer && *timers.decrementer <= now)
                {
                    timers.decrementer.reset(); // the handler sets it again
                    PostEvent([base] {
                        CallGuest(base, g_game.decrementerHandler, DecrementerException, InterruptedContext);
                    });
                }
                if (timers.retrace && *timers.retrace <= now)
                {
                    *timers.retrace += RetracePeriod;
                    if (*timers.retrace < now)
                        *timers.retrace = now + RetracePeriod; // far behind: no burst of retraces
                    if (!timers.retracePending.exchange(true))
                    {
                        PostEvent([base] {
                            TheTimers().retracePending = false;
                            // The interrupt's status bit, which the handler checks and clears.
                            base[DisplayInterrupt0] |= 0x80;
                            CallGuest(base, g_game.retraceHandler, RetraceInterrupt, InterruptedContext);
                        });
                    }
                }
            }
        }

        void Start(uint8_t* base)
        {
            Timers& timers = TheTimers();
            if (timers.started)
                return;
            timers.started = true;
            timers.base = base;
            std::thread(RunTimers).detach();
        }
    } // namespace

    void SetDecrementer(PPCContext& ctx, uint8_t* base)
    {
        // The decrementer counts down at the timebase's rate and interrupts past zero.
        const int32_t ticks = ctx.r3.s32;
        const auto wait = std::chrono::nanoseconds(ticks <= 0 ? 0 : int64_t(ticks) * 1000000000 / TimeBaseClock);
        Timers& timers = TheTimers();
        {
            std::lock_guard lock(timers.mutex);
            Start(base);
            timers.decrementer = Clock::now() + wait;
        }
        timers.changed.notify_all();
    }

    void StartRetrace(uint8_t* base)
    {
        Timers& timers = TheTimers();
        {
            std::lock_guard lock(timers.mutex);
            Start(base);
            if (!timers.retrace)
                timers.retrace = Clock::now() + RetracePeriod;
        }
        timers.changed.notify_all();
    }
}
