// Skipping a spoken line held by a cinematic the game does not let the player skip [Lux] [SSA]
// (docs/engines/lux/dialogue_skip.md, engines/lux/dialogue_skip.h for the rules).
//
// lux::UITextDialog shows a line and plays its voice; a box that the game lets the player close by hand has the byte
// BoxAdvance set, and setting BoxDone closes it as the game does then, stopping the voice. lux::CinemaTrigger runs a
// cinematic: each of its tracks is an animation whose time steps by rate * dt and which reports its end once it reaches
// it; the cinematic ends when a track has ended and no line holds it. A cinematic whose own skip is off keeps its clock
// running under a line, so closing the box alone would leave the player waiting: the tracks then play fast (FastTime) and
// the cinematic ends, or reaches its next line, through the game's own steps. A line shown with the hero's controls
// locked and no cinematic is held by the level's script, which waits for its box: closing it is enough.
#include "dialogue_skip.h"
#include "game_hooks.h"
#include "memory.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ppc_config.h>
#include <ppc_context.h>

#if defined(GAME_HOOK_TextDialog_Update) && defined(GAME_HOOK_Cinematic_Update) && defined(GAME_HOOK_Cinematic_End)
namespace
{
    // lux::UITextDialog: done (the game closes the box), the hand advance flag, its voice (a sound whose word
    // SoundPlaying is the voice's playing id, 0 once it ended).
    constexpr uint32_t BoxDone = 12, BoxAdvance = 63, BoxVoice = 84, SoundPlaying = 8;
    // lux::CinemaTrigger: its tracks (a list: the head at Tracks, the first node at Tracks + 4, the next at node + 4,
    // the animation at node + 20) and its own skip (on when SkipAlways, or SkipAllowed without SkipBlocked).
    constexpr uint32_t Tracks = 84, NodeNext = 4, NodeTrack = 20;
    constexpr uint32_t SkipAllowed = 496, SkipBlocked = 497, SkipAlways = 498;
    // An animation: rate (1: one frame per 60th of a second), time and end (frames), loops.
    constexpr uint32_t TrackRate = 40, TrackTime = 240, TrackEnd = 248, TrackLoops = 268;

    const auto g_processStart = std::chrono::steady_clock::now(); // set at start-up, like the file log's origin
    lux::DialogueSkip g_skip;
    uint32_t g_cinematic = 0; // the locked cinematic updated last (0: none)
    uint32_t g_skippable = 0; // the cinematic the game skips itself updated last (0: none)
    bool g_running = false; // its clock ran at its last update
    // Box updates so far, and their count at each kind of cinematic's last update.
    uint32_t g_boxUpdates = 0, g_cinematicAt = 0, g_skippableAt = 0;
    uint32_t g_fastCinematic = 0;

    float ReadFloat(uint32_t address)
    {
        uint32_t word = g_memory.Read<uint32_t>(address);
        float value;
        memcpy(&value, &word, sizeof value);
        return value;
    }

    void WriteFloat(uint32_t address, float value)
    {
        uint32_t word;
        memcpy(&word, &value, sizeof word);
        g_memory.Write<uint32_t>(address, word);
    }

    bool Paused()
    {
        return g_memory.Read<uint32_t>(game::PausedFlag) != 0 || g_memory.Read<uint32_t>(game::MenuState) != 0;
    }

    bool SkipPressed()
    {
        uint32_t mask = g_memory.Read<uint32_t>(game::SkipMask);
        for (uint32_t pad = 0; pad < game::PadCount; pad++)
        {
            uint32_t buttons = game::PadButtons + pad * game::PadStride;
            if ((g_memory.Read<uint32_t>(buttons - game::PadConnected) & 1) != 0 &&
                (g_memory.Read<uint32_t>(buttons) & mask) != 0)
                return true;
        }
        return false;
    }

    bool GameSkips(uint32_t cinematic)
    {
        return g_memory.Read<uint8_t>(cinematic + SkipAlways) != 0 ||
               (g_memory.Read<uint8_t>(cinematic + SkipAllowed) != 0 &&
                   g_memory.Read<uint8_t>(cinematic + SkipBlocked) == 0);
    }

    void PlayFast(uint32_t cinematic, float dt)
    {
        uint32_t head = cinematic + Tracks;
        for (uint32_t node = g_memory.Read<uint32_t>(head + NodeNext); node != head && node != 0;
            node = g_memory.Read<uint32_t>(node + NodeNext))
        {
            uint32_t track = g_memory.Read<uint32_t>(node + NodeTrack);
            if (track == 0 || g_memory.Read<uint8_t>(track + TrackLoops) != 0)
                continue;
            float time = ReadFloat(track + TrackTime);
            float fast = lux::FastTime(time, ReadFloat(track + TrackEnd), ReadFloat(track + TrackRate), dt * 60.0f);
            if (fast != time)
                WriteFloat(track + TrackTime, fast);
        }
    }
}

PPC_FUNC_IMPL(GAME_IMP(TextDialog_Update));
PPC_FUNC(GAME_HOOK(TextDialog_Update))
{
    uint32_t box = ctx.r3.u32;
    uint32_t sound = g_memory.Read<uint32_t>(box + BoxVoice);
    uint32_t playing = sound != 0 ? g_memory.Read<uint32_t>(sound + SoundPlaying) : 0;
    g_boxUpdates++;
    // A cinematic that went away without ending (its level unloaded) holds no box.
    bool held = g_cinematic != 0 && g_boxUpdates - g_cinematicAt <= 2;
    bool skippable = g_skippable != 0 && g_boxUpdates - g_skippableAt <= 2;
    bool frozen =
        g_memory.Read<uint32_t>(game::ControlLocks) != 0 || int8_t(g_memory.Read<uint8_t>(game::ControlLockCount)) > 0;
    bool locked =
        lux::LineLocked(held, skippable, frozen) && g_memory.Read<uint8_t>(box + BoxAdvance) == 0 && !Paused();
    if (g_skip.Box(playing, locked && SkipPressed(), locked, held && g_running))
    {
        g_memory.Write<uint8_t>(box + BoxDone, 1);
        g_fastCinematic = g_skip.Fast() ? g_cinematic : 0;
        fprintf(stderr, "[dialogue] line skipped at %.3f s%s\n",
            std::chrono::duration<double>(std::chrono::steady_clock::now() - g_processStart).count(),
            g_fastCinematic != 0 ? ", the cinematic plays fast" : "");
    }
    GAME_IMP(TextDialog_Update)(ctx, base);
}

PPC_FUNC_IMPL(GAME_IMP(Cinematic_Update));
PPC_FUNC(GAME_HOOK(Cinematic_Update))
{
    uint32_t cinematic = ctx.r3.u32;
    float dt = float(ctx.f1.f64); // seconds
    bool locked = !GameSkips(cinematic);
    if (locked)
    {
        g_cinematic = cinematic;
        g_running = dt > 0.0f;
        g_cinematicAt = g_boxUpdates;
    }
    else
    {
        g_skippable = cinematic;
        g_skippableAt = g_boxUpdates;
        if (g_cinematic == cinematic)
            g_cinematic = 0;
    }
    if (g_skip.Fast() && cinematic == g_fastCinematic && !Paused())
        PlayFast(cinematic, dt);
    GAME_IMP(Cinematic_Update)(ctx, base);
}

PPC_FUNC_IMPL(GAME_IMP(Cinematic_End));
PPC_FUNC(GAME_HOOK(Cinematic_End))
{
    uint32_t cinematic = ctx.r3.u32;
    if (cinematic == g_cinematic)
        g_cinematic = 0;
    if (cinematic == g_skippable)
        g_skippable = 0;
    if (cinematic == g_fastCinematic)
    {
        g_fastCinematic = 0;
        g_skip.Ended();
    }
    GAME_IMP(Cinematic_End)(ctx, base);
}
#endif
