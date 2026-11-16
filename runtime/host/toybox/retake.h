#pragma once

// When the Toy Box puts a figure on again because the game has not read it (docs/runtime/host/toybox_ux.md, "A quick
// swap the game missed"). A figure swapped in while the game still loads the previous one can be left unread, the game
// showing it as loading for good; lifting and placing it again recovers. Plain C++ for ArkeyanToyboxSwapTest.
#include <cstdint>

namespace toybox::retake
{
    constexpr int MaxTries = 3;

    // The title starts reading a toy at block 0. A read of another block right after a swap is the end of the previous
    // toy's read (Giants then cancels its arrival and waits on "Loading" for good): not the new figure taken.
    constexpr bool Taken(uint64_t headRead, uint64_t placedAt) { return headRead != 0 && headRead >= placedAt; }

    constexpr uint64_t FirstWaitNs = 1'000'000'000ull;
    // The first wait when the game asks for the seat the unread figure is on: Swap Force raises its prompt at the swap's
    // removal report and leaves the figure that arrives next unread. A few prompt polls, not a second of prompt.
    constexpr uint64_t AskedWaitNs = 300'000'000ull;

    // `tries` re-places done since the player's own placement; `unreadNs` since the last one; `asked`: the game asks for
    // that seat now. Waits 1 s (0.3 s when asked), 2 s, then 4 s.
    constexpr bool Due(int tries, uint64_t unreadNs, bool asked = false)
    {
        return tries < MaxTries && unreadNs > (tries == 0 && asked ? AskedWaitNs : FirstWaitNs << tries);
    }

    // A seat whose figure the Toy Box put on is held from the prompt while the game has not read it and a re-place is
    // still to come: the prompt is the swap's removal, not a request to the player.
    constexpr bool Holds(bool unread, int tries) { return unread && tries < MaxTries; }

    // Whether the game's prompt for a seat opens the Toy Box by itself. At once for a free seat or a figure taken; for a
    // held seat, a moment after the hold ends with the prompt still up (the take answers it within a few polls).
    class PromptGate
    {
    public:
        static constexpr uint64_t GraceNs = 1'000'000'000ull;

        // The prompt appeared. True: open now.
        bool Appeared(bool held)
        {
            m_waiting = held;
            m_releasedAt = 0;
            return !held;
        }

        // Once a frame while that prompt stays up. True once, when it opens after a hold.
        bool Update(bool held, uint64_t now)
        {
            if (!m_waiting)
                return false;
            if (held)
                m_releasedAt = 0;
            else if (m_releasedAt == 0)
                m_releasedAt = now;
            else if (now - m_releasedAt >= GraceNs)
            {
                m_waiting = false;
                return true;
            }
            return false;
        }

        void Gone() { m_waiting = false; }

    private:
        bool m_waiting = false;
        uint64_t m_releasedAt = 0;
    };
}
