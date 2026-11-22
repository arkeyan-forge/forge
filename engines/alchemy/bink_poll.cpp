// Bink's async decode thread waits for its next job by polling a mailbox in a loop that makes no kernel call and reads
// no clock (games/<id>/addresses.h, BinkAsync_Poll; Giants' Bink has no such thread). Online
// (docs/runtime/host/netplay.md, step 0c) one guest thread runs at a time and hands over only at such calls, so that loop
// kept the baton and the other threads waited for the watchdog, 2 s at a time. A poll that found nothing now yields, at
// the same point of the guest's run on both computers. Off line nothing changes [Alchemy].
#include "game_hooks.h"
#include "netplay/scheduler.h"
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_BinkAsync_Poll
PPC_FUNC_IMPL(GAME_IMP(BinkAsync_Poll));
PPC_FUNC(GAME_HOOK(BinkAsync_Poll))
{
    GAME_IMP(BinkAsync_Poll)(ctx, base);
    if (ctx.r3.u32 == 0 && netplay::Cooperative())
        netplay::GuestScheduler().Yield();
}
#endif
