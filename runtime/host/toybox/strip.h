#pragma once

// Where the section strip's chips go: they wrap onto as many rows as they need, like words, so that every section shows.
// Plain C++, so a test can drive it (ArkeyanToyboxStripTest).
#include <span>
#include <vector>

namespace toybox::strip
{
    struct Place
    {
        float x = 0, y = 0; // from the strip's top left corner
    };

    struct Layout
    {
        std::vector<Place> chips;
        float height = 0; // 0 with no chip
        int rows = 0;
    };

    // A chip wider than `width` gets a row of its own.
    inline Layout Wrap(std::span<const float> widths, float width, float gap, float rowHeight)
    {
        Layout layout;
        layout.chips.reserve(widths.size());
        float x = 0;
        for (float chip : widths)
        {
            if (layout.rows == 0 || (x > 0 && x + chip > width))
            {
                layout.rows++;
                x = 0;
            }
            layout.chips.push_back({ x, float(layout.rows - 1) * (rowHeight + gap) });
            x += chip + gap;
        }
        if (layout.rows > 0)
            layout.height = float(layout.rows) * rowHeight + float(layout.rows - 1) * gap;
        return layout;
    }
}
