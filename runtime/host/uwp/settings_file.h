#pragma once

#include "host/launcher.h"
#include "host/settings.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// LocalState\launcher.ini on the console, apart from what applies it (host/uwp/settings_uwp.cpp), so that a computer
// can test it: the same format and sections as on a computer, with the game's own section ([giants], [ssa]...).

// The file written at the first start, so that it can be edited through Device Portal.
std::string UwpDefaultSettingsFile(std::string_view gameId, uint32_t defaultScale);

// The same format as LoadLauncherConfig (host/launcher.cpp, not linked on UWP). False when the file cannot be read.
bool UwpLoadConfig(const std::string& path, LauncherConfig& config);

// The file the console reads (LocalState\launcher.ini), set by the host before anything reads it: what
// LauncherConfigPath returns there. LoadLauncherConfig and SaveLauncherConfig (host/launcher.h, the F3 menu's Commands
// page) read and write it in the same format as on a computer.
void UwpSetLauncherConfigPath(std::string path);

// LocalState\environment.txt: one NAME=value line per environment variable ('#' comments), set before anything
// reads them. The console has no environment of its own: this is how a test aid (SKY_TOYBOX_SCRIPT...) reaches it.
// Empty when the file is missing.
std::vector<std::pair<std::string, std::string>> UwpReadEnvironmentFile(const std::string& path);

// The console's defaults (the gamepad, Xbox One glyphs, `defaultScale`), then `config`'s [video], [input] and
// [<gameId>] sections and the environment. There is no command line.
void UwpParseSettings(const LauncherConfig& config, const std::string& gameId, uint32_t defaultScale,
    const GetEnv& getEnv, RuntimeSettings& settings, std::vector<std::string>& warnings);
