// What the game shows of an online session: the banner while it is held, the chip on a slow connection, the countdown.
#include "host/toybox/online_banner.h"
#include <cstdio>
#include <cstdlib>

namespace
{
    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            fprintf(stderr, "FAIL: %s\n", what);
            std::exit(1); // NOLINT(concurrency-mt-unsafe): one thread
        }
    }
}

int main()
{
    using namespace toybox;
    netplay::SessionStats stats;
    Check(OnlineBannerOf(stats) == OnlineBanner::None && !OnlineChipShown(stats), "a good session shows nothing");
    stats.roundTripMs = SlowRoundTripMs - 1;
    Check(!OnlineChipShown(stats), "a round trip under the threshold shows no chip");
    stats.roundTripMs = 180;
    Check(OnlineChipShown(stats), "a slow round trip shows the chip");
    stats.waiting = true;
    stats.waitedMs = WaitingBannerMs - 1;
    Check(OnlineBannerOf(stats) == OnlineBanner::None, "a short hold passes unseen");
    stats.waitedMs = WaitingBannerMs;
    Check(OnlineBannerOf(stats) == OnlineBanner::Waiting && !OnlineChipShown(stats),
        "a long hold shows the waiting banner, and the banner hides the chip");
    stats.connectionLost = true;
    stats.lostMs = 18000;
    stats.answerMs = 60000;
    Check(OnlineBannerOf(stats) == OnlineBanner::Lost && OnlineSecondsLeft(stats) == 42,
        "a lost connection shows its countdown");
    stats.lostMs = 61000;
    Check(OnlineSecondsLeft(stats) == 0, "the countdown stops at zero");
    stats.askContinue = true;
    Check(OnlineBannerOf(stats) == OnlineBanner::Ask, "after the wait the player is asked");
    puts("online banner: all checks passed");
    return 0;
}
