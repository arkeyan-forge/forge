#include "ssa_portal_prompt.h"

namespace portalprompt
{
    Prompt ClassifySsa(uint32_t mode, uint32_t textHash)
    {
        if (mode != SsaPromptScreenMode)
            return {};
        if (textHash == SsaPlayer1Hash)
            return { Kind::Player, 1 };
        if (textHash == SsaPlayer2Hash)
            return { Kind::Player, 2 };
        // The "too many" texts (Skylanders, toys of different kinds, magic items...) and any other portal text.
        return { Kind::TooMany, 0 };
    }
}
