#pragma once

#include <string_view>

// [trapteam] nightmare_from_start (engines/alchemy/nightmare.cpp): the difficulty menu offers Nightmare before the story
// is beaten; the save is not changed.
void AlchemySetNightmareFromStart(bool on);

// Trap Team's own code, shared by the files of games/trapteam/.
namespace trapteam
{
    // The text language in any language of the title's list, whatever the console language (language.cpp).
    bool SetTextLanguage(std::string_view code);
    // Independent voices: en/de or fr/nl, according to the mounted regional disc; empty follows the text.
    bool SetVoiceLanguage(std::string_view code);
    // Checked after mounting, from archive indices only; no speech payloads are read.
    bool VoiceLanguageOnDisc(std::string_view code);
}
