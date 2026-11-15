#pragma once

#include <SDL3/SDL.h>

namespace toybox
{
    // The keyboard as the game's bindings see it: a key pressed while the Toy Box, the wheel or the F3 menu is open is
    // the overlay's (its key events) and reads as up until it is released, even once the overlay closed; otherwise the
    // bindings would turn it into a pad button the overlay runs a second time, or the game gets the key that closed it.
    class KeyMask
    {
    public:
        // `keys`: the keyboard now (SDL_GetKeyboardState); `open`: the overlay is open. Returns `keys` itself when no key
        // is taken. Reading the same keyboard twice in a frame gives the same answer.
        const bool* Apply(const bool* keys, bool open)
        {
            bool any = false;
            for (int i = 0; i < SDL_SCANCODE_COUNT; i++)
            {
                if (!keys[i])
                    m_taken[i] = false;
                else if (open && !m_down[i])
                    m_taken[i] = true;
                m_down[i] = keys[i];
                any = any || m_taken[i];
            }
            if (!any)
                return keys;
            for (int i = 0; i < SDL_SCANCODE_COUNT; i++)
                m_out[i] = keys[i] && !m_taken[i];
            return m_out;
        }

    private:
        bool m_down[SDL_SCANCODE_COUNT] = {};
        bool m_taken[SDL_SCANCODE_COUNT] = {};
        bool m_out[SDL_SCANCODE_COUNT] = {};
    };
}
