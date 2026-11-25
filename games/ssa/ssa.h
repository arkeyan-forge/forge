#pragma once

#include <string_view>

// Spyro's Adventure's own code, shared by the files of games/ssa/.
namespace ssa
{
    // Voices in another language than the text (language.cpp).
    bool SetVoiceLanguage(std::string_view code);

    // One of the game's menus or notices is on screen: a page that takes input holds the focus or has its own actions
    // (menus.cpp).
    bool MenuOpen();
    // The pointer in the game's menus, each frame on the game's thread after its UI has read the pads (menus.cpp).
    void MenuFrame();
}
