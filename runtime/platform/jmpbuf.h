#pragma once

#include <csetjmp>
#include <cstdint>

extern "C" jmp_buf* ppc_setjmp_buf(uint32_t guestAddress);
extern "C" jmp_buf* ppc_longjmp_buf(uint32_t guestAddress);
