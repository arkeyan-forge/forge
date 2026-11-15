#include "host/glyphs.h"
#include "host/art/alchemy_archive.h"
#include <algorithm>
#include <array>
#include <cctype>
#define XXH_INLINE_ALL
#include <xxhash.h>

namespace glyphs
{
    bool PromptAssetMatches(std::string_view name, std::string_view asset)
    {
        auto normalize = [](std::string_view text) {
            std::string out;
            for (unsigned char c : text)
            {
                if (c == '@')
                    continue;
                out += c == '!' || c == '\\' ? '/' : c == '`' ? '.' : char(std::tolower(c));
            }
            return out;
        };
        const std::string path = normalize(name), wanted = normalize(asset);
        if (wanted.empty())
            return false;
        const size_t at = path.rfind(wanted);
        if (at == std::string::npos || (at != 0 && path[at - 1] != '/'))
            return false;
        const size_t end = at + wanted.size();
        return end == path.size() || path[end] == '.';
    }

    bool ResolvePrompt(Prompt& prompt, const art::alchemy::IgzFile& file)
    {
        if (file.Version() != 9)
            return false;
        size_t buffers = 0;
        for (uint64_t object : file.ObjectOffsets())
            buffers += file.TypeName(object) == "igIndexBuffer";
        if (buffers != 2)
            return false;

        struct Mesh
        {
            uint32_t count;
            uint64_t hash;
        };
        std::array<Mesh, 2> meshes{};
        size_t found = 0;
        const std::vector<uint32_t> handles = file.RecordWords("TMHN");
        if (handles.size() % 2 != 0)
            return false;
        // A named two-mesh model stores its triangle lists in memory handles. Validate whole handles, rather than
        // searching for index subsequences that could also occur in unrelated models or vertex streams.
        for (size_t i = 0; i + 1 < handles.size(); i += 2)
        {
            const uint32_t bytes = handles[i] & 0xFFFFFF;
            const auto at = file.Pointer(handles[i + 1]);
            if (bytes < 6 || bytes % 6 != 0 || !at || *at > file.Bytes().size() || bytes > file.Bytes().size() - *at)
                continue;
            const uint32_t count = bytes / 2;
            const uint8_t* data = file.Bytes().data() + *at;
            uint32_t minimum = UINT16_MAX, maximum = 0;
            for (uint32_t j = 0; j < bytes; j += 2)
            {
                const uint32_t index = uint32_t(data[j]) << 8 | data[j + 1];
                minimum = std::min(minimum, index);
                maximum = std::max(maximum, index);
            }
            const uint32_t range = maximum - minimum + 1;
            if (range < 3 || range > count)
                continue;
            std::vector<bool> used(range);
            for (uint32_t j = 0; j < bytes; j += 2)
                used[(uint32_t(data[j]) << 8 | data[j + 1]) - minimum] = true;
            if (std::ranges::find(used, false) != used.end())
                continue;
            if (found == meshes.size())
                return false;
            meshes[found++] = { count, XXH3_64bits(data, bytes) };
        }
        if (found != meshes.size() || meshes[0].count == meshes[1].count)
            return false;
        // The mash models' backing disc has fewer triangles than the raised button and lettering.
        if (meshes[0].count > meshes[1].count)
            std::swap(meshes[0], meshes[1]);
        prompt.faceCount = meshes[0].count;
        prompt.faceHash = meshes[0].hash;
        prompt.lettersCount = meshes[1].count;
        prompt.lettersHash = meshes[1].hash;
        return true;
    }
}
