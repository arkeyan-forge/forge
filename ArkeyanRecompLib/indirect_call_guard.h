// Forced into every ppc/ translation unit (ArkeyanRecompLib/CMakeLists.txt); overrides the generated
// PPC_CALL_INDIRECT_FUNC, which reads the function table at any guest target. A target outside the code range
// (a freed or corrupt object's vtable) made it jump to a garbage host pc (SIGBUS, docs/engines/lux/boot_saves.md).
// Here such a call, or one to a code address without a function, is logged and skipped (r3 = 0).
#pragma once
#include <cstdint>

struct PPCContext;
extern "C" void SsaBadIndirectCall(PPCContext& ctx, uint8_t* base, uint32_t target);

// One compare: the rotate moves the two alignment bits to the top, so a misaligned target is out of range too.
#define PPC_CALL_INDIRECT_FUNC(x) \
    do \
    { \
        uint32_t ssaTarget = uint32_t(x); \
        PPCFunc* ssaFunc = __builtin_rotateright32(ssaTarget - uint32_t(PPC_CODE_BASE), 2) < uint32_t(PPC_CODE_SIZE / 4) \
            ? PPC_LOOKUP_FUNC(base, ssaTarget) : nullptr; \
        if (__builtin_expect(ssaFunc != nullptr, 1)) \
            ssaFunc(ctx, base); \
        else \
            SsaBadIndirectCall(ctx, base, ssaTarget); \
    } while (0)
