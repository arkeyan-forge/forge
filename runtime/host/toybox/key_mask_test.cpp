// Checks which keys the game's keyboard bindings see around the Toy Box and the F3 menu (host/toybox/key_mask.h): a key
// pressed while the overlay is open never reaches them (the overlay reads it as a key), until it is released, even after
// the overlay closed; a key held before it opened still does. usage: ArkeyanToyboxKeyMaskTest
#include "host/toybox/key_mask.h"
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

    bool g_keys[SDL_SCANCODE_COUNT];

    // One frame: the keyboard as g_keys holds it, the overlay `open`; read twice, as both players' inputs read it.
    const bool* Frame(toybox::KeyMask& mask, bool open)
    {
        mask.Apply(g_keys, open);
        return mask.Apply(g_keys, open);
    }
}

int main()
{
    toybox::KeyMask mask;

    // Closed: the keyboard as it is.
    g_keys[SDL_SCANCODE_DOWN] = true;
    CHECK(Frame(mask, false)[SDL_SCANCODE_DOWN]);
    g_keys[SDL_SCANCODE_DOWN] = false;
    CHECK(!Frame(mask, false)[SDL_SCANCODE_DOWN]);

    // Down pressed in the open Toy Box: the overlay's key only (no d-pad press as well), for as long as it is held.
    Frame(mask, true);
    g_keys[SDL_SCANCODE_DOWN] = true;
    CHECK(!Frame(mask, true)[SDL_SCANCODE_DOWN]);
    CHECK(!Frame(mask, true)[SDL_SCANCODE_DOWN]);
    g_keys[SDL_SCANCODE_DOWN] = false;
    CHECK(!Frame(mask, true)[SDL_SCANCODE_DOWN]);
    g_keys[SDL_SCANCODE_DOWN] = true;
    CHECK(!Frame(mask, true)[SDL_SCANCODE_DOWN]);
    g_keys[SDL_SCANCODE_DOWN] = false;
    Frame(mask, true);

    // Space, W (the left stick) and every other key: the same.
    g_keys[SDL_SCANCODE_SPACE] = g_keys[SDL_SCANCODE_W] = true;
    const bool* keys = Frame(mask, true);
    CHECK(!keys[SDL_SCANCODE_SPACE] && !keys[SDL_SCANCODE_W]);
    g_keys[SDL_SCANCODE_SPACE] = g_keys[SDL_SCANCODE_W] = false;
    Frame(mask, true);

    // Escape closes it: still held, it stays away from the game (no Back); released and pressed again, it is the game's.
    g_keys[SDL_SCANCODE_ESCAPE] = true;
    CHECK(!Frame(mask, true)[SDL_SCANCODE_ESCAPE]);
    CHECK(!Frame(mask, false)[SDL_SCANCODE_ESCAPE]);
    CHECK(!Frame(mask, false)[SDL_SCANCODE_ESCAPE]);
    g_keys[SDL_SCANCODE_ESCAPE] = false;
    CHECK(!Frame(mask, false)[SDL_SCANCODE_ESCAPE]);
    g_keys[SDL_SCANCODE_ESCAPE] = true;
    CHECK(Frame(mask, false)[SDL_SCANCODE_ESCAPE]);
    g_keys[SDL_SCANCODE_ESCAPE] = false;
    Frame(mask, false);

    // A key held before it opened (Backspace held for the wheel: its release confirms the swap) stays the game's.
    g_keys[SDL_SCANCODE_BACKSPACE] = true;
    CHECK(Frame(mask, false)[SDL_SCANCODE_BACKSPACE]);
    CHECK(Frame(mask, true)[SDL_SCANCODE_BACKSPACE]);
    g_keys[SDL_SCANCODE_BACKSPACE] = false;
    CHECK(!Frame(mask, true)[SDL_SCANCODE_BACKSPACE]);
    // Pressed again while still open: the overlay's.
    g_keys[SDL_SCANCODE_BACKSPACE] = true;
    CHECK(!Frame(mask, true)[SDL_SCANCODE_BACKSPACE]);
    g_keys[SDL_SCANCODE_BACKSPACE] = false;
    Frame(mask, false);

    // The key that opens it, pressed in the frame it opens: the overlay's.
    g_keys[SDL_SCANCODE_F6] = true;
    CHECK(!Frame(mask, true)[SDL_SCANCODE_F6]);
    g_keys[SDL_SCANCODE_F6] = false;
    Frame(mask, true);
    Frame(mask, false);

    // Nothing left behind: every key is the game's again.
    g_keys[SDL_SCANCODE_DOWN] = g_keys[SDL_SCANCODE_SPACE] = true;
    keys = Frame(mask, false);
    CHECK(keys[SDL_SCANCODE_DOWN] && keys[SDL_SCANCODE_SPACE]);

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
