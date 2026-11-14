// A host unit probe: its output is compared in memory, never saved in a report.
#include <cstddef>
#include <cstdio>
#include <type_traits>
#include "ppc_context.h"

static_assert(std::is_standard_layout_v<PPCContext>);
static_assert(std::is_same_v<PPCFunc, void(PPCContext&, uint8_t*)>);
static_assert(std::is_same_v<decltype(PPCFuncMapping::host), PPCFunc*>);

#define FIELD(name) std::printf("%zu,", offsetof(PPCContext, name))
#define TEN(prefix)                                                                                                    \
    FIELD(prefix##0);                                                                                                  \
    FIELD(prefix##1);                                                                                                  \
    FIELD(prefix##2);                                                                                                  \
    FIELD(prefix##3);                                                                                                  \
    FIELD(prefix##4);                                                                                                  \
    FIELD(prefix##5);                                                                                                  \
    FIELD(prefix##6);                                                                                                  \
    FIELD(prefix##7);                                                                                                  \
    FIELD(prefix##8);                                                                                                  \
    FIELD(prefix##9)
#define THIRTY_TWO(prefix)                                                                                             \
    TEN(prefix);                                                                                                       \
    TEN(prefix##1);                                                                                                    \
    TEN(prefix##2);                                                                                                    \
    FIELD(prefix##30);                                                                                                 \
    FIELD(prefix##31)

int main()
{
    std::printf("%zu,%zu,", sizeof(PPCContext), alignof(PPCContext));
    THIRTY_TWO(r);
#ifndef PPC_CONFIG_SKIP_LR
    FIELD(lr);
#endif
    FIELD(ctr);
    FIELD(xer);
    FIELD(reserved);
#ifndef PPC_CONFIG_SKIP_MSR
    FIELD(msr);
#endif
    FIELD(cr0);
    FIELD(cr1);
    FIELD(cr2);
    FIELD(cr3);
    FIELD(cr4);
    FIELD(cr5);
    FIELD(cr6);
    FIELD(cr7);
    FIELD(fpscr);
    THIRTY_TWO(f);
    TEN(v);
    TEN(v1);
    TEN(v2);
    TEN(v3);
    TEN(v4);
    TEN(v5);
    TEN(v6);
    TEN(v7);
    TEN(v8);
    TEN(v9);
    TEN(v10);
    TEN(v11);
    FIELD(v120);
    FIELD(v121);
    FIELD(v122);
    FIELD(v123);
    FIELD(v124);
    FIELD(v125);
    FIELD(v126);
    FIELD(v127);
    std::puts("");
}
