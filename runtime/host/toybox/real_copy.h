#pragma once

// The Toy Box's side of the copies of real toys ([portal] keep_real_copies, docs/runtime/host/portal.md "Copies of real
// toys"): which side the "which one to keep" question focuses, and which waiting toy it asks about next. Plain C++, so a
// test can drive it (ArkeyanToyboxRealCopyTest).
#include "host/figures/figure_tag.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace toybox::realcopy
{
    // When the figure was last placed, as a number that grows with the time; 0: no date (never played, unreadable).
    inline uint64_t PlacedStamp(figures::AreaState state, const figures::FigureStats& stats)
    {
        const figures::Date& d = stats.lastPlaced;
        if (state != figures::AreaState::Valid || d.year == 0)
            return 0;
        return uint64_t(d.year) << 32 | uint64_t(d.month) << 24 | uint64_t(d.day) << 16 | uint64_t(d.hour) << 8 |
               d.minute;
    }

    // The question focuses the side placed last; the toy when either date is missing.
    inline bool ToyFirst(uint64_t toyPlaced, uint64_t copyPlaced)
    {
        return toyPlaced == 0 || copyPlaced == 0 || toyPlaced >= copyPlaced;
    }

    // One question at a time, about the first toy still waiting. A question the player closed (B) comes back the next
    // time the Toy Box opens; its toy stays off the game until then.
    class Questions
    {
    public:
        // The copy to ask about now (empty: none). `waiting`: the copies whose toy waits for the player's choice;
        // `shown`: whether the last question asked is still on screen.
        std::string Next(const std::vector<std::string>& waiting, bool shown)
        {
            if (shown && !m_asked.empty() && std::find(waiting.begin(), waiting.end(), m_asked) != waiting.end())
                return {};
            m_asked.clear();
            for (const std::string& path : waiting)
            {
                if (std::find(m_closed.begin(), m_closed.end(), path) == m_closed.end())
                    return m_asked = path;
            }
            return {};
        }
        void Closed(const std::string& path)
        {
            m_closed.push_back(path);
            m_asked.clear();
        }
        void Answered() { m_asked.clear(); }
        void Reopened() { m_closed.clear(); }

    private:
        std::string m_asked;
        std::vector<std::string> m_closed;
    };

    // A save that did not reach a toy: true once for each new failure count (PortalRealWriteFailures).
    class Failures
    {
    public:
        bool New(uint64_t count)
        {
            const bool more = count > m_seen;
            m_seen = count;
            return more;
        }

    private:
        uint64_t m_seen = 0;
    };
}
