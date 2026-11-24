#pragma once

// What the console's loader leaves in low memory before it jumps to the game: the disc's header, the memory sizes and
// the arenas, the clocks, the IOS the game runs on. The operating system's start (OSInit) reads them. From the public
// description of the Wii's low memory (WiiBrew, "Memory map"); docs/runtime/wii/memory_boot.md.
#include <cstdint>

namespace wii
{
    struct BootInfo
    {
        uint8_t discHeader[0x20] = {}; // the first bytes of the game partition: its ID, maker, disc number, magic
        uint32_t ios = 0;              // the title metadata's IOS (56 for IOS56)
        uint32_t iosRevision = 0;
    };

    // Clock speeds of the Wii's bus and processor, in Hz.
    constexpr uint32_t BusClock = 243000000;
    constexpr uint32_t CpuClock = 729000000;
    // The timebase ticks at a quarter of the bus clock.
    constexpr uint32_t TimeBaseClock = BusClock / 4;
    // MEM2's top, kept by IOS for its IPC buffers: the game's arena ends below it.
    constexpr uint32_t Mem2ArenaEnd = 0x933E0000;
    constexpr uint32_t IpcBufferEnd = 0x93400000;

    // Writes the boot information into guest memory at `base` (MEM1's first 16 KB).
    void WriteBootInfo(uint8_t* base, const BootInfo& info);
}
