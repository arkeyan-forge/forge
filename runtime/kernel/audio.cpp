// The kernel's audio render driver. The title's XAudio2 is driven by it: the driver calls the registered client
// once per audio frame (256 samples at 48 kHz) and the client answers with XAudioSubmitRenderDriverFrame.
// Submitted frames go to the host output device (host/audio), which also paces the driver.
#include "host/audio.h"
#include "netplay/delivery.h"
#include "netplay/events.h"
#include "netplay/scheduler.h"
#include "host/speakers.h"
#include "gpu/frame_stats.h"
#include "platform/platform.h"
#include "memory.h"
#include "thread.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include "byteswap.h"
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    constexpr uint32_t StatusSuccess = 0;
    constexpr uint32_t DriverHandleTag = 0x41550000;
    constexpr uint32_t MaxClients = 8;
    constexpr auto FramePeriod = std::chrono::nanoseconds(1'000'000'000ull * AudioFrameSamples / 48000);
    constexpr uint32_t TargetQueuedFrames = 6; // ~32 ms of latency

    struct Client
    {
        // Held while the callback runs: unregistering takes it, so no callback outlives XAudioUnregisterRenderDriverClient
        // (the console guarantees that). Recursive in case the client unregisters from its own callback.
        std::recursive_mutex mutex;
        std::atomic<bool> active{ false };
        bool rendering = false;
        std::thread::id renderer;
        std::chrono::steady_clock::time_point firstFrame;
        uint32_t callback = 0;
        uint32_t contextCell = 0; // the callback receives a pointer to a cell holding the client's context
    };

    class ClientLock
    {
    public:
        explicit ClientLock(Client& client) : m_lock(client.mutex, std::defer_lock)
        {
            if (!netplay::Cooperative())
                m_lock.lock();
            else if (client.rendering && client.renderer != std::this_thread::get_id())
                netplay::GuestScheduler().Block([&client] { return !client.rendering; });
        }

    private:
        std::unique_lock<std::recursive_mutex> m_lock;
    };

    Client g_clients[MaxClients];

    // Each client submits its frame from inside its callback, so the frames of one period are summed here, on the
    // driver thread, before the next client's callback runs.
    float g_mix[AudioFrameChannels * AudioFrameSamples];

    uint32_t g_framesOut = 0;            // frames output so far (the driver thread)
    std::atomic<uint32_t> g_underruns;   // outputs that found the device's queue empty

    // One thread plays the hardware: every frame period it asks each client for a frame, in turn, then outputs the mix.
    void DriverThread(PPCContext& ctx)
    {
        gpu::framestats::RegisterThread(gpu::framestats::Thread::Audio, PlatformThreadCpuClock());
        auto next = std::chrono::steady_clock::now();
        const bool frameDelivery = netplay::FrameDelivery();
        const bool cooperative = netplay::Cooperative();
        if (frameDelivery && !cooperative)
        {
            netplay::g_audioDeliveries.Start();
            netplay::NoteEvent("audio", "start");
        }
        uint32_t ungranted = 0;
        while (true)
        {
            std::fill(std::begin(g_mix), std::end(g_mix), 0.0f);
            // Netplay: the clients render a period only when a frame granted one; the device gets silence otherwise.
            bool granted = false, deliver = true;
            if (cooperative)
            {
                netplay::GuestScheduler().Block([] { return netplay::g_audioDeliveries.TryTake(); });
                granted = true;
            }
            else if (frameDelivery)
            {
                granted = netplay::g_audioDeliveries.TryTake();
                deliver = granted || (!netplay::g_flipping.load(std::memory_order_acquire) &&
                                         ++ungranted >= netplay::AudioFallbackPeriods);
                if (deliver)
                {
                    if (!granted)
                        netplay::NoteEvent("audio", "fallback");
                    ungranted = 0;
                }
            }
            for (uint32_t index = 0; deliver && index < MaxClients; index++)
            {
                Client& client = g_clients[index];
                ClientLock lock(client);
                if (!client.active || (!cooperative && std::chrono::steady_clock::now() < client.firstFrame))
                    continue;

                netplay::CountEvent(netplay::Counter::AudioCallback);
                ctx.r3.u64 = client.contextCell;
                if (cooperative)
                {
                    client.rendering = true;
                    client.renderer = std::this_thread::get_id();
                }
                CallGuest(ctx, client.callback);
                if (cooperative)
                    client.rendering = false;
            }
            if (granted)
                netplay::g_audioDeliveries.Done();
            HostAudioSubmit(g_mix);
            if (cooperative)
                continue;

            // One frame per period, never a burst, like the hardware: a title may read XAudio2's SamplesPlayed as the
            // play cursor of a looping buffer (Giants mixes into a 1024-sample loop and reads SamplesPlayed mod 1024),
            // so the cursor must move one frame at a time. Devices take their data in larger chunks; refilling the
            // queue as they do advanced it 4 frames at once, the title saw no progress and the loop replayed.
            // The device still sets the pace: its queue level trims the period, 1/64 per frame off the target.
            auto period = FramePeriod;
            if (HostAudioActive())
            {
                uint32_t queued = HostAudioQueuedFrames();
                // The device found nothing queued once started (the debug screen's underruns): this frame came late.
                if (queued == 0 && ++g_framesOut > 2 * TargetQueuedFrames)
                    g_underruns.fetch_add(1, std::memory_order_relaxed);
                period += FramePeriod * (int32_t(queued) - int32_t(TargetQueuedFrames)) / 64;
            }
            next += period;
            auto now = std::chrono::steady_clock::now();
            if (now - next > FramePeriod * TargetQueuedFrames)
                next = now; // after a stall, resume at once instead of catching up in a burst
            std::this_thread::sleep_until(next);
        }
    }
}

PPC_FUNC(__imp__XAudioRegisterRenderDriverClient)
{
    uint32_t callbackPointer = ctx.r3.u32; // { callback, context }
    uint32_t driverPointer = ctx.r4.u32;

    for (uint32_t index = 0; index < MaxClients; index++)
    {
        Client& client = g_clients[index];
        {
            ClientLock lock(client);
            if (client.active)
                continue;

            // Like the hardware, the first frame is requested one frame period after registration, which leaves
            // the client time to finish initialising.
            client.active = true;
            client.firstFrame = std::chrono::steady_clock::now() + FramePeriod;
            client.callback = g_memory.Read<uint32_t>(callbackPointer);
            if (client.contextCell == 0)
                client.contextCell = g_memory.SystemAlloc(4);
            g_memory.Write<uint32_t>(client.contextCell, g_memory.Read<uint32_t>(callbackPointer + 4));
        }

        g_memory.Write<uint32_t>(driverPointer, DriverHandleTag | index);
        static std::once_flag started;
        std::call_once(started, [] {
            if (netplay::Cooperative())
            {
                netplay::g_audioDeliveries.Start();
                netplay::NoteEvent("audio", "start");
            }
            StartHostDrivenThread(DriverThread, 0);
        });
        ctx.r3.u64 = StatusSuccess;
        return;
    }

    ctx.r3.u64 = 0x8007000E; // E_OUTOFMEMORY
}

PPC_FUNC(__imp__XAudioUnregisterRenderDriverClient)
{
    Client& client = g_clients[(ctx.r3.u32 & 0xFFFF) % MaxClients];
    ClientLock lock(client);
    client.active = false;
    ctx.r3.u64 = StatusSuccess;
}

// (driver, samples): 6 planar channels of 256 big-endian floats.
PPC_FUNC(__imp__XAudioSubmitRenderDriverFrame)
{
    const uint32_t* samples = static_cast<const uint32_t*>(g_memory.Translate(ctx.r4.u32));
    for (uint32_t i = 0; i < AudioFrameChannels * AudioFrameSamples; i++)
        g_mix[i] += std::bit_cast<float>(ByteSwap(samples[i]));
    ctx.r3.u64 = StatusSuccess;
}

// At the title's audio start (its XAudio2 registers its render client right after; SSA asks twice).
PPC_FUNC(__imp__XAudioGetSpeakerConfig)
{
    const uint32_t config = speakers::GuestSpeakerConfig(HostAudioLayout());
    fprintf(stderr, "[audio] speaker config 0x%08X (%s)\n", config, speakers::Name(HostAudioLayout()));
    g_memory.Write<uint32_t>(ctx.r3.u32, config);
    ctx.r3.u64 = StatusSuccess;
}

PPC_FUNC(__imp__XAudioGetVoiceCategoryVolume)
{
    g_memory.Write<uint32_t>(ctx.r4.u32, std::bit_cast<uint32_t>(1.0f));
    ctx.r3.u64 = StatusSuccess;
}

PPC_FUNC(__imp__XAudioGetDuckerLevel)
{
    g_memory.Write<uint32_t>(ctx.r3.u32, 0);
    ctx.r3.u64 = StatusSuccess;
}

uint32_t AudioUnderruns() { return g_underruns.load(std::memory_order_relaxed); }

uint32_t AudioVoicesActive()
{
    uint32_t active = 0;
    for (Client& client : g_clients)
    {
        if (netplay::Cooperative())
            active += client.active.load(std::memory_order_relaxed);
        else
        {
            std::lock_guard lock(client.mutex);
            active += client.active.load(std::memory_order_relaxed);
        }
    }
    return active;
}
