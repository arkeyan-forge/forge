#include "host/launcher.h"

// Trap Team, for the disc check (host/launcher.cpp). The hashes identify the player's own default.xex; they are not game
// content.
namespace
{
    const GameRelease TrapTeamReleases[] = {
        // Redump "Skylanders - Trap Team (Europe) (En,De)", 8738846720 bytes.
        // SHA-256 of its default.xex (9252864 bytes), computed from the player's image.
        { "Europe", 0x41560911, 0x7A74A451, 0x00000014,
            "f1d2373e4507e21cc371e6f22d02e909602a1b5bc4704896c3c099cc64002a35",
            "265c807d93716525e611839fb30859c1eb8a2a7cc700074d57f7b4cb3f78d458" },
        // Redump "Skylanders - Trap Team (Europe) (Fr,Nl)", 8738846720 bytes: the same loaded image as the disc above (byte for
        // byte); only the XEX header differs (media id, version).
        { "Europe (Fr, Nl)", 0x41560911, 0x3AF0B6AF, 0x00000012,
            "95e273f479ad71198d7daaa6234e6ee6364861ce29b002ba782154a1fcf08d3c",
            "265c807d93716525e611839fb30859c1eb8a2a7cc700074d57f7b4cb3f78d458" },
    };
}

extern const GameInfo g_gameInfo = { "trapteam", "Skylanders Trap Team", TrapTeamReleases, "Skylanders Trap Team",
    0xDE0018 };
