#include "host/toybox/grid.h"
#include <algorithm>

namespace toybox
{
    int Grid::HeaderY(int section) const
    {
        const ListSection& s = sections[section];
        return RowY(s.firstRow) - (s.titled ? headerHeight : 0);
    }

    void Grid::Fix()
    {
        if (cells.empty())
        {
            cursor = 0;
            scroll = 0;
            return;
        }
        cursor = std::clamp(cursor, 0, int(cells.size()) - 1);
        const Cell& cell = cells[cursor];
        int top = RowY(cell.row), bottom = RowY(cell.row) + rowHeight;
        // The section's header stays in view with its first row.
        if (cell.row == sections[cell.section].firstRow)
            top = HeaderY(cell.section);
        scroll = std::clamp(scroll, bottom - visibleHeight, top);
        scroll = std::clamp(scroll, 0, MaxScroll());
    }

    void Grid::ShowSection(int section)
    {
        for (size_t i = 0; i < cells.size(); i++)
        {
            if (cells[i].section == section)
            {
                cursor = int(i);
                break;
            }
        }
        scroll = HeaderY(section);
        Fix();
    }
}
