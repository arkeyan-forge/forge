#pragma once

// The Wii's address space as the game's code sees it: its two memories, each at a cached address and at an uncached
// mirror of it, the hardware's register blocks and the processor's locked cache. Mapped into a host reservation of the
// whole 32-bit space, so guest address A lives at base + A, as for the Xbox 360 games (docs/runtime/wii/memory_boot.md).
#include <cstdint>
#include <string>
#include <vector>

namespace wii
{
    constexpr uint32_t Mem1Size = 0x01800000; // 24 MB
    constexpr uint32_t Mem2Size = 0x04000000; // 64 MB
    constexpr uint32_t Mem1Cached = 0x80000000, Mem1Uncached = 0xC0000000;
    constexpr uint32_t Mem2Cached = 0x90000000, Mem2Uncached = 0xD0000000;
    constexpr uint32_t ProcessorRegisters = 0xCC000000; // the GameCube's blocks: VI, PI, MI, DSP, DI, SI, EXI, AI
    constexpr uint32_t StarletRegisters = 0xCD000000;   // the IPC registers to IOS
    constexpr uint32_t HollywoodRegisters = 0xCD800000; // the Wii's own blocks
    constexpr uint32_t RegisterBlockSize = 0x10000;
    constexpr uint32_t LockedCache = 0xE0000000; // dcbz_l and the locked cache's DMA
    constexpr uint32_t LockedCacheSize = 0x4000;
    constexpr uint32_t MapGranularity = 0x10000; // every range is rounded out to 64 KB

    struct Range
    {
        uint32_t address = 0;
        uint32_t size = 0;
    };

    // The guest's 4 GB: MEM1 and MEM2 (each at both addresses, the same bytes), the register blocks, the locked cache
    // and `extra` (the translated code's function table), every other address inaccessible. False with `error` set.
    bool MapAddressSpace(uint8_t*& base, const std::vector<Range>& extra, std::string& error);

    // True when [address, address + size) lies in one memory, through either of its addresses.
    bool InMemory(uint32_t address, uint32_t size);
}
