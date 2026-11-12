// Swap Force's render thread waits for its last swap to be shown by spinning on a status call: the busy wait made a
// sleep (docs/runtime/gpu/renderer_threads.md, "Swap Force's swap wait") [SwapForce].
//
// Before each D3DDevice_Swap the scene's end (sub_82515D68) loops on SwapWait_Status (device, out) until out[2], the
// swaps issued (device +16968, written by Swap) minus the swaps shown (+16976, written by D3D's vertical blank callback
// sub_82F5C128 when a pending swap's blank comes), is 0. On the console that spin costs a hardware thread the title
// owns; here it is a host core spinning for the rest of every frame. The hook waits for the next graphics interrupt's
// callback whenever a swap is still pending, then reads the status: the loop sees the values the spin would have seen,
// at the same interrupt. SKY_SWAP_WAIT=0 keeps the spin (to measure).
#include "game_hooks.h"
#include "host/env.h"
#include "kernel/video.h"
#include "memory.h"
#include <ppc_config.h>
#include <ppc_context.h>

#ifdef GAME_HOOK_SwapWait_Status
PPC_FUNC_IMPL(GAME_IMP(SwapWait_Status));
// (device, out): out[0] = 0, out[1] = swaps issued, out[2] = swaps issued - swaps shown.
PPC_FUNC(GAME_HOOK(SwapWait_Status))
{
    static const bool wait = [] {
        const char* v = skyenv::Get("SKY_SWAP_WAIT");
        return v == nullptr || v[0] != '0';
    }();
    uint32_t device = ctx.r3.u32;
    const uint64_t handled = InterruptsHandled(); // before the check: one handled after it ends the wait at once
    if (wait && g_memory.Read<uint32_t>(device + game::SwapsIssuedOffset) !=
                    g_memory.Read<uint32_t>(device + game::SwapsShownOffset))
        WaitForInterruptHandled(handled);
    GAME_IMP(SwapWait_Status)(ctx, base);
}
#endif
