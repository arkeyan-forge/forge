#pragma once
// The checks (SKY_TESTS_ONLY, no game data): stands in for the generated ppc_config.h so that clang-tidy can parse the runtime's
// sources. No policy macro is set (the translation's defaults); the four layout bounds read the active layout, as with
// ARKEYAN_LAYOUT_RECORD. Never built into a game.
#define PPC_CONFIG_H_INCLUDED
#include "../guest_layout_macros.h"
