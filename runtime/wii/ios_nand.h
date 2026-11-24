#pragma once

// IOS's file system and its title service on the host's NAND (ios_nand.cpp): /dev/fs, the NAND's files opened by their
// path, and /dev/es, which tells the game its title and its data folder (docs/runtime/wii/ios.md).
#include "wii/ios.h"
#include <memory>
#include <string>

namespace wii
{
    namespace nand
    {
        class FileSystem;
    }
    struct ConsoleSettings;

    std::unique_ptr<ios::Device> MakeFileSystemDevice(nand::FileSystem& nand);
    std::unique_ptr<ios::Device> MakeNandFile(nand::FileSystem& nand, const std::string& path);
    std::unique_ptr<ios::Device> MakeTitleService(uint64_t titleId);

    // What the console's NAND holds before a game runs: the title's data folder and the console's settings (each made
    // only when missing, so the game's own saves and changes stay).
    bool PrepareNand(nand::FileSystem& nand, uint64_t titleId, const ConsoleSettings& settings, char region);

    // "/title/<high>/<low>/data", the title's own folder on the NAND.
    std::string TitleDataFolder(uint64_t titleId);
}
