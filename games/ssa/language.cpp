// Spyro's Adventure: voices in another language than the text. The title has one language for everything (sub_82808190
// picks it from the console language or country and keeps its index at LanguageIndex): text, UI and localized movies
// read it, and so do the two places that pick voices, patched here.
#include "game_hooks.h"
#include "memory.h"
#include "ssa.h"
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    // The title's language index (game::LanguageIndex) and its table of language codes (game::LanguageCodes, char* per
    // index, the folder names on the disc: 0 chs, 1 cht, 2 dut, 3 euk, 4 eng, 5 fre, 6 ger, 7 ita, 8 jap, 9 kor,
    // 10 spa, 11 swe, 12 fin, 13 nor, 14 dan).
    constexpr uint32_t LanguageCount = 15;

    struct VoiceLanguage
    {
        std::string_view code;
        uint32_t index;
        uint32_t movieTrack; // Bink audio track holding this language's voices in the cutscenes (sub_827F4400)
    };

    constexpr VoiceLanguage VoiceLanguages[] = {
        { "en", 4, 0 },
        { "fr", 5, 6 },
        { "de", 6, 10 },
        { "es", 10, 13 },
        { "it", 7, 11 },
        { "nl", 2, 8 },
        { "sv", 11, 14 },
        { "no", 13, 12 },
        { "da", 14, 7 },
        { "fi", 12, 9 },
    };

    const VoiceLanguage* g_voices = &VoiceLanguages[0]; // English
}

bool ssa::SetVoiceLanguage(std::string_view code)
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

// Audio language (r3: the language code, called once by the audio start-up sub_827FD4D8). It sets Wwise's
// language-specific folder, "<code>\", where voice.bnk and the levels' language banks are looked up, and the current
// language of every file package (sub_82156A00): streaming.pck holds each language's streamed voice lines under the
// same codes. Passing the voice language's code there makes every voice line that language.
#ifdef GAME_HOOK_Audio_SetLanguage
PPC_FUNC_IMPL(GAME_IMP(Audio_SetLanguage));
PPC_FUNC(GAME_HOOK(Audio_SetLanguage))
{
    auto code = [](uint32_t index) {
        if (index >= LanguageCount)
            return "?";
        return static_cast<const char*>(g_memory.Translate(g_memory.Read<uint32_t>(game::LanguageCodes + index * 4)));
    };
    fprintf(stderr, "[language] text %s, voices %s\n", code(g_memory.Read<uint32_t>(game::LanguageIndex)),
        code(g_voices->index));
    ctx.r3.u64 = g_memory.Read<uint32_t>(game::LanguageCodes + g_voices->index * 4);
    GAME_IMP(Audio_SetLanguage)(ctx, base);
}
#endif

// Cutscene voice track (r3: where to store the Bink track id, r4: the movie has one voice track per language). The
// movie's other tracks (music, effects) are chosen by the caller.
#ifdef GAME_HOOK_Movie_VoiceTrack
PPC_FUNC_IMPL(GAME_IMP(Movie_VoiceTrack));
PPC_FUNC(GAME_HOOK(Movie_VoiceTrack))
{
    uint32_t track = ctx.r3.u32;
    bool localizedVoices = (ctx.r4.u32 & 0xFF) == 1;
    GAME_IMP(Movie_VoiceTrack)(ctx, base);
    if (track != 0 && localizedVoices)
        g_memory.Write<uint32_t>(track, g_voices->movieTrack);
}
#endif
