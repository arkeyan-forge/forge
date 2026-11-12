#include "superchargers_portal_prompt.h"

namespace portalprompt
{
    Prompt ClassifySuperChargers(uint32_t state, uint32_t reason, uint32_t player)
    {
        if (state == SuperChargersStateError)
            return { Kind::TooMany, 0 };
        if (state != SuperChargersStateIdle && state != SuperChargersStateDead)
            return {};
        if (player < 2)
            return { Kind::Player, int(player) + 1, state == SuperChargersStateDead };
        if (reason == SuperChargersReasonVehicleRequired || reason == SuperChargersReasonVehicleAdded)
            return { Kind::Player, 1, false, true };
        return {};
    }
}
