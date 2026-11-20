#include "host/toybox/version_game.h"

namespace toybox
{
    std::string ShortGameName(const collection::Game& game)
    {
        constexpr std::string_view Prefix = "Skylanders: ";
        return game.name.rfind(Prefix, 0) == 0 ? game.name.substr(Prefix.size()) : game.name;
    }

    std::string VersionGame(const collection::Toy& toy, uint16_t variant)
    {
        const collection::Game* origin = collection::Catalogue::Get().Origin(toy, variant);
        return origin != nullptr ? ShortGameName(*origin) : std::string();
    }
}
