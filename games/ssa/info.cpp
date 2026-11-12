#include "host/launcher.h"

// Spyro's Adventure, for the disc check (host/launcher.cpp). The hashes identify the player's own default.xex; they are
// not game content.
namespace
{
    const GameRelease SpyrosAdventureReleases[] = {
        // Redump "Skylanders - Spyro's Adventure (USA, Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da,Fi)", XGD3, 8738846720 bytes.
        // SHA-256 of its default.xex (12595200 bytes), computed from the player's image.
        { "USA, Europe", 0x415608AB, 0x53CE7B97, 0x00000002,
            "e195f5130c1e23fb4654baf7c383ef97d02c89aeb7d4389705ae31235646ef7b",
            "f53c42d4e3575f56d39ffa2428bcf86923b7dd80e5b59927b86cf070b33b4c62" },
    };
}

extern const GameInfo g_gameInfo = { "ssa", "Skylanders Spyro's Adventure", SpyrosAdventureReleases,
    "Skylanders Spyro's Adventure", 0x6BBAFF };
