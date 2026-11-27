// Checks the spoken-line skip of the Alchemy engine (engines/alchemy/dialogue_skip.h): which presses close a line, the
// game's own skip left to it, when the fast run stops, and which flags say a cutscene is skippable. No game data.
// usage: ArkeyanAlchemyDialogueSkipTest
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

    using alchemy::DialogueSkip;
    constexpr double After = DialogueSkip::GameSeconds + 0.01;
    constexpr double Frame = 1.0 / 30;

    // The game updates a playing sound every frame: `sound` as `playback` from `from` to `to`. True if a call ended a
    // fast run.
    bool Speak(DialogueSkip& skip, uint32_t sound, uint32_t playback, double from, double to)
    {
        bool ended = false;
        const int frames = int((to - from) / Frame + 1e-9);
        for (int i = 0; i <= frames; i++)
            ended |= skip.Played(sound, playback, from + i * Frame);
        return ended;
    }
}

int main()
{
    // A press closes the line once it has spoken MinSeconds, after the game's GameSeconds: its voice stops, the logic
    // runs fast until the next line starts.
    {
        DialogueSkip skip;
        Speak(skip, 7, 70, 10.0, 10.2);
        skip.Press(10.2, true, false);
        CHECK(skip.Update(10.2 + After, true, false).stop == 0);
        Speak(skip, 7, 70, 10.2, 10.6);
        skip.Press(10.6, true, false);
        CHECK(skip.Update(10.6 + 0.05, true, false).stop == 0); // the game's time to skip
        DialogueSkip::Step step = skip.Update(10.6 + After, true, false);
        CHECK(step.stop == 7 && step.fast);
        CHECK(skip.Update(11.5, true, false).fast);
        CHECK(skip.Played(8, 80, 12.0)); // the next line ends it
        CHECK(!skip.Fast());
        // The new line needs its own MinSeconds: tapping goes line by line.
        skip.Press(12.1, true, false);
        CHECK(skip.Update(12.1 + After, true, false).stop == 0);
        Speak(skip, 8, 80, 12.0, 12.6);
        skip.Press(12.6, true, false);
        CHECK(skip.Update(12.6 + After, true, false).stop == 8);
    }

    // A cutscene the game lets the player skip, the player in control, or the game skipping: the press is the game's.
    {
        DialogueSkip skippable;
        Speak(skippable, 3, 30, 0.0, 1.2);
        skippable.Press(1.0, true, true);
        CHECK(skippable.Update(1.0 + After, true, false).stop == 0);

        DialogueSkip free;
        Speak(free, 3, 30, 0.0, 1.2);
        free.Press(1.0, false, false);
        CHECK(free.Update(1.0 + After, false, false).stop == 0);

        DialogueSkip game;
        Speak(game, 3, 30, 0.0, 1.2);
        game.Press(1.0, true, false);
        CHECK(game.Update(1.0 + After, true, true).stop == 0);
        CHECK(!game.Fast());
    }

    // No line playing (stopped, ended by itself without a stop, or none started): nothing to close.
    {
        DialogueSkip stopped;
        Speak(stopped, 5, 50, 0.0, 1.0);
        stopped.Stopped(5);
        stopped.Press(1.0, true, false);
        CHECK(stopped.Update(1.0 + After, true, false).stop == 0);

        DialogueSkip ended;
        Speak(ended, 5, 50, 0.0, 1.0);
        ended.Press(1.0 + DialogueSkip::GoneSeconds + 0.1, true, false);
        CHECK(ended.Update(1.5 + After, true, false).stop == 0);

        DialogueSkip none;
        none.Press(1.0, true, false);
        CHECK(none.Update(1.0 + After, true, false).stop == 0);
    }

    // Every playing sound is updated each frame: a sound that keeps playing is one line, even with other voices around
    // it; the newest line is the one a press closes.
    {
        DialogueSkip skip;
        for (int i = 0; i <= 60; i++)
        {
            skip.Played(1, 10, i * Frame); // a voice that loops all along
            if (i >= 30)
                skip.Played(2, 20, i * Frame); // the line, from 1 s
        }
        const double t = 61 * Frame;
        skip.Press(t, true, false);
        Speak(skip, 2, 20, t, t + After);
        CHECK(skip.Update(t + After, true, false).stop == 2);
    }

    // The same sound object played again (a new playback, or after a pause) is a new line.
    {
        DialogueSkip skip;
        Speak(skip, 9, 90, 0.0, 1.0);
        CHECK(!skip.Played(9, 90, 1.0 + Frame));
        skip.Press(1.0 + Frame, true, false);
        CHECK(skip.Update(1.0 + Frame + After, true, false).fast);
        CHECK(skip.Played(9, 91, 1.3)); // its next line, another playback
        CHECK(!skip.Fast());
        Speak(skip, 9, 91, 1.3, 1.5);
        // After a silence it starts over: a press needs MinSeconds of it again.
        const double again = 1.5 + DialogueSkip::GoneSeconds + 0.1;
        Speak(skip, 9, 91, again, again + 0.2);
        skip.Press(again + 0.2, true, false);
        CHECK(skip.Update(again + 0.2 + After, true, false).stop == 0);
    }

    // The fast run stops at the cutscene's end, the game's own skip, or after MaxFastSeconds with no line.
    {
        DialogueSkip end;
        Speak(end, 1, 10, 0.0, 1.2);
        end.Press(1.0, true, false);
        CHECK(end.Update(1.0 + After, true, false).fast);
        CHECK(!end.Update(1.5, false, false).fast);

        DialogueSkip game;
        Speak(game, 1, 10, 0.0, 1.2);
        game.Press(1.0, true, false);
        CHECK(game.Update(1.0 + After, true, false).fast);
        CHECK(!game.Update(1.5, true, true).fast);

        DialogueSkip time;
        Speak(time, 1, 10, 0.0, 1.2);
        time.Press(1.0, true, false);
        CHECK(time.Update(1.0 + After, true, false).fast);
        CHECK(time.Update(1.0 + DialogueSkip::MaxFastSeconds, true, false).fast);
        CHECK(!time.Update(1.5 + DialogueSkip::MaxFastSeconds, true, false).fast);
    }

    // The skippable flags of the holding cutscene: the ones written around its start, not a line's later ones.
    {
        alchemy::SkippableFlags flags;
        flags.Written(0x100, 33.87); // the cutscene's own
        flags.Written(0x200, 33.87);
        flags.HoldStarted(33.87);
        flags.Written(0x300, 34.9); // a line's
        int near = 0;
        bool line = false;
        for (int i = 0; i < alchemy::SkippableFlags::Size; i++)
        {
            near += flags.Flag(i) != 0;
            line |= flags.Flag(i) == 0x300;
        }
        CHECK(near == 2 && !line);
        flags.HoldStarted(60.0); // the next cutscene: the old flags are not its own
        for (int i = 0; i < alchemy::SkippableFlags::Size; i++)
            CHECK(flags.Flag(i) == 0);
        flags.Written(0x100, 60.1); // rewritten for it
        bool found = false;
        for (int i = 0; i < alchemy::SkippableFlags::Size; i++)
            found |= flags.Flag(i) == 0x100;
        CHECK(found);
    }

    // Which sounds are characters' lines: the category is bits 1-5 of the properties word.
    {
        CHECK(alchemy::SoundCategory(0x02040CAA) == 21); // Eon in Giants' tutorial
        CHECK(alchemy::IsSpokenLine(21)); // Voice/NPC/Dialog
        CHECK(alchemy::IsSpokenLine(18)); // Voice/Enemy/Dialog
        CHECK(alchemy::IsSpokenLine(16)); // Voice/Enemy/
        CHECK(!alchemy::IsSpokenLine(28)); // music
        CHECK(!alchemy::IsSpokenLine(13)); // the Skylander's own voice
        CHECK(!alchemy::IsSpokenLine(17)); // an enemy's grunt
        CHECK(!alchemy::IsSpokenLine(20)); // an NPC's emote
        CHECK(!alchemy::IsSpokenLine(26)); // the interface
    }

    if (g_failures == 0)
        printf("ArkeyanAlchemyDialogueSkipTest: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
