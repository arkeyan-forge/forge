#pragma once

// A relocatable module (an RSO) the host places: the game loads it at run time into its heap, wherever that is, but
// its translation is for one address (tools/rso_link.py). The host keeps an area for it past MEM1, where no Wii game's
// memory is, and gives the module's two blocks (the file, then its BSS) from there; the game's own linker then links it
// there, as the translation did, and the host checks it did (docs/runtime/wii/modules.md).
#include <cstdint>

namespace wii::module
{
    class Area
    {
    public:
        Area(uint32_t address, uint32_t size) : m_address(address), m_size(size) {}

        // The next block of `size` bytes (32-byte aligned), 0 when the area is full.
        uint32_t Place(uint32_t size);
        bool Holds(uint32_t address) const { return address - m_address < m_size; }
        uint32_t Address() const { return m_address; }

    private:
        uint32_t m_address, m_size, m_used = 0;
    };

    // The FNV-1a hash of a linked module's sections, in order (`module` its address, `fileSize` its file's: a section
    // past the file is its BSS, left out), as tools/rso_link.py writes it for the translation.
    uint32_t LinkedHash(const uint8_t* base, uint32_t module, uint32_t fileSize);
}
