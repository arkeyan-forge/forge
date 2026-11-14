#pragma once
// The look-at modifier's gain for one call (games/swapforce/look_at.cpp, issue #79) [SwapForce]: the game moves a head's
// look-at weight by a fixed share `gain` of the gap per call, tuned for 30 calls a second; a call `seconds` after the
// previous one gets 1 - (1 - gain)^(seconds x 30), the same share per second at any frame rate. A gap of 0, or of more
// than 0.1 s (a first call, a pause), counts as a frame at 30 fps; a gain outside (0, 1) is left as it is.
#include <cmath>

namespace lookat
{
    inline double CallSeconds(double seconds) { return seconds > 0.0 && seconds <= 0.1 ? seconds : 1.0 / 30.0; }

    inline float Gain(float gain, double seconds)
    {
        if (!(gain > 0.0f && gain < 1.0f))
            return gain;
        return float(1.0 - std::pow(1.0 - double(gain), CallSeconds(seconds) * 30.0));
    }
}
