// The look-at gain per call (games/swapforce/look_at_gain.h, issue #79): a head turns at the same speed at 30 and 60 fps.
#include "look_at_gain.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>

static int failures = 0;
#define CHECK(x) ((x) ? (void)0 : (void)(fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x), failures++))

// The weight after `seconds` of calls `step` apart, from 0 toward 1.
static double Weight(float gain, double step, double seconds)
{
    double weight = 0.0;
    for (double t = step; t <= seconds + 1e-9; t += step)
        weight += (1.0 - weight) * lookat::Gain(gain, step);
    return weight;
}

int main()
{
    // At 30 fps a call keeps the game's own gain.
    CHECK(std::fabs(lookat::Gain(0.2f, 1.0 / 30.0) - 0.2f) < 1e-6f);
    // 60 calls a second reach the same weight in a second as 30, and the same in 1.43 s (0.05: Woodburrow's gain).
    for (float gain : { 0.05f, 0.1f, 0.2f })
    {
        CHECK(std::fabs(Weight(gain, 1.0 / 60.0, 1.0) - Weight(gain, 1.0 / 30.0, 1.0)) < 1e-4);
        CHECK(std::fabs(Weight(gain, 1.0 / 144.0, 1.0) - Weight(gain, 1.0 / 30.0, 1.0)) < 1e-3);
    }
    // A first call or a pause counts as a frame at 30 fps; a gain of 0 or 1 is left alone.
    CHECK(std::fabs(lookat::Gain(0.2f, 0.0) - 0.2f) < 1e-6f && std::fabs(lookat::Gain(0.2f, 2.0) - 0.2f) < 1e-6f);
    CHECK(lookat::Gain(0.0f, 1.0 / 60.0) == 0.0f && lookat::Gain(1.0f, 1.0 / 60.0) == 1.0f);
    printf("look-at gain: %s\n", failures == 0 ? "all checks pass" : "FAILED");
    return failures == 0 ? 0 : 1;
}
