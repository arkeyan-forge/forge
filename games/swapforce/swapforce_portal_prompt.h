#pragma once

#include "host/portal_prompt.h"
#include <cstdint>

// Swap Force's "place your Skylander on the Portal of Power" prompt. Everything around it (asking for a figure, bringing
// one in, the "too many" screens) is run by one engine object, the CMagicMoment singleton (public facts:
// tsapZ/SwapForceRecomp src/game/magic_moment_watch.cpp, MIT; the addresses were checked against this disc's code and a
// run, games/swapforce/docs/history.md, "Figures and the portal"):
// - 0x83131BE0 holds the instance pointer (set by sub_82858F70, cleared by sub_82858FC0), 0 when there is none;
// - +0x20 the state (EMagicMomentState, names at 0x820921C4): 0 Inactive, 3 the zoom out to the portal, 5 Idle (the
//   prompt is up, waiting for a figure), 7 IdleOutro (a figure came), 13 Dead (a player's Skylander died, the game waits
//   for that player's next one), 16 Error (a portal error screen: too many toys...);
// - +0xE8 the reason (EMagicMomentReason): 0 ToyRemoved, 1 TooManyCharacterToys, 2 TooManyItemToys, 3 InvalidToy,
//   4 ErrorOnToy, 5 PlayerDeath, 6 FirstTime, 7 MapStart...;
// - +0x148 the player asked: 0 player 1, 1 player 2, -1 none.
namespace portalprompt
{
    constexpr uint32_t SwapForceMagicMomentInstance = 0x83131BE0;
    constexpr uint32_t SwapForceStateOffset = 0x20, SwapForceReasonOffset = 0xE8, SwapForcePlayerOffset = 0x148;
    constexpr uint32_t SwapForceStateIdle = 5, SwapForceStateDead = 13, SwapForceStateError = 16;

    // The prompt for a CMagicMoment state and player index (0, 1; anything else is nobody): Idle and Dead ask that
    // player for a Skylander; Error is a "too many" screen, not a request.
    Prompt ClassifySwapForce(uint32_t state, uint32_t player);
}
