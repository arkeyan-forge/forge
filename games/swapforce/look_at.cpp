// Swap Force's head turns at a frame rate above 30 (issue #79) [SwapForce].
//
// The characters' behaviour graphs turn their heads toward a target with Havok Behavior's look-at modifier. Its modify step
// (sub_82F0E3B8, games/swapforce/addresses.h) moves the look-at weight toward 1 or 0 by a fixed share of the gap per call,
// the on gain or the off gain (0.05 to 0.2 in Woodburrow), and is called once a frame per modifier: at 60 fps it is called
// twice as often and a head turns twice as fast (a turn of the weight from 0.1 to 0.9: 1.43-1.47 s at 30, 0.72 s at 60).
// The time step it is given is 0, so the time since the modifier's previous call is measured here and the gains are
// scaled to it for the call only (look_at_gain.h; restored after it): 1.43-1.45 s at 60. Untouched at a limit of 30.
#include "game_hooks.h"
#include "kernel/video.h"
#include "look_at_gain.h"
#include "memory.h"
#include <SDL3/SDL_timer.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <ppc_config.h>
#include <ppc_context.h>
#include <unordered_map>

#ifdef GAME_HOOK_LookAtModify
namespace
{
    // The seconds since `modifier`'s previous call (0 for a first call); the Havok work pool may call modifiers from
    // several threads.
    double SecondsSinceLastCall(uint32_t modifier)
    {
        static std::mutex mutex;
        static std::unordered_map<uint32_t, uint64_t> lastCall; // modifier -> SDL_GetTicksNS
        const uint64_t now = SDL_GetTicksNS();
        std::lock_guard lock(mutex);
        if (lastCall.size() > 512) // modifiers of unloaded characters
            std::erase_if(lastCall, [now](const auto& entry) { return now - entry.second > 10'000'000'000ull; });
        auto [entry, added] = lastCall.try_emplace(modifier, now);
        const double seconds = added ? 0.0 : double(now - entry->second) / 1e9;
        entry->second = now;
        return seconds;
    }
}

PPC_FUNC_IMPL(GAME_IMP(LookAtModify));
// (modifier, context, output) -> void.
PPC_FUNC(GAME_HOOK(LookAtModify))
{
    const uint32_t modifier = ctx.r3.u32;
    if (FrameScale() >= 1.0f || modifier == 0)
    {
        GAME_IMP(LookAtModify)(ctx, base);
        return;
    }
    const double seconds = SecondsSinceLastCall(modifier);
    const uint32_t onAt = modifier + game::LookAtOnGainOffset, offAt = modifier + game::LookAtOffGainOffset;
    const float on = g_memory.Read<float>(onAt), off = g_memory.Read<float>(offAt);
    static std::atomic<bool> logged{ false };
    if (!logged.exchange(true))
        fprintf(stderr, "[lookat] head-turn gains follow the frame time (on %g, off %g at 30 fps)\n", on, off);
    g_memory.Write<float>(onAt, lookat::Gain(on, seconds));
    g_memory.Write<float>(offAt, lookat::Gain(off, seconds));
    GAME_IMP(LookAtModify)(ctx, base);
    g_memory.Write<float>(onAt, on);
    g_memory.Write<float>(offAt, off);
}
#endif
