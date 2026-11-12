#include "giants_portal_prompt.h"

namespace portalprompt
{
    Prompt ClassifyGiants(std::string_view errorKey, std::string_view stateName)
    {
        if (errorKey.empty())
            return {};
        bool asks = errorKey.starts_with("203_") || errorKey.starts_with("301_") || errorKey.starts_with("304_");
        if (asks)
            return { Kind::Player, stateName.find(" P2") != std::string_view::npos ? 2 : 1 };
        // 101 TooManyTags_Gen, 102 TooManyMinions, 103 TooManyItems, 105 RemoveItems, 106 RemoveLocations, 111
        // TooManyLocations, 112 ToyDataMismatch, 305 ClearThePortal; 306 ConfirmPVP is a question the buttons answer.
        for (const char* many : { "101_", "102_", "103_", "105_", "106_", "111_", "112_", "305_" })
        {
            if (errorKey.starts_with(many))
                return { Kind::TooMany, 0 };
        }
        return {};
    }
}
