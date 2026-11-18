// Checks the game's options both hosts apply (host/game_options.cpp): the FPS cap, the languages, Nightmare.
// usage: ArkeyanGameOptionsTest
#include "host/game_options.h"
#include "language.h"
#include <cstdio>
#include <string>

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

    std::string g_text, g_gameText, g_voice;
    bool g_nightmare = false;

    bool GameText(std::string_view code)
    {
        g_gameText = code;
        return code == "nl";
    }
    bool Voice(std::string_view code)
    {
        g_voice = code;
        return code == "fr";
    }
    void Nightmare(bool on) { g_nightmare = on; }

    const GameInfo g_info{ .id = "test", .name = "Test", .releases = {}, .runtime = "" };
}

// language.h: the console languages kernel/xconfig.cpp knows.
bool SetTextLanguage(std::string_view code)
{
    g_text = code;
    return code == "de";
}

int main()
{
    const game::Adapter capped{ .info = g_info,
        .ReadPortalPrompt = nullptr,
        .ultrawide = false,
        .SetVoiceLanguage = Voice,
        .glyphTextures = {},
        .introMovies = {},
        .SetTextLanguage = GameText,
        .SetNightmareFromStart = Nightmare,
        .maxFpsLimit = 30 };
    const game::Adapter open{ .info = g_info,
        .ReadPortalPrompt = nullptr,
        .ultrawide = false,
        .SetVoiceLanguage = nullptr,
        .glyphTextures = {},
        .introMovies = {} };

    // The cap lowers 60 to the game's 30, unless the host is told not to (SKY_NO_FPS_CAP).
    RuntimeSettings settings;
    settings.fpsLimit = 60;
    gameoptions::Apply(capped, settings, true);
    CHECK(settings.fpsLimit == 30);
    settings.fpsLimit = 60;
    gameoptions::Apply(capped, settings, false);
    CHECK(settings.fpsLimit == 60);
    gameoptions::Apply(open, settings, true);
    CHECK(settings.fpsLimit == 60);

    // Languages: the console's first, then the game's own; the voices only through the game.
    settings.language = "nl";
    settings.voiceLanguage = "fr";
    settings.nightmareFromStart = true;
    gameoptions::Apply(capped, settings, true);
    CHECK(g_text == "nl");
    CHECK(g_gameText == "nl");
    CHECK(g_voice == "fr");
    CHECK(g_nightmare);

    // A game with no voice setting, no own text language and no locked difficulty is left alone.
    g_gameText.clear();
    g_voice.clear();
    g_nightmare = false;
    settings.language = "de";
    gameoptions::Apply(open, settings, true);
    CHECK(g_text == "de");
    CHECK(g_gameText.empty());
    CHECK(g_voice.empty());
    CHECK(!g_nightmare);

    // Nothing asked: nothing set.
    g_text.clear();
    RuntimeSettings none;
    gameoptions::Apply(capped, none, true);
    CHECK(g_text.empty());
    CHECK(!g_nightmare);

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
