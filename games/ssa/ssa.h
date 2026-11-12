#pragma once

#include <string_view>

// Spyro's Adventure's own code, shared by the files of games/ssa/.
namespace ssa
{
    // Voices in another language than the text (language.cpp).
    bool SetVoiceLanguage(std::string_view code);
}
