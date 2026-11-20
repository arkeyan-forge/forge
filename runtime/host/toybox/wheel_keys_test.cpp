// Checks the stick keys aiming the quick swap wheel (host/toybox/wheel_keys.h): only keys pressed while it is open count,
// both sticks' keys aim, an unbound key does nothing. usage: ArkeyanToyboxWheelKeysTest
#include "host/toybox/wheel_keys.h"
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

    bool Points(const toybox::WheelKeys& keys, float wantX, float wantY)
    {
        float x = 0, y = 0;
        keys.Direction(x, y);
        return x == wantX && y == wantY;
    }
}

int main()
{
    toybox::WheelKeys keys;
    // The defaults: WASD the left stick, IJKL the right one.
    keys.Bind({ SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_I, SDL_SCANCODE_K,
        SDL_SCANCODE_J, SDL_SCANCODE_L });

    // K held from before the wheel opened (the wheel's own key) was never pressed here: nothing aims.
    CHECK(Points(keys, 0, 0));

    // W (Z on an AZERTY keyboard: the same physical key) aims up; D with it, up right.
    CHECK(keys.Press(SDL_SCANCODE_W));
    CHECK(Points(keys, 0, 1));
    CHECK(keys.Press(SDL_SCANCODE_D));
    CHECK(Points(keys, 1, 1));
    CHECK(keys.Release(SDL_SCANCODE_W)); // the wheel aims again from what is still held
    CHECK(Points(keys, 1, 0));
    CHECK(!keys.Release(SDL_SCANCODE_W));
    CHECK(!keys.Release(SDL_SCANCODE_K)); // held from before the wheel: never pressed here

    // The right stick's keys aim too; opposite keys cancel.
    CHECK(keys.Press(SDL_SCANCODE_J));
    CHECK(Points(keys, 0, 0));
    CHECK(keys.Release(SDL_SCANCODE_D));
    CHECK(Points(keys, -1, 0));

    // Keys bound to nothing else, and an unknown scancode, are not stick keys.
    CHECK(!keys.Press(SDL_SCANCODE_RETURN));
    CHECK(!keys.Press(SDL_SCANCODE_UNKNOWN));
    CHECK(Points(keys, -1, 0));

    // The wheel closed: nothing held any more.
    keys.Clear();
    CHECK(Points(keys, 0, 0));

    // An unbound direction (an empty key name) never matches.
    keys.Bind({ SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_I, SDL_SCANCODE_K,
        SDL_SCANCODE_J, SDL_SCANCODE_L });
    CHECK(!keys.Press(SDL_SCANCODE_UNKNOWN));
    CHECK(Points(keys, 0, 0));

    fprintf(stderr, "%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
