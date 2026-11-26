#include "wii/gx_texture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    using namespace wii::gx;
    using Bytes = std::vector<uint8_t>;
    using Pixel = std::array<uint8_t, 4>;
    using Picture = std::vector<Pixel>;
    int checks = 0, failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

    struct FormatCase
    {
        TextureFormat format;
        uint32_t tileWidth, tileHeight, tileBytes;
        std::array<uint32_t, 4> sizes;
    };
    constexpr FormatCase Formats[] = {
        { TextureFormat::I4, 8, 8, 32, { 64, 32, 32, 480 } },
        { TextureFormat::I8, 8, 4, 32, { 128, 32, 32, 800 } },
        { TextureFormat::IA4, 8, 4, 32, { 128, 32, 32, 800 } },
        { TextureFormat::IA8, 4, 4, 32, { 256, 32, 64, 1440 } },
        { TextureFormat::RGB565, 4, 4, 32, { 256, 32, 64, 1440 } },
        { TextureFormat::RGB5A3, 4, 4, 32, { 256, 32, 64, 1440 } },
        { TextureFormat::RGBA8, 4, 4, 64, { 512, 64, 128, 2880 } },
        { TextureFormat::C4, 8, 8, 32, { 64, 32, 32, 480 } },
        { TextureFormat::C8, 8, 4, 32, { 128, 32, 32, 800 } },
        { TextureFormat::C14X2, 4, 4, 32, { 256, 32, 64, 1440 } },
        { TextureFormat::CMPR, 8, 8, 32, { 64, 32, 32, 480 } },
    };
    constexpr std::array<uint32_t, 2> Dimensions[] = { { 13, 7 }, { 1, 1 }, { 8, 4 }, { 33, 17 } };
    constexpr TlutFormat Palettes[] = { TlutFormat::IA8, TlutFormat::RGB565, TlutFormat::RGB5A3 };

    uint8_t Three(uint32_t v) { return uint8_t(v * 36 + v / 2); }
    uint8_t Five(uint32_t v) { return uint8_t(v * 8 + v / 4); }
    uint8_t Six(uint32_t v) { return uint8_t(v * 4 + v / 16); }

    Pixel Color565(uint32_t seed) { return { Five(seed % 32), Six((seed / 3) % 64), Five((seed / 7) % 32), 255 }; }

    Pixel Color5A3(uint32_t seed)
    {
        if (seed % 2)
            return { Five(seed % 32), Five((seed / 3) % 32), Five((seed / 7) % 32), 255 };
        return { uint8_t((seed % 16) * 17), uint8_t(((seed / 3) % 16) * 17), uint8_t(((seed / 7) % 16) * 17),
            Three(seed % 7) };
    }

    uint16_t Pack565(Pixel p) { return uint16_t((p[0] / 8) * 2048 + (p[1] / 4) * 32 + p[2] / 8); }

    uint16_t Pack5A3(Pixel p)
    {
        if (p[3] == 255)
            return uint16_t(0x8000 + (p[0] / 8) * 1024 + (p[1] / 8) * 32 + p[2] / 8);
        return uint16_t((p[3] / 32) * 4096 + (p[0] / 17) * 256 + (p[1] / 17) * 16 + p[2] / 17);
    }

    uint32_t PaletteEntries(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::C4: return 16;
        case TextureFormat::C8: return 256;
        case TextureFormat::C14X2: return 16384;
        default: return 0;
        }
    }

    uint32_t Index(uint32_t x, uint32_t y, uint32_t entries) { return (x * 1013 + y * 877 + x * y * 71) % entries; }

    Picture PalettePicture(TlutFormat format, uint32_t entries)
    {
        Picture palette;
        for (uint32_t i = 0; i < entries; ++i)
        {
            const uint32_t seed = i * 139 + 37;
            if (format == TlutFormat::IA8)
            {
                const uint8_t intensity = uint8_t(seed), alpha = uint8_t(seed / 5);
                palette.push_back({ intensity, intensity, intensity, alpha });
            }
            else
                palette.push_back(format == TlutFormat::RGB565 ? Color565(seed) : Color5A3(seed));
        }
        return palette;
    }

    void Put16(Bytes& data, size_t offset, uint16_t value)
    {
        data[offset] = uint8_t(value / 256);
        data[offset + 1] = uint8_t(value % 256);
    }

    Bytes EncodePalette(TlutFormat format, const Picture& palette)
    {
        Bytes bytes(palette.size() * 2);
        for (size_t i = 0; i < palette.size(); ++i)
        {
            const Pixel p = palette[i];
            const uint16_t word = format == TlutFormat::IA8      ? uint16_t(p[3] * 256 + p[0])
                                  : format == TlutFormat::RGB565 ? Pack565(p)
                                                                 : Pack5A3(p);
            Put16(bytes, 2 * i, word);
        }
        return bytes;
    }

    std::array<Pixel, 4> BlockColors(uint32_t x, uint32_t y)
    {
        if ((x / 4 + y / 4) % 2 == 0)
            return { Pixel{ 255, 0, 0, 255 }, Pixel{ 0, 255, 0, 255 }, Pixel{ 170, 85, 0, 255 },
                Pixel{ 85, 170, 0, 255 } };
        return { Pixel{ 0, 0, 255, 255 }, Pixel{ 255, 0, 0, 255 }, Pixel{ 127, 0, 127, 255 }, Pixel{} };
    }

    Picture KnownPicture(TextureFormat format, uint32_t width, uint32_t height, const Picture& palette)
    {
        Picture picture;
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint32_t seed = x * 37 + y * 113 + x * y * 11;
                const uint8_t intensity = uint8_t(seed), nibble = uint8_t((seed % 16) * 17);
                Pixel p{};
                switch (format)
                {
                case TextureFormat::I4: p = { nibble, nibble, nibble, nibble }; break;
                case TextureFormat::I8: p = { intensity, intensity, intensity, intensity }; break;
                case TextureFormat::IA4: p = { nibble, nibble, nibble, uint8_t(((seed / 7) % 16) * 17) }; break;
                case TextureFormat::IA8: p = { intensity, intensity, intensity, uint8_t(seed / 7) }; break;
                case TextureFormat::RGB565: p = Color565(seed); break;
                case TextureFormat::RGB5A3: p = Color5A3(seed); break;
                case TextureFormat::RGBA8:
                    p = { uint8_t(seed), uint8_t(seed / 3), uint8_t(seed / 7), uint8_t(seed / 11) };
                    break;
                case TextureFormat::C4:
                case TextureFormat::C8:
                case TextureFormat::C14X2: p = palette[Index(x, y, uint32_t(palette.size()))]; break;
                case TextureFormat::CMPR: p = BlockColors(x, y)[(x + 3 * y + x / 4) % 4]; break;
                }
                picture.push_back(p);
            }
        }
        return picture;
    }

    Bytes Encode(const FormatCase& test, uint32_t width, uint32_t height, const Picture& picture)
    {
        const uint32_t columns = (width + test.tileWidth - 1) / test.tileWidth;
        const uint32_t rows = (height + test.tileHeight - 1) / test.tileHeight;
        Bytes data(size_t(columns) * rows * test.tileBytes, 0xE7);
        if (test.format == TextureFormat::CMPR)
        {
            for (uint32_t y = 0; y < rows * 8; y += 4)
            {
                for (uint32_t x = 0; x < columns * 8; x += 4)
                {
                    const size_t offset = ((y / 8) * columns + x / 8) * 32 + ((y % 8) / 4 * 2 + (x % 8) / 4) * 8;
                    const auto colors = BlockColors(x, y);
                    Put16(data, offset, Pack565(colors[0]));
                    Put16(data, offset + 2, Pack565(colors[1]));
                    for (uint32_t dy = 0; dy < 4; ++dy)
                    {
                        uint8_t indices = 0;
                        for (uint32_t dx = 0; dx < 4; ++dx)
                        {
                            uint32_t index = 0;
                            if (x + dx < width && y + dy < height)
                            {
                                const Pixel p = picture[(y + dy) * width + x + dx];
                                while (index < 3 && colors[index] != p)
                                    ++index;
                            }
                            indices = uint8_t(indices * 4 + index);
                        }
                        data[offset + 4 + dy] = indices;
                    }
                }
            }
            return data;
        }

        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const size_t tile = ((y / test.tileHeight) * columns + x / test.tileWidth) * test.tileBytes;
                const size_t pixel = (y % test.tileHeight) * test.tileWidth + x % test.tileWidth;
                const Pixel p = picture[y * width + x];
                switch (test.format)
                {
                case TextureFormat::I4:
                case TextureFormat::C4:
                {
                    const uint32_t value = test.format == TextureFormat::I4 ? p[0] / 17 : Index(x, y, 16);
                    const uint32_t shift = pixel % 2 == 0 ? 4 : 0;
                    data[tile + pixel / 2] = uint8_t((data[tile + pixel / 2] & ~(15 << shift)) | (value << shift));
                    break;
                }
                case TextureFormat::I8: data[tile + pixel] = p[0]; break;
                case TextureFormat::IA4: data[tile + pixel] = uint8_t((p[3] / 17) * 16 + p[0] / 17); break;
                case TextureFormat::IA8: Put16(data, tile + 2 * pixel, uint16_t(p[3] * 256 + p[0])); break;
                case TextureFormat::RGB565: Put16(data, tile + 2 * pixel, Pack565(p)); break;
                case TextureFormat::RGB5A3: Put16(data, tile + 2 * pixel, Pack5A3(p)); break;
                case TextureFormat::RGBA8:
                    data[tile + 2 * pixel] = p[3];
                    data[tile + 2 * pixel + 1] = p[0];
                    data[tile + 2 * pixel + 32] = p[1];
                    data[tile + 2 * pixel + 33] = p[2];
                    break;
                case TextureFormat::C8: data[tile + pixel] = uint8_t(Index(x, y, 256)); break;
                case TextureFormat::C14X2: Put16(data, tile + 2 * pixel, uint16_t(0xC000 | Index(x, y, 16384))); break;
                default: break;
                }
            }
        }
        return data;
    }

    bool Matches(const Bytes& data, size_t offset, Pixel pixel)
    {
        return std::equal(pixel.begin(), pixel.end(), data.begin() + offset);
    }

    void RoundTrips()
    {
        for (const auto& test : Formats)
        {
            for (size_t size = 0; size < std::size(Dimensions); ++size)
            {
                const auto [width, height] = Dimensions[size];
                EXPECT(EncodedSize(test.format, width, height) == test.sizes[size]);
                const uint32_t entries = PaletteEntries(test.format);
                for (const TlutFormat mode : Palettes)
                {
                    if (entries == 0 && mode != TlutFormat::IA8)
                        continue;
                    const Picture palette = PalettePicture(mode, entries);
                    const Picture picture = KnownPicture(test.format, width, height, palette);
                    const Bytes tlut = EncodePalette(mode, palette);
                    const Bytes source = Encode(test, width, height, picture);
                    Bytes out(size_t(width) * height * 4 + 32, 0xCD);
                    EXPECT(source.size() == test.sizes[size]);
                    EXPECT(Decode(test.format, source.data(), source.size(), width, height, out.data() + 16,
                        tlut.empty() ? nullptr : tlut.data(), mode, entries));
                    bool matches = true;
                    for (size_t i = 0; i < picture.size(); ++i)
                        matches = matches && Matches(out, 16 + i * 4, picture[i]);
                    if (!matches)
                        fprintf(stderr, "round trip: format %u, palette %u, %ux%u\n", unsigned(test.format),
                            unsigned(mode), width, height);
                    EXPECT(matches);
                    EXPECT(std::all_of(out.begin(), out.begin() + 16, [](uint8_t b) { return b == 0xCD; }) &&
                           std::all_of(out.end() - 16, out.end(), [](uint8_t b) { return b == 0xCD; }));
                }
            }
        }
    }

    void KnownBytes()
    {
        Bytes source(64), out(256, 0xCD);
        source[0] = 0x2D;
        EXPECT(Decode(TextureFormat::I4, source.data(), 32, 2, 1, out.data()));
        EXPECT(Matches(out, 0, { 34, 34, 34, 34 }) && Matches(out, 4, { 221, 221, 221, 221 }));
        EXPECT(Decode(TextureFormat::I8, source.data(), 32, 1, 1, out.data()));
        EXPECT(Matches(out, 0, { 45, 45, 45, 45 }));
        source[0] = 0xA3;
        EXPECT(Decode(TextureFormat::IA4, source.data(), 32, 1, 1, out.data()));
        EXPECT(Matches(out, 0, { 51, 51, 51, 170 }));
        source[0] = 0x12;
        source[1] = 0x9A;
        EXPECT(Decode(TextureFormat::IA8, source.data(), 32, 1, 1, out.data()));
        EXPECT(Matches(out, 0, { 154, 154, 154, 18 }));
        source[0] = 0x12;
        source[1] = 0x34;
        EXPECT(Decode(TextureFormat::RGB565, source.data(), 32, 1, 1, out.data()));
        EXPECT(Matches(out, 0, { 16, 69, 165, 255 }));
        source[0] = 0x8C;
        source[1] = 0x65;
        source[2] = 0x31;
        source[3] = 0x24;
        source[4] = 0x7A;
        source[5] = 0xBC;
        EXPECT(Decode(TextureFormat::RGB5A3, source.data(), 32, 3, 1, out.data()));
        EXPECT(Matches(out, 0, { 24, 24, 41, 255 }));
        EXPECT(Matches(out, 4, { 17, 34, 68, 109 }));
        EXPECT(Matches(out, 8, { 170, 187, 204, 255 }));
        for (uint8_t i = 0; i < 16; ++i)
        {
            source[2 * i] = uint8_t(0x10 + i);
            source[2 * i + 1] = uint8_t(0x40 + i);
            source[32 + 2 * i] = uint8_t(0x80 + i);
            source[33 + 2 * i] = uint8_t(0xE0 + i);
        }
        EXPECT(Decode(TextureFormat::RGBA8, source.data(), 64, 4, 4, out.data()));
        for (uint8_t i = 0; i < 16; ++i)
            EXPECT(Matches(out, i * 4, { uint8_t(0x40 + i), uint8_t(0x80 + i), uint8_t(0xE0 + i), uint8_t(0x10 + i) }));
    }

    void PaletteBytes()
    {
        const uint8_t tlut[] = { 0x12, 0x34, 0xAB, 0xCD };
        constexpr Pixel First[] = { { 52, 52, 52, 18 }, { 16, 69, 165, 255 }, { 34, 51, 68, 36 } };
        constexpr Pixel Second[] = { { 205, 205, 205, 171 }, { 173, 121, 107, 255 }, { 82, 247, 107, 255 } };
        for (const TextureFormat format : { TextureFormat::C4, TextureFormat::C8, TextureFormat::C14X2 })
        {
            Bytes source(32), out(16);
            if (format == TextureFormat::C4)
            {
                source[0] = 0x10;
                source[1] = 0x21;
            }
            else if (format == TextureFormat::C8)
            {
                source[0] = 1;
                source[1] = 0;
                source[2] = 2;
                source[3] = 1;
            }
            else
            {
                source[0] = 0xC0;
                source[1] = 1;
                source[2] = 0x40;
                source[3] = 0;
                source[4] = 0x80;
                source[5] = 2;
                source[7] = 1;
            }
            for (size_t i = 0; i < std::size(Palettes); ++i)
            {
                EXPECT(Decode(format, source.data(), 32, 4, 1, out.data(), tlut, Palettes[i], 2));
                EXPECT(Matches(out, 0, Second[i]) && Matches(out, 4, First[i]) && Matches(out, 8, {}) &&
                       Matches(out, 12, Second[i]));
                EXPECT(Decode(format, source.data(), 32, 4, 1, out.data(), tlut, Palettes[i], 0));
                EXPECT(std::all_of(out.begin(), out.end(), [](uint8_t b) { return b == 0; }));
            }
            const uint32_t entries = PaletteEntries(format);
            Bytes fullPalette(size_t(entries) * 2);
            Put16(fullPalette, size_t(entries - 1) * 2, 0xABCD);
            std::fill(source.begin(), source.end(), 0xFF);
            for (size_t i = 0; i < std::size(Palettes); ++i)
            {
                EXPECT(Decode(format, source.data(), 32, 1, 1, out.data(), fullPalette.data(), Palettes[i], entries));
                EXPECT(Matches(out, 0, Second[i]));
            }
        }
    }

    void CmprBytes()
    {
        const uint8_t source[32] = {
            0xF8,
            0x00,
            0x07,
            0xE0,
            0x1B,
            0xE4,
            0x55,
            0xAA,
            0x00,
            0x1F,
            0xF8,
            0x00,
            0x1B,
            0xE4,
            0xAA,
            0xFF,
            0x07,
            0xE0,
            0x07,
            0xE0,
            0x1B,
            0xFF,
            0xAA,
            0x00,
            0x12,
            0x34,
            0x00,
            0x00,
            0x1B,
            0x1B,
            0x1B,
            0x1B,
        };
        Bytes out(8 * 8 * 4);
        EXPECT(Decode(TextureFormat::CMPR, source, sizeof(source), 8, 8, out.data()));
        constexpr Pixel Colors[4][4] = {
            { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 170, 85, 0, 255 }, { 85, 170, 0, 255 } },
            { { 0, 0, 255, 255 }, { 255, 0, 0, 255 }, { 127, 0, 127, 255 }, {} },
            { { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, { 0, 255, 0, 255 }, {} },
            { { 16, 69, 165, 255 }, { 0, 0, 0, 255 }, { 10, 46, 110, 255 }, { 5, 23, 55, 255 } },
        };
        constexpr uint8_t Indices[4][4] = { { 0x1B, 0xE4, 0x55, 0xAA }, { 0x1B, 0xE4, 0xAA, 0xFF },
            { 0x1B, 0xFF, 0xAA, 0x00 }, { 0x1B, 0x1B, 0x1B, 0x1B } };
        for (size_t block = 0; block < 4; ++block)
        {
            for (size_t row = 0; row < 4; ++row)
            {
                uint8_t indices = Indices[block][row];
                for (size_t col = 0; col < 4; ++col)
                {
                    const size_t pixel = (block / 2 * 4 + row) * 8 + block % 2 * 4 + col;
                    EXPECT(Matches(out, pixel * 4, Colors[block][indices / 64]));
                    indices = uint8_t(indices * 4);
                }
            }
        }
    }

    void Refusals()
    {
        const Bytes source(64, 0xFF), tlut(32768, 0xFF), untouched(8 * 8 * 4, 0xCD);
        Bytes out = untouched;
        for (const auto& test : Formats)
        {
            const uint32_t size = EncodedSize(test.format, 1, 1);
            EXPECT(
                !Decode(test.format, source.data(), size - 1, 1, 1, out.data(), tlut.data(), TlutFormat::IA8, 16384));
            EXPECT(out == untouched);
            EXPECT(!Decode(test.format, nullptr, size, 1, 1, out.data(), tlut.data()));
            EXPECT(out == untouched);
            EXPECT(!Decode(test.format, source.data(), size, 1, 1, nullptr, tlut.data()));
            EXPECT(EncodedSize(test.format, 0, 7) == 0 && EncodedSize(test.format, 13, 0) == 0);
            EXPECT(Decode(test.format, nullptr, 0, 0, 7, nullptr, tlut.data()));
            constexpr uint32_t Max = std::numeric_limits<uint32_t>::max();
            EXPECT(EncodedSize(test.format, Max, Max) == 0);
            EXPECT(!Decode(test.format, source.data(), source.size(), Max, Max, out.data(), tlut.data()));
            EXPECT(out == untouched);
            if (PaletteEntries(test.format))
            {
                EXPECT(!Decode(test.format, source.data(), size, 1, 1, out.data()));
                EXPECT(out == untouched);
                // A palette format the register's 2 bits can hold but GX does not define, as a guest could set it.
                const uint8_t undefinedFormat = 3;
                TlutFormat undefined{};
                std::memcpy(&undefined, &undefinedFormat, 1);
                EXPECT(!Decode(test.format, source.data(), size, 1, 1, out.data(), tlut.data(), undefined, 16384));
                EXPECT(out == untouched);
            }
        }
        for (uint32_t value = 0; value < 256; ++value)
        {
            const TextureFormat format = TextureFormat(value);
            if (std::any_of(std::begin(Formats), std::end(Formats),
                    [&](const FormatCase& test) { return test.format == format; }))
                continue;
            EXPECT(EncodedSize(format, 1, 1) == 0);
            EXPECT(!Decode(format, source.data(), source.size(), 1, 1, out.data(), tlut.data()));
            EXPECT(out == untouched);
            EXPECT(!Decode(format, nullptr, 0, 0, 0, nullptr));
        }
    }

    void Speed()
    {
        constexpr uint32_t Width = 1024, Height = 1024;
        const Picture picture = KnownPicture(TextureFormat::CMPR, Width, Height, {});
        const Bytes source = Encode(Formats[10], Width, Height, picture);
        Bytes out(size_t(Width) * Height * 4);
        const auto start = std::chrono::steady_clock::now();
        const bool decoded = Decode(TextureFormat::CMPR, source.data(), source.size(), Width, Height, out.data());
        const double milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        printf("CMPR 1024x1024: %.3f ms\n", milliseconds);
        EXPECT(decoded);
        bool matches = true;
        for (size_t i = 0; i < picture.size(); ++i)
            matches = matches && Matches(out, i * 4, picture[i]);
        EXPECT(matches);
#ifdef NDEBUG
        EXPECT(milliseconds < 50.0);
#endif
    }
}

int main()
{
    RoundTrips();
    KnownBytes();
    PaletteBytes();
    CmprBytes();
    Refusals();
    Speed();
    printf("gx texture test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
