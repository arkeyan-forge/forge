// Giants' own menus are run by its script (engines/alchemy/script_menu.h); the vtables of its script objects, found by
// hand (docs/runtime/xbox360/input.md, "Script menus").
#include "giants.h"
#include "script_menu.h"

namespace
{
    constexpr scriptmenu::Layout Layout = { 0x820DF750, 0x820E1038, 0x820DF2A8, 0x820DF500, 0x820DF7C0, 0x820E22E8,
        0xB4, "Menu Page Info", "Menu Item Info" };

    const struct Start
    {
        Start() { scriptmenu::Start(Layout); }
    } g_start;
}

bool giants::MenuOpen() { return scriptmenu::MenuOpen(); }
