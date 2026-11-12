#pragma once

#include "host/portal_prompt.h"
#include <cstdint>

// SuperChargers' "place your Skylander / your vehicle on the Portal of Power" prompt: Swap Force's CMagicMoment singleton
// again (games/swapforce/swapforce_portal_prompt.h), its fields moved and two states added (games/superchargers/docs/toybox.md,
// "The portal prompt"):
// - 0x83764ED0 holds the instance pointer (the class's vtable slot 6, sub_82B49390, stores it; slot 7, sub_82B493A0, clears
//   it), 0 when there is none;
// - +0x2C the state (EMagicMomentState, names at 0x820976A0 in value order): 0 Inactive, 3 TransitionIn, 5 Idle (the prompt
//   is up), 7 FirstVehicleSequence (new here), 8 IdleOutro, 11 PlayIntro, 14 Dead, 17 Error;
// - +0x168 the reason (EMagicMomentReason, values at 0x820974B0): 13 VehicleRequired as "Please place a LAND VEHICLE" comes up,
//   then 12 (VehicleAdded by the names' order) while it stays up with no vehicle (run scP3), 0 for a Skylander;
// - +0x1FC the player asked for a Skylander: 0 player 1, -1 none (still -1 while a vehicle is asked for).
namespace portalprompt
{
    constexpr uint32_t SuperChargersMagicMomentInstance = 0x83764ED0;
    constexpr uint32_t SuperChargersStateOffset = 0x2C, SuperChargersReasonOffset = 0x168,
                       SuperChargersPlayerOffset = 0x1FC;
    constexpr uint32_t SuperChargersStateIdle = 5, SuperChargersStateDead = 14, SuperChargersStateError = 17;
    constexpr uint32_t SuperChargersReasonVehicleRequired = 13, SuperChargersReasonVehicleAdded = 12;

    // The prompt for a CMagicMoment state, reason and player index (0, 1; anything else is nobody): Idle and Dead ask that
    // player for a Skylander; with nobody named and a vehicle reason (13 or 12) they ask for a vehicle (player 1's: no
    // field naming the player was seen); Error is a "too many" screen, not a request.
    Prompt ClassifySuperChargers(uint32_t state, uint32_t reason, uint32_t player);
}
