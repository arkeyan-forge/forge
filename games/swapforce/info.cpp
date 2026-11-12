#include "host/launcher.h"

// Swap Force, for the disc check (host/launcher.cpp). The hashes identify the player's own default.xex; they are not game
// content.
namespace
{
    const GameRelease SwapForceReleases[] = {
        // Redump "Skylanders - Swap Force (USA, Australia) (En,Fr)", 8738846720 bytes: the reference disc.
        // SHA-256 of its default.xex (18563072 bytes), computed from the player's image.
        { "USA, Australia", 0x415608E9, 0x22F1FB91, 0x00000021,
            "5d9a3bc3c2d85ed59684a9c5f94daf6bffcfdfedcef7650a866737edbf3f4ac3",
            "f6648b7ea47b43136e82e2b7dab5935018e86983179c4db6ecef177c5a1edf26" },
        // Redump "Skylanders - Swap Force (Europe) (Es,It)", 8738846720 bytes: the same loaded image as the reference disc
        // (byte for byte); only the XEX header differs (media id, version, region).
        { "Europe", 0x415608E9, 0x74FDDE06, 0x0000001C,
            "3db455f6ee17e8cb3a4d58873d96583d9160396d360a0130e5eb88adad8a5518",
            "f6648b7ea47b43136e82e2b7dab5935018e86983179c4db6ecef177c5a1edf26" },
    };
}

extern const GameInfo g_gameInfo = { "swapforce", "Skylanders Swap Force", SwapForceReleases, "Skylanders Swap Force",
    0xBFE04A };
