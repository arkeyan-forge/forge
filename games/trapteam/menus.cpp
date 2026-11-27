// Trap Team's own menus are run by its script, as Giants' are (engines/alchemy/script_menu.h); the vtables of its
// script objects, found by hand (docs/runtime/xbox360/input.md, "Script menus").
#include "script_menu.h"

namespace
{
    constexpr scriptmenu::Layout Layout = { 0x821095A8, 0x8210B5F0, 0x82109B48, 0x82109360, 0x82109610, 0x820F1F78,
        0xBC, "Menu - Page Info", "Menu - Item Info" };

    const struct Start
    {
        Start() { scriptmenu::Start(Layout); }
    } g_start;
}
