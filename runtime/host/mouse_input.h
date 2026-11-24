#pragma once

// The mouse in play (docs/runtime/xbox360/input.md, "Mouse"): its buttons and wheel turned into the game's actions for Player 1,
// and the pointer held in the window while the game is played (mouse::Capture, applied with SDL's relative mouse mode, which
// hides the pointer and keeps it in the window; moving the mouse does nothing), and the pointer in the game's own menus
// (mouse::MenuPointer). With the switch off, or nothing mapped, out of those menus nothing reaches the game.
#include "host/mouse.h"
#include <SDL3/SDL.h>
#include <cstddef>
#include <span>

struct RuntimeSettings;

namespace mouseinput
{
    // From the settings (at start and after the F3 Commands page saves): the switch, the bindings over the game's defaults.
    void Configure(const RuntimeSettings& settings, std::span<const mouse::Default> defaults);
    // Every SDL event of the main loop (before the Toy Box sees it): focus, minimize, buttons, wheel, quit.
    void OnEvent(const SDL_Event& event, SDL_Window* window);
    // Once per frame: whether an overlay (F3 menu, Toy Box, wheel) is open or waits for a mouse button, whether one of the
    // game's menus is on screen (game::InMenu), and whether the game takes the pointer there (Adapter::menuPointer); holds
    // or frees the pointer, and gives it to the game's menu (mouse::Menu) whatever the switch says. Then which actions the
    // mouse holds this frame (g_keyActions order); none while the pointer is free; nullptr when the mouse is off.
    void Frame(SDL_Window* window, bool overlay, bool gameMenu, bool menuPointer);
    const bool* HeldActions();
    // Frees the pointer for good (quitting).
    void Release(SDL_Window* window);
}
