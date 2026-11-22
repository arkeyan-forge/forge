#pragma once

#include <cstdint>
#include <map>

// What MmQueryAddressProtect answers for physical memory: the protection the title allocated it with, cache attributes
// included. Swap Force reads PAGE_WRITECOMBINE there to tell a render buffer the GPU reads from write-combined memory
// from one it must copy for the CPU first; answering plain read-write made it copy every dynamic buffer into its 247 MB
// pool, which then ran out (forge issue #84, games/swapforce/docs/status.md).
namespace kernel
{
    constexpr uint32_t PageReadWrite = 0x004, PageNoCache = 0x200, PageWriteCombine = 0x400;

    // MmAllocatePhysicalMemoryEx's protect argument as MmQueryAddressProtect returns it: the page protection and cache
    // attributes (low half), without the page-size flags; read-write when it names no access.
    inline uint32_t AddressProtect(uint32_t protect)
    {
        const uint32_t page = protect & 0xFFFF;
        return (page & 0xFF) != 0 ? page : page | PageReadWrite;
    }

    // The protection of each physical allocation, by physical base. Not locked: the caller holds its own lock.
    class PhysicalProtects
    {
    public:
        void Set(uint32_t base, uint32_t protect) { m_protects[base] = AddressProtect(protect); }
        void Erase(uint32_t base) { m_protects.erase(base); }
        // An allocation's answer; read-write for one made without a protect argument.
        uint32_t Get(uint32_t base) const
        {
            auto it = m_protects.find(base);
            return it != m_protects.end() ? it->second : PageReadWrite;
        }

    private:
        std::map<uint32_t, uint32_t> m_protects;
    };
}
