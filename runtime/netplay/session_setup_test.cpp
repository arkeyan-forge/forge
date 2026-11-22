#include "host/settings.h"
#include "host/game_options.h"
#include "host/profiles.h"
#include "netplay/session_end.h"
#include "netplay/session_settings.h"
#include "host/debug_screen.h"
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
            std::exit(1); // NOLINT(concurrency-mt-unsafe): one thread
        }
    }

    // The C runtime's own environment, which skyenv reads (on Windows SDL's setter does not reach it). One thread.
    void SetEnv(const char* name, const char* value)
    {
#ifdef _WIN32
        _putenv_s(name, value);
#else
        setenv(name, value, 1); // NOLINT(concurrency-mt-unsafe)
#endif
    }

    void UnsetEnv(const char* name)
    {
#ifdef _WIN32
        _putenv_s(name, "");
#else
        unsetenv(name); // NOLINT(concurrency-mt-unsafe)
#endif
    }

    const char* GetEnvVar(const char* name) { return std::getenv(name); } // NOLINT(concurrency-mt-unsafe)
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
    SetEnv("SKY_NETPLAY_HOST", "1234");
    SetEnv("SKY_LEVEL", "stand-in");
    SetEnv("SSA_LEVEL", "stand-in");
    SetEnv("SKY_PRESS", "stand-in");
    SetEnv("SKY_SAVE_DIR", ".scratch/session-tests/save");
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
    Check(!GetEnvVar("SKY_LEVEL") && !GetEnvVar("SSA_LEVEL"),
        "guest-changing debug switches and their legacy aliases are removed");
    // Pads, scripted or not, reach the game through the exchanged inputs: both games see them at the same frame.
    Check(GetEnvVar("SKY_PRESS") != nullptr, "scripted presses are inputs and stay");
    // The launcher passes the host's Nightmare choice to both games: the session's value, not the guest's settings.
    SetEnv("SKY_NIGHTMARE_FROM_START", "1");
    RuntimeSettings nightmare;
    gameoptions::Apply(adapter, nightmare, true);
    Check(nightmare.nightmareFromStart, "the session's Nightmare from start applies");
    UnsetEnv("SKY_NIGHTMARE_FROM_START");
    // Scripted tests of a session set SKY_NETPLAY_TEST=1 on both games: their test switches are kept, the same on both.
    SetEnv("SKY_NETPLAY_TEST", "1");
    SetEnv("SKY_SKIP_MOVIES", "1");
    RuntimeSettings testing;
    testing.skipIntroMovies = true;
    gameoptions::Apply(adapter, testing, true);
    Check(testing.skipIntroMovies && GetEnvVar("SKY_SKIP_MOVIES") != nullptr, "a session test keeps its switches");
    UnsetEnv("SKY_NETPLAY_TEST");
    UnsetEnv("SKY_SKIP_MOVIES");
    Check(std::string(GetEnvVar("SKY_SAVE_DIR")) == ".scratch/session-tests/save", "save isolation is preserved");
    netplay::SavePolicy saves;
    Check(saves.Allowed(), "solo saves initially enabled");
    saves.End(true, true, netplay::EndReason::Quit, false);
    Check(saves.Allowed(), "host still saves after guest leaves");
    saves.End(false, true, netplay::EndReason::Timeout, false);
    Check(!saves.Allowed(), "guest stops saving after host vanishes");
    saves.End(true, true, netplay::EndReason::None, false);
    Check(!saves.Allowed(), "save suppression persists for the remainder of the run");
    for (auto reason : { netplay::EndReason::Quit, netplay::EndReason::Timeout, netplay::EndReason::Transport,
             netplay::EndReason::Desync })
    {
        netplay::SavePolicy guest;
        guest.End(false, true, reason, false);
        Check(!guest.Allowed(), "all remote departure reasons suppress guest saves");
        netplay::SavePolicy host;
        host.End(true, true, reason, false);
        Check(host.Allowed(), "host saves survive solo continuation");
        Check(netplay::ContinueSolo(true, reason, false) && !netplay::ContinueSolo(true, reason, true) &&
                  !netplay::ContinueSolo(false, reason, false),
            "only established remote departures continue");
    }
    Check(std::string_view(netplay::EndMessage(netplay::EndReason::Desync)).find("continue alone") !=
              std::string_view::npos,
        "desync message explains solo continuation");
    netplay::SessionStats stats;
    stats.roundTripMs = 42;
    stats.delay = 3;
    char online[192];
    debugscreen::OnlineCompact(online, sizeof(online), stats);
    Check(std::string_view(online) == "42 ms  3f  checking", "compact online before hash check");
    stats.goodHash = 300;
    stats.goodChecks = 10;
    stats.goodChecks = 10;
    debugscreen::OnlineCompact(online, sizeof(online), stats);
    Check(std::string_view(online) == "42 ms  3f  ok", "compact online after hash check");
    stats.minPingMs = 12;
    stats.maxPingMs = 67;
    stats.waitMs = 1.25;
    stats.waitedFrames = 7;
    stats.sentPerSecond = 120;
    stats.receivedPerSecond = 98;
    stats.lost = 4;
    stats.rejected = 2;
    stats.sentKB = 10.5;
    stats.receivedKB = 8.25;
    debugscreen::Panel panel;
    debugscreen::OnlineFull(panel, stats);
    Check(panel.count == 6 && std::string_view(panel.rows[0].value) == "42 ms  min 12  max 67" &&
              std::string_view(panel.rows[2].value) == "1.25 ms per frame  7 frames waited (60 s)" &&
              std::string_view(panel.rows[3].value) == "tx 120  rx 98  lost 4  bad tag 2" &&
              std::string_view(panel.rows[4].value) == "tx 10.50  rx 8.25 KB per s" &&
              std::string_view(panel.rows[5].value) == "frame 300  10 good checks  0 repaired",
        "full online formatting");
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
    Check(netplay::WriteOpenOnlineFile(folder, "test"), "open-online marker written");
    {
        std::ifstream file(folder / "open-online.txt");
        std::string content((std::istreambuf_iterator<char>(file)), {});
        Check(content == "test\n", "launcher receives exactly the game id and newline");
    }
    Check(!netplay::WriteOpenOnlineFile(folder, "bad\nline"), "invalid game id cannot add launcher instructions");
    std::filesystem::remove_all(folder);
    puts("session setup: forced settings, profiles and end marker passed");
}
