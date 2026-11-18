#pragma once

#include "game.h"
#include "host/settings.h"

// The game's own options that every host applies the same way before the disc opens: the FPS cap of the game, the text
// and voice languages, Nightmare from the start (runtime/main.cpp on a computer, host/uwp on the console).
namespace gameoptions
{
    // Lowers settings.fpsLimit to the game's cap unless `capFps` is false (SKY_NO_FPS_CAP), and gives the languages and
    // nightmare_from_start to `adapter` and language.h. Logs one line for each value the game cannot take.
    void Apply(const game::Adapter& adapter, RuntimeSettings& settings, bool capFps);
}
