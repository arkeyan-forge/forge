#include "host/settings.h"
#include "host/game_options.h"
#include "host/profiles.h"
#include "netplay/session_end.h"
#include "netplay/session_settings.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>

namespace
{
    void Check(bool value, const char* message)
    {
        if (!value)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            std::exit(1);
        }
    }
}

bool SetTextLanguage(std::string_view) { return true; }

int main()
{
    Check(netplay::SessionFrameRate("30") == 30 && netplay::SessionFrameRate("60") == 60, "valid session frame rates");
    for (const char* invalid : std::array<const char*, 6>{ nullptr, "", "0", "120", "30x", " 60" })
        Check(netplay::SessionFrameRate(invalid) == 0, "session FPS is required and exact");
    std::map<std::string, std::string> env;
    const GetEnv lookup = [&](const char* name) -> const char* {
        const auto found = env.find(name);
        return found == env.end() ? nullptr : found->second.c_str();
    };
    for (const char* role : { "SKY_NETPLAY_HOST", "SKY_NETPLAY_JOIN" })
        for (const char* fps : { "30", "60" })
        {
            env = { { role, "1234" }, { "SKY_NETPLAY_FPS", fps }, { "SKY_PLAYER2_NAME", "Player Two" } };
            RuntimeSettings settings;
            LauncherConfig config{ { "video", { { "aspect", "fill" }, { "fps_limit", "60" },
                                                  { "fps_auto_fallback", "1" }, { "skip_intro_movies", "1" } } },
                { "test", { { "nightmare_from_start", "1" } } } };
            std::vector<std::string> warnings;
            std::string error;
            char program[] = "test", option[] = "--fps-limit", value[] = "30";
            char* args[] = { program, option, value };
            Check(ParseSettings(config, "test", lookup, 3, args, settings, warnings, error), "session settings parse");
            Check(settings.fpsLimit == uint32_t(std::atoi(fps)), "session FPS overrides file and command line");
            Check(!settings.aspectAuto && !settings.aspectFill && !settings.fpsAutoFallback &&
                      settings.fpsAutoFallbackSet,
                "session aspect and fallback forced");
            Check(!settings.nightmareFromStart && !settings.skipIntroMovies, "guest state options disabled");
            Check(profiles::Parse(lookup, "")[1].signedIn, "second profile signed in as user one");
        }
    env = { { "SKY_NETPLAY_JOIN", "1234" } };
    Check(profiles::Parse(lookup, "")[1].signedIn, "session keeps user one signed in with fallback profile");
    SDL_setenv_unsafe("SKY_NETPLAY_HOST", "1234", 1);
    SDL_setenv_unsafe("SKY_LEVEL", "stand-in", 1);
    SDL_setenv_unsafe("SSA_LEVEL", "stand-in", 1);
    SDL_setenv_unsafe("SKY_PRESS", "stand-in", 1);
    SDL_setenv_unsafe("SKY_SAVE_DIR", ".scratch/session-tests/save", 1);
    RuntimeSettings fixed;
    fixed.fpsLimit = 60;
    fixed.nightmareFromStart = true;
    fixed.skipIntroMovies = true;
    const GameInfo info{ .id = "test", .name = "Test", .releases = {}, .runtime = "" };
    game::Adapter adapter{ .info = info,
        .ReadPortalPrompt = nullptr,
        .ultrawide = false,
        .SetVoiceLanguage = nullptr,
        .glyphTextures = {},
        .introMovies = {} };
    adapter.maxFpsLimit = 30;
    gameoptions::Apply(adapter, fixed, true);
    Check(fixed.fpsLimit == 60 && !fixed.nightmareFromStart && !fixed.skipIntroMovies,
        "game options preserve session FPS and disable cheats");
    Check(!SDL_getenv_unsafe("SKY_LEVEL") && !SDL_getenv_unsafe("SSA_LEVEL"),
        "guest-changing debug switches and their legacy aliases are removed");
    // Pads, scripted or not, reach the game through the exchanged inputs: both games see them at the same frame.
    Check(SDL_getenv_unsafe("SKY_PRESS") != nullptr, "scripted presses are inputs and stay");
    // The launcher passes the host's Nightmare choice to both games: the session's value, not the guest's settings.
    SDL_setenv_unsafe("SKY_NIGHTMARE_FROM_START", "1", 1);
    RuntimeSettings nightmare;
    gameoptions::Apply(adapter, nightmare, true);
    Check(nightmare.nightmareFromStart, "the session's Nightmare from start applies");
    SDL_unsetenv_unsafe("SKY_NIGHTMARE_FROM_START");
    // Scripted tests of a session set SKY_NETPLAY_TEST=1 on both games: their test switches are kept, the same on both.
    SDL_setenv_unsafe("SKY_NETPLAY_TEST", "1", 1);
    SDL_setenv_unsafe("SKY_SKIP_MOVIES", "1", 1);
    RuntimeSettings testing;
    testing.skipIntroMovies = true;
    gameoptions::Apply(adapter, testing, true);
    Check(testing.skipIntroMovies && SDL_getenv_unsafe("SKY_SKIP_MOVIES") != nullptr,
        "a session test keeps its switches");
    SDL_unsetenv_unsafe("SKY_NETPLAY_TEST");
    SDL_unsetenv_unsafe("SKY_SKIP_MOVIES");
    Check(
        std::string(SDL_getenv_unsafe("SKY_SAVE_DIR")) == ".scratch/session-tests/save", "save isolation is preserved");
    netplay::EndScreenClock clock;
    Check(!clock.QuitDue(123), "no quit before the end screen is shown");
    clock.Show(1000);
    clock.Show(3000);
    Check(!clock.QuitDue(4999) && clock.QuitDue(5000), "end message lasts four seconds and does not restart");
    const std::filesystem::path folder = ".scratch/session-tests/end";
    std::filesystem::remove_all(folder);
    for (const auto reason : { netplay::EndReason::Game, netplay::EndReason::Timeout, netplay::EndReason::Desync,
             netplay::EndReason::Quit, netplay::EndReason::Transport })
    {
        Check(netplay::WriteEndFile(folder, reason), "end marker written in save directory");
        std::ifstream file(folder / "online-ended.txt");
        std::string line;
        std::getline(file, line);
        Check(line == std::string("reason=") + netplay::ReasonCode(reason), "stable launcher reason code");
        Check(*netplay::EndMessage(reason), "short end message available");
    }
    std::filesystem::remove_all(folder);
    puts("session setup: forced settings, profiles and end marker passed");
}
