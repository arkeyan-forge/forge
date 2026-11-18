#include "host/uwp/settings_file.h"

#include <fmt/format.h>
#include <fstream>

namespace
{
    std::string g_launcherConfigPath;

    std::string Trim(const std::string& text)
    {
        const size_t begin = text.find_first_not_of(" \t\r");
        const size_t end = text.find_last_not_of(" \t\r");
        return begin == std::string::npos ? std::string() : text.substr(begin, end - begin + 1);
    }
}

std::string UwpDefaultSettingsFile(std::string_view gameId, uint32_t defaultScale)
{
    return fmt::format(
        "# ArkeyanRuntime settings (docs/getting-started/settings.md). Edit through Device Portal, then restart "
        "the app.\n"
        "\n[video]\nresolution_scale={}\nanisotropy=1\nvsync=1\n# fps_limit=60\n"
        "\n[{}]\n# language=en\n# voice_language=en\n"
        "\n[portal]\n# auto: a real portal plugged in at start, else the emulated one; real: wait for a real one\n"
        "mode=auto\n"
        "\n[gpu]\n# thread: the GPU thread (off: as 1.0.2488.555, which ran on this console); pipeline_library: "
        "keep\n"
        "# the pipelines built (pipelines.d3d12; on, it removed the device on an Xbox Series X). Turn one on to "
        "try it.\n"
        "thread=off\npipeline_library=off\n",
        defaultScale, gameId);
}

bool UwpLoadConfig(const std::string& path, LauncherConfig& config)
{
    std::ifstream file(path);
    if (!file)
        return false;
    std::string line, section;
    while (std::getline(file, line))
    {
        line = Trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';')
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            section = Trim(line.substr(1, line.size() - 2));
            continue;
        }
        if (const size_t equals = line.find('='); equals != std::string::npos)
            config[section][Trim(line.substr(0, equals))] = Trim(line.substr(equals + 1));
    }
    return true;
}

std::vector<std::pair<std::string, std::string>> UwpReadEnvironmentFile(const std::string& path)
{
    std::vector<std::pair<std::string, std::string>> variables;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line))
    {
        line = Trim(line);
        const size_t equals = line.find('=');
        if (line.empty() || line.front() == '#' || equals == std::string::npos || equals == 0)
            continue;
        variables.emplace_back(Trim(line.substr(0, equals)), Trim(line.substr(equals + 1)));
    }
    return variables;
}

void UwpSetLauncherConfigPath(std::string path) { g_launcherConfigPath = std::move(path); }

std::string LauncherConfigPath() { return g_launcherConfigPath; }

LauncherConfig LoadLauncherConfig(const std::string& path)
{
    LauncherConfig config;
    UwpLoadConfig(path, config);
    return config;
}

bool SaveLauncherConfig(const std::string& path, const LauncherConfig& config)
{
    std::string text = "# ArkeyanRuntime settings (docs/getting-started/settings.md).\n";
    for (const auto& [section, values] : config)
    {
        if (values.empty())
            continue;
        if (!section.empty())
            text.append("\n[").append(section).append("]\n");
        for (const auto& [key, value] : values)
            text.append(key).append("=").append(value).append("\n");
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    return file.write(text.data(), std::streamsize(text.size())) && file.flush();
}

void UwpParseSettings(const LauncherConfig& config, const std::string& gameId, uint32_t defaultScale,
    const GetEnv& getEnv, RuntimeSettings& settings, std::vector<std::string>& warnings)
{
    settings.resolutionScale = defaultScale;
    settings.inputDevice = "gamepad";
    settings.glyphs = "xboxone";
    std::string error; // only the command line can fail, and there is none
    ParseSettings(config, gameId, getEnv, 0, nullptr, settings, warnings, error);
    // The console's pad is always an Xbox One / Series one, and no device events reach glyphs::OnEvent here.
    if (settings.glyphs == "auto" || settings.glyphs == "xbox")
        settings.glyphs = "xboxone";
}
