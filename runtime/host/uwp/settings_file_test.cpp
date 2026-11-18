// Checks the console's settings file (host/uwp/settings_file.cpp): the file written at the first start names the game's
// own section, and the settings are read from that section, not another game's.
// usage: ArkeyanUwpSettingsTest
#include "host/uwp/settings_file.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    const char* NoEnvironment(const char*) { return nullptr; }
}

int main()
{
    // The first start's file: the game's section, the scale the host chose.
    const std::string file = UwpDefaultSettingsFile("giants", 2);
    CHECK(file.find("\n[giants]\n") != std::string::npos);
    CHECK(file.find("[ssa]") == std::string::npos);
    CHECK(file.find("resolution_scale=2\n") != std::string::npos);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "arkeyan_uwp_settings_test.ini";
    std::ofstream(path) << file;
    LauncherConfig config;
    CHECK(UwpLoadConfig(path.string(), config));
    CHECK(config["video"]["resolution_scale"] == "2");
    CHECK(config["portal"]["mode"] == "auto");
    CHECK(config["gpu"]["thread"] == "off");

    // The console's defaults with that file.
    RuntimeSettings settings;
    std::vector<std::string> warnings;
    UwpParseSettings(config, "giants", 2, NoEnvironment, settings, warnings);
    CHECK(warnings.empty());
    CHECK(settings.resolutionScale == 2);
    CHECK(settings.inputDevice == "gamepad");
    CHECK(settings.glyphs == "xboxone");
    CHECK(settings.language.empty());

    // The game's own section wins; another game's is not read.
    config["ssa"]["language"] = "fr";
    config["ssa"]["nightmare_from_start"] = "1";
    config["giants"]["language"] = "de";
    config["giants.video"]["fps_limit"] = "60";
    config["input"]["glyphs"] = "xbox";
    RuntimeSettings giants;
    warnings.clear();
    UwpParseSettings(config, "giants", 1, NoEnvironment, giants, warnings);
    CHECK(warnings.empty());
    CHECK(giants.language == "de");
    CHECK(!giants.nightmareFromStart);
    CHECK(giants.fpsLimit == 60);
    CHECK(giants.resolutionScale == 2); // the file's, over the host's default
    CHECK(giants.glyphs == "xboxone"); // the console's pad, whatever the file says

    RuntimeSettings ssa;
    UwpParseSettings(config, "ssa", 1, NoEnvironment, ssa, warnings);
    CHECK(ssa.language == "fr");
    CHECK(ssa.nightmareFromStart);
    CHECK(ssa.fpsLimit == 30);

    // The F3 menu's Commands page: what it saves reads back the same, at the path the host set.
    UwpSetLauncherConfigPath(path.string());
    CHECK(LauncherConfigPath() == path.string());
    CHECK(SaveLauncherConfig(LauncherConfigPath(), config));
    CHECK(LoadLauncherConfig(LauncherConfigPath()) == config);

    // environment.txt: NAME=value lines, comments and malformed lines skipped.
    std::ofstream(path) << "# test aids\nSKY_TOYBOX_SCRIPT = pad=back*0.8:30\n=nothing\nno equals\nSKY_EMPTY=\n";
    const auto variables = UwpReadEnvironmentFile(path.string());
    CHECK(variables.size() == 2);
    CHECK(
        variables.size() == 2 && variables[0].first == "SKY_TOYBOX_SCRIPT" && variables[0].second == "pad=back*0.8:30");
    CHECK(variables.size() == 2 && variables[1].first == "SKY_EMPTY" && variables[1].second.empty());

    // No file: false, nothing read.
    std::filesystem::remove(path);
    CHECK(LoadLauncherConfig(path.string()).empty());
    LauncherConfig missing;
    CHECK(!UwpLoadConfig(path.string(), missing));
    CHECK(missing.empty());

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
