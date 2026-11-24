#include "wii/os.h"
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

        void Interrupts(PPCContext& ctx, bool on) { ctx.r3.u64 = g_interrupts.exchange(on) ? 1 : 0; }
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
        fprintf(stderr, "[os] panic in %s line %u: %s\n", GuestString(base, ctx.r3.u32).c_str(), ctx.r4.u32,
            Message(ctx, base, ctx.r5.u32, 6).c_str());
        StopBoot(1);
    }

    void Fatal(PPCContext& ctx, uint8_t* base)
    {
        fprintf(stderr, "[os] fatal: %s\n", GuestString(base, ctx.r5.u32).c_str());
        StopBoot(1);
    }

    void DisableInterrupts(PPCContext& ctx) { Interrupts(ctx, false); }
    void EnableInterrupts(PPCContext& ctx) { Interrupts(ctx, true); }
    void RestoreInterrupts(PPCContext& ctx) { Interrupts(ctx, ctx.r3.u32 != 0); }

    void Skip(PPCContext& ctx, const char* function)
    {
        static std::mutex mutex;
        static std::set<std::string> logged;
        {
            std::lock_guard lock(mutex);
            if (logged.insert(function).second)
                fprintf(stderr, "[os] %s: skipped (the host answers this hardware later)\n", function);
        }
        ctx.r3.u64 = 0;
    }

    void OpenDevice(PPCContext& ctx, uint8_t* base)
    {
        fprintf(stderr, "[ios] open \"%s\" (mode %u): the first request to IOS, where the boot stops for now\n",
            GuestString(base, ctx.r3.u32).c_str(), ctx.r4.u32);
        StopBoot(0);
    }
}
