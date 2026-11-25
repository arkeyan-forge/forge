#pragma once

#include <cstdint>

// Skipping a spoken line held by a cinematic the game does not let the player skip, or shown while the hero is frozen
// [Lux] [SSA]
// (docs/engines/lux/dialogue_skip.md). The skip button (the one that skips cutscenes) closes the text box being spoken,
// which stops its voice; when the cinematic's clock runs on under the line, the cinematic then plays fast until the next
// line or its own end, so every step of it still runs.
namespace lux
{
    class DialogueSkip
    {
    public:
        // A box must have spoken this many updates before the one a press counts in, so one press closes one line.
        static constexpr int MinFrames = 15;
        // How much faster a skipped cinematic plays.
        static constexpr float FastForward = 16.0f;

        // One update of a text box. `playing` is its voice's playing id (0: no voice), `press` a press of the skip
        // button this frame, `locked` whether a cinematic that the game does not skip holds it (and the game is not
        // paused), `running` whether that cinematic's clock runs. True: close the box now.
        bool Box(uint32_t playing, bool press, bool locked, bool running)
        {
            if (playing == 0)
                return false;
            if (playing != m_playing)
            {
                m_playing = playing;
                m_age = 0;
                if (playing != m_skipped)
                    m_fast = false;
            }
            bool ready = m_age >= MinFrames;
            if (!ready)
                m_age++;
            if (!locked || !press || !ready || playing == m_skipped)
                return false;
            m_skipped = playing;
            m_fast = running;
            return true;
        }

        // Whether the cinematic plays fast now (after a skipped line, until the next one starts).
        bool Fast() const { return m_fast; }

        // The cinematic ended or went away.
        void Ended() { m_fast = false; }

    private:
        uint32_t m_playing = 0, m_skipped = 0;
        int m_age = 0;
        bool m_fast = false;
    };

    // Whether a press may close a line: held by a cinematic the game does not skip (`lockedCinematic`), or shown while
    // the hero's controls are locked, unless a cinematic that the game skips itself holds it (B then does the game's
    // own skip).
    inline bool LineLocked(bool lockedCinematic, bool skippableCinematic, bool controlsLocked)
    {
        return lockedCinematic || (!skippableCinematic && controlsLocked);
    }

    // An animation's time after one update of a fast cinematic, before the game's own step of rate * frames (frames:
    // the update's time step in 60ths of a second): never past one step before its end, so the game's own step reaches
    // the end and reports it.
    inline float FastTime(float time, float end, float rate, float frames)
    {
        float step = rate * frames;
        if (step <= 0.0f)
            return time;
        float target = time + step * (DialogueSkip::FastForward - 1.0f);
        float limit = end - step;
        if (target > limit)
            target = limit;
        return target > time ? target : time;
    }
}
