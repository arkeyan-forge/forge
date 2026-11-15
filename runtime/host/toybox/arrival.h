#pragma once

// When the Toy Box holds a swap (docs/runtime/host/toybox_ux.md, "A quick swap the game missed"): a figure swapped in
// while the game still brings the previous one in is often dropped (read and cancelled, or left on "Loading"), so the
// new one waits for the arrival to end, the player's last pick winning. Plain C++ for ArkeyanToyboxSwapTest.
#include <cstdint>

namespace toybox::arrival
{
    // A figure the Toy Box just put on and the game has not taken yet: its arrival is about to start.
    constexpr uint64_t SettleNs = 1'000'000'000ull;
    // At most this long, so a game that never says its arrival ended (a menu, a state read wrong) still gets the figure.
    constexpr uint64_t MaxWaitNs = 5'000'000'000ull;

    // `arriving`: game::FigureArriving; `unsettledNs`: since the newest figure the game has not taken yet (UINT64_MAX:
    // none).
    constexpr bool Locked(bool arriving, uint64_t unsettledNs) { return arriving || unsettledNs < SettleNs; }
    constexpr bool Release(bool locked, uint64_t queuedNs) { return !locked || queuedNs >= MaxWaitNs; }
}
