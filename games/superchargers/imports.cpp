// Imports that Skylanders: SuperChargers links against and the earlier games do not (ten). Kept with the game until a second
// game needs them. The console is offline with one local profile: the network and Live calls fail as with no cable, the
// audio ducker is off. Every output is written; the first call of each is logged with its argument registers, because
// none of them is documented here.
#include "kernel/alloc_log.h"
#include "memory.h"
#include <atomic>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    constexpr uint32_t StatusSuccess = 0;
    constexpr uint32_t SocketError = 0xFFFFFFFF;
    constexpr uint32_t WsaNetDown = 10050;
    constexpr uint32_t ErrorNotLoggedOn = 1245;

    // Logs the first call of each import, with its first argument registers.
#define LOG_FIRST_CALL(name)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        static std::atomic_flag reported;                                                                              \
        if (!reported.test_and_set())                                                                                  \
        {                                                                                                              \
            fprintf(stderr, "[kernel] " name " (lr=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X)\n",                 \
                uint32_t(ctx.lr), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);                                     \
        }                                                                                                              \
    } while (0)

    // The ducker getters: (value out); the ducker is off, every value is zero.
    void DuckerValue(PPCContext& ctx)
    {
        if (ctx.r3.u32 != 0)
            g_memory.Write<uint32_t>(ctx.r3.u32, 0);
        ctx.r3.u64 = StatusSuccess;
    }
}

// (caller, socket, sockaddr out, length in/out): no socket has a local address without a network.
PPC_FUNC(__imp__NetDll_getsockname)
{
    LOG_FIRST_CALL("NetDll_getsockname");
    ctx.r3.u64 = SocketError;
}

// (caller, key id, data, size, bits a second, flags): no listening without a network.
PPC_FUNC(__imp__NetDll_XNetQosListen)
{
    LOG_FIRST_CALL("NetDll_XNetQosListen");
    ctx.r3.u64 = WsaNetDown;
}

// (caller, ... 13 arguments, the XNQOS** out the last, on the stack): no lookup without a network; the out pointer is not
// written (its stack slot is not known yet [?]).
PPC_FUNC(__imp__NetDll_XNetQosLookup)
{
    LOG_FIRST_CALL("NetDll_XNetQosLookup");
    ctx.r3.u64 = WsaNetDown;
}

// (name, buffer, size, ...): a Live string; there is no Live, the buffer is left empty.
PPC_FUNC(__imp__XamQueryLiveHiveA)
{
    LOG_FIRST_CALL("XamQueryLiveHiveA");
    if (ctx.r4.u32 >= 0x10000 && ctx.r5.u32 != 0) // a guest pointer, not a small argument of an unknown signature
        g_memory.Write<uint8_t>(ctx.r4.u32, 0);
    AllocLog(ctx, "XamQueryLiveHiveA (name, buffer, size)", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ErrorNotLoggedOn);
    ctx.r3.u64 = ErrorNotLoggedOn;
}

// (user index, ...): no Live profile to invite from.
PPC_FUNC(__imp__XamShowGameInviteUI)
{
    LOG_FIRST_CALL("XamShowGameInviteUI");
    ctx.r3.u64 = ErrorNotLoggedOn;
}

// (enable): the ducker lowers the music under voices; our mixer has none, so it stays off.
PPC_FUNC(__imp__XAudioEnableDucker)
{
    LOG_FIRST_CALL("XAudioEnableDucker");
    ctx.r3.u64 = StatusSuccess;
}

PPC_FUNC(__imp__XAudioGetDuckerAttackTime)
{
    LOG_FIRST_CALL("XAudioGetDuckerAttackTime");
    DuckerValue(ctx);
}

PPC_FUNC(__imp__XAudioGetDuckerHoldTime)
{
    LOG_FIRST_CALL("XAudioGetDuckerHoldTime");
    DuckerValue(ctx);
}

PPC_FUNC(__imp__XAudioGetDuckerReleaseTime)
{
    LOG_FIRST_CALL("XAudioGetDuckerReleaseTime");
    DuckerValue(ctx);
}

PPC_FUNC(__imp__XAudioGetDuckerThreshold)
{
    LOG_FIRST_CALL("XAudioGetDuckerThreshold");
    DuckerValue(ctx);
}
