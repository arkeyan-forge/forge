// Checks the game Add a figure's versions panel names for each version (host/toybox/version_game.h), from the
// catalogue; no game data. usage: ArkeyanToyboxVersionGameTest
#include "host/toybox/version_game.h"
#include <cstdio>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)
}

int main()
{
    using namespace collection;
    const Catalogue& catalogue = Catalogue::Get();
    const Game* giants = catalogue.FindGame("giants");
    CHECK(giants != nullptr && toybox::ShortGameName(*giants) == "Giants");
    // Eruptor came with Spyro's Adventure; its Series 2 with Giants, its Volcanic deco (0x2C02) with Swap Force.
    const Toy* eruptor = catalogue.Find(9);
    CHECK(eruptor != nullptr);
    if (eruptor != nullptr)
    {
        CHECK(toybox::VersionGame(*eruptor, 0) == "Spyro's Adventure");
        CHECK(toybox::VersionGame(*eruptor, 0x1801) == "Giants");
        CHECK(toybox::VersionGame(*eruptor, 0x2C02) == "Swap Force");
    }
    // A Swap Force toy sold under 0x1000 (Jet-Vac's "base" entry) stays a Swap Force toy, not Giants.
    for (const Toy& toy : catalogue.Toys())
        if (toy.game == "swapforce" && StandardVariant(toy) != 0)
        {
            CHECK(toybox::VersionGame(toy, StandardVariant(toy)) == "Swap Force");
            break;
        }
    printf("toybox version game test: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
