// What MmQueryAddressProtect answers for physical memory (kernel/address_protect.h). usage: ArkeyanAddressProtectTest
#include "kernel/address_protect.h"
#include <cstdio>

namespace
{
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)
}

int main()
{
    using kernel::AddressProtect;
    // Swap Force's 247 MB pool: 64 KB pages, read-write, write-combined. The game makes a CPU copy of every render
    // buffer whose memory does not answer PAGE_WRITECOMBINE (forge issue #84).
    CHECK(AddressProtect(0x20000404) == 0x404);
    CHECK((AddressProtect(0x20000404) & kernel::PageWriteCombine) != 0);
    // Plain and uncached read-write memory, in each page size.
    CHECK(AddressProtect(0x20000004) == 0x004);
    CHECK(AddressProtect(0x80000004) == 0x004);
    CHECK(AddressProtect(0x00000204) == 0x204);
    CHECK(AddressProtect(0x00000404) == 0x404);
    // No protection named (memory not allocated through MmAllocatePhysicalMemoryEx): read-write.
    CHECK(AddressProtect(0) == 0x004);
    CHECK(AddressProtect(0x20000000) == 0x004);
    CHECK(AddressProtect(0x400) == 0x404);

    kernel::PhysicalProtects protects;
    protects.Set(0x0BF50000, 0x20000404);
    protects.Set(0x014D0000, 0x00000204);
    CHECK(protects.Get(0x0BF50000) == 0x404);
    CHECK(protects.Get(0x014D0000) == 0x204);
    CHECK(protects.Get(0x01000000) == 0x004); // an allocation made another way
    protects.Erase(0x0BF50000);
    CHECK(protects.Get(0x0BF50000) == 0x004);

    printf("%s\n", g_failures == 0 ? "address protect test: PASS" : "address protect test: FAILED");
    return g_failures == 0 ? 0 : 1;
}
