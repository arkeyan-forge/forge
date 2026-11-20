#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

namespace toybox
{
    // The keys bound to the sticks (WASD, IJKL by default) aiming the quick swap wheel. Only a key pressed while the
    // wheel is open counts: one held from before (the wheel's own key may be one) never aims. The game's bindings do
    // not see these presses (KeyMask), so the wheel reads them from the key events.
    class WheelKeys
    {
    public:
        // `keys`: the left stick's up, down, left, right, then the right stick's.
        void Bind(const SDL_Scancode (&keys)[8])
        {
            for (int i = 0; i < 8; i++)
                m_keys[i] = keys[i];
        }

        // A key pressed while the wheel is open: true when it is a stick key.
        bool Press(SDL_Scancode code)
        {
            uint8_t bits = Bits(code);
            m_held |= bits;
            return bits != 0;
        }

        // A key released: true when it was a held stick key.
        bool Release(SDL_Scancode code)
        {
            uint8_t bits = m_held & Bits(code);
            m_held &= uint8_t(~bits);
            return bits != 0;
        }
        void Clear() { m_held = 0; }

        // The direction the held keys point to (x right, y up), both sticks summed, each axis -1 to 1.
        void Direction(float& x, float& y) const
        {
            auto held = [&](int i) { return float((m_held & (1 << i)) != 0 || (m_held & (1 << (i + 4))) != 0); };
            x = held(3) - held(2);
            y = held(0) - held(1);
        }

    private:
        uint8_t Bits(SDL_Scancode code) const
        {
            uint8_t bits = 0;
            for (int i = 0; i < 8; i++)
                if (code != SDL_SCANCODE_UNKNOWN && m_keys[i] == code)
                    bits |= uint8_t(1 << i);
            return bits;
        }

        SDL_Scancode m_keys[8] = {};
        uint8_t m_held = 0;
    };
}
