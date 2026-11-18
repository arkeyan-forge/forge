#include "host/window_size.h"
#include <algorithm>
#include <charconv>

bool windowsize::Parse(std::string_view text, Size& size)
{
    const size_t x = text.find_first_of("xX");
    if (x == std::string_view::npos)
        return false;
    auto number = [](std::string_view part, int& value) {
        auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
        return !part.empty() && error == std::errc() && end == part.data() + part.size() && value > 0;
    };
    Size parsed;
    if (!number(text.substr(0, x), parsed.width) || !number(text.substr(x + 1), parsed.height))
        return false;
    size = { std::max(parsed.width, Minimum.width), std::max(parsed.height, Minimum.height) };
    return true;
}

std::string windowsize::Format(Size size) { return std::to_string(size.width) + "x" + std::to_string(size.height); }

windowsize::Size windowsize::Choose(Size chosen, Size last, Size usable)
{
    Size size = chosen.width > 0 && chosen.height > 0 ? chosen : last.width > 0 && last.height > 0 ? last : Default;
    size = { std::max(size.width, Minimum.width), std::max(size.height, Minimum.height) };
    // A size kept from a bigger screen must not open past this one's edges.
    if (usable.width > 0 && usable.height > 0)
        size = { std::min(size.width, usable.width), std::min(size.height, usable.height) };
    return size;
}
