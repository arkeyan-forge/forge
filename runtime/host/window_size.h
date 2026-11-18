#pragma once

#include <string>
#include <string_view>

// The game window's size in windowed mode ([video] window_size, the remembered window_last): parsing and the size the
// window opens at. Pure functions, no SDL (host/display.cpp applies them).
namespace windowsize
{
    struct Size
    {
        int width = 0;
        int height = 0;
        bool operator==(const Size&) const = default;
    };

    // The window is never made smaller than this, by a setting or by hand.
    constexpr Size Minimum{ 640, 360 };
    constexpr Size Default{ 1280, 720 };

    // "<width>x<height>" with both numbers above 0 (an X works too); false for anything else. A size under the
    // minimum is raised to it, each side on its own.
    bool Parse(std::string_view text, Size& size);

    // "1600x900".
    std::string Format(Size size);

    // The size the window opens at: the chosen one, else the remembered one, else 1280x720; then made to fit `usable`
    // (the display's usable area, both 0 when unknown), never under the minimum unless the display itself is smaller.
    Size Choose(Size chosen, Size last, Size usable);
}
