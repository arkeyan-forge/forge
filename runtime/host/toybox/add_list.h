#pragma once

// Add a figure's list (docs/runtime/host/toybox_ux.md, "Add a figure"): the toys a game reads, in their sections, and
// the grid of cards they make. Plain C++ on the catalogue, so a test can drive it (ArkeyanToyboxAddListTest).
#include "host/collection/catalogue.h"
#include "host/toybox/grid.h"
#include <string>
#include <vector>

namespace toybox
{
    // One section of the list: an element's characters, or one category of the other toys.
    struct AddGroup
    {
        collection::Section::Kind kind;
        int value = 0;
        std::vector<const collection::Toy*> toys;
    };

    // The toys `game` reads, by element then by category; empty groups are kept. `fits`: the families of the seat the
    // list is for (Seat::accepts), the other toys left out; null: every toy.
    std::vector<AddGroup> AddGroups(const std::string& game, const std::vector<collection::Category>* fits);

    // The cards of the non-empty groups, `columns` per row; `toys` gets one entry per card (Cell::figure indexes it).
    // `showAll`: a last row of its own, a Cell::Kind::Fold that lists every toy again.
    Grid AddGrid(
        const std::vector<AddGroup>& groups, int columns, bool showAll, std::vector<const collection::Toy*>& toys);
}
