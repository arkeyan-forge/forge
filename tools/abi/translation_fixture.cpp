// Exercises the complete translation header path with an invented section and no guest instructions.
#include "recompiler.h"
#include <cstdio>
#include <exception>

int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    try
    {
        Recompiler recompiler;
        recompiler.config.Load(argv[1]);
        // Discard all output paths from TOML; only the explicitly selected test folder can be written.
        recompiler.config.directoryPath.clear();
        recompiler.config.outDirectoryPath = argv[3];
        std::filesystem::create_directories(argv[3]);
        uint8_t invented[16]{};
        recompiler.image.base = 0x10000;
        recompiler.image.size = sizeof(invented);
        recompiler.image.Map("fixture", 0, sizeof(invented), SectionFlags_Code, invented);
        recompiler.Recompile(argv[2]);
    }
    catch (const std::exception&)
    {
        std::fprintf(stderr, "Translation header fixture failed\n");
        return 1;
    }
    return 0;
}
