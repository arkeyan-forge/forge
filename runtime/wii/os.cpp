#include "wii/os.h"
#include "wii/gx_fifo.h"
#include "kernel/guest_format.h"
#include "ppc_context.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

namespace wii::os
{
    namespace
    {
        std::atomic<bool> g_interrupts{ false }; // off until OSInit turns them on, as on the console

        std::string GuestString(const uint8_t* base, uint32_t address)
        {
            std::string text;
            for (char c; address != 0 && text.size() < 4096 && (c = char(base[address])) != 0; address++)
                text += c;
            return text;
        }

        // A message with its arguments: words from r(first) to r10, doubles from f1 to f8 (the registers only: the
        // SDK's messages have at most a handful).
        std::string Message(PPCContext& ctx, const uint8_t* base, uint32_t format, int first)
        {
            const uint32_t words[] = { ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32,
                ctx.r9.u32, ctx.r10.u32 };
            const double floats[] = { ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64, ctx.f5.f64, ctx.f6.f64,
                ctx.f7.f64, ctx.f8.f64 };
            int word = first - 3, real = 0;
            std::string text = FormatGuest(GuestString(base, format).c_str(), [&](bool floating) -> uint64_t {
                if (floating)
                {
                    uint64_t bits = 0;
                    if (real < 8)
                        memcpy(&bits, &floats[real++], sizeof(bits));
                    return bits;
                }
                return word < 8 ? words[word++] : 0;
            });
            while (!text.empty() && text.back() == '\n')
                text.pop_back();
            return text;
        }

        constexpr uint32_t CurrentThread = 0x800000E4; // the operating system's, in low memory

        uint32_t Read32(const uint8_t* base, uint32_t address)
        {
            const uint8_t* p = base + address;
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        // Interrupts turned on. With a thread running, the events posted meanwhile run now. With none (the scheduler
        // idles, its current thread cleared, and spins until a priority is ready), the processor waits for events
        // until one makes a thread ready.
        void SafePoint(PPCContext& ctx, uint8_t* base)
        {
            if (Read32(base, CurrentThread) != 0)
            {
                RunPendingEvents(ctx, base);
                return;
            }
            while (Read32(base, g_game.runQueueBits) == 0)
            {
                if (!WaitForEvent(ctx))
                {
                    fprintf(stderr, "[os] every thread waits, and nothing has come for 10 s: the boot stops\n");
                    LogThreads(base);
                    StopBoot(1);
                }
            }
        }

        void Interrupts(PPCContext& ctx, uint8_t* base, bool on)
        {
            const bool was = g_interrupts.exchange(on);
            ctx.r3.u64 = was ? 1 : 0;
            if (on && !was && base != nullptr && !InHandler())
                SafePoint(ctx, base);
        }
    } // namespace

    void Report(PPCContext& ctx, uint8_t* base)
    {
        // One log line for each of the message's lines (the SDK also splits a line over several calls).
        const std::string text = Message(ctx, base, ctx.r3.u32, 4);
        for (size_t start = 0; start <= text.size();)
        {
            size_t end = text.find('\n', start);
            if (end == std::string::npos)
                end = text.size();
            if (end > start)
                fprintf(stderr, "[os] %.*s\n", int(end - start), text.data() + start);
            start = end + 1;
        }
    }

    void Panic(PPCContext& ctx, uint8_t* base)
    {
        fprintf(stderr, "[os] panic in %s line %u (called from 0x%08X): %s\n", GuestString(base, ctx.r3.u32).c_str(),
            ctx.r4.u32, uint32_t(ctx.lr), Message(ctx, base, ctx.r5.u32, 6).c_str());
        StopBoot(1);
    }

    void Fatal(PPCContext& ctx, uint8_t* base)
    {
        fprintf(stderr, "[os] fatal: %s\n", GuestString(base, ctx.r5.u32).c_str());
        StopBoot(1);
    }

    bool InterruptsEnabled() { return g_interrupts; }
    void SetInterrupts(bool on) { g_interrupts = on; }

    void Poll(PPCContext& ctx, uint8_t* base)
    {
        if (g_interrupts && !InHandler())
            PollEvents(ctx, base);
    }

    void DisableInterrupts(PPCContext& ctx) { Interrupts(ctx, nullptr, false); }
    void EnableInterrupts(PPCContext& ctx, uint8_t* base) { Interrupts(ctx, base, true); }
    void RestoreInterrupts(PPCContext& ctx, uint8_t* base) { Interrupts(ctx, base, ctx.r3.u32 != 0); }

    void Skip(PPCContext& ctx, const char* function, uint32_t result)
    {
        static std::mutex mutex;
        static std::set<std::string> logged;
        {
            std::lock_guard lock(mutex);
            if (logged.insert(function).second)
                fprintf(stderr, "[os] %s: skipped (the host answers this hardware later)\n", function);
        }
        ctx.r3.u64 = result;
    }

    void DrawSyncReached(uint8_t* base, uint16_t token)
    {
        constexpr uint32_t PixelEngineToken = 0xCC00100E;
        base[PixelEngineToken] = uint8_t(token >> 8);
        base[PixelEngineToken + 1] = uint8_t(token);
    }

    void FrameBuffer(PPCContext& ctx)
    {
        static int count = 0;
        gx::CommandProcessor& gp = gx::Processor();
        if (++count <= 3 || count == 60 || count == 600)
        {
            const gx::Stats& stats = gp.Counters();
            fprintf(stderr,
                "[vi] frame buffer %d at 0x%08X; GX since the last: %u lines, %u commands, %u draws (%u vertices), "
                "%u lists (%u bad), %u copies, %u unknown\n",
                count, ctx.r3.u32, stats.lines, stats.commands, stats.draws, stats.vertices, stats.lists,
                stats.badLists, stats.copies, stats.unknown);
        }
        gp.ResetCounters();
    }
}
