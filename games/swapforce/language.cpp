// Swap Force: voices in another language than the text (docs: games/swapforce/docs/boot.md, "Text and voice language").
// The disc has two languages, English and French, for the text and for the voices. The title has one language for
// everything: Language_Get (the console language as an IG_CORE_LANGUAGE value). The voices are the localized sound
// packages "<set>_<en|fr>.pak" opened by Language_SoundPak, which asks Language_Get for the abbreviation: while that one
// runs, Language_Get answers with the voice language. The movies carry one Bink voice track per language (Movie_Open).
#include "game_hooks.h"
#include "memory.h"
#include "swapforce.h"
#include <cstdio>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    struct VoiceLanguage
    {
        std::string_view code;
        uint32_t index; // its IG_CORE_LANGUAGE value
    };

    constexpr VoiceLanguage VoiceLanguages[] = { { "en", 6 }, { "fr", 8 } };

    constexpr uint32_t EnglishTrack = 5, FrenchTrack = 6; // a movie's language track ids

    const VoiceLanguage* g_voices = nullptr; // nullptr: the game's own (the text language)
    thread_local bool t_inSoundPak = false;
}

bool swapforce::SetVoiceLanguage(std::string_view code)
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
// the text language: 5 for English, 6 for French (every story movie carries the tracks 0, 1, 2 and 5, 6; the logos have
// none of them). With a voice language the id is put to its own just before BinkSetSoundTrack and BinkOpen.
#ifdef GAME_HOOK_Movie_Open
PPC_FUNC_IMPL(GAME_IMP(Movie_Open));
PPC_FUNC(GAME_HOOK(Movie_Open))
{
    uint32_t description = g_memory.Read<uint32_t>(ctx.r3.u32 + 32);
    if (g_voices != nullptr && description != 0)
    {
        uint32_t track = g_memory.Read<uint32_t>(description + 56);
        if (track == EnglishTrack || track == FrenchTrack)
        {
            uint32_t voices = g_voices->index == 8 ? FrenchTrack : EnglishTrack;
            g_memory.Write<uint32_t>(description + 56, voices);
            fprintf(stderr, "[language] movie voice track %u (%s), the text's was %u\n", voices,
                std::string(g_voices->code).c_str(), track);
        }
    }
    GAME_IMP(Movie_Open)(ctx, base);
}
#endif
