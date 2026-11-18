#include "host/game_options.h"

#include "host/gamemenu.h"
#include "language.h"
#include <cstdio>

namespace gameoptions
{
    void Apply(const game::Adapter& adapter, RuntimeSettings& settings, bool capFps)
    {
        if (const uint32_t capped = gamemenu::CapFrameRate(settings.fpsLimit, adapter.maxFpsLimit);
            capFps && capped != settings.fpsLimit)
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
