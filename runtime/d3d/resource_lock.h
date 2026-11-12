#pragma once

// The XDK's resource lock (a texture or buffer locked for the CPU) answers a read-only lock (D3DLOCK_READONLY) with the
// cached 0xC0000000 view of the resource's physical memory, which the console maps onto the same memory as the other
// windows. The windows do not alias here (memory.h), so a read through that view read zeros: Imaginators' created-character
// portrait, read back from a texture this way, was black (#47). The lock's address is mapped to the window the
// allocation is used through instead (d3d/resource_lock.cpp), as MmMapIoSpace does.
#include <cstdint>

namespace d3d
{
    constexpr uint32_t CachedWindow = 0xC0000000, CachedWindowEnd = 0xE0000000, PhysicalAddressMask = 0x1FFFFFFF;

    // The address a lock hands the game: one in the cached window becomes `toGuest(physical)` (PhysicalToGuest: the
    // allocation's own window, 0 when no allocation holds it); any other, or one no allocation holds, is unchanged.
    inline uint32_t LockedAddress(uint32_t address, uint32_t (*toGuest)(uint32_t))
    {
        if (address < CachedWindow || address >= CachedWindowEnd)
            return address;
        uint32_t guest = toGuest(address & PhysicalAddressMask);
        return guest != 0 ? guest : address;
    }
}
