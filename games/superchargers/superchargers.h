#pragma once

#include <string_view>

// SuperChargers' own code, shared by the files of games/superchargers/.
namespace superchargers
{
    // Voices in another language than the text (language.cpp): "en" or "de", the two the disc has.
    bool SetVoiceLanguage(std::string_view code);
}
