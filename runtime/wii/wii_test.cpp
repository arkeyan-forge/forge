// The Wii host's address space, executable loader and boot information, on synthetic executables. No game data.
// usage: ArkeyanWiiTest
#include "wii/boot_info.h"
#include "wii/executable.h"
#include "wii/memory_map.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    int checks = 0, failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

    uint8_t* g_base = nullptr;
    constexpr uint32_t TableAddress = 0x81EE1E60; // an indirect-call table after MEM1, unaligned as it comes

    uint32_t Get32(uint32_t address)
    {
        const uint8_t* p = g_base + address;
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
    }

    using Bytes = std::vector<uint8_t>;
    void Put32(Bytes& b, size_t at, uint32_t v)
    {
        if (b.size() < at + 4)
            b.resize(at + 4);
        for (int i = 0; i < 4; i++)
            b[at + i] = uint8_t(v >> (24 - 8 * i));
    }
    void Put16(Bytes& b, size_t at, uint32_t v)
    {
        if (b.size() < at + 2)
            b.resize(at + 2);
        b[at] = uint8_t(v >> 8);
        b[at + 1] = uint8_t(v);
    }

    // An ELF: .text (code) at 0x80004000, .data at 0x80005000, .bss (no bits) at 0x80006000, a non-allocated section.
    Bytes MakeElf(uint32_t textAddress = 0x80004000, uint32_t entry = 0x80004004, bool cut = false)
    {
        Bytes b(0x200, 0);
        memcpy(b.data(),
            "\x7F"
            "ELF\x01\x02\x01",
            7);
        Put16(b, 16, 2);
        Put16(b, 18, 20); // PowerPC
        Put32(b, 24, entry);
        Put32(b, 32, 0x100); // the section table
        Put16(b, 46, 40);
        Put16(b, 48, 5);
        auto section = [&](int i, uint32_t type, uint32_t flags, uint32_t address, uint32_t offset, uint32_t size) {
            const size_t at = 0x100 + size_t(i) * 40;
            Put32(b, at + 4, type);
            Put32(b, at + 8, flags);
            Put32(b, at + 12, address);
            Put32(b, at + 16, offset);
            Put32(b, at + 20, size);
        };
        section(1, 1, 6, textAddress, 0x40, 8);        // .text: alloc, exec
        section(2, 1, 3, 0x80005000, 0x48, 8);         // .data: alloc, write
        section(3, 8, 3, 0x80006000, 0, 0x20);         // .bss
        section(4, 1, 0, 0, 0x50, cut ? 0x1000 : 4);   // a comment: not loaded
        Put32(b, 0x40, 0x38600007);
        Put32(b, 0x44, 0x4E800020);
        Put32(b, 0x48, 0x11223344);
        Put32(b, 0x4C, 0x55667788);
        b.resize(0x100 + 5 * 40);
        return b;
    }

    // A DOL: one text section, one data section inside the zeroed range (as .sdata sits in the real one's .bss).
    Bytes MakeDol(uint32_t dataAddress = 0x80008000)
    {
        Bytes b(0x100, 0);
        Put32(b, 0x00, 0x100);       // text 0: offset
        Put32(b, 0x48, 0x80004000);  // its address
        Put32(b, 0x90, 8);           // its size
        Put32(b, 0x1C, 0x108);       // data 0 (index 7)
        Put32(b, 0x48 + 28, dataAddress);
        Put32(b, 0x90 + 28, 8);
        Put32(b, 0xD8, 0x80007000);  // zeroed: 0x80007000-0x8000A000
        Put32(b, 0xDC, 0x3000);
        Put32(b, 0xE0, 0x80004000);
        Put32(b, 0x100, 0x60000000);
        Put32(b, 0x104, 0x4E800020);
        Put32(b, 0x108, 0xCAFEF00D);
        Put32(b, 0x10C, 0x0BADBEEF);
        return b;
    }

    void AddressSpace()
    {
        std::string error;
        const bool mapped = wii::MapAddressSpace(g_base, { { TableAddress, 0x1000 } }, error);
        EXPECT(mapped && error.empty());
        if (!mapped)
        {
            fprintf(stderr, "%s\n", error.c_str());
            return;
        }
        // Each memory at both of its addresses: the same bytes.
        g_base[0x80001234] = 0x5A;
        EXPECT(g_base[0xC0001234] == 0x5A);
        g_base[0xD3FFFFFF] = 0xA5;
        EXPECT(g_base[0x93FFFFFF] == 0xA5);
        g_base[0x90000000] = 1;
        EXPECT(g_base[0x80000000] == 0 && g_base[0xD0000000] == 1); // MEM2 is not MEM1
        EXPECT(g_base[0x817FFFFF] == 0 && g_base[0x80001235] == 0); // zero-filled
        // The register blocks, the locked cache and the table: plain memory.
        for (uint32_t address :
            { 0xCC00FFFFu, 0xCD000000u, 0xCD80FFFFu, 0xE0003FFFu, TableAddress, TableAddress + 0xFFF })
        {
            g_base[address] = 7;
            EXPECT(g_base[address] == 7);
        }
        EXPECT(wii::InMemory(0x80000000, wii::Mem1Size) && wii::InMemory(0xD0000000, wii::Mem2Size));
        // A range over a memory, even in part, is refused: it would hide the memory's bytes at one of its addresses.
        uint8_t* other = nullptr;
        EXPECT(!wii::MapAddressSpace(other, { { 0x817F0000, 0x20000 } }, error) &&
               error == "cannot map 0x817F0000-0x81810000");
        EXPECT(!wii::InMemory(0x817FFFFF, 2) && !wii::InMemory(0x70000000, 4) && !wii::InMemory(0xCC000000, 4));
    }

    void Elf()
    {
        memset(g_base + 0x80006000, 0xAA, 0x40);
        wii::Executable executable;
        std::string error;
        const Bytes elf = MakeElf();
        EXPECT(wii::LoadExecutable(elf.data(), elf.size(), g_base, executable, error));
        EXPECT(executable.entry == 0x80004004 && executable.sections.size() == 3 && executable.end == 0x80006020);
        EXPECT(Get32(0x80004000) == 0x38600007 && Get32(0xC0004004) == 0x4E800020); // through the mirror too
        EXPECT(Get32(0x80005000) == 0x11223344 && Get32(0x80005004) == 0x55667788);
        EXPECT(Get32(0x80006000) == 0 && Get32(0x8000601C) == 0 && g_base[0x80006020] == 0xAA); // .bss, not past it
        EXPECT(executable.sections[0].code && !executable.sections[1].code && executable.sections[2].zeroed);

        const Bytes outside = MakeElf(0x70000000);
        EXPECT(!wii::LoadExecutable(outside.data(), outside.size(), g_base, executable, error) &&
               error.find("outside MEM1 and MEM2") != std::string::npos);
        const Bytes entry = MakeElf(0x80004000, 0x80005000);
        EXPECT(!wii::LoadExecutable(entry.data(), entry.size(), g_base, executable, error) &&
               error == "the entry point is not in the code");
        Bytes cut = MakeElf();
        Put32(cut, 0x100 + 40 + 16, 0x1F0); // .text's bytes past the file's end
        EXPECT(
            !wii::LoadExecutable(cut.data(), cut.size(), g_base, executable, error) && error == "the ELF is cut short");
        Bytes table = MakeElf();
        Put32(table, 32, 0x180); // the section table runs past the end
        EXPECT(!wii::LoadExecutable(table.data(), table.size(), g_base, executable, error) &&
               error == "the ELF's section table is cut short");
        const uint8_t garbage[8] = {};
        EXPECT(!wii::LoadExecutable(garbage, sizeof(garbage), g_base, executable, error) &&
               error == "not a DOL or an ELF");
    }

    void Dol()
    {
        memset(g_base + 0x80007000, 0xAA, 0x3010);
        wii::Executable executable;
        std::string error;
        const Bytes dol = MakeDol();
        EXPECT(wii::LoadExecutable(dol.data(), dol.size(), g_base, executable, error));
        EXPECT(executable.entry == 0x80004000 && executable.sections.size() == 3 && executable.end == 0x8000A000);
        EXPECT(Get32(0x80004004) == 0x4E800020);
        // The data section inside the zeroed range keeps its bytes; the rest of the range clears, nothing past it.
        EXPECT(Get32(0x80008000) == 0xCAFEF00D && Get32(0x80008004) == 0x0BADBEEF);
        EXPECT(Get32(0x80007000) == 0 && Get32(0x80007FFC) == 0 && Get32(0x80008008) == 0 && Get32(0x80009FFC) == 0);
        EXPECT(g_base[0x8000A000] == 0xAA);

        Bytes outside = MakeDol(0x81800000);
        EXPECT(!wii::LoadExecutable(outside.data(), outside.size(), g_base, executable, error) &&
               error.find("outside MEM1 and MEM2") != std::string::npos);
        Bytes cut = MakeDol();
        cut.resize(0x10C);
        EXPECT(
            !wii::LoadExecutable(cut.data(), cut.size(), g_base, executable, error) && error == "the DOL is cut short");
    }

    void BootInformation()
    {
        wii::BootInfo info;
        memcpy(info.discHeader, "RTST01", 6);
        info.discHeader[0x18] = 0x5D;
        info.discHeader[0x19] = 0x1C;
        info.discHeader[0x1A] = 0x9E;
        info.discHeader[0x1B] = 0xA3;
        info.ios = 56;
        info.iosRevision = 5918;
        g_base[0x80003FFF] = 1;
        wii::WriteBootInfo(g_base, info);
        EXPECT(memcmp(g_base + 0x80000000, "RTST01", 6) == 0 && Get32(0x80000018) == 0x5D1C9EA3);
        EXPECT(Get32(0x80000020) == 0x0D15EA5E && Get32(0x80000028) == wii::Mem1Size);
        EXPECT(Get32(0x800000F8) == 243000000 && Get32(0x800000FC) == 729000000);
        EXPECT(
            Get32(0x80003118) == wii::Mem2Size && Get32(0x80003128) == 0x933E0000 && Get32(0x80003134) == 0x93400000);
        EXPECT(Get32(0x80003140) == (56u << 16 | 5918) && memcmp(g_base + 0x80003180, "RTST", 4) == 0);
        EXPECT(g_base[0x80003FFF] == 0 && Get32(0xC0000020) == 0x0D15EA5E); // the rest cleared; through the mirror
    }

} // namespace

int main()
{
    AddressSpace();
    if (g_base != nullptr)
    {
        Elf();
        Dol();
        BootInformation();
    }
    printf("wii test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
