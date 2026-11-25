// The audio hardware on the host (docs/runtime/wii/audio.md): the audio interface (AI), whose DMA plays a buffer and
// interrupts at its end, and the DSP, which runs AX's task. The AI's interrupt comes from the host's timers every
// buffer (3 ms); the DSP's task runs on the host: its start at once (AX waits for it in a loop), each command list as
// its mails come (ax.cpp), then the task's resume, as the DSP's yield back to AX would make it run.
#include "wii/ax.h"
#include "wii/os.h"
#include "ppc_recomp_shared.h"
#include <chrono>
#include <cstdio>
#include <mutex>
#include <set>

namespace wii::os
{
    namespace
    {
        uint32_t Read32(const uint8_t* base, uint32_t address)
        {
            const uint8_t* p = base + address;
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        // The audio interface: the DMA callback, the buffer set for the next DMA.
        struct AudioInterface
        {
            uint32_t callback = 0;
            uint32_t next = 0, nextLength = 0;
            bool running = false;
        } g_ai;

        // AX's DSP task: its callbacks at 40 (start), 44 (resume) in the SDK's task block; the mail waited for.
        constexpr uint32_t TaskInit = 40, TaskResume = 44;
        constexpr uint32_t CommandListMail = 0xBABE0000; // then the list's address
        struct Dsp
        {
            uint32_t task = 0;
            bool listAddressNext = false;
            uint64_t frames = 0;
            uint32_t playing = ~0u;
            int changesLogged = 0;
        } g_dsp;

        void LogOnce(const char* what, uint32_t value)
        {
            static std::set<uint32_t> logged;
            if (logged.insert(value).second)
                fprintf(stderr, "[dsp] %s 0x%08X\n", what, value);
        }
    } // namespace

    void AiRegisterDmaCallback(PPCContext& ctx)
    {
        const uint32_t previous = g_ai.callback;
        g_ai.callback = ctx.r3.u32;
        ctx.r3.u64 = previous;
    }

    void AiInitDma(PPCContext& ctx)
    {
        g_ai.next = ctx.r3.u32;
        g_ai.nextLength = ctx.r4.u32;
    }

    void AiStartDma(uint8_t* base)
    {
        if (g_ai.running || g_ai.nextLength < 4)
            return;
        g_ai.running = true;
        // 16-bit stereo: 4 bytes a sample, at the DSP's 32 kHz.
        const auto period = std::chrono::nanoseconds(int64_t(g_ai.nextLength / 4) * 1000000000 / ax::SampleRate);
        fprintf(stderr, "[ai] DMA started: %u bytes every %lld us\n", g_ai.nextLength,
            (long long)std::chrono::duration_cast<std::chrono::microseconds>(period).count());
        StartAudio(base, period);
    }

    void AiStopDma()
    {
        g_ai.running = false;
        StopAudio();
    }

    void AudioInterrupt(uint8_t* base)
    {
        if (g_ai.running && g_ai.callback != 0)
            CallGuest(base, g_ai.callback, 0, 0);
    }

    void DspAddTask(PPCContext& ctx, uint8_t* base)
    {
        g_dsp.task = ctx.r3.u32;
        const uint32_t start = Read32(base, g_dsp.task + TaskInit);
        if (start != 0)
        {
            PPCContext call = ctx;
            call.r3.u64 = g_dsp.task;
            PPCFunc* function = PPC_LOOKUP_FUNC(base, start);
            function(call, base);
        }
        ctx.r3.u64 = g_dsp.task;
    }

    void DspSendMail(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t mail = ctx.r3.u32;
        if (!g_dsp.listAddressNext)
        {
            if ((mail & 0xFFFF0000) == CommandListMail)
                g_dsp.listAddressNext = true;
            else
                LogOnce("mail not known:", mail);
            return;
        }
        g_dsp.listAddressNext = false;
        const ax::Frame frame = ax::RunCommandList(base, mail);
        if (!frame.complete)
            LogOnce("a command list stops at command", uint32_t(frame.unknownCommand));
        if (frame.playing != g_dsp.playing && g_dsp.changesLogged < 20)
        {
            g_dsp.changesLogged++;
            fprintf(stderr, "[ax] frame %llu: %u voices, %u playing\n", (unsigned long long)g_dsp.frames, frame.voices,
                frame.playing);
        }
        g_dsp.playing = frame.playing;
        g_dsp.frames++;
        const uint32_t task = g_dsp.task;
        const uint32_t resume = task != 0 ? Read32(base, task + TaskResume) : 0;
        if (resume != 0)
            PostEvent([base, task, resume] { CallGuest(base, resume, task, 0); });
    }
}
