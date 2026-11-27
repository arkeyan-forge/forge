#pragma once

#include <cstdint>
#include <cstdlib>

namespace toybox
{
    // The pad's stick aiming the quick swap wheel. A tilt aims; the stick coming back to rest aims the centre (cancel)
    // once, so a stick snapping back cancels while a stick left at rest never clears what the d-pad or keys aimed.
    class WheelStick
    {
    public:
        static constexpr int Aim = 16000; // one axis past this aims
        static constexpr int Rest = 9830; // back under this radius (0.3 of a full tilt): the centre

        // The wheel opened: no stick aims yet.
        void Reset() { m_aiming = false; }

        // One frame of the stick: true when the wheel aims (x, y) (x right, y up, -1 to 1); (0, 0) is the centre.
        bool Step(int16_t sx, int16_t sy, float& x, float& y)
        {
            if (std::abs(sx) > Aim || std::abs(sy) > Aim)
            {
                m_aiming = true;
                x = sx / 32767.0f;
                y = sy / 32767.0f;
                return true;
            }
            if (m_aiming && int64_t(sx) * sx + int64_t(sy) * sy < int64_t(Rest) * Rest)
            {
                m_aiming = false;
                x = y = 0;
                return true;
            }
            return false;
        }

    private:
        bool m_aiming = false;
    };
}
