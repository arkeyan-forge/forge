// Checks the spoken-line skip of the Lux engine (engines/lux/dialogue_skip.h): which presses close a line, when the
// cinematic plays fast and when it stops, and the fast clock never stepping past an animation's end. No game data.
// usage: ArkeyanDialogueSkipTest
#include "dialogue_skip.h"
#include <cstdio>

namespace
{
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    // Runs `frames` updates of a box without a press; true if one of them closed it.
    bool Idle(lux::DialogueSkip& skip, uint32_t playing, int frames, bool locked = true, bool running = true)
    {
        bool closed = false;
        for (int i = 0; i < frames; i++)
            closed |= skip.Box(playing, false, locked, running);
        return closed;
    }
}

int main()
{
    using lux::DialogueSkip;

    // A press closes the line only once it has spoken MinFrames updates.
    {
        DialogueSkip skip;
        CHECK(!Idle(skip, 7, DialogueSkip::MinFrames - 1));
        CHECK(!skip.Box(7, true, true, true));
        DialogueSkip ready;
        CHECK(!Idle(ready, 7, DialogueSkip::MinFrames));
        CHECK(ready.Box(7, true, true, true));
        CHECK(ready.Fast());
        // The same line again (its box still fading out): no second close.
        CHECK(!ready.Box(7, true, true, true));
    }

    // Outside a locked cinematic (player in control, a cinematic the game skips itself, the game paused): never.
    {
        DialogueSkip skip;
        CHECK(!Idle(skip, 3, 40, false));
        CHECK(!skip.Box(3, true, false, true));
        CHECK(!skip.Fast());
    }

    // No voice: nothing to skip.
    {
        DialogueSkip skip;
        CHECK(!Idle(skip, 0, 40));
        CHECK(!skip.Box(0, true, true, true));
    }

    // A cinematic waiting on the line (its clock stopped): the box closes, the cinematic does not play fast.
    {
        DialogueSkip skip;
        Idle(skip, 9, 20, true, false);
        CHECK(skip.Box(9, true, true, false));
        CHECK(!skip.Fast());
    }

    // The next line stops the fast play and needs its own MinFrames before a press counts: tapping goes line by line.
    {
        DialogueSkip skip;
        Idle(skip, 10, 20);
        CHECK(skip.Box(10, true, true, true));
        CHECK(skip.Fast());
        CHECK(!skip.Box(11, true, true, true));
        CHECK(!skip.Fast());
        CHECK(!Idle(skip, 11, DialogueSkip::MinFrames - 1));
        CHECK(skip.Box(11, true, true, true));
        CHECK(skip.Fast());
        skip.Ended();
        CHECK(!skip.Fast());
    }

    // Which lines a press may close: one held by a cinematic the game does not skip, or one shown while the hero's
    // controls are locked (Blobbers' lines in level_027), unless a cinematic the game skips itself holds it.
    {
        CHECK(lux::LineLocked(true, false, false));
        CHECK(lux::LineLocked(false, false, true));
        CHECK(!lux::LineLocked(false, true, true));
        CHECK(!lux::LineLocked(false, false, false));
        CHECK(!lux::LineLocked(false, true, false));
    }

    // The fast clock (rate 1 frame per 60th of a second, 2 frames per update at 30 fps): 16 times the game's step, at
    // most one step before the end, never backwards.
    {
        const float rate = 1.0f, frames = 2.0f;
        float step = rate * frames;
        float fast = lux::FastTime(100.0f, 1672.0f, rate, frames);
        CHECK(fast == 100.0f + step * 15.0f);
        CHECK(lux::FastTime(1660.0f, 1672.0f, rate, frames) == 1672.0f - step);
        CHECK(lux::FastTime(1671.0f, 1672.0f, rate, frames) == 1671.0f);
        CHECK(lux::FastTime(500.0f, 1672.0f, rate, 0.0f) == 500.0f);
        CHECK(lux::FastTime(500.0f, 1672.0f, 0.0f, frames) == 500.0f);
    }

    if (g_failures == 0)
        printf("ArkeyanDialogueSkipTest: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
