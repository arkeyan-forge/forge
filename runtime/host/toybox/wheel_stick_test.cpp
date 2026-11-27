// Checks the pad's stick aiming the quick swap wheel (host/toybox/wheel_stick.h): a tilt aims, the stick snapping back
// aims the centre once, a stick at rest leaves the d-pad's aim alone. usage: ArkeyanToyboxWheelStickTest
#include "host/toybox/wheel_stick.h"
#include <cstdio>

namespace
{
    int g_checks = 0, g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    // The step aims, and at (wantX, wantY) (the sign of each axis is enough).
    bool Aims(toybox::WheelStick& stick, int16_t sx, int16_t sy, int wantX, int wantY)
    {
        float x = 2, y = 2;
        if (!stick.Step(sx, sy, x, y))
            return false;
        auto sign = [](float v) { return v > 0 ? 1 : v < 0 ? -1 : 0; };
        return sign(x) == wantX && sign(y) == wantY;
    }

    bool Silent(toybox::WheelStick& stick, int16_t sx, int16_t sy)
    {
        float x = 0, y = 0;
        return !stick.Step(sx, sy, x, y);
    }
}

int main()
{
    toybox::WheelStick stick;

    // The wheel opens with the stick at rest (never exactly centred): nothing aims, so a d-pad aim stays.
    CHECK(Silent(stick, 0, 0));
    CHECK(Silent(stick, 1200, -900));

    // A full tilt up aims up; the stick snapping back to rest in one frame aims the centre: a cancel.
    CHECK(Aims(stick, 0, 32767, 0, 1));
    CHECK(Aims(stick, 300, -200, 0, 0));
    // Once: the stick left at rest aims nothing more (a d-pad aim made now stays).
    CHECK(Silent(stick, 300, -200));
    CHECK(Silent(stick, 0, 0));

    // Between rest and a tilt the last aim holds (no flicker at the edge of a tilt).
    CHECK(Aims(stick, -32767, 0, -1, 0));
    CHECK(Silent(stick, -12000, 0));
    CHECK(Aims(stick, -16001, 0, -1, 0));

    // Snapping past the centre to the other side for a frame aims there, then the centre: still a cancel.
    CHECK(Aims(stick, 18000, 0, 1, 0));
    CHECK(Aims(stick, 0, 0, 0, 0));

    // The wheel closed and opened again: a stick at rest aims nothing, even after a tilt in the last wheel.
    CHECK(Aims(stick, 0, -32767, 0, -1));
    stick.Reset();
    CHECK(Silent(stick, 0, 0));

    fprintf(stderr, "%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
