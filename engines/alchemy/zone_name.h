#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

// SKY_LEVEL's zone for the games that ask for their next zone by name (zone_request.cpp): a map's name gives
// <prefix><map>/<map> (academy -> academy/academy; Imaginators' prefix 2016/), a full zone path is taken as it is
// (actionpacks/apa1/apa1, 2015/ezones/airzone/airzone: the zones whose path is not <map>/<map>). Lower case; nothing
// when the value is empty, has a backslash or a dot, or an empty part of a path.
namespace alchemy
{
    inline std::optional<std::string> LevelZone(std::string_view value, std::string_view prefix)
    {
        if (value.empty() || value.find_first_of("\\.") != std::string_view::npos)
            return std::nullopt;
        std::string zone(value);
        for (char& c : zone)
            c = char(tolower(static_cast<unsigned char>(c)));
        if (zone.find('/') == std::string::npos)
            return std::string(prefix) + zone + "/" + zone;
        if (zone.front() == '/' || zone.back() == '/' || zone.find("//") != std::string::npos)
            return std::nullopt;
        return zone;
    }

    // Swap Force's zone names (zone.cpp): "<area>/<map>" or a full zone name ("<area>/<map>/<map>",
    // "multiplayer/<mode>/<map>/<map>"), written with the game's backslashes, lower case; nothing without a slash.
    inline std::optional<std::string> BackslashZone(std::string_view value)
    {
        std::string zone(value);
        for (char& c : zone)
            c = c == '/' ? '\\' : char(tolower(static_cast<unsigned char>(c)));
        size_t first = zone.find('\\'), last = zone.rfind('\\');
        if (first == std::string::npos)
            return std::nullopt;
        if (first == last)
            zone += zone.substr(last); // <area>\<map> -> <area>\<map>\<map>
        return zone;
    }
}
