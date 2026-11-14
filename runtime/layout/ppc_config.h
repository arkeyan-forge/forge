#pragma once
// ARKEYAN_LAYOUT_RECORD builds put this folder before the game's ppc/ for runtime sources: the generated ppc_config.h's
// policy macros as they are, the four layout bounds from the active layout (guest_layout_macros.h). Only
// guest_layout_record.cpp, which defines ARKEYAN_LAYOUT_CONSTANTS, sees the generated constants.
#include_next <ppc_config.h>
#ifndef ARKEYAN_LAYOUT_CONSTANTS
#include "../guest_layout_macros.h"
#endif
