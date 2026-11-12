// Giants: voices in another language than the text (docs: games/giants/docs/boot.md, "Voice language"). The title has
// one language (the app object's _appLanguage, an IG_CORE_LANGUAGE value) for everything. Each archive "<name>.bld"
// carries one language pack per language, "<LANGUAGE>.pak", loaded before the archive's level.bld: the pack holds the
// archive's text (a LanguagePackInfo) and its localized voice lines (tfbSoundDataInfo objects named like the English
// ones, pointing at that language's streams in "<name>.arc", with their lip-sync animations). When two packs are loaded
// into the same directory, the text of the last one is shown and the voice lines of the first one are played [V]. So
// loading the voice language's pack just before the text language's gives each its own language. The story movies have
// one Bink voice track per language (track id = language + 1), picked the same way. The text language is that same value:
// the console language gives it for the languages of the region the title thinks it is in (NTSC: English, French, Spanish read
// as Latin-American, Portuguese), the host sets it for any of the twelve (SetTextLanguage below).
#include "game_hooks.h"
#include "giants.h"
#include "language.h"
#include "memory.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    struct VoiceLanguage
    {
        std::string_view code;
        std::string_view pack; // the language pack's name, IG_CORE_LANGUAGE_<pack>
        uint32_t index; // its IG_CORE_LANGUAGE value (the story movies' voice track is index + 1)
    };

    // The twelve voice languages on the disc (every level's language packs and story movies have them all).
    constexpr VoiceLanguage VoiceLanguages[] = {
        { "en", "ENGLISH", 6 },
        { "fr", "FRENCH", 8 },
        { "de", "GERMAN", 9 },
        { "es", "SPANISH", 21 },
        { "es-419", "HISPANIC", 0 },
        { "it", "ITALIAN", 14 },
        { "nl", "DUTCH", 5 },
        { "pt", "PORTUGUESE", 19 },
        { "sv", "SWEDISH", 22 },
        { "no", "NORWEGIAN", 17 },
        { "da", "DANISH", 4 },
        { "fi", "FINNISH", 7 },
    };

    const VoiceLanguage* g_voices = nullptr; // nullptr: the game's own (the text language)
    const VoiceLanguage* g_text = nullptr; // nullptr: the console language's, as the region allows it

    uint32_t AppLanguage()
    {
        uint32_t app = g_memory.Read<uint32_t>(game::AppObject);
        return app != 0 ? g_memory.Read<uint32_t>(app + game::AppLanguageOffset) : ~0u;
    }
}

bool giants::SetVoiceLanguage(std::string_view code)
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

// The text in any of the twelve languages. The console language is set too, as far as there is one (Latin-American Spanish is
// Spanish to it), so that anything else that reads it agrees.
bool giants::SetTextLanguage(std::string_view code)
{
    for (const VoiceLanguage& language : VoiceLanguages)
    {
        if (language.code == code)
        {
            g_text = &language;
            ::SetTextLanguage(code == "es-419" ? "es" : code);
            return true;
        }
    }
    return false;
}

// The app's language from the console's, then checked against the region's list (r3: the app object): a language the list
// lacks (Dutch, Swedish, German... on an NTSC console) becomes English. The host's choice is put back after that check.
#ifdef GAME_HOOK_Language_Region
PPC_FUNC_IMPL(GAME_IMP(Language_Region));
PPC_FUNC(GAME_HOOK(Language_Region))
{
    uint32_t app = ctx.r3.u32;
    GAME_IMP(Language_Region)(ctx, base);
    if (g_text != nullptr)
    {
        g_memory.Write<uint32_t>(app + game::AppLanguageOffset, g_text->index);
        fprintf(stderr, "[language] text %s: app language %u\n", std::string(g_text->code).c_str(), g_text->index);
    }
}
#endif

// The resource loader (r3: the loader, r4: the path, "<archive>.bld:/<name>", r5: the directory to fill; returns 0 on
// success). sub_8217A0C0 calls it for "<archive>.bld:/<TEXT LANGUAGE>.pak", then "<archive>.bld:/level.bld". For a
// language pack in another language than the voices, the voice language's pack is loaded first into the same
// directory (an archive without it just fails that load), then the text language's pack.
#ifdef GAME_HOOK_Language_LoadPack
PPC_FUNC_IMPL(GAME_IMP(Language_LoadPack));
PPC_FUNC(GAME_HOOK(Language_LoadPack))
{
    if (g_voices == nullptr)
    {
        GAME_IMP(Language_LoadPack)(ctx, base);
        return;
    }
    char* path = static_cast<char*>(g_memory.Translate(ctx.r4.u32));
    std::string original(path, strnlen(path, 1024));
    size_t slash = original.rfind('/');
    std::string_view pack =
        slash == std::string::npos ? std::string_view() : std::string_view(original).substr(slash + 1);
    bool otherLanguage = false;
    for (const VoiceLanguage& language : VoiceLanguages)
        otherLanguage |= pack.size() == language.pack.size() + 4 && pack.starts_with(language.pack) &&
                         pack.ends_with(".pak") && language.pack != g_voices->pack;
    if (!otherLanguage)
    {
        GAME_IMP(Language_LoadPack)(ctx, base);
        return;
    }

    // The path is the caller's string buffer (2048 bytes): the voice pack's name is written over the text pack's,
    // then the original put back.
    std::string voices = original.substr(0, slash + 1) + std::string(g_voices->pack) + ".pak";
    PPCContext call = ctx;
    memcpy(path, voices.c_str(), voices.size() + 1);
    GAME_IMP(Language_LoadPack)(ctx, base);
    static bool logged = false;
    if (!logged)
    {
        fprintf(stderr, "[language] voices %s: %s loaded before %s\n", std::string(g_voices->code).c_str(),
            voices.c_str(), original.c_str());
        logged = true;
    }
    memcpy(path, original.c_str(), original.size() + 1);
    ctx = call;
    GAME_IMP(Language_LoadPack)(ctx, base);
}
#endif

// A movie starts (r3: the movie; its description at +28). With a voice track per language (mode +100 = 0), the
// description's track id +40 is the language + 1, set by sub_8222C738 from _appLanguage; BinkSetSoundTrack is called
// here with it, just before BinkOpen.
#ifdef GAME_HOOK_Movie_Open
PPC_FUNC_IMPL(GAME_IMP(Movie_Open));
PPC_FUNC(GAME_HOOK(Movie_Open))
{
    uint32_t description = g_memory.Read<uint32_t>(ctx.r3.u32 + 28);
    if (g_voices != nullptr && description != 0 && g_memory.Read<uint32_t>(description + 100) == 0 &&
        g_memory.Read<uint32_t>(description + 40) == AppLanguage() + 1)
    {
        g_memory.Write<uint32_t>(description + 40, g_voices->index + 1);
        fprintf(
            stderr, "[language] movie voice track %u (%s)\n", g_voices->index + 1, std::string(g_voices->code).c_str());
    }
    GAME_IMP(Movie_Open)(ctx, base);
}
#endif
