#include "wii/gx_texture.h"
#include <algorithm>
#include <limits>

namespace wii::gx
{
    namespace
    {
        struct Tile
        {
            uint32_t width, height, bytes;
        };

        Tile Layout(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::I4:
            case TextureFormat::C4:
            case TextureFormat::CMPR: return { 8, 8, 32 };
            case TextureFormat::I8:
            case TextureFormat::IA4:
            case TextureFormat::C8: return { 8, 4, 32 };
            case TextureFormat::IA8:
            case TextureFormat::RGB565:
            case TextureFormat::RGB5A3:
            case TextureFormat::C14X2: return { 4, 4, 32 };
            case TextureFormat::RGBA8: return { 4, 4, 64 };
            default: return {};
            }
        }

        struct Color
        {
            uint8_t r, g, b, a;
        };

        uint16_t Read16(const uint8_t* bytes) { return uint16_t(uint16_t(bytes[0]) << 8 | bytes[1]); }
        uint8_t Expand3(uint32_t v) { return uint8_t((v << 5) | (v << 2) | (v >> 1)); }
        uint8_t Expand4(uint32_t v) { return uint8_t((v << 4) | v); }
        uint8_t Expand5(uint32_t v) { return uint8_t((v << 3) | (v >> 2)); }
        uint8_t Expand6(uint32_t v) { return uint8_t((v << 2) | (v >> 4)); }

        Color Rgb565(uint16_t v) { return { Expand5(v >> 11), Expand6((v >> 5) & 63), Expand5(v & 31), 255 }; }

        Color Rgb5A3(uint16_t v)
        {
            if (v & 0x8000)
                return { Expand5((v >> 10) & 31), Expand5((v >> 5) & 31), Expand5(v & 31), 255 };
            return { Expand4((v >> 8) & 15), Expand4((v >> 4) & 15), Expand4(v & 15), Expand3(v >> 12) };
        }

        Color Palette(const uint8_t* tlut, TlutFormat format, uint32_t entries, uint32_t index)
        {
            if (index >= entries)
                return {};
            const uint16_t v = Read16(tlut + size_t(index) * 2);
            switch (format)
            {
            case TlutFormat::IA8: return { uint8_t(v), uint8_t(v), uint8_t(v), uint8_t(v >> 8) };
            case TlutFormat::RGB565: return Rgb565(v);
            case TlutFormat::RGB5A3: return Rgb5A3(v);
            default: return {};
            }
        }

        void Store(uint8_t* out, Color color)
        {
            out[0] = color.r;
            out[1] = color.g;
            out[2] = color.b;
            out[3] = color.a;
        }

        Color Blend(Color first, Color second, uint32_t weight, uint32_t divisor)
        {
            return { uint8_t((weight * first.r + second.r) / divisor), uint8_t((weight * first.g + second.g) / divisor),
                uint8_t((weight * first.b + second.b) / divisor), 255 };
        }

        void Cmpr(const uint8_t* source, uint8_t* rgba, uint32_t width, uint32_t x, uint32_t y, uint32_t columns,
            uint32_t rows)
        {
            for (uint32_t block = 0; block < 4; ++block)
            {
                const uint8_t* data = source + block * 8;
                const uint16_t first = Read16(data), second = Read16(data + 2);
                Color colors[4] = { Rgb565(first), Rgb565(second), {}, {} };
                if (first > second)
                {
                    colors[2] = Blend(colors[0], colors[1], 2, 3);
                    colors[3] = Blend(colors[1], colors[0], 2, 3);
                }
                else
                    colors[2] = Blend(colors[0], colors[1], 1, 2);

                const uint32_t bx = (block % 2) * 4, by = (block / 2) * 4;
                if (bx >= columns || by >= rows)
                    continue;
                for (uint32_t row = by; row < std::min(by + 4, rows); ++row)
                {
                    uint8_t* out = rgba + (size_t(y + row) * width + x + bx) * 4;
                    for (uint32_t col = bx; col < std::min(bx + 4, columns); ++col)
                    {
                        Store(out, colors[(data[4 + row - by] >> (6 - 2 * (col - bx))) & 3]);
                        out += 4;
                    }
                }
            }
        }
    }

    uint32_t EncodedSize(TextureFormat format, uint32_t width, uint32_t height)
    {
        const Tile tile = Layout(format);
        if (tile.bytes == 0 || width == 0 || height == 0)
            return 0;
        const uint64_t count = uint64_t(1 + (width - 1) / tile.width) * (1 + (height - 1) / tile.height);
        if (count > std::numeric_limits<uint32_t>::max() / tile.bytes)
            return 0;
        return uint32_t(count * tile.bytes);
    }

    bool Decode(TextureFormat format, const uint8_t* source, size_t sourceSize, uint32_t width, uint32_t height,
        uint8_t* rgba, const uint8_t* tlut, TlutFormat tlutFormat, uint32_t tlutEntries)
    {
        const Tile tile = Layout(format);
        const bool indexed =
            format == TextureFormat::C4 || format == TextureFormat::C8 || format == TextureFormat::C14X2;
        if (tile.bytes == 0 || (indexed && (tlut == nullptr || uint8_t(tlutFormat) > uint8_t(TlutFormat::RGB5A3))))
            return false;
        if (width == 0 || height == 0)
            return true;
        const uint32_t size = EncodedSize(format, width, height);
        if (size == 0 || sourceSize < size || source == nullptr || rgba == nullptr ||
            uint64_t(width) * height > std::numeric_limits<size_t>::max() / 4)
            return false;

        const uint32_t tileColumns = 1 + (width - 1) / tile.width;
        const uint32_t tileRows = 1 + (height - 1) / tile.height;
        for (uint32_t ty = 0; ty < tileRows; ++ty)
        {
            const uint32_t y = ty * tile.height, rows = std::min(tile.height, height - y);
            for (uint32_t tx = 0; tx < tileColumns; ++tx, source += tile.bytes)
            {
                const uint32_t x = tx * tile.width, columns = std::min(tile.width, width - x);
                if (format == TextureFormat::CMPR)
                {
                    Cmpr(source, rgba, width, x, y, columns, rows);
                    continue;
                }
                for (uint32_t row = 0; row < rows; ++row)
                {
                    uint8_t* out = rgba + (size_t(y + row) * width + x) * 4;
                    for (uint32_t col = 0; col < columns; ++col, out += 4)
                    {
                        const uint32_t pixel = row * tile.width + col;
                        Color color{};
                        switch (format)
                        {
                        case TextureFormat::I4:
                        {
                            const uint8_t i = Expand4((source[pixel / 2] >> (pixel % 2 == 0 ? 4 : 0)) & 15);
                            color = { i, i, i, i };
                            break;
                        }
                        case TextureFormat::I8:
                        {
                            const uint8_t i = source[pixel];
                            color = { i, i, i, i };
                            break;
                        }
                        case TextureFormat::IA4:
                        {
                            const uint8_t v = source[pixel], i = Expand4(v & 15);
                            color = { i, i, i, Expand4(v >> 4) };
                            break;
                        }
                        case TextureFormat::IA8:
                        {
                            const uint8_t i = source[2 * pixel + 1];
                            color = { i, i, i, source[2 * pixel] };
                            break;
                        }
                        case TextureFormat::RGB565: color = Rgb565(Read16(source + 2 * pixel)); break;
                        case TextureFormat::RGB5A3: color = Rgb5A3(Read16(source + 2 * pixel)); break;
                        case TextureFormat::RGBA8:
                            color = { source[2 * pixel + 1], source[32 + 2 * pixel], source[33 + 2 * pixel],
                                source[2 * pixel] };
                            break;
                        case TextureFormat::C4:
                            color = Palette(
                                tlut, tlutFormat, tlutEntries, (source[pixel / 2] >> (pixel % 2 == 0 ? 4 : 0)) & 15);
                            break;
                        case TextureFormat::C8: color = Palette(tlut, tlutFormat, tlutEntries, source[pixel]); break;
                        case TextureFormat::C14X2:
                            color = Palette(tlut, tlutFormat, tlutEntries, Read16(source + 2 * pixel) & 0x3FFF);
                            break;
                        default: break;
                        }
                        Store(out, color);
                    }
                }
            }
        }
        return true;
    }
}
