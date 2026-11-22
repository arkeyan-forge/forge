#pragma once

// The Toy Box's sectioned grids (your figures, Add a figure): the cells, the sections and the scroll.
#include "host/collection/collection.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace toybox
{
    // One cell of the list: a figure, the "+ Add" card of a section, or the folded "Not for this game" line.
    struct Cell
    {
        enum class Kind
        {
            Figure,
            Add,
            Fold
        } kind = Kind::Figure;
        int figure = -1; // index in Model::figures
        int section = 0; // index in Model::sections
        int row = 0, column = 0;
        collection::Element element = collection::Element::None; // Add: the section's element
    };

    struct ListSection
    {
        collection::Section::Kind kind;
        int value = 0;
        std::string game;
        int count = 0;
        bool foreign = false; // "Not for this game": greyed, cannot be placed
        bool titled = true; // a header above its first row (the shelf has none: the screen says what it is for)
        bool folded = false;
        int firstRow = 0; // grid rows, the header is drawn above firstRow
    };

    // The cells of a sectioned grid with a fixed number of columns.
    struct Grid
    {
        std::vector<ListSection> sections;
        std::vector<Cell> cells;
        int rows = 0;
        int cursor = 0; // index in cells
        // The column Up and Down keep (nav::GridMove), while the cursor stays where the last Up or Down put it.
        int keep = 0, keptAt = -1;
        int scroll = 0; // pixels scrolled
        // Layout in pixels, set by the model for the drawing: a header above the first row of each titled section.
        int rowHeight = 100, headerHeight = 40, visibleHeight = 400;
        int RowY(int row) const // top of a row, from the top of the list
        {
            int headers = 0;
            for (const ListSection& section : sections)
            {
                if (section.firstRow <= row && section.titled)
                    headers++;
            }
            return row * rowHeight + headers * headerHeight;
        }
        int HeaderY(int section) const; // top of a section's header (the row above its first, when titled)
        void
        Fix(); // the cursor in the cells, and the least scroll that shows it (its section's header too, on a first row)
        void ShowSection(int section); // the cursor on the section's first card, its header at the top of the view
        int MaxScroll() const { return std::max(RowY(rows - 1) + rowHeight - visibleHeight, 0); }
        // The mouse wheel: the view moves `count` rows (down when positive), the cursor stays. False at an end.
        bool ScrollRows(int count)
        {
            if (cells.empty())
                return false;
            int next = std::clamp(scroll + count * rowHeight, 0, MaxScroll());
            return std::exchange(scroll, next) != next;
        }
    };
}
