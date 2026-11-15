#pragma once

// When the Toy Box puts a figure on again because the game has not read it (docs/runtime/host/toybox_ux.md, "A quick
// swap the game missed"). A figure swapped in while the game still loads the previous one can be left unread, the game
// showing it as loading for good; lifting and placing it again recovers. Plain C++ for ArkeyanToyboxSwapTest.
#include <cstdint>

namespace toybox::retake
{
    constexpr int MaxTries = 3;

    // The title starts reading a toy at block 0. A read of another block right after a swap is the end of the previous
    // toy's read (Giants then cancels its arrival and waits on "Loading" for good): not the new figure taken.
    constexpr bool Taken(uint64_t headRead, uint64_t placedAt) { return headRead != 0 && headRead >= placedAt; }

    constexpr uint64_t FirstWaitNs = 1'000'000'000ull;

    // `tries` re-places done since the player's own placement; `unreadNs` since the last one. Waits 1 s, 2 s, then 4 s.
    constexpr bool Due(int tries, uint64_t unreadNs) { return tries < MaxTries && unreadNs > (FirstWaitNs << tries); }
}
