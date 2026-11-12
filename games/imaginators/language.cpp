// Imaginators: the text language from the settings and voices that follow it, or another one (games/imaginators/docs/boot.md,
// "Text and voice language"). SuperChargers' scheme (games/superchargers/language.cpp), re-addressed: the disc has French
// and Dutch, text and voices. Language_Get turns the console language (XGetLanguage) into an IG_CORE_LANGUAGE value by a
// jump table: French (4) is 8, Dutch is console language 16 (5), which our console does not answer (runtime/kernel/
// xconfig.cpp gives "nl" as English with the Netherlands), so for Dutch Language_Get answers 5 itself. The voices are the
// sound packages "<set>_<fr|nl>.pak" opened by two Language_SoundPak functions, which ask Language_Get for the
// abbreviation: while one runs, Language_Get answers with the voice language. The movies carry one Bink voice track per
// language (Movie_Open).
#include "game_hooks.h"
#include "imaginators.h"
#include "memory.h"
#include <cstdio>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    struct DiscLanguage
    {
        std::string_view code;
        uint32_t index; // its IG_CORE_LANGUAGE value (Language_Get's table: XGetLanguage 4 -> 8, 16 -> 5)
        uint32_t track; // a movie's language track id
    };

    constexpr DiscLanguage DiscLanguages[] = { { "fr", 8, 5 }, { "nl", 5, 6 } };

    const DiscLanguage* Find(std::string_view code)
    {
        for (const DiscLanguage& language : DiscLanguages)
            if (language.code == code)
                return &language;
        return nullptr;
    }

    const DiscLanguage* g_text = nullptr; // nullptr: the console language's
    const DiscLanguage* g_voices = nullptr; // nullptr: the text language's
    thread_local bool t_inSoundPak = false;
}

bool imaginators::SetTextLanguage(std::string_view code)
{
    g_text = Find(code);
    return g_text != nullptr;
}

bool imaginators::SetVoiceLanguage(std::string_view code)
{
    g_voices = Find(code);
    return g_voices != nullptr;
}

#ifdef GAME_HOOK_Language_Get
PPC_FUNC_IMPL(GAME_IMP(Language_Get));
PPC_FUNC(GAME_HOOK(Language_Get))
{
    GAME_IMP(Language_Get)(ctx, base);
    const DiscLanguage* language = t_inSoundPak && g_voices != nullptr ? g_voices : g_text;
    if (language == nullptr || ctx.r3.u32 == language->index)
        return;
    static bool logged[2];
    if (!logged[t_inSoundPak])
    {
        fprintf(stderr, "[language] %s: %s (%u), the console's %u\n", t_inSoundPak ? "sound packages" : "text",
            std::string(language->code).c_str(), language->index, ctx.r3.u32);
        logged[t_inSoundPak] = true;
    }
    ctx.r3.u64 = language->index;
}
#endif

#ifdef GAME_HOOK_Language_SoundPak
PPC_FUNC_IMPL(GAME_IMP(Language_SoundPak));
PPC_FUNC(GAME_HOOK(Language_SoundPak))
{
    t_inSoundPak = true;
    GAME_IMP(Language_SoundPak)(ctx, base);
    t_inSoundPak = false;
}
#endif

#ifdef GAME_HOOK_Language_SoundPak2
PPC_FUNC_IMPL(GAME_IMP(Language_SoundPak2));
PPC_FUNC(GAME_HOOK(Language_SoundPak2))
{
    t_inSoundPak = true;
    GAME_IMP(Language_SoundPak2)(ctx, base);
    t_inSoundPak = false;
}
#endif

// A movie starts (r3: the movie; its description at +32). The description's +56 is the movie's language track id, set from
// the text language; with a voice language it is put to that language's just before BinkSetSoundTrack and BinkOpen.
#ifdef GAME_HOOK_Movie_Open
PPC_FUNC_IMPL(GAME_IMP(Movie_Open));
PPC_FUNC(GAME_HOOK(Movie_Open))
{
    uint32_t description = g_memory.Read<uint32_t>(ctx.r3.u32 + 32);
    if (description != 0)
    {
        uint32_t track = g_memory.Read<uint32_t>(description + 56);
        bool known = false;
        for (const DiscLanguage& language : DiscLanguages)
            known = known || track == language.track;
        if (g_voices != nullptr && known && track != g_voices->track)
        {
            g_memory.Write<uint32_t>(description + 56, g_voices->track);
            fprintf(stderr, "[language] movie voice track %u (%s), the text's was %u\n", g_voices->track,
                std::string(g_voices->code).c_str(), track);
        }
        else
            fprintf(stderr, "[language] movie language track %u\n", track);
    }
    GAME_IMP(Movie_Open)(ctx, base);
}
#endif
