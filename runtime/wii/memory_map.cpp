#include "wii/memory_map.h"
#include "platform/platform.h"
#include <cstdio>
#include <utility>

namespace wii
{
    namespace
    {
        constexpr uint64_t AddressSpaceSize = 0x100000000ull;

        uint32_t AlignDown(uint32_t value) { return value & ~(MapGranularity - 1); }
        uint64_t AlignUp(uint64_t value) { return (value + MapGranularity - 1) & ~uint64_t(MapGranularity - 1); }

        // True when [start, end) touches either address of either memory.
        bool TouchesMemory(uint64_t start, uint64_t end)
        {
            const std::pair<uint64_t, uint64_t> memories[] = { { Mem1Cached, Mem1Size }, { Mem1Uncached, Mem1Size },
                { Mem2Cached, Mem2Size }, { Mem2Uncached, Mem2Size } };
            for (const auto& [at, size] : memories)
                if (start < at + size && at < end)
                    return true;
            return false;
        }

        bool Shared(
            uint8_t* base, uint32_t size, uint32_t cached, uint32_t uncached, const char* name, std::string& error)
        {
            const PlatformSharedMemory memory = PlatformCreateSharedMemory(size);
            if (memory == -1 || !PlatformMapShared(base + cached, size, memory) ||
                !PlatformMapShared(base + uncached, size, memory))
            {
                error =
                    std::string("cannot map ") + name + " (system error " + std::to_string(PlatformLastError()) + ")";
                return false;
            }
            return true;
        }
    } // namespace

    bool MapAddressSpace(uint8_t*& base, const std::vector<Range>& extra, std::string& error)
    {
        base = PlatformReserveAddressSpace(AddressSpaceSize);
        if (base == nullptr)
        {
            error = "cannot reserve 4 GB of address space for guest memory";
            return false;
        }
        if (!Shared(base, Mem1Size, Mem1Cached, Mem1Uncached, "MEM1", error) ||
            !Shared(base, Mem2Size, Mem2Cached, Mem2Uncached, "MEM2", error))
            return false;
        std::vector<Range> privateRanges = { { ProcessorRegisters, RegisterBlockSize },
            { StarletRegisters, RegisterBlockSize }, { HollywoodRegisters, RegisterBlockSize },
            { LockedCache, LockedCacheSize } };
        privateRanges.insert(privateRanges.end(), extra.begin(), extra.end());
        for (const Range& range : privateRanges)
        {
            const uint32_t start = AlignDown(range.address);
            const uint64_t end = AlignUp(uint64_t(range.address) + range.size);
            if (range.size == 0 || end > AddressSpaceSize || TouchesMemory(start, end) ||
                !PlatformMapPrivate(base + start, size_t(end - start)))
            {
                char text[96];
                snprintf(text, sizeof(text), "cannot map 0x%08X-0x%08llX", start, (unsigned long long)end);
                error = text;
                return false;
            }
        }
        return true;
    }

    bool InMemory(uint32_t address, uint32_t size)
    {
        const uint64_t end = uint64_t(address) + size;
        for (uint32_t start : { Mem1Cached, Mem1Uncached })
            if (address >= start && end <= uint64_t(start) + Mem1Size)
                return true;
        for (uint32_t start : { Mem2Cached, Mem2Uncached })
            if (address >= start && end <= uint64_t(start) + Mem2Size)
                return true;
        return false;
    }
}
