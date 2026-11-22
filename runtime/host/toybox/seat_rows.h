#pragma once

// The portal's seats on the Toy Box's home page: one row, or two when a seat's label would be cut on one
// (docs/runtime/host/toybox_ux.md, "Home"). Pure logic, so that ArkeyanToyboxLookTest checks it.
#include "host/toybox/nav.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace toybox::seat_rows
{
    // The seats as drawn, row by row, each a list of seat indices (the game's order). Two rows: the players' seats
    // (players and vehicles) first, then the shared ones (item, adventure, trap).
    inline std::vector<std::vector<int>> Rows(const std::vector<bool>& playerSeat, bool two)
    {
        std::vector<std::vector<int>> rows(1);
        for (size_t i = 0; i < playerSeat.size(); i++)
            if (!two || playerSeat[i])
                rows[0].push_back(int(i));
        if (two)
        {
            rows.emplace_back();
            for (size_t i = 0; i < playerSeat.size(); i++)
                if (!playerSeat[i])
                    rows[1].push_back(int(i));
            if (rows[1].empty() || rows[0].empty())
                return Rows(playerSeat, false);
        }
        return rows;
    }

    // The seats' rows and their width, the same on every row so that they line up.
    struct Layout
    {
        std::vector<std::vector<int>> rows;
        float seatWidth = 0;
    };

    // `room`: the width the first row's seats share (the page less Add a figure and Manage), `gap` after each seat;
    // `textRoom(width)`: the room a seat that wide leaves its text, `widest`: the widest seat label. Two rows when the
    // widest label does not fit on one.
    template <typename TextRoom>
    Layout Fit(
        const std::vector<bool>& playerSeat, float room, float gap, float maxWidth, float widest, TextRoom textRoom)
    {
        auto width = [&](size_t count) {
            return std::min(maxWidth, std::floor((room - gap * float(count)) / float(std::max<size_t>(count, 1))));
        };
        Layout layout{ Rows(playerSeat, false), width(playerSeat.size()) };
        if (textRoom(layout.seatWidth) >= widest)
            return layout;
        layout.rows = Rows(playerSeat, true);
        size_t most = 0;
        for (const std::vector<int>& row : layout.rows)
            most = std::max(most, row.size());
        layout.seatWidth = width(most);
        return layout;
    }

    // The row and the place in it of a seat; {-1, -1} when it is not drawn.
    struct Place
    {
        int row = -1, index = -1;
    };

    inline Place Find(const std::vector<std::vector<int>>& rows, int seat)
    {
        for (size_t r = 0; r < rows.size(); r++)
            for (size_t i = 0; i < rows[r].size(); i++)
                if (rows[r][i] == seat)
                    return { int(r), int(i) };
        return {};
    }

    // A pad move from a seat: Left and Right along its row, Up and Down to the seat nearest under it in the other row
    // (`spans` by seat index, as drawn). Returns the new seat, or -1 when the move leaves the seats: Up from the first
    // row, Down from the last, Right from the end of the first row (to Add a figure and Manage).
    inline int Move(
        const std::vector<std::vector<int>>& rows, const std::vector<nav::Span>& spans, int seat, nav::Move move)
    {
        const Place at = Find(rows, seat);
        if (at.row < 0)
            return -1;
        const std::vector<int>& row = rows[size_t(at.row)];
        switch (move)
        {
        case nav::Move::Left: return at.index > 0 ? row[size_t(at.index) - 1] : seat;
        case nav::Move::Right:
            if (at.index + 1 < int(row.size()))
                return row[size_t(at.index) + 1];
            return at.row == 0 ? -1 : seat;
        case nav::Move::Up:
        case nav::Move::Down:
        {
            const int next = at.row + (move == nav::Move::Down ? 1 : -1);
            if (next < 0 || next >= int(rows.size()))
                return -1;
            std::vector<nav::Span> under;
            for (int s : rows[size_t(next)])
                under.push_back(s < int(spans.size()) ? spans[size_t(s)] : nav::Span{});
            const float x = seat < int(spans.size()) ? spans[size_t(seat)].Centre() : 0;
            const int nearest = nav::Nearest(under, x);
            return rows[size_t(next)][size_t(nearest < 0 ? 0 : nearest)];
        }
        }
        return seat;
    }
}
