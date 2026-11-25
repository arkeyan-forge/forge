#include "wii/module.h"

namespace wii::module
{
    namespace
    {
        uint32_t Get32(const uint8_t* base, uint32_t address)
        {
            const uint8_t* p = base + address;
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }
    } // namespace

    uint32_t Area::Place(uint32_t size)
    {
        const uint32_t at = (m_used + 31) & ~31u;
        if (size > m_size || at > m_size - size)
            return 0;
        m_used = at + size;
        return m_address + at;
    }

    uint32_t LinkedHash(const uint8_t* base, uint32_t module, uint32_t fileSize)
    {
        // The RSO's header: its sections' count at 8 and their table at 12, each (address, size) once linked.
        const uint32_t count = Get32(base, module + 8), table = Get32(base, module + 12);
        uint32_t hash = 0x811C9DC5;
        for (uint32_t i = 0; i < count && i < 256; i++)
        {
            const uint32_t address = Get32(base, table + 8 * i) & ~1u, size = Get32(base, table + 8 * i + 4);
            if (address < module || address - module >= fileSize || size == 0 || size > fileSize - (address - module))
                continue;
            for (uint32_t at = address; at < address + size; at++)
                hash = (hash ^ base[at]) * 0x01000193;
        }
        return hash;
    }
}
