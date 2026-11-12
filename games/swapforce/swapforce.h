#pragma once

#include <string_view>

// Swap Force's own code, shared by the files of games/swapforce/.
namespace swapforce
{
    // Voices in another language than the text (language.cpp): "en" or "fr", the two the disc has.
    bool SetVoiceLanguage(std::string_view code);

}

// [swapforce] nightmare_from_start (engines/alchemy/nightmare.cpp): the difficulty menu offers Nightmare before the story
// is beaten.
void AlchemySetNightmareFromStart(bool on);
