// The render thread's wait for its last swap to be shown, a spin on the D3D device's swap counts, made a sleep: Trap
// Team's (games/trapteam/swap_wait.cpp) for the games after it [Alchemy] [XDK] (docs/runtime/gpu/renderer_threads.md).
//
// The scene's end reads the swaps issued (device +16968) once, then loops reading the swaps shown (+16976, written by
// D3D's vertical blank callback at a pending swap's blank) until they are equal. The loop is inlined, so the hook is a
// mid-instruction one at its top (config/<Game>.toml, `AlchemySwapWait`, r10 the swaps issued, r11 the device): while
// a swap is still pending it waits for the next graphics interrupt's callback, then the loop reads the count as the
// spin would. SKY_SWAP_WAIT=0 keeps the spin (to measure). A game whose config does not name it never calls it.
#include "host/env.h"
#include "kernel/video.h"
#include "memory.h"
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    constexpr uint32_t SwapsShownOffset = 16976;
}

// Called by the recompiled loop (the declaration XenonRecomp writes in front of it).
void AlchemySwapWait(PPCRegister& r10, PPCRegister& r11);

void AlchemySwapWait(PPCRegister& r10, PPCRegister& r11)
{
    static const bool wait = [] {
        const char* v = skyenv::Get("SKY_SWAP_WAIT");
        return v == nullptr || v[0] != '0';
    }();
    if (!wait)
        return;
    const uint64_t handled = InterruptsHandled(); // before the check: one handled after it ends the wait at once
    if (r10.u32 != g_memory.Read<uint32_t>(r11.u32 + SwapsShownOffset))
        WaitForInterruptHandled(handled);
}
