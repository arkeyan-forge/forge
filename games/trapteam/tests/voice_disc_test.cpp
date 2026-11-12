// Synthetic archive indices only: no disc content or speech payloads.
#include "trapteam.h"
#include "vfs.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace
{
    struct Archive
    {
        DiscFileSystem::Entry entry{};
        std::vector<uint8_t> index;
    };
    std::map<std::string, Archive> archives;
    int failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            ++failures;
        }
    }

    void Add(const std::string& path, std::initializer_list<const char*> names)
    {
        auto& archive = archives[path];
        auto& index = archive.index;
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
        for (const char* name : names)
        {
            le(namesAt + 4 * i++, uint32_t(index.size()) - namesAt);
            index.insert(index.end(), name, name + strlen(name) + 1);
        }
        le(0x30, uint32_t(index.size()) - namesAt);
        archive.entry.name = path;
        archive.entry.size = uint32_t(index.size());
    }
}

DiscFileSystem g_disc;
DiscFileSystem::~DiscFileSystem() = default;
const DiscFileSystem::Entry* DiscFileSystem::Find(const std::string& path) const
{
    auto found = archives.find(path);
    return found == archives.end() ? nullptr : &found->second.entry;
}
uint32_t DiscFileSystem::Read(const Entry& entry, uint64_t position, void* buffer, uint32_t size) const
{
    const auto& index = archives.at(entry.name).index;
    if (position > index.size() || size > index.size() - position)
        return 0;
    memcpy(buffer, index.data() + position, size);
    return size;
}
std::vector<std::pair<std::string, const DiscFileSystem::Entry*>> DiscFileSystem::Files() const
{
    std::vector<std::pair<std::string, const Entry*>> files;
    for (const auto& [path, archive] : archives)
        files.emplace_back(path, &archive.entry);
    return files;
}

int main()
{
    Add("character/init_setup.bld", { "ENGLISH.pak", "GERMAN.pak" });
    Add("character/test.arc", { "test/vo_test.wav/line.wav", "test/german/vo_test.wav/line.wav" });
    Check(trapteam::VoiceLanguageOnDisc("de"), "German speech detected without a global language pack");
    Check(trapteam::VoiceLanguageOnDisc("en"), "English speech has no language folder");
    Check(!trapteam::VoiceLanguageOnDisc("fr"), "absent regional speech rejected");

    archives.clear();
    Add("character/init_setup.bld", { "french.pak", "dutch.pak" });
    Add("permanent/global.bld", { "level.bld" });
    Add("character/test.arc",
        { "test/french/vo_test.wav/line.wav", "test/dutch/vo_test.wav/line.wav", "test/vo_shared.wav/line.wav" });
    Check(trapteam::VoiceLanguageOnDisc("fr"), "French speech detected independently of pack case");
    Check(trapteam::VoiceLanguageOnDisc("nl"), "Dutch speech detected");
    Check(!trapteam::VoiceLanguageOnDisc("en"), "unmarked shared sounds do not imply English speech");

    archives.clear();
    Add("permanent/global.bld", { "ENGLISH.pak", "GERMAN.pak" });
    Check(!trapteam::VoiceLanguageOnDisc("de"), "text pack alone does not prove speech availability");
    Add("character/test.arc", { "test/german/vo_test.wav/line.wav", "test/vo_test.wav/line.wav" });
    Check(trapteam::VoiceLanguageOnDisc("de"), "global packs are a fallback when setup has none");
    Check(trapteam::VoiceLanguageOnDisc("en"), "global fallback also detects unmarked English speech");
    Add("character/init_setup.bld", { "FRENCH.pak", "DUTCH.pak" });
    Check(!trapteam::VoiceLanguageOnDisc("en"), "setup languages take precedence over global packs");
    archives.clear();
    Check(!trapteam::VoiceLanguageOnDisc("de"), "missing archives fall back safely");
    Check(!trapteam::VoiceLanguageOnDisc("unknown"), "unknown code rejected");
    printf("Trap Team disc voices: %s\n", failures ? "FAIL" : "pass");
    return failures ? 1 : 0;
}
