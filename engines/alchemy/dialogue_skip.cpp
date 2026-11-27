// Skipping a spoken line of a cutscene the game does not let the player skip [Alchemy] [TrapTeam]
// (docs/engines/alchemy/dialogue_skip.md, engines/alchemy/dialogue_skip.h for the rules).
//
// A cutscene's lines are voices it starts (GAME_HOOK_Sound_Play) on its own clock, which follows the game's logic clock
// scaled by game::GameLogicScale. A press of the game's skip button (game::DialogueSkipButton) stops the line's voice
// through the game's own stop (GAME_HOOK_Sound_Stop) and runs that clock fast until the next line starts: the cutscene
// reaches it through its own steps. Whether a cutscene holds the player, whether the game is skipping it, and whether it
// lets the game's skip end it are script variables of its cutscene system, found by name as the scripts write them
// (GAME_HOOK_ScriptValue_Set), which is also where the button is read, many times a frame.
#include "dialogue_skip.h"
#include "game_hooks.h"
#include "host/input.h"
#include "memory.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ppc_config.h>
#include <ppc_context.h>

#if defined(GAME_HOOK_Sound_Play) && defined(GAME_HOOK_Sound_Stop) && defined(GAME_HOOK_ScriptValue_Set)
namespace
{
    using Clock = std::chrono::steady_clock;
    const auto g_processStart = Clock::now(); // set at start-up, like the file log's origin

    double Now() { return std::chrono::duration<double>(Clock::now() - g_processStart).count(); }

    // Whether the name at `pointer` (as the record holds it) is `name`.
    bool NameIs(uint32_t pointer, std::string_view name)
    {
        const uint32_t text = pointer - game::ScriptNameOffset;
        if (pointer == 0 || g_memory.Read<uint8_t>(text) != uint8_t(name[0]))
            return false;
        for (size_t i = 1; i <= name.size(); i++)
        {
            uint8_t c = g_memory.Read<uint8_t>(text + uint32_t(i));
            if (c != (i < name.size() ? uint8_t(name[i]) : 0))
                return false;
        }
        return true;
    }

    // A cutscene system variable: its record, once a script wrote it, and the interned name it was found by.
    struct Variable
    {
        std::string_view name;
        uint32_t record = 0, namePointer = 0;

        // Whether a script now writes this variable (`value` its record); reads the name only for a name not seen yet.
        bool Matches(uint32_t value, uint32_t pointer)
        {
            if (pointer == namePointer && namePointer != 0)
            {
                record = value;
                return true;
            }
            if (!NameIs(pointer, name))
                return false;
            record = value;
            namePointer = pointer;
            return true;
        }

        // Its integer; 0 while unknown, or once its record holds another variable (a level unloaded).
        uint32_t Read() const
        {
            if (record == 0 || g_memory.Read<uint32_t>(record + alchemy::ValueName) != namePointer)
                return 0;
            return g_memory.Read<uint32_t>(record + alchemy::ValueWord);
        }
    };

    Variable g_cutscene{ alchemy::CutsceneVariable }, g_skipping{ alchemy::SkippingVariable };
    uint32_t g_skippableNames[8] = {}; // the name pointers of the skippable flags seen so far
    alchemy::SkippableFlags g_flags;
    alchemy::DialogueSkip g_skip;
    bool g_held = false;
    uint16_t g_buttons = 0; // the skip button's state at the last check, per user
    double g_lastCheck = 0;
    uint32_t g_scaleBefore = 0; // the logic scale the fast run replaced (its float's bits; 0: not running)

    bool IsSkippableFlag(uint32_t pointer)
    {
        if (pointer == 0)
            return false;
        for (uint32_t known : g_skippableNames)
            if (known == pointer)
                return true;
        for (std::string_view name : alchemy::SkippableVariables)
            if (NameIs(pointer, name))
            {
                for (uint32_t& slot : g_skippableNames)
                    if (slot == 0)
                    {
                        slot = pointer;
                        break;
                    }
                return true;
            }
        return false;
    }

    // Whether the holding cutscene lets the game's own skip end it.
    bool Skippable()
    {
        for (int i = 0; i < alchemy::SkippableFlags::Size; i++)
        {
            uint32_t record = g_flags.Flag(i);
            if (record != 0 && IsSkippableFlag(g_memory.Read<uint32_t>(record + alchemy::ValueName)) &&
                g_memory.Read<uint32_t>(record + alchemy::ValueWord) != 0)
                return true;
        }
        return false;
    }

    bool SkipPressed()
    {
        bool pressed = false;
        for (int user = 0; user < MaxUsers; user++)
        {
            if (!GamepadConnected(user))
                continue;
            uint32_t packet;
            bool down = (GetGamepad(packet, user).buttons & game::DialogueSkipButton) != 0;
            uint16_t bit = uint16_t(1u << user);
            pressed |= down && (g_buttons & bit) == 0;
            g_buttons = uint16_t(down ? g_buttons | bit : g_buttons & ~bit);
        }
        return pressed;
    }

    void RunFast(bool fast)
    {
        if (fast && g_scaleBefore == 0)
        {
            g_scaleBefore = g_memory.Read<uint32_t>(game::GameLogicScale);
            fprintf(stderr, "[dialogue] line skipped at %.3f s, the cutscene plays fast\n", Now());
        }
        if (fast)
        {
            uint32_t word;
            memcpy(&word, &alchemy::DialogueSkip::FastScale, sizeof word);
            g_memory.Write<uint32_t>(game::GameLogicScale, word);
        }
        else if (g_scaleBefore != 0)
        {
            g_memory.Write<uint32_t>(game::GameLogicScale, g_scaleBefore);
            g_scaleBefore = 0;
            fprintf(stderr, "[dialogue] the cutscene is back to its pace at %.3f s\n", Now());
        }
    }
}

PPC_FUNC_IMPL(GAME_IMP(Sound_Stop));
PPC_FUNC(GAME_HOOK(Sound_Stop))
{
    g_skip.Stopped(ctx.r3.u32);
    GAME_IMP(Sound_Stop)(ctx, base);
}

// sub(sound, float f1, ...): plays a sound, every frame while it plays, or stops it at once if it cannot play.
PPC_FUNC_IMPL(GAME_IMP(Sound_Play));
PPC_FUNC(GAME_HOOK(Sound_Play))
{
    const uint32_t sound = ctx.r3.u32;
    GAME_IMP(Sound_Play)(ctx, base);
    if (!g_held)
        return;
    const uint32_t playback = g_memory.Read<uint32_t>(sound + game::SoundPlayback);
    const uint32_t category = alchemy::SoundCategory(g_memory.Read<uint32_t>(sound + game::SoundProperties));
    if (playback != 0 && alchemy::IsSpokenLine(category) && g_skip.Played(sound, playback, Now()))
        RunFast(false);
}

// A script writes an integer variable: sub(record, const int* value), or sub(const int* value, record*) when the game
// passes script arguments by reference.
PPC_FUNC_IMPL(GAME_IMP(ScriptValue_Set));
PPC_FUNC(GAME_HOOK(ScriptValue_Set))
{
    const uint32_t record = game::ScriptArgumentsByReference ? g_memory.Read<uint32_t>(ctx.r4.u32) : ctx.r3.u32;
    const uint32_t name = g_memory.Read<uint32_t>(record + alchemy::ValueName);
    const double now = Now();
    if (!g_cutscene.Matches(record, name) && !g_skipping.Matches(record, name) && IsSkippableFlag(name))
        g_flags.Written(record, now);
    GAME_IMP(ScriptValue_Set)(ctx, base);

    const bool held = g_cutscene.Read() != 0;
    if (held && !g_held)
        g_flags.HoldStarted(now);
    g_held = held;
    if (!held && !g_skip.Fast())
        return;
    if (now - g_lastCheck < 0.004) // the variables are written many times a frame
        return;
    g_lastCheck = now;
    if (SkipPressed())
    {
        g_skip.Press(now, held, Skippable());
    }
    alchemy::DialogueSkip::Step step = g_skip.Update(now, held, g_skipping.Read() != 0);
    if (step.stop != 0)
    {
        const PPCContext saved = ctx;
        ctx.r3.u32 = step.stop;
        GAME_HOOK(Sound_Stop)(ctx, base);
        ctx = saved;
    }
    RunFast(step.fast);
}
#endif
