#pragma once

// Sampling a texture pack's replacement (docs/runtime/gpu/texture_packs.md) in place of the guest texture. Plain C++,
// so a test can use it.
#include "host/dds.h"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

namespace d3d
{
    struct LodRange
    {
        float minLod, maxLod;
    };

    // How many times larger the replacement is than the guest texture, as a power of two (2x: 1); 0 when not larger.
    constexpr int32_t PackScaleLog2(uint32_t guestWidth, uint32_t replacementWidth)
    {
        if (guestWidth == 0 || replacementWidth <= guestWidth)
            return 0;
        return int32_t(std::bit_width(replacementWidth / guestWidth)) - 1;
    }

    // The guest's mip range, in the replacement's levels. Replacement level `level + scale` is the size of guest level
    // `level`, so the guest's range moves by `scale` levels, except its finest end: a range from the guest's top level
    // starts at the replacement's top level (the detail the pack adds). A guest texture without mips (its range 0-0)
    // keeps the replacement's level the size of the guest's: drawn smaller, the replacement then looks like the guest
    // texture instead of shimmering on its top level.
    constexpr LodRange PackLodRange(uint32_t guestMin, uint32_t guestMax, int32_t scale, uint32_t replacementLevels)
    {
        const int32_t last = int32_t(std::max(replacementLevels, 1u)) - 1;
        const int32_t low = guestMin == 0 ? 0 : std::clamp(int32_t(guestMin) + scale, 0, last);
        const int32_t high = std::clamp(int32_t(guestMax) + scale, low, last);
        return { float(low), float(high) };
    }

    // SKY_TEXTURE_PACK_TINT (docs/porting/debugging.md): one level of a replacement in a single colour, its alpha kept,
    // to see on screen where that level is sampled. BC1, BC2, BC3 and RGBA8 levels; BC4 and BC5 (one or two channels:
    // masks, normal maps) stay as they are. `rgb565` for the block formats, `rgba` (red in the low byte) for RGBA8.
    inline void TintLevel(dds::Format format, std::vector<uint8_t>& data, uint16_t rgb565, uint32_t rgba)
    {
        auto solidColourBlock = [rgb565](uint8_t* block, bool keepTransparent) {
            uint16_t ends[2];
            uint32_t indices;
            memcpy(ends, block, sizeof(ends));
            memcpy(&indices, block + 4, sizeof(indices));
            // BC1's three-colour mode (first end not above the second) makes index 3 transparent: kept as it was.
            const bool threeColour = keepTransparent && ends[0] <= ends[1];
            uint32_t tinted = 0;
            for (uint32_t texel = 0; texel < 16; texel++)
                if (threeColour && ((indices >> (texel * 2)) & 3) == 3)
                    tinted |= 3u << (texel * 2);
            ends[0] = ends[1] = rgb565;
            memcpy(block, ends, sizeof(ends));
            memcpy(block + 4, &tinted, sizeof(tinted));
        };
        switch (format)
        {
        case dds::Format::BC1:
            for (size_t offset = 0; offset + 8 <= data.size(); offset += 8)
                solidColourBlock(data.data() + offset, true);
            break;
        case dds::Format::BC2:
        case dds::Format::BC3:
            for (size_t offset = 0; offset + 16 <= data.size(); offset += 16)
                solidColourBlock(data.data() + offset + 8, false);
            break;
        case dds::Format::RGBA8:
            for (size_t offset = 0; offset + 4 <= data.size(); offset += 4)
            {
                const uint8_t alpha = data[offset + 3];
                memcpy(data.data() + offset, &rgba, 4);
                data[offset + 3] = alpha;
            }
            break;
        case dds::Format::BC4:
        case dds::Format::BC5: break;
        }
    }
}
