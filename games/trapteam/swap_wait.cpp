// Trap Team's render thread waits for its last swap to be shown by spinning on the D3D device's swap counts: the busy
// wait made a sleep, as Swap Force's (games/swapforce/swap_wait.cpp; docs/runtime/gpu/renderer_threads.md) [TrapTeam].
//
// The scene's end (sub_823EE370) reads the swaps issued (device +16968) once, then loops at 0x823EE484 reading the swaps
// shown (+16976, written by D3D's vertical blank callback at a pending swap's blank) until they are equal. Unlike Swap
// Force's, the loop is inlined, so the hook is a mid-instruction one at its top (config/TrapTeam.toml): while a swap is
// still pending it waits for the next graphics interrupt's callback, then the loop reads the count as the spin would. It
// was 36 % of the render thread in the hub. SKY_SWAP_WAIT=0 keeps the spin (to measure).
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
void TrapTeamSwapWait(PPCRegister& r10, PPCRegister& r11);

void TrapTeamSwapWait(PPCRegister& r10, PPCRegister& r11)
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
