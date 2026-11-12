#include "trapteam.h"
#include "host/art/alchemy_archive.h"
#include "host/art/disc_languages.h"
#include "vfs.h"
#include <algorithm>

bool trapteam::VoiceLanguageOnDisc(std::string_view code)
{
    std::vector<std::string> files;
    for (const auto& [path, entry] : g_disc.Files())
        files.push_back(path);
    // Use the launcher's distinction between text packs and localized speech, including unmarked English lines.
    // LanguagesOnDisc bounds the character-archive scan and reads only index names through this callback.
    const auto languages = art::LanguagesOnDisc("trapteam", files, [](const std::string& path) {
        std::vector<std::string> names;
        const auto* entry = g_disc.Find(path);
        if (entry == nullptr)
            return names;
        auto archive = art::alchemy::IgaArchive::Open(
            [entry](uint64_t offset, size_t size, uint8_t* out) {
                return g_disc.Read(*entry, offset, out, uint32_t(size)) == size;
            },
            entry->size);
        if (archive)
            for (size_t i = 0; i < archive->Count(); ++i)
                names.push_back(archive->Name(i));
        return names;
    });
    return std::find(languages.voice.begin(), languages.voice.end(), code) != languages.voice.end();
}
