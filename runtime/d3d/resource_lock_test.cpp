// Checks the read-only lock's address mapping (d3d/resource_lock.h): a cached-window address becomes the allocation's
// own window, anything else stays. usage: ArkeyanResourceLockTest
#include "d3d/resource_lock.h"
#include <cstdio>

namespace
{
    // One allocation at physical 0x1614A000-0x1615A000, used through the 0xA0000000 window.
    uint32_t ToGuest(uint32_t physical)
    {
        return physical >= 0x1614A000 && physical < 0x1615A000 ? 0xA0000000 | physical : 0;
    }
}

int main()
{
    int failures = 0;
    auto check = [&](uint32_t address, uint32_t expected) {
        uint32_t got = d3d::LockedAddress(address, ToGuest);
        if (got != expected)
        {
            fprintf(stderr, "FAIL 0x%08X: 0x%08X, expected 0x%08X\n", address, got, expected);
            failures++;
        }
    };
    check(0xD614A000, 0xB614A000); // a read-only lock of the 0xA0000000 allocation
    check(0xD6150010, 0xB6150010); // inside it
    check(0xB614A000, 0xB614A000); // already the allocation's window
    check(0xE614A000, 0xE614A000); // the 4 KB window: not the lock's cached view
    check(0x4614A000, 0x4614A000); // virtual memory
    check(0xD0000000, 0xD0000000); // no allocation there: unchanged
    check(0xC0000000, 0xC0000000);
    if (failures == 0)
        printf("resource lock test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
