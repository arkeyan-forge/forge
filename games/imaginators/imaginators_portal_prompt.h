#pragma once

#include "../swapforce/swapforce_portal_prompt.h"
#include <cstdint>

// Imaginators' "place your Skylander on the Portal of Power" prompt: SuperChargers' CMagicMoment singleton again
// (games/superchargers/superchargers_portal_prompt.h), its fields moved, and Swap Force's states again
// (games/imaginators/docs/toybox.md, "The portal prompt"):
// - 0x837F649C holds the instance pointer (slot 7 of the class's vtable 0x8216099C, sub_82CC2118, stores it; slot 8,
//   sub_82CC2128, clears it), 0 when there is none;
// - +0x40 the state (EMagicMomentState, names at 0x82157FA0 in value order): Swap Force's numbers, 0 Inactive,
//   3 TransitionIn, 5 Idle (the prompt is up), 7 IdleOutro, 13 Dead, 16 Error (SuperChargers' FirstVehicleSequence is
//   gone), so ClassifySwapForce reads them;
// - +0x210 the player asked for a Skylander: 0 player 1, 1 player 2, -1 none.
namespace portalprompt
{
    constexpr uint32_t ImaginatorsMagicMomentInstance = 0x837F649C;
    constexpr uint32_t ImaginatorsStateOffset = 0x40, ImaginatorsPlayerOffset = 0x210;
}
