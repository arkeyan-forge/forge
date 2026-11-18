// The settings the console uses: LocalState\launcher.ini ([video], [input] and the game's section as on PC,
// docs/getting-started/settings.md, and [portal] mode) over console defaults (host/uwp/settings_file.cpp). The file is
// written with those defaults at the first start, so it can be edited through Device Portal. Compiled for the desktop
// API family like the other files that include SDL's headers (gpu/gpu.h).
#include "game.h"
#include "gpu/gpu.h"
#include "host/game_options.h"
#include "host/glyphs.h"
#include "host/settings.h"
#include "host/uwp/settings_file.h"
#include "host/uwp/uwp.h"
#include "kernel/video.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>

void UwpApplySettings(const std::string& localState, uint32_t defaultScale, RuntimeSettings& settings)
{
    const std::string path = localState + "launcher.ini";
    UwpSetLauncherConfigPath(path);
    LauncherConfig config;
    if (!UwpLoadConfig(path, config))
    {
        std::ofstream(path) << UwpDefaultSettingsFile(g_gameInfo.id, defaultScale);
        fprintf(stderr, "[settings] wrote %s\n", path.c_str());
    }

    // [portal] mode (auto, real, emulated): read by PortalInit as SKY_PORTAL.
    auto portal = config.find("portal");
    std::string portalMode =
        portal != config.end() && portal->second.count("mode") ? portal->second.at("mode") : "auto";
    if (portalMode != "auto" && portalMode != "real" && portalMode != "emulated")
    {
        fprintf(stderr, "[settings] unknown [portal] mode \"%s\" (auto, real or emulated): auto\n", portalMode.c_str());
        portalMode = "auto";
    }
    _putenv_s("SKY_PORTAL", portalMode.c_str());
    fprintf(stderr, "[settings] portal %s\n", portalMode.c_str());

    std::vector<std::string> warnings;
    UwpParseSettings(config, g_gameInfo.id, defaultScale, getenv, settings, warnings);
    for (const std::string& warning : warnings)
        fprintf(stderr, "[settings] %s\n", warning.c_str());
    gameoptions::Apply(game::Current(), settings, true);
    fprintf(stderr, "[settings] %s\n", DescribeSettings(settings).c_str());

    gpu::g_settings = {
        .resolutionScale = settings.resolutionScale,
        .anisotropy = settings.anisotropy,
        .vsync = settings.vsync,
        .fpsLimit = settings.fpsLimit,
    };
    SetFrameRateLimit(settings.fpsLimit);
    // [gpu] thread and pipeline_library (on/off). The GPU thread is off by default on the console: its first run there
    // lost the device at start (docs/runtime/gpu/plume_backends.md); the single-threaded renderer showed the game.
    auto flag = [&](const char* key, bool fallback) {
        auto section = config.find("gpu");
        if (section == config.end() || section->second.find(key) == section->second.end())
            return fallback;
        const std::string& value = section->second.at(key);
        return !(value == "off" || value == "0" || value == "false" || value == "no");
    };
    gpu::g_settings.gpuThread = flag("thread", false);
    // The pipeline library removed the device at start on an Xbox Series X (run 8): off unless asked for.
    gpu::g_settings.pipelineLibrary = flag("pipeline_library", false);
    fprintf(stderr, "[settings] gpu thread %s, pipeline library %s\n", gpu::g_settings.gpuThread ? "on" : "off",
        gpu::g_settings.pipelineLibrary ? "on" : "off");
    glyphs::Init(settings);
}
