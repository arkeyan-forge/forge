#pragma once

// The game's executable loaded into guest memory: the disc's DOL (sys/main.dol, what the console runs) or an ELF with
// the same sections. Written from the public descriptions of both formats (docs/runtime/wii/memory_boot.md).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wii
{
    struct Executable
    {
        struct Section
        {
            uint32_t address = 0, size = 0;
            bool code = false, zeroed = false; // zeroed: .bss and kin, cleared rather than copied
        };
        uint32_t entry = 0;
        std::vector<Section> sections;
        uint32_t end = 0; // the end of the highest section: where the loader's arena may start
    };

    // Copies the executable's sections into guest memory at `base` (MapAddressSpace's) and clears its zeroed ones.
    // False with `error` set: not a DOL or an ELF of the 750CL, or a section outside MEM1 and MEM2, or past the file.
    bool LoadExecutable(const uint8_t* data, size_t size, uint8_t* base, Executable& out, std::string& error);
}
