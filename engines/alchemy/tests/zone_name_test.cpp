// Checks SKY_LEVEL's zone name for the zone-request games (engines/alchemy/zone_name.h): a map's name, a full zone path,
// the values refused. No game data. usage: ArkeyanZoneNameTest
#include "zone_name.h"
#include <cstdio>

namespace
{
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    bool Gives(std::string_view value, std::string_view prefix, std::string_view zone)
    {
        std::optional<std::string> out = alchemy::LevelZone(value, prefix);
        return out && *out == zone;
    }
}

int main()
{
    CHECK(Gives("academy", "", "academy/academy"));
    CHECK(Gives("Sky1", "", "sky1/sky1"));
    CHECK(Gives("hub_2016", "2016/", "2016/hub_2016/hub_2016"));
    CHECK(Gives("actionpacks/apa1/apa1", "", "actionpacks/apa1/apa1"));
    CHECK(Gives("2015/EZones/airzone/airzone", "", "2015/ezones/airzone/airzone"));
    CHECK(Gives("actionpacks/apl2/apl2", "2016/", "actionpacks/apl2/apl2")); // a full path keeps no prefix
    CHECK(!alchemy::LevelZone("", ""));
    CHECK(!alchemy::LevelZone("academy.pak", ""));
    CHECK(!alchemy::LevelZone("archives\\academy", ""));
    CHECK(!alchemy::LevelZone("/academy", ""));
    CHECK(!alchemy::LevelZone("academy/", ""));
    CHECK(!alchemy::LevelZone("a//b", ""));
    CHECK(alchemy::BackslashZone("barrens/brn1") == std::string("barrens\\brn1\\brn1"));
    CHECK(alchemy::BackslashZone("Multiplayer/PvP/pvp_barrens/pvp_barrens") ==
          std::string("multiplayer\\pvp\\pvp_barrens\\pvp_barrens"));
    CHECK(!alchemy::BackslashZone("brn1"));
    if (g_failures == 0)
        printf("zone names: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
