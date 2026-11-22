// When the stall report fires (d3d/stall_watch.h), with a fake clock: ./ArkeyanStallWatchTest. Exit code 0 = pass.
#include "d3d/stall_watch.h"
#include <cstdio>

namespace
{
    int g_checks = 0, g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            g_failures++;                                                                                              \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition);                                     \
        }                                                                                                              \
    } while (0)

    constexpr uint64_t Ms = 1'000'000;
    constexpr uint64_t Frame = 16'666'667; // 60 Hz, rounded up: 300 frames are 5 s
    using Event = d3d::StallWatch::Event;

    // Swaps `frames` frames from `now` with `draws` each; the events seen, counted.
    struct Run
    {
        int stalled = 0, resumed = 0;
    };
    Run Swaps(d3d::StallWatch& watch, uint64_t& now, int frames, uint32_t draws, bool movie = false)
    {
        Run run;
        for (int i = 0; i < frames; i++, now += Frame)
        {
            Event event = watch.Swap(now, draws, movie);
            run.stalled += event == Event::Stalled;
            run.resumed += event == Event::Resumed;
        }
        return run;
    }
}

int main()
{
    {
        // A game that draws every frame never stalls.
        d3d::StallWatch watch(5000 * Ms);
        uint64_t now = 0;
        Run run = Swaps(watch, now, 3600, 900);
        CHECK(run.stalled == 0 && run.resumed == 0);
    }
    {
        // Swaps with no draw: once at 5 s, not before, and only once however long it lasts; then once when it draws.
        d3d::StallWatch watch(5000 * Ms);
        uint64_t now = 0;
        CHECK(Swaps(watch, now, 60, 500).stalled == 0);
        CHECK(Swaps(watch, now, 299, 0).stalled == 0);
        CHECK(Swaps(watch, now, 1, 0).stalled == 1);
        CHECK(Swaps(watch, now, 3600, 0).stalled == 0);
        CHECK(watch.QuietNs(now) >= 3900 * Frame);
        Run back = Swaps(watch, now, 60, 400);
        CHECK(back.resumed == 1 && back.stalled == 0);
        // A second stall is reported again.
        CHECK(Swaps(watch, now, 300, 0).stalled == 1);
    }
    {
        // Short quiet spells (a load, a fade) and movies are not stalls; a movie resets the quiet time.
        d3d::StallWatch watch(5000 * Ms);
        uint64_t now = 0;
        Run run;
        for (int i = 0; i < 20; i++)
        {
            Run quiet = Swaps(watch, now, 200, 0);
            Run drawn = Swaps(watch, now, 1, 10);
            run.stalled += quiet.stalled + drawn.stalled;
            run.resumed += quiet.resumed + drawn.resumed;
        }
        CHECK(run.stalled == 0 && run.resumed == 0);
        CHECK(Swaps(watch, now, 600, 0, true).stalled == 0);
        CHECK(Swaps(watch, now, 200, 0).stalled == 0);
        CHECK(Swaps(watch, now, 200, 0, true).stalled == 0);
        CHECK(Swaps(watch, now, 299, 0).stalled == 0);
        CHECK(Swaps(watch, now, 1, 0).stalled == 1);
    }
    {
        // Before the game first draws (start-up, a build that skips every draw) nothing is reported; from it, yes.
        d3d::StallWatch watch(100 * Ms);
        CHECK(watch.Swap(0, 0, false) == Event::None);
        CHECK(watch.Swap(10'000 * Ms, 0, false) == Event::None);
        CHECK(watch.Swap(10'100 * Ms, 0, true) == Event::None);
        CHECK(watch.Swap(10'300 * Ms, 0, false) == Event::None);
        CHECK(watch.Swap(11'000 * Ms, 5, false) == Event::None);
        CHECK(watch.Swap(11'100 * Ms, 0, false) == Event::Stalled);
        CHECK(watch.Swap(11'200 * Ms, 1, false) == Event::Resumed);
    }
    {
        // At most `limit` reports in a run: a game that keeps stalling does not fill the log.
        d3d::StallWatch watch(100 * Ms, 2);
        uint64_t now = 0;
        int stalled = 0;
        for (int i = 0; i < 5; i++)
        {
            stalled += Swaps(watch, now, 10, 0).stalled;
            Swaps(watch, now, 1, 5);
        }
        CHECK(stalled == 2);
    }
    {
        // The report's counts are the frames that drew nothing since the last one that drew, that frame's own left out.
        d3d::StallCounts quiet{};
        d3d::StallCounts drawn{ 900, 0, 0, 40, 0, 0, 0, 0 };
        d3d::StallCounts none{ 3, 3, 0, 60, 0, 0, 0, 1 };
        d3d::AddQuietFrame(quiet, 850, drawn);
        CHECK(quiet.guestDraws == 0 && quiet.resolves == 0);
        for (int i = 0; i < 4; i++)
            d3d::AddQuietFrame(quiet, 0, none);
        CHECK(
            quiet.guestDraws == 12 && quiet.skippedShader == 12 && quiet.resolves == 240 && quiet.droppedVertices == 4);
        d3d::AddQuietFrame(quiet, 1, drawn);
        CHECK(quiet.guestDraws == 0 && quiet.resolves == 0);
    }

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
