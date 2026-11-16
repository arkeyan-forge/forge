#include "host/sdl_hints.h"

#include <SDL3/SDL.h>
#include <initializer_list>

namespace sdlhints
{
    void ApplyStartup()
    {
        // The Xbox portals are vendor-class USB devices that look like Xbox pads: SDL's gamepad code would claim them
        // through libusb (the portal's own reads then fail, "cannot claim interface") and list them as a second gamepad
        // (#82).
        for (const char* hint : { SDL_HINT_HIDAPI_IGNORE_DEVICES, SDL_HINT_JOYSTICK_BLACKLIST_DEVICES })
            SDL_SetHint(hint, "0x1430/0x1f17,0x1430/0x09aa,0x1430/0x09ab,0x24c6/0xfa00");
        // Unset, SDL shows Steam's keyboard on every SDL_StartTextInput when Steam sets SteamDeck=1 (#17). The Toy Box
        // has its own keyboard for a pad, and a real keyboard still sends text events.
        SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    }
}
