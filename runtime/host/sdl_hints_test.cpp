// Checks the SDL hints set at startup, without initializing SDL: ./ArkeyanSdlHintsTest
#include "host/sdl_hints.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char* what) {
        checks++;
        if (!ok)
        {
            fprintf(stderr, "FAIL %s\n", what);
            failures++;
        }
    };
    auto is = [](const char* name, const char* want) {
        const char* value = SDL_GetHint(name);
        return value != nullptr && strcmp(value, want) == 0;
    };

    // Steam's Game Mode sets SteamDeck=1, which SDL reads (as a hint) to show Steam's keyboard on SDL_StartTextInput.
    SDL_SetHint("SteamDeck", "1");
    sdlhints::ApplyStartup();
    check(is(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0"), "no on-screen keyboard from SDL, even with SteamDeck=1");
    const char* portals = "0x1430/0x1f17,0x1430/0x09aa,0x1430/0x09ab,0x24c6/0xfa00";
    check(is(SDL_HINT_HIDAPI_IGNORE_DEVICES, portals), "HIDAPI ignores the portals");
    check(is(SDL_HINT_JOYSTICK_BLACKLIST_DEVICES, portals), "the joystick code ignores the portals");

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
