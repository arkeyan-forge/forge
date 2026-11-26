#pragma once

// The GX texture formats (tiled, big-endian, in guest memory) decoded to linear RGBA8 for the host renderer
// (docs/runtime/wii/gx_textures.md).
#include <cstddef>
#include <cstdint>

namespace wii::gx
{
    enum class TextureFormat : uint8_t
    {
        I4 = 0,
        I8 = 1,
        IA4 = 2,
        IA8 = 3,
        RGB565 = 4,
        RGB5A3 = 5,
        RGBA8 = 6,
        C4 = 8,
        C8 = 9,
        C14X2 = 10,
        CMPR = 14
    };
    enum class TlutFormat : uint8_t
    {
        IA8 = 0,
        RGB565 = 1,
        RGB5A3 = 2
    };

    // Whole tiles for one mip level; zero for unknown formats, empty levels or sizes exceeding uint32_t.
    uint32_t EncodedSize(TextureFormat format, uint32_t width, uint32_t height);

    // Output is width * height * 4 bytes, row-major RGBA, top row first; buffers must not overlap.
    // Invalid indices become transparent black. A palette must hold tlutEntries big-endian 16-bit entries.
    // Unknown formats (including palette formats), short sources, missing palettes, null nonempty buffers or
    // unrepresentable sizes return false without touching rgba. Empty levels succeed with no buffer access.
    bool Decode(TextureFormat format, const uint8_t* source, size_t sourceSize, uint32_t width, uint32_t height,
        uint8_t* rgba, const uint8_t* tlut = nullptr, TlutFormat tlutFormat = TlutFormat::IA8,
        uint32_t tlutEntries = 0);
}
