#pragma once

// Add a figure's versions panel (docs/runtime/host/toybox_ux.md, "Add a figure"): the game each version came with.
#include "host/collection/catalogue.h"
#include <cstdint>
#include <string>

namespace toybox
{
    // A game's name without the series' prefix: "Giants" for "Skylanders: Giants".
    std::string ShortGameName(const collection::Game& game);

    // The short name of the game a toy's variant first came with (Catalogue::Origin): "Giants" for Series 2 Eruptor.
    // Empty when the catalogue does not know the toy's game.
    std::string VersionGame(const collection::Toy& toy, uint16_t variant);
}
