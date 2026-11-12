// Trap Team: text and independent voices (docs: games/trapteam/docs/boot.md, "Language"). The title has one language
// for everything. It picks it in sub_821D29B8 from the console language (XGetLanguage, wrapped by sub_8258A230: 3
// German, 4 French, 16 Dutch...) and from which language files the disc has (3d_font_msg_<language>, the packs), and
// keeps an IG_CORE_LANGUAGE value that Language_Get makes from the same console language (17 cases); each archive
// "<name>.bld" holds one language pack per language, "<LANGUAGE>.pak", with the archive's text and the localized voice
// lines [V]. The host sets both reads, the console language where the console has the language (Dutch is 16; Danish and
// Finnish have none) and the value for every language of the title's list.
#include "game_hooks.h"
#include "memory.h"
#include "trapteam.h"
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    struct Language
    {
        std::string_view code;
        uint32_t index; // its IG_CORE_LANGUAGE value; the pack is <NAME>.pak
        uint32_t console; // its XGetLanguage value; 0: the console has no such language
    };

    // The IG_CORE_LANGUAGE values come from Language_Get's own table (en 6, ja 15, de 9, fr 8, es 21, it 14, ko 16, pt
    // 19, pl 18, ru 20, sv 23, no 17, nl 5) and from the order of the enum's names in the executable (sorted by name:
    // ARABIC 0 ... SPANISH 21, SPANISH_MEXICAN 22, SWEDISH 23, TAIWANESE 24, TURKISH 25), which gives Danish 4, Finnish
    // 7, Czech 3, Greek 10 and Hungarian 13 [?]: only English, French, German and Dutch were seen on a disc. The
    // console values are the XDK's (Language_Get's switch).
    constexpr Language Languages[] = {
        { "en", 6, 1 },
        { "fr", 8, 4 },
        { "de", 9, 3 },
        { "es", 21, 5 },
        { "es-419", 22, 5 },
        { "it", 14, 6 },
        { "nl", 5, 16 },
        { "pt", 19, 9 },
        { "sv", 23, 13 },
        { "no", 17, 15 },
        { "da", 4, 0 },
        { "fi", 7, 0 },
        { "pl", 18, 11 },
        { "ru", 20, 12 },
        { "cs", 3, 0 },
        { "hu", 13, 0 },
        { "tr", 25, 14 },
        { "el", 10, 0 },
        { "ja", 15, 2 },
        { "ko", 16, 7 },
        { "zh", 2, 10 },
    };

    const Language* g_text = nullptr; // nullptr: the console language's, as the game maps it

    struct VoiceLanguage
    {
        std::string_view code;
        std::string_view pack;
        uint32_t index;
    };
    // The two regional discs inspected: English/German and French/Dutch. A language must also be on the mounted disc.
    constexpr VoiceLanguage VoiceLanguages[] = {
        { "en", "ENGLISH", 6 },
        { "de", "GERMAN", 9 },
        { "fr", "FRENCH", 8 },
        { "nl", "DUTCH", 5 },
    };
    const VoiceLanguage* g_voices = nullptr;
    bool g_checkedVoices = false;
    std::mutex g_voiceMutex;

    const VoiceLanguage* VoicesOnDisc()
    {
        std::lock_guard lock(g_voiceMutex);
        if (g_voices == nullptr)
            return nullptr;
        if (!g_checkedVoices)
        {
            // Settings arrive before the disc is mounted. Inspect archive indices at the first hooked load.
            g_checkedVoices = true;
            if (!trapteam::VoiceLanguageOnDisc(g_voices->code))
            {
                fprintf(stderr, "[language] voices %s unavailable on this disc; following text\n",
                    std::string(g_voices->code).c_str());
                g_voices = nullptr;
                return nullptr;
            }
        }
        return g_voices;
    }
}

bool trapteam::SetVoiceLanguage(std::string_view code)
{
    std::lock_guard lock(g_voiceMutex);
    if (code.empty())
    {
        g_voices = nullptr;
        g_checkedVoices = false;
        return true;
    }
    for (const VoiceLanguage& language : VoiceLanguages)
    {
        if (language.code == code)
        {
            g_voices = &language;
            g_checkedVoices = false;
            return true;
        }
    }
    return false;
}

bool trapteam::SetTextLanguage(std::string_view code)
{
    for (const Language& language : Languages)
    {
        if (language.code == code)
        {
            g_text = &language;
            return true;
        }
    }
    return false;
}

// The console language (XGetLanguage's wrapper): the title's language chooser reads it, and Language_Get too.
#ifdef GAME_HOOK_Console_Language
PPC_FUNC_IMPL(GAME_IMP(Console_Language));
PPC_FUNC(GAME_HOOK(Console_Language))
{
    GAME_IMP(Console_Language)(ctx, base);
    if (g_text != nullptr && g_text->console != 0)
        ctx.r3.u64 = g_text->console;
}
#endif

// sub_821CD058 loads the language pack through this helper (r3: loader, r4: path, r5: directory, r6: allocator;
// nonzero low byte = success). Only its language-pack call uses the caller's writable 2048-byte path buffer.
// As in Giants, load the voice resources first, then the text pack into the same directory. The first sound objects
// and the last LanguagePackInfo must win: this ordering still needs a Trap Team run (boot.md).
#ifdef GAME_HOOK_Language_LoadPack
PPC_FUNC_IMPL(GAME_IMP(Language_LoadPack));
PPC_FUNC(GAME_HOOK(Language_LoadPack))
{
    const VoiceLanguage* voice = ctx.lr == 0x821CD4C4 ? VoicesOnDisc() : nullptr;
    if (voice == nullptr)
    {
        GAME_IMP(Language_LoadPack)(ctx, base);
        return;
    }
    char* path = static_cast<char*>(g_memory.Translate(ctx.r4.u32));
    const size_t length = strnlen(path, 2048);
    if (length == 2048)
    {
        GAME_IMP(Language_LoadPack)(ctx, base);
        return;
    }
    const std::string original(path, length);
    const size_t slash = original.rfind('/');
    const std::string_view pack =
        slash == std::string::npos ? std::string_view() : std::string_view(original).substr(slash + 1);
    bool languagePack = false;
    for (const VoiceLanguage& language : VoiceLanguages)
        languagePack |= pack == std::string(language.pack) + ".pak";
    const std::string voices = original.substr(0, slash + 1) + std::string(voice->pack) + ".pak";
    if (!languagePack || pack == std::string(voice->pack) + ".pak" || voices.size() >= 2048)
    {
        GAME_IMP(Language_LoadPack)(ctx, base);
        return;
    }

    const PPCContext saved = ctx;
    memcpy(path, voices.c_str(), voices.size() + 1);
    GAME_IMP(Language_LoadPack)(ctx, base);
    const bool loaded = ctx.r3.u8 != 0;
    // Log failures as well: accepting a code does not mean that this region's disc carries its pack.
    fprintf(stderr, "[language] voices %s: %s %s before %s\n", std::string(voice->code).c_str(), voices.c_str(),
        loaded ? "loaded" : "unavailable", original.c_str());
    memcpy(path, original.c_str(), original.size() + 1);
    ctx = saved;
    GAME_IMP(Language_LoadPack)(ctx, base);
}
#endif

// sub_8223A780 sets description +56 to app language (+12) + 1. The movie worker passes that track together with
// tracks 0..3 to BinkSetSoundTrack; mode +40 == 1 requests no audio. Logos and custom track selections are left alone.
#ifdef GAME_HOOK_Movie_Open
PPC_FUNC_IMPL(GAME_IMP(Movie_Open));
PPC_FUNC(GAME_HOOK(Movie_Open))
{
    const VoiceLanguage* voice = VoicesOnDisc();
    if (voice == nullptr)
    {
        GAME_IMP(Movie_Open)(ctx, base);
        return;
    }
    const uint32_t description = g_memory.Read<uint32_t>(ctx.r3.u32 + 32);
    const uint32_t app = g_memory.Read<uint32_t>(game::LanguageAppObject);
    if (description != 0 && app != 0 && g_memory.Read<uint32_t>(description + 40) == 0 &&
        g_memory.Read<uint32_t>(description + 56) == g_memory.Read<uint32_t>(app + 12) + 1)
    {
        g_memory.Write<uint32_t>(description + 56, voice->index + 1);
        fprintf(stderr, "[language] movie voice track %u (%s)\n", voice->index + 1, std::string(voice->code).c_str());
    }
    GAME_IMP(Movie_Open)(ctx, base);
}
#endif

#ifdef GAME_HOOK_Language_Get
PPC_FUNC_IMPL(GAME_IMP(Language_Get));
PPC_FUNC(GAME_HOOK(Language_Get))
{
    GAME_IMP(Language_Get)(ctx, base);
    if (g_text != nullptr)
    {
        static bool logged = false;
        if (!logged)
        {
            fprintf(stderr, "[language] text %s: language %u, the title's was %u\n", std::string(g_text->code).c_str(),
                g_text->index, ctx.r3.u32);
            logged = true;
        }
        ctx.r3.u64 = g_text->index;
    }
}
#endif
