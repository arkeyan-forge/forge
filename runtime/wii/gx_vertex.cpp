#include "wii/gx_vertex.h"
#include "wii/memory_map.h"
#include <bit>
#include <cmath>

namespace wii::gx
{
    namespace
    {
        constexpr uint32_t ComponentBytes[8] = { 1, 1, 2, 2, 4, 0, 0, 0 };
        constexpr uint32_t ColourBytes[8] = { 2, 3, 4, 2, 3, 4, 0, 0 };

        uint32_t Read(const uint8_t*& p, uint32_t bytes)
        {
            uint32_t value = 0;
            for (uint32_t i = 0; i < bytes; ++i)
                value = (value << 8) | *p++;
            return value;
        }

        float Component(const uint8_t*& p, uint32_t format, uint32_t shift)
        {
            const uint32_t value = Read(p, ComponentBytes[format]);
            float decoded = 0;
            switch (format)
            {
            case 0: decoded = float(value); break;
            case 1: decoded = float(std::bit_cast<int8_t>(uint8_t(value))); break;
            case 2: decoded = float(value); break;
            case 3: decoded = float(std::bit_cast<int16_t>(uint16_t(value))); break;
            case 4: return std::bit_cast<float>(value);
            default: break; // formats 5-7 are not defined: VertexSize gives them no bytes
            }
            return std::ldexp(decoded, -int(shift));
        }

        const uint8_t* Attribute(const CommandProcessor& gp, const uint8_t* base, const uint8_t*& stream, uint32_t type,
            uint32_t array, uint32_t size)
        {
            if (type == 1)
            {
                const uint8_t* value = stream;
                stream += size;
                return value;
            }
            const uint32_t index = Read(stream, type == 2 ? 1 : 2);
            const uint32_t physical = gp.Cp(uint8_t(0xA0 + array)) + index * gp.Cp(uint8_t(0xB0 + array));
            const uint32_t address = 0x80000000u | (physical & 0x1FFFFFFFu);
            return InMemory(address, size) ? base + address : nullptr;
        }

        uint8_t Expand(uint32_t value, uint32_t bits)
        {
            return uint8_t((value << (8 - bits)) | (value >> (2 * bits - 8)));
        }

        void Colour(const uint8_t* p, uint32_t format, uint8_t* out)
        {
            const uint32_t value = Read(p, ColourBytes[format]);
            switch (format)
            {
            case 0:
                out[0] = Expand((value >> 11) & 31, 5);
                out[1] = Expand((value >> 5) & 63, 6);
                out[2] = Expand(value & 31, 5);
                out[3] = 255;
                break;
            case 1:
            case 2:
            {
                const uint32_t rgb = format == 2 ? value >> 8 : value;
                out[0] = uint8_t(rgb >> 16);
                out[1] = uint8_t(rgb >> 8);
                out[2] = uint8_t(rgb);
                out[3] = 255;
                break;
            }
            case 3:
                for (uint32_t i = 0; i < 4; ++i)
                    out[i] = Expand((value >> (12 - 4 * i)) & 15, 4);
                break;
            case 4:
                for (uint32_t i = 0; i < 4; ++i)
                    out[i] = Expand((value >> (18 - 6 * i)) & 63, 6);
                break;
            case 5:
                for (uint32_t i = 0; i < 4; ++i)
                    out[i] = uint8_t(value >> (24 - 8 * i));
                break;
            default: // formats 6-7 are not defined: opaque black
                out[0] = out[1] = out[2] = 0;
                out[3] = 255;
                break;
            }
        }
    }

    bool DecodeVertices(const CommandProcessor& gp, const uint8_t* base, const DrawCall& draw, Vertex* out)
    {
        const uint32_t low = gp.Cp(0x50), high = gp.Cp(0x60);
        const uint8_t format = draw.format & 7;
        const uint32_t a = gp.Cp(uint8_t(0x70 + format)), b = gp.Cp(uint8_t(0x80 + format)),
                       c = gp.Cp(uint8_t(0x90 + format));
        const uint32_t texture[8][3] = { { (a >> 21) & 1, (a >> 22) & 7, (a >> 25) & 31 },
            { b & 1, (b >> 1) & 7, (b >> 4) & 31 }, { (b >> 9) & 1, (b >> 10) & 7, (b >> 13) & 31 },
            { (b >> 18) & 1, (b >> 19) & 7, (b >> 22) & 31 }, { (b >> 27) & 1, (b >> 28) & 7, c & 31 },
            { (c >> 5) & 1, (c >> 6) & 7, (c >> 9) & 31 }, { (c >> 14) & 1, (c >> 15) & 7, (c >> 18) & 31 },
            { (c >> 23) & 1, (c >> 24) & 7, (c >> 27) & 31 } };

        for (uint32_t v = 0; v < draw.count; ++v)
        {
            const uint8_t* stream = draw.vertices + size_t(v) * draw.stride;
            Vertex& vertex = out[v];
            vertex = {};
            const uint32_t matrixA = gp.Cp(0x30), matrixB = gp.Cp(0x40);
            vertex.positionMatrix = low & 1 ? *stream++ : uint8_t(matrixA & 63);
            for (uint32_t i = 0; i < 8; ++i)
                vertex.textureMatrix[i] =
                    low & (1u << (i + 1))
                        ? *stream++
                        : uint8_t(((i < 4 ? matrixA >> (6 * (i + 1)) : matrixB >> (6 * (i - 4)))) & 63);

            const uint32_t positionType = (low >> 9) & 3, positionFormat = (a >> 1) & 7;
            if (positionType != 0)
            {
                const uint32_t count = (a & 1) ? 3 : 2;
                const uint8_t* p = Attribute(gp, base, stream, positionType, 0, count * ComponentBytes[positionFormat]);
                if (p == nullptr)
                    return false;
                for (uint32_t i = 0; i < count; ++i)
                    vertex.position[i] = Component(p, positionFormat, (a >> 4) & 31);
            }

            const uint32_t normalType = (low >> 11) & 3, normalFormat = (a >> 10) & 7;
            if (normalType != 0)
            {
                const bool nbt = (a & (1u << 9)) != 0;
                const bool separate = nbt && normalType >= 2 && (a & (1u << 31)) != 0;
                const uint32_t vectors = nbt ? 3 : 1;
                const uint32_t size = 3 * ComponentBytes[normalFormat];
                const uint8_t* p = Attribute(gp, base, stream, normalType, 1, separate ? size : size * vectors);
                if (p == nullptr)
                    return false;
                float* destinations[] = { vertex.normal, vertex.binormal, vertex.tangent };
                for (uint32_t vector = 0; vector < vectors; ++vector)
                {
                    if (separate && vector != 0)
                        p = Attribute(gp, base, stream, normalType, 1, size);
                    if (p == nullptr)
                        return false;
                    for (uint32_t i = 0; i < 3; ++i)
                        destinations[vector][i] = Component(p, normalFormat, normalFormat == 1 ? 6 : 14);
                }
            }

            for (uint32_t i = 0; i < 2; ++i)
            {
                const uint32_t type = (low >> (13 + 2 * i)) & 3, colourFormat = (a >> (14 + 4 * i)) & 7;
                if (type == 0)
                    continue;
                const uint8_t* p = Attribute(gp, base, stream, type, 2 + i, ColourBytes[colourFormat]);
                if (p == nullptr)
                    return false;
                Colour(p, colourFormat, vertex.colour[i]);
            }

            for (uint32_t i = 0; i < 8; ++i)
            {
                const uint32_t type = (high >> (2 * i)) & 3;
                if (type == 0)
                    continue;
                const uint32_t count = texture[i][0] + 1, componentFormat = texture[i][1];
                const uint8_t* p = Attribute(gp, base, stream, type, 4 + i, count * ComponentBytes[componentFormat]);
                if (p == nullptr)
                    return false;
                for (uint32_t j = 0; j < count; ++j)
                    vertex.texture[i][j] = Component(p, componentFormat, texture[i][2]);
            }
        }
        return true;
    }

    uint32_t TriangleListSize(Primitive primitive, uint32_t count)
    {
        switch (primitive)
        {
        case Primitive::Quads:
        case Primitive::Quads2: return (count / 4) * 6;
        case Primitive::Triangles: return (count / 3) * 3;
        case Primitive::TriangleStrip:
        case Primitive::TriangleFan: return count < 3 ? 0 : (count - 2) * 3;
        default: return 0;
        }
    }

    void TriangleList(Primitive primitive, uint32_t count, uint32_t first, uint32_t* indices)
    {
        const uint32_t size = TriangleListSize(primitive, count);
        for (uint32_t i = 0; i < size / 3; ++i)
        {
            uint32_t a = 0, b = 0, c = 0;
            switch (primitive)
            {
            case Primitive::Quads:
            case Primitive::Quads2:
                a = (i / 2) * 4;
                b = a + (i % 2 == 0 ? 1 : 2);
                c = a + (i % 2 == 0 ? 2 : 3);
                break;
            case Primitive::Triangles:
                a = i * 3;
                b = a + 1;
                c = a + 2;
                break;
            case Primitive::TriangleStrip:
                a = i + i % 2;
                b = i + (i % 2 == 0 ? 1 : 0);
                c = i + 2;
                break;
            case Primitive::TriangleFan:
                b = i + 1;
                c = i + 2;
                break;
            default: break;
            }
            *indices++ = first + a;
            *indices++ = first + b;
            *indices++ = first + c;
        }
    }
}
