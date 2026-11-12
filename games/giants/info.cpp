#include "host/launcher.h"

// Giants, for the disc check (host/launcher.cpp). The hashes identify the player's own default.xex; they are not game
// content.
namespace
{
    const GameRelease GiantsReleases[] = {
        // Redump "Skylanders - Giants (USA, Europe) (En,Fr,De,Es,It,Nl,Pt,Sv,No,Da,Fi)", XGD3, 8738846720 bytes.
        // SHA-256 of its default.xex (7020544 bytes), computed from the player's image.
        { "USA, Europe", 0x415608DA, 0x52FFE94B, 0x00000002,
            "12d1e29e4f07fbc559bc1a4988dd6bf346c3d16e30cbf862e2683e862f9fc09e",
            "43a64a6d9795408e48fd39cb655361dc1be4ef9f22f60ddc09621139aa4c4b17" },
    };
}

extern const GameInfo g_gameInfo = { "giants", "Skylanders Giants", GiantsReleases, "Skylanders Giants", 0xFFB43A };
