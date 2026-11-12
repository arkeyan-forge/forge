// SuperChargers: voices in another language than the text (games/superchargers/docs/boot.md, "Text and voice language").
// Swap Force's scheme (games/swapforce/language.cpp), re-addressed: the disc has English and German for the text and the
// voices; the title has one language for everything, Language_Get (the console language as an IG_CORE_LANGUAGE value). The
// voices are the localized sound packages "<set>_<en|de>.pak" opened by Language_SoundPak, which asks Language_Get for the
// abbreviation: while that one runs, Language_Get answers with the voice language. The movies carry one Bink voice track
// per language (Movie_Open).
#include "game_hooks.h"
#include "memory.h"
#include "superchargers.h"
#include <cstdio>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    struct VoiceLanguage
    {
        std::string_view code;
        uint32_t index; // its IG_CORE_LANGUAGE value (Language_Get's table: XGetLanguage 1 -> 6, 3 -> 9)
        uint32_t track; // a movie's language track id
    };

    constexpr VoiceLanguage VoiceLanguages[] = { { "en", 6, 5 }, { "de", 9, 6 } };

    const VoiceLanguage* g_voices = nullptr; // nullptr: the game's own (the text language)
    thread_local bool t_inSoundPak = false;
}

bool superchargers::SetVoiceLanguage(std::string_view code)
{
    for (const VoiceLanguage& language : VoiceLanguages)
    {
        if (language.code == code)
        {
            g_voices = &language;
            return true;
        }
    }
    return false;
}

#ifdef GAME_HOOK_Language_Get
PPC_FUNC_IMPL(GAME_IMP(Language_Get));
PPC_FUNC(GAME_HOOK(Language_Get))
{
    GAME_IMP(Language_Get)(ctx, base);
    if (t_inSoundPak && g_voices != nullptr)
    {
        static bool logged = false;
        if (!logged)
        {
            fprintf(stderr, "[language] sound packages: voices %s (%u), text %u\n", std::string(g_voices->code).c_str(),
                g_voices->index, ctx.r3.u32);
            logged = true;
        }
        ctx.r3.u64 = g_voices->index;
    }
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
        for (const VoiceLanguage& language : VoiceLanguages)
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
