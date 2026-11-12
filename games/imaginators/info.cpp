#include "host/launcher.h"

// Imaginators, for the disc check (host/launcher.cpp). The hash identifies the player's own default.xex; it is not game
// content.
namespace
{
    const GameRelease ImaginatorsReleases[] = {
        // Redump "Skylanders - Imaginators (Europe) (Fr,Nl)", 8738846720 bytes.
        // SHA-256 of its default.xex (25739264 bytes), computed from the player's image.
        { "Europe", 0x41560930, 0x0133F901, 0x00000004,
            "1c6d7e05ef6d30ba0b86ff3a810ba115bef3050998254471230cd8c07641d0ac",
            "4f793c38af18299cc2dd71a062dcd5ab5e9c4fb8b42769b791bd947a32859d02" },
    };
}

extern const GameInfo g_gameInfo = { "imaginators", "Skylanders Imaginators", ImaginatorsReleases,
    "Skylanders Imaginators", 0x58A8D8 };
