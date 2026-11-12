// Swap Force's save tasks: a race in the game's own code that stops it at the save slots on a loaded machine
// (games/swapforce/docs/history.md, "The stop at the save slots") [SwapForce].
//
// The storage object runs each task (load the slots, save the options, load or save a game) on an igThread, then polls
// for the task's end every frame (SaveTask_Poll: busy, and the done byte set -> it joins the thread and clears busy). The
// task's start, sub_829CF218, starts the thread first and clears the done byte after: when the worker ends before the
// starting thread gets there (a short task, "0 packages", on a busy host), its done byte is cleared again and the poll
// waits for ever (the title stays up, one "SL_LOAD_SLOTS Thread" line and nothing after). On the console the starting
// thread wins the few instructions it needs. Here the poll sets the done byte again once the task's thread has ended
// (its running byte cleared by the thread's trampoline after the worker, whose last store is the done byte).
#include "game_hooks.h"
#include "memory.h"
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_SaveTask_Poll
PPC_FUNC_IMPL(GAME_IMP(SaveTask_Poll));
// (storage) -> busy.
PPC_FUNC(GAME_HOOK(SaveTask_Poll))
{
    uint32_t storage = ctx.r3.u32;
    if (storage != 0 && g_memory.Read<uint8_t>(storage + game::SaveTaskBusyOffset) != 0 &&
        g_memory.Read<uint8_t>(game::SaveTaskDone) == 0)
    {
        uint32_t thread = g_memory.Read<uint32_t>(storage + game::SaveTaskThreadOffset);
        if (thread != 0 && g_memory.Read<uint8_t>(thread + game::ThreadRunningOffset) == 0)
        {
            fprintf(stderr, "[save] the task's thread ended before its start cleared the done flag: set again\n");
            g_memory.Write<uint8_t>(game::SaveTaskDone, 1);
        }
    }
    GAME_IMP(SaveTask_Poll)(ctx, base);
}
#endif
