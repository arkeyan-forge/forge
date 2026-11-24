#include "wii/boot_info.h"
#include "wii/memory_map.h"
#include <cstring>

namespace wii
{
    namespace
    {
        void Put32(uint8_t* base, uint32_t address, uint32_t value)
        {
            const uint8_t bytes[4] = { uint8_t(value >> 24), uint8_t(value >> 16), uint8_t(value >> 8),
                uint8_t(value) };
            memcpy(base + address, bytes, 4);
        }
    } // namespace

    void WriteBootInfo(uint8_t* base, const BootInfo& info)
    {
        memset(base + Mem1Cached, 0, 0x4000);
        memcpy(base + Mem1Cached, info.discHeader, sizeof(info.discHeader));
        Put32(base, 0x80000020, 0x0D15EA5E); // the loader's "booted normally" mark
        Put32(base, 0x80000024, 1);          // its version
        Put32(base, 0x80000028, Mem1Size);   // the physical memory the operating system reports
        Put32(base, 0x8000002C, 0x00000023); // a retail console
        Put32(base, 0x800000F0, Mem1Size);   // the memory the game may assume
        Put32(base, 0x800000F8, BusClock);
        Put32(base, 0x800000FC, CpuClock);
        Put32(base, 0x80003100, Mem1Size); // MEM1: physical, then what the game sees
        Put32(base, 0x80003104, Mem1Size);
        Put32(base, 0x80003118, Mem2Size); // MEM2 likewise
        Put32(base, 0x8000311C, Mem2Size);
        Put32(base, 0x80003120, IpcBufferEnd);   // MEM2's end
        Put32(base, 0x80003124, 0x90000800);     // the game's MEM2 arena: from the start, past the loader's
        Put32(base, 0x80003128, Mem2ArenaEnd);   // to IOS's buffers
        Put32(base, 0x80003130, Mem2ArenaEnd);   // IOS's IPC buffers
        Put32(base, 0x80003134, IpcBufferEnd);
        Put32(base, 0x80003138, 0x00000011);     // the Hollywood's version
        // The IOS running, and the one the loader started for the game: OSInit stops when their numbers differ, or
        // when the first's revision is older ("This firmware is an improper version for this SDK").
        Put32(base, 0x80003140, info.ios << 16 | (info.iosRevision & 0xFFFF));
        Put32(base, 0x80003188, info.ios << 16 | (info.iosRevision & 0xFFFF));
        memcpy(base + 0x80003180, info.discHeader, 4); // the game's ID again, for IOS's checks
    }
}
