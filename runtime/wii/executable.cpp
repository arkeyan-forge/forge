#include "wii/executable.h"
#include "wii/memory_map.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace wii
{
    namespace
    {
        uint32_t Read32(const uint8_t* p)
        {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }
        uint16_t Read16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

        bool Fits(uint64_t offset, uint64_t length, size_t size) { return offset <= size && length <= size - offset; }

        std::string Where(const char* what, uint32_t address, uint32_t size)
        {
            char text[96];
            snprintf(text, sizeof(text), "%s at 0x%08X (0x%X bytes) is outside MEM1 and MEM2", what, address, size);
            return text;
        }

        // A DOL [WiiBrew "DOL"]: 7 text and 11 data sections (file offsets at 0x00, addresses at 0x48, sizes at 0x90),
        // the zeroed range at 0xD8 and 0xDC, the entry at 0xE0; 0x100 bytes of header.
        bool LoadDol(const uint8_t* data, size_t size, uint8_t* base, Executable& out, std::string& error)
        {
            for (int i = 0; i < 18; i++)
            {
                const uint32_t offset = Read32(data + 4 * i), address = Read32(data + 0x48 + 4 * i),
                               length = Read32(data + 0x90 + 4 * i);
                if (length == 0)
                    continue;
                if (!Fits(offset, length, size))
                {
                    error = "the DOL is cut short";
                    return false;
                }
                if (!InMemory(address, length))
                {
                    error = Where("a DOL section", address, length);
                    return false;
                }
                memcpy(base + address, data + offset, length);
                out.sections.push_back({ address, length, i < 7, false });
            }
            const uint32_t bss = Read32(data + 0xD8), bssSize = Read32(data + 0xDC);
            if (bssSize != 0)
            {
                if (!InMemory(bss, bssSize))
                {
                    error = Where("the DOL's zeroed range", bss, bssSize);
                    return false;
                }
                // The DOL's range spans the data sections that sit inside it (.sdata, .sdata2): only the gaps clear.
                std::vector<std::pair<uint32_t, uint32_t>> loaded;
                loaded.reserve(out.sections.size());
                for (const auto& section : out.sections)
                    loaded.emplace_back(section.address, section.address + section.size);
                std::sort(loaded.begin(), loaded.end());
                uint32_t at = bss;
                const uint32_t end = bss + bssSize;
                for (const auto& [from, to] : loaded)
                {
                    if (to <= at || from >= end)
                        continue;
                    if (from > at)
                        memset(base + at, 0, from - at);
                    at = std::max(at, to);
                }
                if (at < end)
                    memset(base + at, 0, end - at);
                out.sections.push_back({ bss, bssSize, false, true });
            }
            out.entry = Read32(data + 0xE0);
            return true;
        }

        // An ELF [the System V ABI, the PowerPC supplement]: 32-bit, big-endian, its allocated sections at their
        // addresses (SHF_ALLOC: copied, or cleared for SHT_NOBITS).
        bool LoadElf(const uint8_t* data, size_t size, uint8_t* base, Executable& out, std::string& error)
        {
            if (size < 52 || data[4] != 1 || data[5] != 2 || Read16(data + 18) != 20)
            {
                error = "not a 32-bit big-endian PowerPC ELF";
                return false;
            }
            const uint32_t sectionOffset = Read32(data + 32);
            const uint16_t entrySize = Read16(data + 46), count = Read16(data + 48);
            if (entrySize < 40 || !Fits(sectionOffset, uint64_t(entrySize) * count, size))
            {
                error = "the ELF's section table is cut short";
                return false;
            }
            for (uint16_t i = 0; i < count; i++)
            {
                const uint8_t* section = data + sectionOffset + size_t(i) * entrySize;
                const uint32_t type = Read32(section + 4), flags = Read32(section + 8), address = Read32(section + 12),
                               offset = Read32(section + 16), length = Read32(section + 20);
                constexpr uint32_t Progbits = 1, Nobits = 8, Alloc = 2, Exec = 4;
                if (!(flags & Alloc) || length == 0 || (type != Progbits && type != Nobits))
                    continue;
                if (!InMemory(address, length))
                {
                    error = Where("an ELF section", address, length);
                    return false;
                }
                if (type == Nobits)
                    memset(base + address, 0, length);
                else if (!Fits(offset, length, size))
                {
                    error = "the ELF is cut short";
                    return false;
                }
                else
                    memcpy(base + address, data + offset, length);
                out.sections.push_back({ address, length, (flags & Exec) != 0, type == Nobits });
            }
            out.entry = Read32(data + 24);
            return true;
        }
    } // namespace

    bool LoadExecutable(const uint8_t* data, size_t size, uint8_t* base, Executable& out, std::string& error)
    {
        out = {};
        bool loaded;
        if (size >= 4 && memcmp(data,
                             "\x7F"
                             "ELF",
                             4) == 0)
            loaded = LoadElf(data, size, base, out, error);
        else if (size >= 0x100)
            loaded = LoadDol(data, size, base, out, error);
        else
        {
            error = "not a DOL or an ELF";
            return false;
        }
        if (!loaded)
            return false;
        bool entryInCode = false;
        for (const auto& section : out.sections)
        {
            out.end = std::max(out.end, section.address + section.size);
            entryInCode = entryInCode ||
                          (section.code && out.entry >= section.address && out.entry - section.address < section.size);
        }
        if (!entryInCode)
        {
            error = "the entry point is not in the code";
            return false;
        }
        return true;
    }
}
