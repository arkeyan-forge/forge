#include "host/launcher.h"

// SuperChargers, for the disc check (host/launcher.cpp). The hash identifies the player's own default.xex; it is not game
// content.
namespace
{
    const GameRelease SuperChargersReleases[] = {
        // Redump "Skylanders - SuperChargers (Europe)", 8738846720 bytes.
        // SHA-256 of its default.xex (24985600 bytes), computed from the player's image.
        { "Europe", 0x4156091F, 0x74144F53, 0x00000007,
            "02ff6672d460c9aaa54e69d584af67a7138c6c93cde5e04b0da4284207be7c61",
            "f2c26c0153255c100bd5f1baceec7f2d3f26903f79047a0dc6ceb51d9a8e70bc" },
    };
}

extern const GameInfo g_gameInfo = { "superchargers", "Skylanders SuperChargers", SuperChargersReleases,
    "Skylanders SuperChargers", 0xF07008 };
