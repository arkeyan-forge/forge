#pragma once

#include "host/glyphs.h"
#include <array>

namespace alchemy
{
    // These keys name host pictures, not textures or fingerprints from the disc.
    inline constexpr std::array<glyphs::Texture, 4> MashTextures = { {
        { 0xB0770000000000B0ull, "world mash A", glyphs::Glyph::A, {}, false, false, nullptr, true },
        { 0xB0770000000000B1ull, "world mash B", glyphs::Glyph::B, {}, false, false, nullptr, true },
        { 0xB0770000000000B2ull, "world mash X", glyphs::Glyph::X, {}, false, false, nullptr, true },
        { 0xB0770000000000B3ull, "world mash Y", glyphs::Glyph::Y, {}, false, false, nullptr, true },
    } };

    inline constexpr std::array<glyphs::Prompt, 4> MashPrompts = { {
        { 0, 0, 0, 0, MashTextures[0].hash, 128, 128, 0, 1, false, true, 1.0f, "archives/permanent_xenon.pak",
            "models/ui/buttonprompt/xenon/mash_a" },
        { 0, 0, 0, 0, MashTextures[1].hash, 128, 128, 0, 1, false, true, 1.0f, "archives/permanent_xenon.pak",
            "models/ui/buttonprompt/xenon/mash_b" },
        { 0, 0, 0, 0, MashTextures[2].hash, 128, 128, 0, 1, false, true, 1.0f, "archives/permanent_xenon.pak",
            "models/ui/buttonprompt/xenon/mash_x" },
        { 0, 0, 0, 0, MashTextures[3].hash, 128, 128, 0, 1, false, true, 1.0f, "archives/permanent_xenon.pak",
            "models/ui/buttonprompt/xenon/mash_y" },
    } };
}
