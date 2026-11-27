#include "wii/gx_vertex.h"
#include "wii/memory_map.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
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

    using namespace wii::gx;
    uint8_t* g_base = nullptr;

    struct Stream
    {
        std::vector<uint8_t> bytes;
        void Word(uint32_t value, uint32_t size)
        {
            for (uint32_t i = size; i > 0; --i)
                bytes.push_back(uint8_t(value >> (8 * (i - 1))));
        }
        void Scalar(uint32_t format, int32_t value)
        {
            const uint32_t sizes[] = { 1, 1, 2, 2, 4 };
            Word(format == 4 ? std::bit_cast<uint32_t>(float(value)) : uint32_t(value), sizes[format]);
        }
    };

    void Cp(CommandProcessor& gp, uint8_t index, uint32_t value)
    {
        Stream command;
        command.Word(LoadCp, 1);
        command.Word(index, 1);
        command.Word(value, 4);
        gp.Feed(g_base, command.bytes.data(), command.bytes.size());
    }

    bool Decode(const CommandProcessor& gp, const Stream& stream, Vertex* out, uint16_t count = 1, uint8_t format = 0)
    {
        EXPECT(stream.bytes.size() == size_t(gp.VertexSize(format)) * count);
        const uint8_t empty = 0;
        const DrawCall draw{ Primitive::Points, format, count, stream.bytes.empty() ? &empty : stream.bytes.data(),
            gp.VertexSize(format) };
        return DecodeVertices(gp, g_base, draw, out);
    }

    void TextureFormat(uint32_t* vat, uint32_t texture, uint32_t count, uint32_t format, uint32_t shift)
    {
        const uint32_t registers[] = { 0, 1, 1, 1, 1, 2, 2, 2 };
        const uint32_t counts[] = { 21, 0, 9, 18, 27, 5, 14, 23 };
        const uint32_t shifts[] = { 25, 4, 13, 22, 0, 9, 18, 27 };
        vat[registers[texture]] |= count << counts[texture] | format << (counts[texture] + 1);
        vat[texture == 4 ? 2 : registers[texture]] |= shift << shifts[texture];
    }

    void Vat(CommandProcessor& gp, const uint32_t* vat, uint8_t format = 0)
    {
        for (uint32_t i = 0; i < 3; ++i)
            Cp(gp, uint8_t(0x70 + 0x10 * i + format), vat[i]);
    }

    void Positions()
    {
        for (uint32_t format = 0; format < 5; ++format)
            for (uint32_t shift : { 0u, 1u, 7u, 31u })
                for (uint32_t count : { 2u, 3u })
                    for (uint8_t slot = 0; slot < 8; ++slot)
                    {
                        CommandProcessor gp;
                        Cp(gp, 0x50, 1 << 9);
                        Cp(gp, uint8_t(0x70 + slot), (count == 3 ? 1u : 0u) | format << 1 | shift << 4);
                        Stream stream;
                        for (int32_t v = 0; v < 2; ++v)
                            for (uint32_t i = 0; i < count; ++i)
                                stream.Scalar(
                                    format, (format == 1 || format == 3 ? -1 : 1) * (17 + v * 5 + int32_t(i)));
                        Vertex out[3]{};
                        out[2].position[0] = 123;
                        EXPECT(Decode(gp, stream, out, 2, slot));
                        for (int32_t v = 0; v < 2; ++v)
                            for (uint32_t i = 0; i < 3; ++i)
                            {
                                const float raw =
                                    float((format == 1 || format == 3 ? -1 : 1) * (17 + v * 5 + int32_t(i)));
                                EXPECT(out[v].position[i] ==
                                       (i >= count ? 0 : std::ldexp(raw, format == 4 ? 0 : -int(shift))));
                            }
                        EXPECT(out[2].position[0] == 123);
                    }
    }

    void NumericLimits()
    {
        const int32_t values[5][3] = { { 0, 128, 255 }, { -128, -1, 127 }, { 0, 32768, 65535 }, { -32768, -1, 32767 },
            { 0, 0, 0 } };
        const float floats[] = { -1.25f, 0.5f, 123.75f };
        for (uint32_t format = 0; format < 5; ++format)
        {
            CommandProcessor gp;
            Cp(gp, 0x50, 1 << 9);
            Cp(gp, 0x60, 1);
            Cp(gp, 0x70, 1 | format << 1 | 2 << 4 | 1 << 21 | format << 22 | 3 << 25);
            Stream stream;
            for (uint32_t i = 0; i < 5; ++i)
            {
                const uint32_t component = i < 3 ? i : i - 3;
                if (format == 4)
                    stream.Word(std::bit_cast<uint32_t>(floats[component]), 4);
                else
                    stream.Scalar(format, values[format][component]);
            }
            Vertex out;
            EXPECT(Decode(gp, stream, &out));
            for (uint32_t i = 0; i < 3; ++i)
                EXPECT(out.position[i] == (format == 4 ? floats[i] : float(values[format][i]) / 4));
            for (uint32_t i = 0; i < 2; ++i)
                EXPECT(out.texture[0][i] == (format == 4 ? floats[i] : float(values[format][i]) / 8));
        }
        for (uint32_t format : { 1u, 3u })
        {
            CommandProcessor gp;
            Cp(gp, 0x50, 1 << 11);
            Cp(gp, 0x70, format << 10);
            Stream stream;
            for (int32_t value : values[format])
                stream.Scalar(format, value);
            Vertex out;
            EXPECT(Decode(gp, stream, &out));
            for (uint32_t i = 0; i < 3; ++i)
                EXPECT(out.normal[i] == float(values[format][i]) / (format == 1 ? 64 : 16384));
        }
    }

    void MixedDirect()
    {
        CommandProcessor gp;
        Cp(gp, 0x50, 0x1FF | 1 << 9 | 1 << 11 | 1 << 13 | 1 << 15);
        Cp(gp, 0x60, 0x5555);
        uint32_t vat[3] = { 1 << 1 | 1 << 9 | 4 << 10 | 2 << 14 | 4 << 18 | 1u << 31, 0, 0 };
        for (uint32_t i = 0; i < 8; ++i)
            TextureFormat(vat, i, i % 2, i % 5, i);
        Vat(gp, vat);
        Stream stream;
        for (uint32_t v = 0; v < 2; ++v)
        {
            for (uint32_t i = 0; i < 9; ++i)
                stream.Word(64 + v + i, 1);
            stream.Scalar(1, -32 - int32_t(v));
            stream.Scalar(1, 17 + int32_t(v));
            for (uint32_t i = 0; i < 9; ++i)
                stream.Word(std::bit_cast<uint32_t>(float(i) + float(v) / 2), 4);
            stream.Word(0x12345609, 4);
            stream.Word(0x055ABF, 3);
            for (uint32_t i = 0; i < 8; ++i)
                for (uint32_t j = 0; j < i % 2 + 1; ++j)
                    stream.Scalar(i % 5, 10 + int32_t(v + i + j));
        }
        Vertex out[2];
        EXPECT(Decode(gp, stream, out, 2));
        for (uint32_t v = 0; v < 2; ++v)
        {
            EXPECT(out[v].positionMatrix == 64 + v);
            EXPECT(out[v].position[0] == float(-32 - int32_t(v)) && out[v].position[1] == float(17 + v) &&
                   out[v].position[2] == 0);
            const float* vectors[] = { out[v].normal, out[v].binormal, out[v].tangent };
            for (uint32_t i = 0; i < 9; ++i)
                EXPECT(vectors[i / 3][i % 3] == float(i) + float(v) / 2);
            const uint8_t expected[2][4] = { { 18, 52, 86, 255 }, { 4, 85, 170, 255 } };
            EXPECT(memcmp(out[v].colour, expected, sizeof(expected)) == 0);
            for (uint32_t i = 0; i < 8; ++i)
            {
                EXPECT(out[v].textureMatrix[i] == 65 + v + i);
                for (uint32_t j = 0; j < 2; ++j)
                    EXPECT(out[v].texture[i][j] ==
                           (j > i % 2 ? 0 : std::ldexp(float(10 + v + i + j), i % 5 == 4 ? 0 : -int(i))));
            }
        }
    }

    void Normals()
    {
        for (uint32_t format : { 1u, 3u, 4u })
            for (bool nbt : { false, true })
                for (bool threeIndices : { false, true })
                {
                    CommandProcessor gp;
                    Cp(gp, 0x50, 1 << 11);
                    Cp(gp, 0x70, uint32_t(nbt) << 9 | format << 10 | uint32_t(threeIndices) << 31);
                    Stream stream;
                    for (int32_t v = 0; v < 2; ++v)
                        for (int32_t i = 0; i < (nbt ? 9 : 3); ++i)
                            stream.Scalar(format, (i % 2 == 0 ? -1 : 1) * (32 + v * 10 + i));
                    Vertex out[2];
                    EXPECT(Decode(gp, stream, out, 2));
                    for (int32_t v = 0; v < 2; ++v)
                    {
                        const float* vectors[] = { out[v].normal, out[v].binormal, out[v].tangent };
                        for (int32_t i = 0; i < 9; ++i)
                        {
                            const float raw = float((i % 2 == 0 ? -1 : 1) * (32 + v * 10 + i));
                            const int shift = format == 1 ? 6 : format == 3 ? 14 : 0;
                            EXPECT(vectors[i / 3][i % 3] == (i >= 3 && !nbt ? 0 : std::ldexp(raw, -shift)));
                        }
                    }
                }
    }

    void Colours()
    {
        struct Sample
        {
            uint32_t value, bytes;
            std::array<uint8_t, 4> expected;
        };
        const Sample samples[] = { { 0xA55B, 2, { 165, 170, 222, 255 } }, { 0x123456, 3, { 18, 52, 86, 255 } },
            { 0x12345609, 4, { 18, 52, 86, 255 } }, { 0xA37F, 2, { 170, 51, 119, 255 } },
            { 0x055ABF, 3, { 4, 85, 170, 255 } }, { 0x12345678, 4, { 18, 52, 86, 120 } } };
        for (uint32_t format = 0; format < 6; ++format)
        {
            CommandProcessor gp;
            Cp(gp, 0x50, 1 << 13 | 1 << 15);
            Cp(gp, 0x70, format << 14 | format << 18);
            Stream stream;
            for (uint32_t v = 0; v < 3; ++v)
                for (uint32_t channel = 0; channel < 2; ++channel)
                    stream.Word(v == 0 ? samples[format].value : v == 1 ? 0 : 0xFFFFFFFFu, samples[format].bytes);
            Vertex out[3];
            EXPECT(Decode(gp, stream, out, 3));
            for (uint32_t channel = 0; channel < 2; ++channel)
                for (uint32_t i = 0; i < 4; ++i)
                {
                    EXPECT(out[0].colour[channel][i] == samples[format].expected[i]);
                    EXPECT(out[1].colour[channel][i] == (i == 3 && format < 3 ? 255 : 0));
                    EXPECT(out[2].colour[channel][i] == 255);
                }
        }
    }

    void Textures()
    {
        for (uint32_t format = 0; format < 5; ++format)
            for (uint32_t count : { 1u, 2u })
            {
                CommandProcessor gp;
                Cp(gp, 0x60, 0x5555);
                uint32_t vat[3]{};
                const uint32_t shifts[] = { 0, 1, 3, 7, 13, 19, 25, 31 };
                for (uint32_t i = 0; i < 8; ++i)
                    TextureFormat(vat, i, count - 1, format, shifts[i]);
                Vat(gp, vat);
                Stream stream;
                for (int32_t v = 0; v < 2; ++v)
                    for (int32_t i = 0; i < 8; ++i)
                        for (uint32_t j = 0; j < count; ++j)
                            stream.Scalar(
                                format, (format == 1 || format == 3 ? -1 : 1) * (v * 30 + i * 3 + int32_t(j) + 1));
                Vertex out[2];
                EXPECT(Decode(gp, stream, out, 2));
                for (int32_t v = 0; v < 2; ++v)
                    for (int32_t i = 0; i < 8; ++i)
                        for (uint32_t j = 0; j < 2; ++j)
                        {
                            const float raw =
                                float((format == 1 || format == 3 ? -1 : 1) * (v * 30 + i * 3 + int32_t(j) + 1));
                            EXPECT(out[v].texture[i][j] ==
                                   (j >= count ? 0 : std::ldexp(raw, format == 4 ? 0 : -int(shifts[i]))));
                        }
            }
    }

    void Matrices()
    {
        for (uint32_t mask : { 0u, 0x1FFu, 0x155u, 0xAAu })
        {
            CommandProcessor gp;
            Cp(gp, 0x50, mask);
            Cp(gp, 0x30, 3 | 7 << 6 | 13 << 12 | 29 << 18 | 63u << 24);
            Cp(gp, 0x40, 5 | 11 << 6 | 31 << 12 | 61 << 18);
            const uint8_t defaults[] = { 3, 7, 13, 29, 63, 5, 11, 31, 61 };
            Stream stream;
            for (uint32_t v = 0; v < 2; ++v)
                for (uint32_t i = 0; i < 9; ++i)
                    if (mask & (1u << i))
                        stream.Word(128 + 10 * v + i, 1);
            Vertex out[2];
            EXPECT(Decode(gp, stream, out, 2));
            for (uint32_t v = 0; v < 2; ++v)
                for (uint32_t i = 0; i < 9; ++i)
                    EXPECT((i == 0 ? out[v].positionMatrix : out[v].textureMatrix[i - 1]) ==
                           (mask & (1u << i) ? 128 + 10 * v + i : defaults[i]));
            EXPECT(out[0].position[0] == 0 && out[0].normal[2] == 0 && out[0].colour[1][3] == 0 &&
                   out[0].texture[7][1] == 0);
        }
        CommandProcessor gp;
        const DrawCall empty{ Primitive::Points, 0, 0, nullptr, 0 };
        EXPECT(DecodeVertices(gp, nullptr, empty, nullptr));
    }

    void Indexed()
    {
        for (uint32_t type : { 2u, 3u })
            for (uint32_t normalFormat : { 1u, 3u, 4u })
                for (bool nbt : { false, true })
                    for (bool separate : { false, true })
                    {
                        CommandProcessor gp;
                        Cp(gp, 0x50, type << 9 | type << 11 | type << 13 | type << 15);
                        Cp(gp, 0x60, type * 0x5555);
                        uint32_t vat[3] = { 1 | 3 << 1 | 3 << 4 | uint32_t(nbt) << 9 | normalFormat << 10 | 5 << 14 |
                                                3 << 18 | uint32_t(separate) << 31,
                            0, 0 };
                        for (uint32_t i = 0; i < 8; ++i)
                            TextureFormat(vat, i, i % 2, i % 5, i + 1);
                        Vat(gp, vat);
                        Stream stream;
                        constexpr uint32_t Stride = 64;
                        for (uint32_t array = 0; array < 12; ++array)
                        {
                            // Alternating memories and uncached base aliases exercise physical-address normalization.
                            const uint32_t physical = (array % 2 == 0 ? 0x00100000u : 0x10100000u) + array * 0x10000;
                            Cp(gp, uint8_t(0xA0 + array), physical | 0xC0000000u);
                            Cp(gp, uint8_t(0xB0 + array), Stride);
                        }
                        for (uint32_t v = 0; v < 2; ++v)
                            for (uint32_t array = 0; array < 12; ++array)
                            {
                                const uint32_t index = (type == 2 ? 7u : 257u) + v * 4;
                                const uint32_t entries = array == 1 && nbt && separate ? 3 : 1;
                                for (uint32_t entry = 0; entry < entries; ++entry)
                                {
                                    stream.Word(index + entry, type == 2 ? 1 : 2);
                                    Stream data;
                                    if (array == 0)
                                        for (int32_t i = 0; i < 3; ++i)
                                            data.Scalar(3, -100 - int32_t(v) * 8 - i);
                                    else if (array == 1)
                                        for (uint32_t i = 0; i < (nbt && !separate ? 9u : 3u); ++i)
                                            data.Scalar(normalFormat, -32 - int32_t(v * 9 + entry * 3 + i));
                                    else if (array == 2)
                                        data.Word(0x12345678u + v, 4);
                                    else if (array == 3)
                                        data.Word(0xA37F, 2);
                                    else
                                        for (uint32_t i = 0; i < (array - 4) % 2 + 1; ++i)
                                            data.Scalar((array - 4) % 5, 20 + int32_t(v * 8 + array + i));
                                    const uint32_t physical = gp.Cp(uint8_t(0xA0 + array)) + (index + entry) * Stride;
                                    const uint32_t address = 0x80000000u | (physical & 0x1FFFFFFFu);
                                    memcpy(g_base + address, data.bytes.data(), data.bytes.size());
                                }
                            }
                        Vertex out[2];
                        EXPECT(Decode(gp, stream, out, 2));
                        for (uint32_t v = 0; v < 2; ++v)
                        {
                            for (uint32_t i = 0; i < 3; ++i)
                                EXPECT(out[v].position[i] == float(-100 - int32_t(v * 8 + i)) / 8);
                            const float* vectors[] = { out[v].normal, out[v].binormal, out[v].tangent };
                            for (uint32_t i = 0; i < 9; ++i)
                                EXPECT(vectors[i / 3][i % 3] ==
                                       (i >= 3 && !nbt
                                               ? 0
                                               : std::ldexp(float(-32 - int32_t(v * 9 + i)), normalFormat == 1   ? -6
                                                                                             : normalFormat == 3 ? -14
                                                                                                                 : 0)));
                            const uint8_t expected[2][4] = { { 18, 52, 86, uint8_t(120 + v) }, { 170, 51, 119, 255 } };
                            EXPECT(memcmp(out[v].colour, expected, sizeof(expected)) == 0);
                            for (uint32_t i = 0; i < 8; ++i)
                                for (uint32_t j = 0; j < 2; ++j)
                                    EXPECT(out[v].texture[i][j] == (j > i % 2 ? 0
                                                                              : std::ldexp(float(24 + v * 8 + i + j),
                                                                                    i % 5 == 4 ? 0 : -int(i + 1))));
                        }
                        for (uint32_t array = 0; array < 12; ++array)
                        {
                            const uint8_t reg = uint8_t(0xA0 + array);
                            const uint32_t saved = gp.Cp(reg);
                            Cp(gp, reg, wii::Mem1Size);
                            EXPECT(!Decode(gp, stream, out, 2));
                            Cp(gp, reg, saved);
                        }
                    }
    }

    void IndexedBounds()
    {
        CommandProcessor gp;
        Cp(gp, 0x50, 2 << 11);
        Cp(gp, 0x70, 1 << 9 | 3 << 10 | 1u << 31);
        Cp(gp, 0xB1, 6);
        Stream stream;
        stream.Word(0, 1);
        stream.Word(1, 1);
        stream.Word(2, 1);
        Vertex out;
        for (uint32_t end : { wii::Mem1Size, 0x10000000u + wii::Mem2Size })
        {
            Cp(gp, 0xA1, end - 18);
            EXPECT(Decode(gp, stream, &out));
            Cp(gp, 0xA1, end - 17);
            EXPECT(!Decode(gp, stream, &out));
            Cp(gp, 0xA1, end - 11);
            EXPECT(!Decode(gp, stream, &out));
            Cp(gp, 0x70, 1 << 9 | 3 << 10);
            Stream single;
            single.Word(0, 1);
            EXPECT(!Decode(gp, single, &out));
            Cp(gp, 0x70, 1 << 9 | 3 << 10 | 1u << 31);
        }
    }

    void Lists()
    {
        auto check = [](Primitive primitive, uint32_t count, std::initializer_list<uint32_t> expected) {
            EXPECT(TriangleListSize(primitive, count) == expected.size());
            std::vector<uint32_t> indices(expected.size() + 2, 0xFFFFFFFFu);
            TriangleList(primitive, count, 17, indices.data() + 1);
            size_t i = 1;
            for (uint32_t index : expected)
                EXPECT(indices[i++] == index + 17);
            EXPECT(indices.front() == 0xFFFFFFFFu && indices.back() == 0xFFFFFFFFu);
        };
        for (Primitive primitive : { Primitive::Quads, Primitive::Quads2 })
        {
            check(primitive, 8, { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 });
            check(primitive, 7, { 0, 1, 2, 0, 2, 3 });
            check(primitive, 3, {});
        }
        check(Primitive::Triangles, 6, { 0, 1, 2, 3, 4, 5 });
        check(Primitive::Triangles, 5, { 0, 1, 2 });
        check(Primitive::Triangles, 2, {});
        check(Primitive::TriangleStrip, 6, { 0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5 });
        check(Primitive::TriangleFan, 6, { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5 });
        for (Primitive primitive : { Primitive::TriangleStrip, Primitive::TriangleFan })
        {
            check(primitive, 3, { 0, 1, 2 });
            check(primitive, 2, {});
            check(primitive, 1, {});
        }
        for (Primitive primitive : { Primitive::Lines, Primitive::LineStrip, Primitive::Points })
            check(primitive, 8, {});
        for (uint32_t i = 0; i < 8; ++i)
        {
            check(Primitive(i), 0, {});
            TriangleList(Primitive(i), 0, 17, nullptr);
        }
    }
}

int main()
{
    Positions();
    NumericLimits();
    MixedDirect();
    Normals();
    Colours();
    Textures();
    Matrices();
    Lists();
    std::string error;
    if (!wii::MapAddressSpace(g_base, {}, error))
    {
        fprintf(stderr, "cannot map the address space: %s\n", error.c_str());
        ++failures;
    }
    else
    {
        Indexed();
        IndexedBounds();
    }
    printf("gx vertex test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
