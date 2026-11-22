#include "platform/jmpbuf.h"
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace
{
    struct JumpBuffer
    {
        jmp_buf value;
    };

    // Rehash preserves element addresses; an environment belongs only to the thread that saved it.
    thread_local std::unordered_map<uint32_t, JumpBuffer> g_buffers;
}

extern "C" jmp_buf* ppc_setjmp_buf(uint32_t guestAddress) { return &g_buffers[guestAddress].value; }

extern "C" jmp_buf* ppc_longjmp_buf(uint32_t guestAddress)
{
    const auto found = g_buffers.find(guestAddress);
    if (found == g_buffers.end())
    {
        fprintf(stderr, "[setjmp] longjmp buffer 0x%08" PRIX32 " was never set on this thread\n", guestAddress);
        std::abort();
    }
    return &found->second.value;
}
