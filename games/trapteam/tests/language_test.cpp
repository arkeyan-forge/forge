// Synthetic guest calls: no disc, translated code, window or player files.
#include "game_hooks.h"
#include "memory.h"
#include "trapteam.h"
#include "vfs.h"
#include <ppc_config.h>
#include <ppc_context.h>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>

PPC_EXTERN_FUNC(GAME_HOOK(Language_LoadPack));
PPC_EXTERN_FUNC(GAME_HOOK(Movie_Open));
PPC_EXTERN_FUNC(GAME_HOOK(Language_Get));
PPC_EXTERN_FUNC(GAME_HOOK(Console_Language));

namespace
{
    int failures = 0;
    void Check(bool condition, const char* text)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", text);
            ++failures;
        }
    }
    std::vector<uint8_t> discIndex;
    std::vector<uint8_t> voiceIndex;
    std::vector<std::string> loads;
    DiscFileSystem::Entry discEntry;
    DiscFileSystem::Entry voiceEntry;
    uint32_t movieTrack = 0;
    bool failVoiceLoad = false;
    constexpr uint32_t Path = 0x10000, Movie = 0x12000, Description = 0x13000, App = 0x14000;

    void Index(std::vector<uint8_t>& index, const std::vector<std::string>& names)
    {
        // A synthetic IGA v11 index: no payloads, only names used for regional availability.
        const uint32_t count = uint32_t(names.size()), namesAt = 0x38 + 20 * count;
        index.assign(namesAt + 4 * count, 0);
        auto le = [&](uint32_t at, uint32_t value) {
            for (unsigned i = 0; i < 4; ++i)
                index[at + i] = uint8_t(value >> (8 * i));
        };
        memcpy(index.data(), "IGA\x1a", 4);
        le(4, 11);
        le(12, count);
        le(0x28, namesAt);
        uint32_t i = 0;
        for (const std::string& name : names)
        {
            le(namesAt + 4 * i++, uint32_t(index.size()) - namesAt);
            index.insert(index.end(), name.c_str(), name.c_str() + name.size() + 1);
        }
        le(0x30, uint32_t(index.size()) - namesAt);
    }

    void Disc(std::initializer_list<const char*> packs)
    {
        std::vector<std::string> speech;
        for (std::string_view pack : packs)
        {
            const char* folder = pack == "ENGLISH.pak"  ? ""
                                 : pack == "GERMAN.pak" ? "german/"
                                 : pack == "FRENCH.pak" ? "french/"
                                                        : "dutch/";
            speech.push_back(std::string("test/") + folder + "vo_test.wav/line.wav");
        }
        Index(discIndex, std::vector<std::string>(packs.begin(), packs.end()));
        Index(voiceIndex, speech);
        discEntry.size = uint32_t(discIndex.size());
        voiceEntry.size = uint32_t(voiceIndex.size());
    }

    PPCContext Load(const std::string& path, uint32_t caller = 0x821CD4C4)
    {
        loads.clear();
        memcpy(g_memory.Translate(Path), path.c_str(), path.size() + 1);
        PPCContext ctx{};
        ctx.lr = caller;
        ctx.r3.u64 = 71;
        ctx.r4.u64 = Path;
        ctx.r5.u64 = 73;
        ctx.r6.u64 = 74;
        ctx.r10.u64 = 75;
        GAME_HOOK(Language_LoadPack)(ctx, g_memory.base);
        Check(path == static_cast<const char*>(g_memory.Translate(Path)), "caller path restored");
        return ctx;
    }

    void MovieCall(uint32_t track, uint32_t mode = 0, uint32_t description = Description)
    {
        g_memory.Write<uint32_t>(Movie + 32, description);
        g_memory.Write<uint32_t>(Description + 40, mode);
        g_memory.Write<uint32_t>(Description + 56, track);
        PPCContext ctx{};
        ctx.r3.u64 = Movie;
        GAME_HOOK(Movie_Open)(ctx, g_memory.base);
        Check(ctx.r3.u64 == 99, "movie original called and result retained");
    }
}

// A fake mounted disc exposes setup packs and speech names, without any global language pack.
DiscFileSystem g_disc;
DiscFileSystem::~DiscFileSystem() = default;
const DiscFileSystem::Entry* DiscFileSystem::Find(const std::string& path) const
{
    return path == "character/init_setup.bld" ? &discEntry : path == "character/test.arc" ? &voiceEntry : nullptr;
}
uint32_t DiscFileSystem::Read(const Entry& entry, uint64_t position, void* buffer, uint32_t size) const
{
    const auto& index = &entry == &voiceEntry ? voiceIndex : discIndex;
    if (position > index.size() || size > index.size() - position)
        return 0;
    memcpy(buffer, index.data() + position, size);
    return size;
}
std::vector<std::pair<std::string, const DiscFileSystem::Entry*>> DiscFileSystem::Files() const
{
    return { { "character/init_setup.bld", &discEntry }, { "character/test.arc", &voiceEntry } };
}

PPC_FUNC_IMPL(GAME_IMP(Language_LoadPack))
{
    Check(ctx.r3.u64 == 71 && ctx.r5.u64 == 73 && ctx.r6.u64 == 74 && ctx.r10.u64 == 75,
        "original receives its caller context on both loads");
    loads.emplace_back(static_cast<const char*>(g_memory.Translate(ctx.r4.u32)));
    ctx.r3.u64 = failVoiceLoad && loads.size() == 1 ? 0 : 1;
    ctx.r4.u64 = 0;
    ctx.r5.u64 = 0;
    ctx.r6.u64 = 0;
    ctx.r10.u64 = 0;
    ctx.lr = 0;
}
PPC_FUNC_IMPL(GAME_IMP(Movie_Open))
{
    const uint32_t description = g_memory.Read<uint32_t>(ctx.r3.u32 + 32);
    movieTrack = description ? g_memory.Read<uint32_t>(description + 56) : 0;
    ctx.r3.u64 = 99;
}
PPC_FUNC_IMPL(GAME_IMP(Language_Get)) { ctx.r3.u64 = 6; }
PPC_FUNC_IMPL(GAME_IMP(Console_Language)) { ctx.r3.u64 = 1; }

int main()
{
    g_memory.Reserve();
    g_memory.Write<uint32_t>(game::LanguageAppObject, App);
    g_memory.Write<uint32_t>(App + 12, 6);
    Disc({ "ENGLISH.pak", "GERMAN.pak" });
    Load("test.bld:/ENGLISH.pak");
    Check(loads.size() == 1, "unset voices pass through");
    Check(trapteam::SetVoiceLanguage("de"), "German accepted");
    Check(!trapteam::SetVoiceLanguage("ja"), "unsupported voices rejected");
    auto ctx = Load("test.bld:/ENGLISH.pak");
    Check(loads == std::vector<std::string>{ "test.bld:/GERMAN.pak", "test.bld:/ENGLISH.pak" },
        "voice pack precedes text in the same directory");
    Check(ctx.r3.u64 == 1 && ctx.r4.u64 == 0, "final result and context belong to text load");
    failVoiceLoad = true;
    Load("test.bld:/ENGLISH.pak");
    Check(loads.size() == 2, "failed voice pack still loads text");
    failVoiceLoad = false;
    Load("test.bld:/GERMAN.pak");
    Check(loads.size() == 1, "same language loaded once");
    Load("test.bld:/level.bld");
    Check(loads.size() == 1, "non-language resources untouched");
    Load("test.bld:/ENGLISH.pak", 0x821CD8C8);
    Check(loads.size() == 1, "other caller untouched");
    Load(std::string(2030, 'a') + "/DUTCH.pak");
    Check(loads.size() == 2, "longer replacement fits near buffer limit");
    Load(std::string(2037, 'a') + "/DUTCH.pak");
    Check(loads.size() == 1, "oversized replacement avoided");
    MovieCall(7);
    Check(movieTrack == 10, "German movie track selected");
    MovieCall(7, 1);
    Check(movieTrack == 7, "silent movie mode untouched");
    MovieCall(0);
    Check(movieTrack == 0, "custom movie track untouched");
    MovieCall(0, 0, 0);
    Check(movieTrack == 0, "null movie description passed through");
    Check(trapteam::SetTextLanguage("nl"), "Dutch text accepted independently");
    ctx = {};
    GAME_HOOK(Language_Get)(ctx, g_memory.base);
    Check(ctx.r3.u64 == 5, "voice choice leaves Dutch text selection intact");
    GAME_HOOK(Console_Language)(ctx, g_memory.base);
    Check(ctx.r3.u64 == 16, "console still follows text");
    trapteam::SetVoiceLanguage("fr");
    MovieCall(7);
    Check(movieTrack == 7, "absent regional pack leaves movie track untouched");
    Load("test.bld:/ENGLISH.pak");
    Check(loads.size() == 1, "absent regional pack leaves text alone");
    Disc({ "FRENCH.pak", "DUTCH.pak" });
    for (const auto& [code, pack, track] : { std::tuple{ "fr", "FRENCH", 9u }, { "nl", "DUTCH", 6u } })
    {
        trapteam::SetVoiceLanguage(code);
        Load("test.bld:/" + std::string(std::string_view(code) == "fr" ? "DUTCH" : "FRENCH") + ".pak");
        Check(loads.size() == 2 && loads[0] == "test.bld:/" + std::string(pack) + ".pak", "regional pack selected");
        MovieCall(7);
        Check(movieTrack == track, "regional movie track selected");
    }
    Disc({ "ENGLISH.pak", "GERMAN.pak" });
    trapteam::SetVoiceLanguage("en");
    Load("test.bld:/GERMAN.pak");
    Check(loads[0] == "test.bld:/ENGLISH.pak", "English voice pack selected");
    trapteam::SetVoiceLanguage("");
    Load("test.bld:/GERMAN.pak");
    Check(loads.size() == 1, "clearing override restores original behavior");
    printf("Trap Team language hooks: %s\n", failures ? "FAIL" : "pass");
    return failures ? 1 : 0;
}
