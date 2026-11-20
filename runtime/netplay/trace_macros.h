#pragma once

#include "trace.h"

#define PPC_LOAD_U8(x)                                                                                                 \
    netplay::Load<uint8_t>(                                                                                            \
        base, uint32_t(x), uint32_t(ctx.lr), netplay::g_trace, netplay::g_trace ? netplay::ProbeFrame() : 0)
#define PPC_LOAD_U16(x)                                                                                                \
    netplay::Load<uint16_t>(                                                                                           \
        base, uint32_t(x), uint32_t(ctx.lr), netplay::g_trace, netplay::g_trace ? netplay::ProbeFrame() : 0)
#define PPC_LOAD_U32(x)                                                                                                \
    netplay::Load<uint32_t>(                                                                                           \
        base, uint32_t(x), uint32_t(ctx.lr), netplay::g_trace, netplay::g_trace ? netplay::ProbeFrame() : 0)
#define PPC_LOAD_U64(x)                                                                                                \
    netplay::Load<uint64_t>(                                                                                           \
        base, uint32_t(x), uint32_t(ctx.lr), netplay::g_trace, netplay::g_trace ? netplay::ProbeFrame() : 0)
#define PPC_STORE_U8(x, y) netplay::Store<uint8_t>(base, uint32_t(x), uint8_t(y), netplay::g_trace)
#define PPC_STORE_U16(x, y) netplay::Store<uint16_t>(base, uint32_t(x), uint16_t(y), netplay::g_trace)
#define PPC_STORE_U32(x, y) netplay::Store<uint32_t>(base, uint32_t(x), uint32_t(y), netplay::g_trace)
#define PPC_STORE_U64(x, y) netplay::Store<uint64_t>(base, uint32_t(x), uint64_t(y), netplay::g_trace)
