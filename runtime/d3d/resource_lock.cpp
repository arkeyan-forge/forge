// The XDK's resource lock: a read-only lock's cached-window address mapped to the allocation's own window
// (d3d/resource_lock.h).
#include "d3d/resource_lock.h"
#include "game_hooks.h"
#include "memory.h"
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_D3D_LockResource
PPC_FUNC_IMPL(GAME_IMP(D3D_LockResource));

PPC_FUNC(GAME_HOOK(D3D_LockResource))
{
    GAME_IMP(D3D_LockResource)(ctx, base);
    ctx.r3.u64 = d3d::LockedAddress(ctx.r3.u32, PhysicalToGuest);
}
#endif
