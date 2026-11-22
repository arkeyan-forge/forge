#pragma once

#include "netplay/session.h"
#include <cstdint>

// What the game shows of an online session (docs/runtime/host/netplay.md, "Step 6"): nothing while it goes well, a
// corner chip with the round trip when the connection is slow, a banner while the game is held for the other player.
namespace toybox
{
    enum class OnlineBanner
    {
        None,
        Lost,    // no packet for a few seconds: reconnecting, with the seconds left before the question
        Ask,     // no answer for a minute: wait more or continue alone
        Waiting, // the other player answers but does not advance (a long load on a slower PC)
    };

    // A held game shows the banner after this long: a short hold (a load, a late packet) passes unseen.
    inline constexpr uint64_t WaitingBannerMs = 3000;
    // The chip shows from this round trip on: the input delay is then three frames or more at 30 FPS.
    inline constexpr uint64_t SlowRoundTripMs = 120;

    inline OnlineBanner OnlineBannerOf(const netplay::SessionStats& stats)
    {
        if (stats.askContinue)
            return OnlineBanner::Ask;
        if (stats.connectionLost)
            return OnlineBanner::Lost;
        if (stats.waiting && stats.waitedMs >= WaitingBannerMs)
            return OnlineBanner::Waiting;
        return OnlineBanner::None;
    }
    inline bool OnlineBannerShown(const netplay::SessionStats& stats)
    {
        return OnlineBannerOf(stats) != OnlineBanner::None;
    }
    inline bool OnlineChipShown(const netplay::SessionStats& stats)
    {
        return !OnlineBannerShown(stats) && stats.roundTripMs >= SlowRoundTripMs;
    }
    // Seconds left before the question, for the Lost banner's countdown.
    inline uint64_t OnlineSecondsLeft(const netplay::SessionStats& stats)
    {
        return stats.lostMs >= stats.answerMs ? 0 : (stats.answerMs - stats.lostMs + 999) / 1000;
    }
}
