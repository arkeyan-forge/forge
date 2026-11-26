#pragma once

// Included before the Wii game's translation (-include): the stores the translator marked as the write-gather pipe's
// (write_gather_pipe in the game's config, docs/tools/toolchain/xenonrecomp.md) go to the host's FIFO when their
// address is the pipe's, and to memory otherwise (a register that also points elsewhere in the same function).
#include <cstdint>

namespace wii::gx
{
    void GatherWrite(uint8_t* base, uint64_t value, uint32_t bytes);
}

#define ARKEYAN_GATHERS(address) (((address) & ~0x1Fu) == 0xCC008000u)

#define PPC_MM_STORE_U8(x, y)                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint32_t pipeAddress = (x);                                                                              \
        if (ARKEYAN_GATHERS(pipeAddress))                                                                              \
            wii::gx::GatherWrite(base, uint8_t(y), 1);                                                                 \
        else                                                                                                           \
            PPC_STORE_U8(pipeAddress, y);                                                                              \
    } while (0)
#define PPC_MM_STORE_U16(x, y)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint32_t pipeAddress = (x);                                                                              \
        if (ARKEYAN_GATHERS(pipeAddress))                                                                              \
            wii::gx::GatherWrite(base, uint16_t(y), 2);                                                                \
        else                                                                                                           \
            PPC_STORE_U16(pipeAddress, y);                                                                             \
    } while (0)
#define PPC_MM_STORE_U32(x, y)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint32_t pipeAddress = (x);                                                                              \
        if (ARKEYAN_GATHERS(pipeAddress))                                                                              \
            wii::gx::GatherWrite(base, uint32_t(y), 4);                                                                \
        else                                                                                                           \
            PPC_STORE_U32(pipeAddress, y);                                                                             \
    } while (0)
#define PPC_MM_STORE_U64(x, y)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint32_t pipeAddress = (x);                                                                              \
        if (ARKEYAN_GATHERS(pipeAddress))                                                                              \
            wii::gx::GatherWrite(base, uint64_t(y), 8);                                                                \
        else                                                                                                           \
            PPC_STORE_U64(pipeAddress, y);                                                                             \
    } while (0)
// A quantized store: its bytes made in a scratch buffer as memory would get them, then gathered.
#define PPC_MM_STORE_PAIRED(memory, x, gqr, single, ps0, ps1)                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        const uint32_t pipeAddress = (x);                                                                              \
        if (ARKEYAN_GATHERS(pipeAddress))                                                                              \
        {                                                                                                              \
            uint8_t quantized[8] = {};                                                                                 \
            PPCStorePaired(quantized, 0, gqr, single, ps0, ps1);                                                       \
            const uint32_t bytes = PPCQuantizedSize((gqr) & 7) * ((single) ? 1 : 2);                                   \
            for (uint32_t i = 0; i < bytes; i++)                                                                       \
                wii::gx::GatherWrite(memory, quantized[i], 1);                                                         \
        }                                                                                                              \
        else                                                                                                           \
            PPCStorePaired(memory, pipeAddress, gqr, single, ps0, ps1);                                                \
    } while (0)
