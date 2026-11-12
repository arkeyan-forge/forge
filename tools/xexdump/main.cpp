// Decrypts/decompresses a XEX, writes the raw loaded image to <out_dir>/image.bin
// and prints its layout (sections, imports) for analysis.
#include <fstream>
#include <file.h>
#include <image.h>
#include <fmt/core.h>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fmt::println("usage: xexdump <default.xex> <out_dir>");
        return 1;
    }

    const auto file = LoadFile(argv[1]);
    if (file.empty())
    {
        fmt::println("cannot read {}", argv[1]);
        return 1;
    }

    const auto image = Image::ParseImage(file.data(), file.size());
    if (image.data == nullptr)
    {
        fmt::println("cannot parse {}", argv[1]);
        return 1;
    }

    fmt::println("base 0x{:08X} size 0x{:08X} entry 0x{:08X}", image.base, image.size, image.entry_point);
    for (const auto& section : image.sections)
        fmt::println(
            "section {:8} 0x{:08X} 0x{:08X} flags {}", section.name, section.base, section.size, int(section.flags));
    for (const auto& symbol : image.symbols)
        fmt::println("import 0x{:08X} {}", symbol.address, symbol.name);

    std::ofstream out(std::string(argv[2]) + "/image.bin", std::ios::binary);
    out.write(reinterpret_cast<const char*>(image.data.get()), image.size);
    return 0;
}
