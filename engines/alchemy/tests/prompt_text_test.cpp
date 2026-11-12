// Checks which texts name the Start button (engines/alchemy/prompt_text.h): the press-Start prompts of every disc language,
// in capitals or not, get START replaced; script names and longer sentences stay. usage: ArkeyanPromptTextTest
#include "prompt_text.h"
#include <cstdio>
#include <string>

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

    bool Becomes(std::string_view text, std::string_view expected)
    {
        std::optional<std::string> out = alchemy::prompt::WithStartPicture(text);
        return out && *out == expected;
    }

    bool Stays(std::string_view text) { return !alchemy::prompt::WithStartPicture(text); }
}

int main()
{
    // Swap Force (English, French), SuperChargers (English, German), Imaginators (French, Dutch).
    CHECK(Becomes("PRESS START", "PRESS \x01"));
    CHECK(Becomes("APPUYER SUR START", "APPUYER SUR \x01"));
    CHECK(Becomes("Press Start", "Press \x01"));
    CHECK(Becomes("Press START", "Press \x01"));
    CHECK(Becomes("DRÜCKE START", "DRÜCKE \x01"));
    CHECK(Becomes("Drücke Start", "Drücke \x01"));
    CHECK(Becomes("Appuie sur Start", "Appuie sur \x01"));
    CHECK(Becomes("Appuie sur START", "Appuie sur \x01"));
    CHECK(Becomes("DRUK OP START", "DRUK OP \x01"));
    CHECK(Becomes("Druk op Start", "Druk op \x01"));
    CHECK(Becomes("Player 1 Press Start", "Player 1 Press \x01"));

    // Script and property names that end with the word, other prompts, longer sentences, other words.
    for (std::string_view text : { "On Start", "Fog Start", "Player Start", "Puzzle Start", "Catchphrase Start",
             "Bomb Tell Start", "to Start", "TAP TO START", "Tap to Start", "Press - to Start", "Press Start To Begin",
             "APPUIE SUR START POUR COMMENCER", "Zum Beginnen Start drücken", "Start", "START", "", "Express Start",
             "Pressstart", "Press Starts", "Appuie sur Start.", "Press Restart" })
        CHECK(Stays(text));
    // Longer than any prompt.
    CHECK(Stays("This is a very long sentence that happens to end with press start"));

    // Lower case: ASCII and the Latin-1 capitals in UTF-8 (the multiplication sign is not a letter).
    CHECK(alchemy::prompt::Lower("DRÜCKE Été") == "drücke été");
    CHECK(alchemy::prompt::Lower("A×B") == "a×b");

    printf(g_failures == 0 ? "PASS\n" : "FAIL\n");
    return g_failures == 0 ? 0 : 1;
}
