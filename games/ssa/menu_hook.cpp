// Spyro's Adventure: the pointer in its menus (menus.cpp) goes in each frame right after the UI has read the pads.
#include "game_hooks.h"
#include "ssa.h"
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_Ui_ReadPads
PPC_FUNC_IMPL(GAME_IMP(Ui_ReadPads));
PPC_FUNC(GAME_HOOK(Ui_ReadPads))
{
    GAME_IMP(Ui_ReadPads)(ctx, base);
    ssa::MenuFrame();
}
#endif
