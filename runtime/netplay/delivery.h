#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace netplay
{
    // A host-paced delivery (the vblank interrupt, the audio driver's period) counted in frames instead of wall time:
    // each frame grants its share, the delivering thread takes one per tick of its own clock, and the flip waits until
    // the guest callbacks of every grant have returned, so a frame always holds the same deliveries.
    class Deliveries
    {
    public:
        // numerator / divisor deliveries per second.
        Deliveries(uint64_t numerator, uint64_t divisor) : m_numerator(numerator), m_divisor(divisor) {}
        // The delivering thread exists: grants made before are dropped, and until then a flip never waits.
        void Start();
        // Grants `steps` sixtieths of a second of deliveries (the remainder carries to the next grant); returns the
        // total granted so far.
        uint64_t GrantSteps(uint64_t steps);
        // Grants `count` deliveries directly (the frame's vertical blanks).
        uint64_t Grant(uint64_t count);
        // The delivering thread: true when a granted delivery is due, to be followed by Done once its callback ran.
        bool TryTake();
        void Done();
        // The flip: true once every delivery granted up to `granted` is done, false after `timeout`.
        bool Complete(uint64_t granted);
        bool WaitDone(uint64_t granted, std::chrono::nanoseconds timeout);

    private:
        const uint64_t m_numerator, m_divisor;
        std::mutex m_mutex;
        std::condition_variable m_doneSignal;
        bool m_started = false;
        uint64_t m_remainder = 0, m_granted = 0, m_taken = 0, m_done = 0;
    };

    // The cooperative flip's wall-clock pacing, since its blanks no longer wait for their tick: a frame of `blanks`
    // blanks (at least 1) is due that many periods after the previous frame was due; a late or first frame is due now.
    inline uint64_t FlipDue(uint64_t previous, uint64_t now, uint32_t blanks, uint64_t period)
    {
        const uint64_t due = previous + uint64_t(blanks == 0 ? 1 : blanks) * period;
        return previous == 0 || due < now ? now : due;
    }

    // SKY_NETPLAY_DELIVERY=frame (deterministic mode only): vblanks and audio periods follow the frames.
    bool FrameDelivery();
    extern Deliveries g_vblankDeliveries;
    extern Deliveries g_audioDeliveries;
    // Ticks of the delivering thread's own clock without a grant, outside a flip, before it delivers anyway (a game that
    // waits for a vblank or an audio period without presenting): about 500 ms, longer than any frame of play, so a
    // fallback marks a game that stopped presenting, not a slow frame. A flip that hashes the heaps never counts.
    inline constexpr uint32_t VBlankFallbackTicks = 30;
    inline constexpr uint32_t AudioFallbackPeriods = 94;
    // Set by the render thread from the start of the flip (the probe's hash included) to the end of its waits.
    extern std::atomic<bool> g_flipping;
}
