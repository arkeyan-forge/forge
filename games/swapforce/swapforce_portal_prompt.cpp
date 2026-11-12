#include "swapforce_portal_prompt.h"

namespace portalprompt
{
    Prompt ClassifySwapForce(uint32_t state, uint32_t player)
    {
        if (state == SwapForceStateError)
            return { Kind::TooMany, 0 };
        if ((state == SwapForceStateIdle || state == SwapForceStateDead) && player < 2)
            return { Kind::Player, int(player) + 1, state == SwapForceStateDead };
        return {};
    }
}
