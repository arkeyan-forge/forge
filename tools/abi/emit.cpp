// Emits only public headers. RecompilerConfig::Load parses TOML without opening its executable paths.
#include "pch.h"
#include "recompiler_headers.h"
#include <cstdio>
#include <exception>
#include <utility>

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::fprintf(stderr, "usage: ArkeyanPublicHeaders <config TOML> <context template> <output folder>\n");
        return 2;
    }
    try
    {
        RecompilerConfig config;
        config.Load(argv[1]);
        std::ifstream context(argv[2]);
        if (!context.good())
        {
            std::fprintf(stderr, "Cannot read the public context template\n");
            return 1;
        }
        const std::string policy = RecompilerHeaders::ConfigPrefix(config) + "#endif\n";
        const std::string wrapper = RecompilerHeaders::Context(argv[2]);
        std::filesystem::create_directories(argv[3]);
        for (const auto& [name, contents] :
            { std::pair{ "ppc_config.h", policy }, std::pair{ "ppc_context.h", wrapper } })
        {
            std::ofstream stream(std::filesystem::path(argv[3]) / name, std::ios::binary);
            stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            stream.close();
            if (!stream)
            {
                std::fprintf(stderr, "Cannot write a public header\n");
                return 1;
            }
        }
    }
    catch (const std::exception&)
    {
        // Parser diagnostics can contain paths; keep this command's failure output independent of its inputs.
        std::fprintf(stderr, "Public header emission failed\n");
        return 1;
    }
    return 0;
}
