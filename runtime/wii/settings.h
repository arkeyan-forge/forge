#pragma once

// The console's settings, which the SDK's settings library (SC) reads: SYSCONF on the NAND (/shared2/sys/SYSCONF:
// the language, the screen, the sound, the remotes) and setting.txt, the console's region, which the loader leaves in
// low memory. The host makes both (docs/runtime/wii/ios.md). From the public description of the files (WiiBrew,
// "/shared2/sys/SYSCONF" and "setting.txt") and the reads of the SDK the game carries.
#include <cstdint>
#include <string>
#include <vector>

namespace wii
{
    struct ConsoleSettings
    {
        uint8_t language = 1; // 0 Japanese, 1 English, 2 German, 3 French, 4 Spanish, 5 Italian, 6 Dutch
        bool wide = true;     // 16:9
        bool progressive = true;
        bool euRgb60 = true; // a PAL game at 60 Hz
        uint8_t sound = 1;   // 0 mono, 1 stereo, 2 surround
    };

    constexpr uint32_t SysconfSize = 0x4000;

    // SYSCONF: "SCv0", the count of items, each item's offset and the end's, the items, "SCed" at the end. An item is a
    // byte (its type in the top 3 bits, its name's length less one below), the name, then its value: a byte, a 32-bit
    // number, or an array (its length less one first).
    std::vector<uint8_t> MakeSysconf(const ConsoleSettings& settings);

    // setting.txt, as the loader leaves it at 0x80003800: "KEY=value" lines for the console's region, encrypted by
    // its rolling key. `region` is the disc's region letter (the game ID's fourth: J, E, P, K).
    constexpr uint32_t SettingTextSize = 0x100;
    std::vector<uint8_t> MakeSettingText(char region);
    std::string DecryptSettingText(const std::vector<uint8_t>& encrypted); // up to the first NUL
}
