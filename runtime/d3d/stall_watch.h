#pragma once

#include <cstdint>

namespace d3d
{
    // Tells when a game goes on swapping without drawing outside a movie: the window then shows the last presented frame
    // for as long as it lasts (a player's Swap Force picture that stayed green, forge issue #84). Swap() at each swap;
    // times are nanoseconds on any steady clock, so a test runs it without waiting.
    class StallWatch
    {
    public:
        enum class Event
        {
            None,
            Stalled, // `quiet` ns since the last swap that drew: report once (never before the game first drew)
            Resumed, // the first swap that draws after a reported stall
        };

        // At most `limit` stalls reported in a run, so a game that keeps stalling does not fill the log.
        explicit StallWatch(uint64_t quiet, uint32_t limit = 3) : m_quiet(quiet), m_limit(limit) {}

        Event Swap(uint64_t now, uint32_t draws, bool movie)
        {
            if (draws != 0 || movie)
            {
                m_drew = m_drew || draws != 0;
                m_lastDrawn = now;
                if (m_stalled && draws != 0)
                {
                    m_stalled = false;
                    return Event::Resumed;
                }
                return Event::None;
            }
            if (!m_drew || m_stalled || m_reported >= m_limit || now - m_lastDrawn < m_quiet)
                return Event::None;
            m_stalled = true;
            m_reported++;
            m_stalledFrom = m_lastDrawn;
            return Event::Stalled;
        }

        // How long the stall reported last has lasted at `now`.
        uint64_t QuietNs(uint64_t now) const { return now - m_stalledFrom; }

    private:
        uint64_t m_quiet;
        uint32_t m_limit;
        uint32_t m_reported = 0;
        bool m_drew = false, m_stalled = false;
        uint64_t m_lastDrawn = 0, m_stalledFrom = 0;
    };

    // The game's draw calls and resolves of one frame, as the stall report counts them.
    struct StallCounts
    {
        uint32_t guestDraws, skippedShader, skippedTarget, resolves;
        uint32_t droppedStep, droppedSizes, droppedIndices, droppedVertices;
    };

    // Sums into `quiet` the frames that drew nothing since the last one that drew (`draws` the frame's drawn calls).
    inline void AddQuietFrame(StallCounts& quiet, uint32_t draws, const StallCounts& frame)
    {
        if (draws != 0)
        {
            quiet = {};
            return;
        }
        quiet.guestDraws += frame.guestDraws;
        quiet.skippedShader += frame.skippedShader;
        quiet.skippedTarget += frame.skippedTarget;
        quiet.resolves += frame.resolves;
        quiet.droppedStep += frame.droppedStep;
        quiet.droppedSizes += frame.droppedSizes;
        quiet.droppedIndices += frame.droppedIndices;
        quiet.droppedVertices += frame.droppedVertices;
    }
}
