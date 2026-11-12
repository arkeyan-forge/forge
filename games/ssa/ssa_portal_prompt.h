#pragma once

#include "host/portal_prompt.h"
#include <cstdint>

// Spyro's Adventure's "place a Skylander on the portal" prompt: where the title keeps it and how to read it.
// The prompt object (a function-local static of sub_82205D18) has its mode at +56 (2 while the portal screen is up, 4
// while a figure comes in, 0 in play); while the screen is up, the text it shows tells who is asked: the CRC-32 of the
// text's key in global.lxb, which does not depend on the language. Public facts: tsapZ/SSARecomp docs/runtime/host/portal.md (MIT);
// checked here against the running title.
namespace portalprompt
{
    constexpr uint32_t SsaPromptObject = 0x82C569B0, SsaPromptModeOffset = 56, SsaPromptScreenMode = 2;
    constexpr uint32_t SsaHud = 0x82C56AF8, SsaHudMessageOffset = 3260, SsaMessageTextOffset = 376,
                       SsaTextHashOffset = 432;
    constexpr uint32_t SsaPlayer1Hash = 0x049B83C4, SsaPlayer2Hash = 0x9D92D27E;

    // The prompt for what the title shows: its mode and the hash of the portal text.
    Prompt ClassifySsa(uint32_t mode, uint32_t textHash);
}
