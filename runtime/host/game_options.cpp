#include "host/game_options.h"

#include "host/gamemenu.h"
#include "language.h"
#include "netplay/session_runtime.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace gameoptions
{
    namespace
    {
        // skyenv reads the C runtime's environment, which SDL's setters do not reach on Windows: both are cleared.
        void Unset(const char* name)
        {
            SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), name);
#ifdef _WIN32
            _putenv_s(name, "");
#else
            unsetenv(name); // NOLINT(concurrency-mt-unsafe): before the game's threads start
#endif
        }
    }

    void Apply(const game::Adapter& adapter, RuntimeSettings& settings, bool capFps)
    {
        if (netplay::Online())
        {
            // A scripted test of a session sets SKY_NETPLAY_TEST=1 on both games, so its switches are the same on both.
            const char* testing = skyenv::Get("SKY_NETPLAY_TEST");
            const bool test = testing != nullptr && std::string_view(testing) == "1";
            // These switches inject host choices into guest execution before any guest thread starts. Pads (SKY_PRESS*)
            // and the Toy Box's script are not among them: they reach the game as exchanged inputs and portal events.
            constexpr const char* Blocked[] = { "SKY_LEVEL", "SKY_LEVEL_FROM", "SKY_NEXT_LEVEL", "SKY_MOVIE_SWAP",
                "SKY_SKIP_MOVIES", "SKY_CLOCK_STEP", "SKY_FPS_TEST_LATE_MS", "SKY_HOST_LZMA", "SKY_SWAP_WAIT",
                "SKY_TEST_READ_FAIL", "SKY_RESOLVE_DEFER", "SKY_CLEAR_NEW_TEXTURES", "SKY_SKIP_PS", "SKY_PORTAL_MODEL",
                "SKY_TOYBOX_CLOCK", "SKY_TOYBOX_REAL_VIEW", "SKY_CRASH_TEST", "SKY_NETPLAY_TEST_DESYNC",
                "SKY_NETPLAY_TEST_NET", "SKY_NETPLAY_TEST_SLOW", "SKY_NETPLAY_TEST_DEPTH_NOISE",
                "SKY_NETPLAY_TEST_XMA_NATIVE", "SKY_NETPLAY_TEST_HOST_RANDOM" };
            for (const char* variable : Blocked)
            {
                if (test)
                    break;
                std::string older, unused;
                skyenv::OlderNames(variable, older, unused);
                Unset(variable);
                Unset(older.c_str());
            }
            // The launcher gives both games the host's Nightmare choice; the guest's own settings do not count.
            const char* nightmare = skyenv::Get("SKY_NIGHTMARE_FROM_START");
            settings.nightmareFromStart = nightmare != nullptr && std::string_view(nightmare) == "1";
            if (!test)
                settings.skipIntroMovies = false;
        }
        if (const uint32_t capped = gamemenu::CapFrameRate(settings.fpsLimit, adapter.maxFpsLimit);
            capFps && !netplay::Online() && capped != settings.fpsLimit)
        {
            fprintf(stderr, "[settings] fps_limit=%u: this game is capped at %u FPS, using %u\n", settings.fpsLimit,
                adapter.maxFpsLimit, capped);
            settings.fpsLimit = capped;
        }
        if (!settings.language.empty())
        {
            bool known = SetTextLanguage(settings.language);
            if (adapter.SetTextLanguage != nullptr)
                known = adapter.SetTextLanguage(settings.language) || known;
            if (!known)
                fprintf(stderr, "unknown text language \"%s\", using French\n", settings.language.c_str());
        }
        if (!settings.voiceLanguage.empty())
        {
            if (adapter.SetVoiceLanguage == nullptr)
                fprintf(stderr, "%s has no voice language setting yet, ignoring \"%s\"\n", adapter.info.name,
                    settings.voiceLanguage.c_str());
            else if (!adapter.SetVoiceLanguage(settings.voiceLanguage))
                fprintf(stderr, "unknown voice language \"%s\", using English\n", settings.voiceLanguage.c_str());
        }
        if (settings.nightmareFromStart)
        {
            if (adapter.SetNightmareFromStart == nullptr)
                fprintf(stderr, "%s has no locked difficulty, ignoring nightmare_from_start\n", adapter.info.name);
            else
                adapter.SetNightmareFromStart(true);
        }
    }
}
