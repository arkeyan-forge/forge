#pragma once

#include <string_view>

// Giants' own code, shared by the files of games/giants/.
namespace giants
{
    // Voices in another language than the text (language.cpp).
    bool SetVoiceLanguage(std::string_view code);
    // The text in any of the disc's twelve languages, whatever the console language (language.cpp).
    bool SetTextLanguage(std::string_view code);
}
