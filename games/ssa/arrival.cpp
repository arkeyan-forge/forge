// Spyro's Adventure's figure arrival (game::FigureArriving), apart from adapter.cpp: this file needs no translated
// headers, so the code checks of a build without the game read it too.
#include "game.h"
#include "memory.h"
#include "ssa_portal_prompt.h"

bool game::FigureArriving()
{
    using namespace portalprompt;
    return g_memory.base != nullptr && SsaArriving(g_memory.Read<uint32_t>(SsaPromptObject + SsaPromptModeOffset));
}
