#pragma once

#include "host/portal_prompt.h"
#include <cstdint>
#include <string_view>

// Giants' "place a Skylander on the portal" prompt. Giants has no prompt object: its portal screens are run by a game
// script, the "Portal Control" state machine, and two of the script's variables say what is on screen (public facts:
// tsapZ/GiantsRecomp docs/runtime/host/portal.md and src/game/portal_prompt_watch.cpp, MIT; every vtable and offset below was
// checked against the running title, docs/runtime/host/portal.md):
// - script variables are ScriptReference objects (vtable 0x820DF750): +8 a pointer to the variable's name, +0x1C its value;
// - "portal error string": 0 while no portal screen is up, else a StringInfo (vtable 0x820DD548) whose +8 points to its
//   text key, "203_PortalError_TagTypeNotOnPortal" = "Please put a Skylander on the Portal of Power.";
// - "current state name": a state object (vtable 0x820DFE40) whose +8 points to its name: "Portal Control - Portal - 0 -
//   Idle P1 Only" at player 1's prompt, "... Idle P2 Only" at player 2's, "Portal Control - In Game - 0 - Idle" in play.
namespace portalprompt
{
    constexpr uint32_t GiantsScriptReferenceVtable = 0x820DF750, GiantsStringInfoVtable = 0x820DD548,
                       GiantsStateVtable = 0x820DFE40;
    constexpr uint32_t GiantsNameOffset = 8, GiantsValueOffset = 0x1C;
    constexpr std::string_view GiantsErrorVariable = "portal error string", GiantsStateVariable = "current state name";
    constexpr std::string_view GiantsPortalStatePrefix = "Portal Control - ";

    // The prompt for what the title shows: the key of the portal message on screen ("" for none) and the state's name.
    // A figure is asked for (203 TagTypeNotOnPortal, 301 ToyMissing, 304 NeedsRemoval_Minion) by player 1, or by
    // player 2 in the states named "... P2 ..."; the other portal messages (too many toys, locations, data) are not requests.
    Prompt ClassifyGiants(std::string_view errorKey, std::string_view stateName);
}
