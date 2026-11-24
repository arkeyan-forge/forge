#pragma once

// The Toy Box's side of the copies of real toys ([portal] keep_real_copies, docs/runtime/host/portal.md "Copies of real
// toys"): which side the "which one to keep" question focuses, what it shows beyond level and gold, and which waiting toy
// it asks about next. Plain C++, so a
// test can drive it (ArkeyanToyboxRealCopyTest).
#include "host/figures/figure_tag.h"
#include <algorithm>
#include <bit>
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

    // What else differs between the toy and its copy, beyond the level and gold the question always shows: each side then
    // shows that field too, so two sides never look the same when they are not.
    enum Difference : uint32_t
    {
        Upgrades = 1,
        HeroPoints = 2,
        Challenges = 4,
        Hat = 8,
        Nickname = 16,
        PlayTime = 32,
    };
    inline uint32_t Differences(const figures::FigureStats& toy, const figures::FigureStats& copy)
    {
        uint32_t differs = 0;
        if (toy.upgrades != copy.upgrades || toy.wowPow != copy.wowPow)
            differs |= Upgrades;
        if (toy.heroPoints != copy.heroPoints)
            differs |= HeroPoints;
        if (toy.heroicChallenges != copy.heroicChallenges)
            differs |= Challenges;
        if (toy.hat != copy.hat)
            differs |= Hat;
        if (toy.nickname != copy.nickname)
            differs |= Nickname;
        if (toy.playSeconds != copy.playSeconds)
            differs |= PlayTime;
        return differs;
    }

    // The upgrades bought: the basic ones, the chosen path's and the Soul Gem's (bits 2-9; bits 0-1 only say which path is
    // chosen, collection/upgrades.h), and Giants' Wow Pow.
    inline int UpgradeCount(const figures::FigureStats& stats)
    {
        return std::popcount(unsigned(stats.upgrades & 0x3FC)) + (stats.wowPow & 1);
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
