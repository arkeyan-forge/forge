// Swap Force's own menus are Juice pages (engines/alchemy/juice_menu.h): the reader starts with the game, since on the
// title level game::InMenu does not ask MenuOpen and the pointer is the menu's there too.
#include "juice_menu.h"

namespace
{
    const struct Start
    {
        Start() { juicemenu::Start(); }
    } g_start;
}
