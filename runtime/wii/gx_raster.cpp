#include "wii/gx_raster.h"
#include "wii/gx_texture.h"
#include "wii/memory_map.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace wii::gx
{
    namespace
    {
        float XfFloat(const CommandProcessor& gp, uint32_t address)
        {
            return std::bit_cast<float>(gp.Xf(uint16_t(address)));
        }

        // The transform unit's registers [the SDK's GXSetViewport, GXSetProjection, GXSetChanCtrl, GXSetTexCoordGen].
        namespace xf
        {
            constexpr uint32_t Material = 0x100C, ColourControl = 0x100E, Viewport = 0x101A, Projection = 0x1020,
                               TexGens = 0x103F, TexGen = 0x1040;
        }
        constexpr float ScreenOffset = 342; // the viewport's and the scissor's origin

        uint8_t Clamp8(int32_t value) { return uint8_t(std::clamp(value, 0, 255)); }

        // A 32-bit RGBA register (R in the top byte) as 0-255 floats.
        void Unpack(uint32_t rgba, float out[4])
        {
            for (int i = 0; i < 4; i++)
                out[i] = float((rgba >> (24 - 8 * i)) & 0xFF);
        }

        void ToScreen(const CommandProcessor& gp, Rasterizer::Screen& out)
        {
            out.inverseW = out.clip[3] != 0 ? 1 / out.clip[3] : 0;
            const float wd = XfFloat(gp, xf::Viewport), ht = XfFloat(gp, xf::Viewport + 1),
                        zRange = XfFloat(gp, xf::Viewport + 2), xOrigin = XfFloat(gp, xf::Viewport + 3),
                        yOrigin = XfFloat(gp, xf::Viewport + 4), farZ = XfFloat(gp, xf::Viewport + 5);
            out.x = xOrigin - ScreenOffset + out.clip[0] * out.inverseW * wd;
            out.y = yOrigin - ScreenOffset + out.clip[1] * out.inverseW * ht;
            out.z = std::clamp(farZ + out.clip[2] * out.inverseW * zRange, 0.0f, 16777215.0f);
        }
    } // namespace

    static Rasterizer::Screen ToClip(const CommandProcessor& gp, const Vertex& vertex)
    {
        Rasterizer::Screen out{};
        const uint32_t row = uint32_t(vertex.positionMatrix) * 4;
        float view[3];
        for (int r = 0; r < 3; r++)
            view[r] = XfFloat(gp, row + 4 * r) * vertex.position[0] +
                      XfFloat(gp, row + 4 * r + 1) * vertex.position[1] +
                      XfFloat(gp, row + 4 * r + 2) * vertex.position[2] + XfFloat(gp, row + 4 * r + 3);

        float p[6];
        for (int i = 0; i < 6; i++)
            p[i] = XfFloat(gp, xf::Projection + i);
        const bool orthographic = gp.Xf(xf::Projection + 6) != 0;
        float (&clip)[4] = out.clip;
        if (orthographic)
        {
            clip[0] = p[0] * view[0] + p[1];
            clip[1] = p[2] * view[1] + p[3];
            clip[2] = p[4] * view[2] + p[5];
            clip[3] = 1;
        }
        else
        {
            clip[0] = p[0] * view[0] + p[1] * view[2];
            clip[1] = p[2] * view[1] + p[3] * view[2];
            clip[2] = p[4] * view[2] + p[5];
            clip[3] = -view[2];
        }
        // The colour channels, unlit: the vertex's colour or the material register (lighting comes later).
        for (int c = 0; c < 2; c++)
        {
            const uint32_t colourControl = gp.Xf(uint16_t(xf::ColourControl + c)),
                           alphaControl = gp.Xf(uint16_t(xf::ColourControl + 2 + c));
            float material[4];
            Unpack(gp.Xf(uint16_t(xf::Material + c)), material);
            for (int i = 0; i < 3; i++)
                out.colour[c][i] = (colourControl & 1) ? float(vertex.colour[c][i]) : material[i];
            out.colour[c][3] = (alphaControl & 1) ? float(vertex.colour[c][3]) : material[3];
        }

        // Texture coordinates: each generation's source row through its matrix (2 x 4 or 3 x 4).
        const uint32_t generations = std::min(gp.Xf(xf::TexGens) & 0xF, 8u);
        for (uint32_t n = 0; n < generations; n++)
        {
            const uint32_t info = gp.Xf(uint16_t(xf::TexGen + n));
            const uint32_t source = (info >> 7) & 0x1F;
            float input[4] = { 0, 0, 1, 1 };
            if (source == 0)
                for (int i = 0; i < 3; i++)
                    input[i] = vertex.position[i];
            else if (source >= 5 && source <= 12)
            {
                input[0] = vertex.texture[source - 5][0];
                input[1] = vertex.texture[source - 5][1];
            }
            const uint32_t matrix = uint32_t(vertex.textureMatrix[n]) * 4;
            float st[3];
            for (int r = 0; r < 3; r++)
                st[r] = XfFloat(gp, matrix + 4 * r) * input[0] + XfFloat(gp, matrix + 4 * r + 1) * input[1] +
                        XfFloat(gp, matrix + 4 * r + 2) * input[2] + XfFloat(gp, matrix + 4 * r + 3) * input[3];
            if (vertex.textureMatrix[n] == 60) // GX_IDENTITY, whose rows the SDK never loads
            {
                st[0] = input[0];
                st[1] = input[1];
                st[2] = 1;
            }
            const bool projected = (info >> 1) & 1;
            out.texture[n][0] = projected && st[2] != 0 ? st[0] / st[2] : st[0];
            out.texture[n][1] = projected && st[2] != 0 ? st[1] / st[2] : st[1];
        }
        return out;
    }

    Rasterizer::Screen Transform(const CommandProcessor& gp, const Vertex& vertex)
    {
        auto out = ToClip(gp, vertex);
        ToScreen(gp, out);
        return out;
    }

    const Rasterizer::Texture& Rasterizer::Map(const CommandProcessor& gp, const uint8_t* base, uint32_t map)
    {
        Texture& texture = textures[map & 7];
        if (textureDraw[map & 7] == draws)
            return texture;
        textureDraw[map & 7] = draws;
        // Maps 0-3 at BP 0x80/0x88/0x94 + map, 4-7 at 0xA0/0xA8/0xB4 + map - 4 [GXLoadTexObj].
        const uint32_t group = map < 4 ? 0x80 + map : 0xA0 + map - 4;
        const uint32_t image = gp.Bp(uint8_t(group + 8)), address = (gp.Bp(uint8_t(group + 0x14)) & 0xFFFFFF) << 5;
        const uint32_t width = (image & 0x3FF) + 1, height = ((image >> 10) & 0x3FF) + 1, format = (image >> 20) & 0xF;
        texture.mode = gp.Bp(uint8_t(group));
        const uint32_t cached = 0x80000000u | (address & 0x1FFFFFFFu);
        const uint32_t size = EncodedSize(TextureFormat(format), width, height);
        // Decoded again each draw that samples it: the game may have rewritten the bytes (a cache comes with the GPU).
        texture.address = address;
        texture.width = width;
        texture.height = height;
        texture.format = format;
        texture.rgba.resize(size_t(width) * height * 4);
        texture.valid = size != 0 && InMemory(cached, size) &&
                        Decode(TextureFormat(format), base + cached, size, width, height, texture.rgba.data());
        if (!texture.valid)
            stats.badTextures++;
        return texture;
    }

    namespace
    {
        // The texture's colour at (s, t), normalised: nearest, or bilinear when its magnification filter is linear.
        void Sample(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height, uint32_t mode, float s, float t,
            int32_t out[4])
        {
            auto wrap = [](int32_t i, int32_t size, uint32_t how) {
                if (how == 1) // repeat
                    return ((i % size) + size) % size;
                if (how == 2) // mirror
                {
                    const int32_t period = 2 * size;
                    int32_t m = ((i % period) + period) % period;
                    return m < size ? m : period - 1 - m;
                }
                return std::clamp(i, 0, size - 1);
            };
            const uint32_t wrapS = mode & 3, wrapT = (mode >> 2) & 3;
            const float x = s * float(width) - 0.5f, y = t * float(height) - 0.5f;
            if (((mode >> 4) & 1) == 0)
            {
                const int32_t i = wrap(int32_t(std::floor(x + 0.5f)), int32_t(width), wrapS),
                              j = wrap(int32_t(std::floor(y + 0.5f)), int32_t(height), wrapT);
                const uint8_t* p = &rgba[(size_t(j) * width + size_t(i)) * 4];
                for (int c = 0; c < 4; c++)
                    out[c] = p[c];
                return;
            }
            const float fx = std::floor(x), fy = std::floor(y);
            const float ax = x - fx, ay = y - fy;
            const int32_t i0 = wrap(int32_t(fx), int32_t(width), wrapS),
                          i1 = wrap(int32_t(fx) + 1, int32_t(width), wrapS),
                          j0 = wrap(int32_t(fy), int32_t(height), wrapT),
                          j1 = wrap(int32_t(fy) + 1, int32_t(height), wrapT);
            const uint8_t* p00 = &rgba[(size_t(j0) * width + size_t(i0)) * 4];
            const uint8_t* p10 = &rgba[(size_t(j0) * width + size_t(i1)) * 4];
            const uint8_t* p01 = &rgba[(size_t(j1) * width + size_t(i0)) * 4];
            const uint8_t* p11 = &rgba[(size_t(j1) * width + size_t(i1)) * 4];
            for (int c = 0; c < 4; c++)
                out[c] = int32_t(
                    std::lround((p00[c] * (1 - ax) + p10[c] * ax) * (1 - ay) + (p01[c] * (1 - ax) + p11[c] * ax) * ay));
        }

        // The TEV's constant inputs [GXSetTevKColorSel, GXSetTevKAlphaSel]: fractions of one, or a konst register's
        // colour or one of its components.
        int32_t KonstValue(const int32_t konst[4][4], uint32_t select, int channel, bool alpha)
        {
            if (select < 8)
                return (8 - int32_t(select)) * 255 / 8;
            if (!alpha && select >= 12 && select <= 15)
                return konst[select - 12][channel];
            if (select >= 16 && select <= 31)
                return konst[(select - 16) % 4][(select - 16) / 4];
            return 0;
        }

        bool Compare(uint32_t function, int32_t value, int32_t reference)
        {
            switch (function)
            {
            case 0: return false;
            case 1: return value < reference;
            case 2: return value == reference;
            case 3: return value <= reference;
            case 4: return value > reference;
            case 5: return value != reference;
            case 6: return value >= reference;
            default: return true;
            }
        }

        int32_t BlendFactor(uint32_t factor, bool source, const int32_t src[4], const int32_t dst[4], int channel)
        {
            switch (factor)
            {
            case 0: return 0;
            case 1: return 255;
            case 2: return source ? dst[channel] : src[channel];
            case 3: return 255 - (source ? dst[channel] : src[channel]);
            case 4: return src[3];
            case 5: return 255 - src[3];
            case 6: return dst[3];
            default: return 255 - dst[3];
            }
        }
    } // namespace

    void Rasterizer::Triangle(const CommandProcessor& gp, const Screen& a, const Screen& b, const Screen& c, Efb& efb)
    {
        stats.triangles++;
        Screen polygon[9] = { a, b, c }, clipped[9];
        uint32_t count = 3;
        if (gp.Xf(xf::Projection + 6) == 0)
        {
            // Keep the divide away from zero even when the projection's near plane passes through the eye.
            constexpr float MinW = 1e-6f;
            for (int plane = 0; plane < 2; ++plane)
            {
                auto distance = [plane](const Screen& vertex) {
                    return plane == 0 ? vertex.clip[2] + vertex.clip[3] : vertex.clip[3] - MinW;
                };
                auto inside = [plane](float value) { return plane == 0 ? value >= 0 : value > 0; };
                uint32_t output = 0;
                const Screen* previous = &polygon[count - 1];
                float previousDistance = distance(*previous);
                for (uint32_t i = 0; i < count; ++i)
                {
                    const Screen& current = polygon[i];
                    const float currentDistance = distance(current);
                    if (inside(previousDistance) != inside(currentDistance))
                    {
                        const float t = previousDistance / (previousDistance - currentDistance);
                        Screen intersection{};
                        for (int j = 0; j < 4; ++j)
                            intersection.clip[j] = std::lerp(previous->clip[j], current.clip[j], t);
                        for (int channel = 0; channel < 2; ++channel)
                            for (int j = 0; j < 4; ++j)
                                intersection.colour[channel][j] =
                                    std::lerp(previous->colour[channel][j], current.colour[channel][j], t);
                        for (int texture = 0; texture < 8; ++texture)
                            for (int j = 0; j < 2; ++j)
                                intersection.texture[texture][j] =
                                    std::lerp(previous->texture[texture][j], current.texture[texture][j], t);
                        // Roundoff at an intersection must not move it back across the plane.
                        if (plane == 0)
                            intersection.clip[2] = -intersection.clip[3];
                        else
                            intersection.clip[3] = MinW;
                        clipped[output++] = intersection;
                    }
                    if (inside(currentDistance))
                        clipped[output++] = current;
                    previous = &current;
                    previousDistance = currentDistance;
                }
                count = output;
                if (count < 3)
                    return;
                std::copy_n(clipped, count, polygon);
            }
        }
        for (uint32_t i = 0; i < count; ++i)
            ToScreen(gp, polygon[i]);
        float area = 0;
        for (uint32_t i = 1; i + 1 < count && area == 0; ++i)
            area = (polygon[i].x - polygon[0].x) * (polygon[i + 1].y - polygon[0].y) -
                   (polygon[i + 1].x - polygon[0].x) * (polygon[i].y - polygon[0].y);
        if (area == 0)
            return;
        // GX's front faces are clockwise on the screen (y down): area > 0. Cull once for the input triangle.
        const uint32_t cull = (gp.Bp(0x00) >> 14) & 3;
        if (cull == 3 || (cull == 1 && area < 0) || (cull == 2 && area > 0))
        {
            stats.culled++;
            return;
        }
        for (uint32_t i = 1; i + 1 < count; ++i)
            RasterTriangle(gp, polygon[0], polygon[i], polygon[i + 1], efb);
    }

    void Rasterizer::RasterTriangle(
        const CommandProcessor& gp, const Screen& a, const Screen& b, const Screen& c, Efb& efb)
    {
        const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
        if (area == 0)
            return;
        // The scissor [GXSetScissor]: BP 0x20 top (bits 0-10) and left (12-22), BP 0x21 bottom and right, inclusive, all
        // + 342.
        const int32_t top = int32_t(gp.Bp(0x20) & 0x7FF) - 342, left = int32_t((gp.Bp(0x20) >> 12) & 0x7FF) - 342;
        const int32_t bottom = int32_t(gp.Bp(0x21) & 0x7FF) - 342, right = int32_t((gp.Bp(0x21) >> 12) & 0x7FF) - 342;
        const int32_t x0 = std::max({ int32_t(std::floor(std::min({ a.x, b.x, c.x }))), left, 0 });
        const int32_t x1 = std::min({ int32_t(std::ceil(std::max({ a.x, b.x, c.x }))), right, int32_t(EfbWidth) - 1 });
        const int32_t y0 = std::max({ int32_t(std::floor(std::min({ a.y, b.y, c.y }))), top, 0 });
        const int32_t y1 =
            std::min({ int32_t(std::ceil(std::max({ a.y, b.y, c.y }))), bottom, int32_t(EfbHeight) - 1 });
        if (x0 > x1 || y0 > y1)
            return;

        const uint32_t genMode = gp.Bp(0x00);
        const uint32_t stages = ((genMode >> 10) & 0xF) + 1;
        const uint32_t zMode = gp.Bp(0x40), blend = gp.Bp(0x41), alphaTest = gp.Bp(0xF3);

        // The textures each stage samples, looked up once.
        const Texture* stageTexture[16] = {};
        uint32_t stageCoordinate[16] = {}, stageChannel[16] = {};
        for (uint32_t s = 0; s < stages; s++)
        {
            const uint32_t order = gp.Bp(uint8_t(0x28 + s / 2)) >> ((s & 1) * 12);
            stageCoordinate[s] = (order >> 3) & 7;
            stageChannel[s] = (order >> 7) & 7;
            if ((order >> 6) & 1)
            {
                const Texture& texture = Map(gp, base_, order & 7);
                stageTexture[s] = texture.valid ? &texture : nullptr;
            }
        }

        const float inverseArea = 1 / area;
        // A shared edge belongs to just one triangle: include top/left edges, for either winding.
        auto topLeft = [area](const Screen& from, const Screen& to) {
            const float dx = area > 0 ? to.x - from.x : from.x - to.x;
            const float dy = area > 0 ? to.y - from.y : from.y - to.y;
            return dy < 0 || (dy == 0 && dx > 0);
        };
        const bool edgeA = topLeft(b, c), edgeB = topLeft(c, a), edgeC = topLeft(a, b);
        for (int32_t y = y0; y <= y1; y++)
            for (int32_t x = x0; x <= x1; x++)
            {
                const float px = float(x) + 0.5f, py = float(y) + 0.5f;
                const float wa = ((b.x - px) * (c.y - py) - (c.x - px) * (b.y - py)) * inverseArea;
                const float wb = ((c.x - px) * (a.y - py) - (a.x - px) * (c.y - py)) * inverseArea;
                const float wc = ((a.x - px) * (b.y - py) - (b.x - px) * (a.y - py)) * inverseArea;
                if (wa < 0 || wb < 0 || wc < 0 || (wa == 0 && !edgeA) || (wb == 0 && !edgeB) || (wc == 0 && !edgeC))
                    continue;
                const size_t at = size_t(y) * EfbWidth + size_t(x);
                const uint32_t depth = uint32_t(std::clamp(wa * a.z + wb * b.z + wc * c.z, 0.0f, 16777215.0f));
                const bool depthTest = (zMode & 1) != 0;
                if (depthTest && !Compare((zMode >> 1) & 7, int32_t(depth), int32_t(efb.depth[at])))
                    continue;

                // Perspective-correct attributes.
                const float w = wa * a.inverseW + wb * b.inverseW + wc * c.inverseW;
                const float pa = wa * a.inverseW / w, pb = wb * b.inverseW / w, pc = wc * c.inverseW / w;

                int32_t registers[4][4];
                for (int r = 0; r < 4; r++)
                    for (int ch = 0; ch < 4; ch++)
                        registers[r][ch] = tevRegisters[r][ch];
                for (uint32_t s = 0; s < stages; s++)
                {
                    int32_t rasterised[4] = { 0, 0, 0, 0 }, texel[4] = { 255, 255, 255, 255 }, konst[4];
                    const uint32_t channel = stageChannel[s];
                    if (channel < 2)
                        for (int ch = 0; ch < 4; ch++)
                            rasterised[ch] = int32_t(
                                pa * a.colour[channel][ch] + pb * b.colour[channel][ch] + pc * c.colour[channel][ch]);
                    if (stageTexture[s] != nullptr)
                    {
                        const uint32_t n = stageCoordinate[s];
                        const float ts = pa * a.texture[n][0] + pb * b.texture[n][0] + pc * c.texture[n][0];
                        const float tt = pa * a.texture[n][1] + pb * b.texture[n][1] + pc * c.texture[n][1];
                        Sample(stageTexture[s]->rgba, stageTexture[s]->width, stageTexture[s]->height,
                            stageTexture[s]->mode, ts, tt, texel);
                    }
                    const uint32_t ksel = gp.Bp(uint8_t(0xF6 + s / 2)) >> ((s & 1) * 10);
                    for (int ch = 0; ch < 3; ch++)
                        konst[ch] = KonstValue(konstRegisters, (ksel >> 4) & 0x1F, ch, false);
                    konst[3] = KonstValue(konstRegisters, (ksel >> 9) & 0x1F, 3, true);

                    // Colour, then alpha [GXSetTevColorIn, GXSetTevColorOp and the alpha forms]: d + lerp(a, b, c),
                    // biased, scaled, clamped.
                    const uint32_t colourEnv = gp.Bp(uint8_t(0xC0 + 2 * s)), alphaEnv = gp.Bp(uint8_t(0xC1 + 2 * s));
                    int32_t result[4];
                    for (int part = 0; part < 2; part++)
                    {
                        const uint32_t env = part == 0 ? colourEnv : alphaEnv;
                        auto input = [&](uint32_t which, int ch) -> int32_t {
                            if (part == 0)
                                switch (which)
                                {
                                case 0: return registers[0][ch];
                                case 1: return registers[0][3];
                                case 2: return registers[1][ch];
                                case 3: return registers[1][3];
                                case 4: return registers[2][ch];
                                case 5: return registers[2][3];
                                case 6: return registers[3][ch];
                                case 7: return registers[3][3];
                                case 8: return texel[ch];
                                case 9: return texel[3];
                                case 10: return rasterised[ch];
                                case 11: return rasterised[3];
                                case 12: return 255;
                                case 13: return 128;
                                case 14: return konst[ch];
                                default: return 0;
                                }
                            switch (which)
                            {
                            case 0: return registers[0][3];
                            case 1: return registers[1][3];
                            case 2: return registers[2][3];
                            case 3: return registers[3][3];
                            case 4: return texel[3];
                            case 5: return rasterised[3];
                            case 6: return konst[3];
                            default: return 0;
                            }
                        };
                        const uint32_t shift = part == 0 ? 0 : 4, width = part == 0 ? 4 : 3;
                        const uint32_t mask = (1u << width) - 1;
                        const uint32_t d = (env >> shift) & mask, cIn = (env >> (shift + width)) & mask,
                                       bIn = (env >> (shift + 2 * width)) & mask,
                                       aIn = (env >> (shift + 3 * width)) & mask;
                        const uint32_t bias = (env >> 16) & 3, subtract = (env >> 18) & 1, clamp = (env >> 19) & 1,
                                       scale = (env >> 20) & 3;
                        for (int ch = part == 0 ? 0 : 3; ch < (part == 0 ? 3 : 4); ch++)
                        {
                            const int32_t av = input(aIn, ch) & 0xFF, bv = input(bIn, ch) & 0xFF,
                                          cv = input(cIn, ch) & 0xFF;
                            const int32_t lerp = (av * (256 - (cv + (cv >> 7))) + bv * (cv + (cv >> 7))) >> 8;
                            int32_t value;
                            if (bias == 3) // the compare modes: rare, left for the GPU renderer
                                value = input(d, ch);
                            else
                            {
                                value = subtract ? input(d, ch) - lerp : input(d, ch) + lerp;
                                value += bias == 1 ? 128 : bias == 2 ? -128 : 0;
                                value = scale == 1   ? value * 2
                                        : scale == 2 ? value * 4
                                        : scale == 3 ? value / 2
                                                     : value;
                            }
                            result[ch] = clamp ? std::clamp(value, 0, 255) : std::clamp(value, -1024, 1023);
                        }
                        const uint32_t destination = (env >> 22) & 3;
                        for (int ch = part == 0 ? 0 : 3; ch < (part == 0 ? 3 : 4); ch++)
                            registers[destination][ch] = result[ch];
                    }
                    if (s + 1 == stages)
                    {
                        const uint32_t colourDestination = (colourEnv >> 22) & 3,
                                       alphaDestination = (alphaEnv >> 22) & 3;
                        for (int ch = 0; ch < 3; ch++)
                            registers[0][ch] = registers[colourDestination][ch];
                        registers[0][3] = registers[alphaDestination][3];
                    }
                }
                int32_t source[4];
                for (int ch = 0; ch < 4; ch++)
                    source[ch] = std::clamp(registers[0][ch], 0, 255);

                // The alpha test [GXSetAlphaCompare]: two comparisons and their logic.
                const bool first = Compare((alphaTest >> 16) & 7, source[3], int32_t(alphaTest & 0xFF));
                const bool second = Compare((alphaTest >> 19) & 7, source[3], int32_t((alphaTest >> 8) & 0xFF));
                const uint32_t logic = (alphaTest >> 22) & 3;
                const bool pass = logic == 0   ? first && second
                                  : logic == 1 ? first || second
                                  : logic == 2 ? first != second
                                               : first == second;
                if (!pass)
                    continue;
                if (depthTest && ((zMode >> 4) & 1))
                    efb.depth[at] = depth;

                uint32_t& pixel = efb.colour[at];
                int32_t destination[4] = { int32_t(pixel & 0xFF), int32_t((pixel >> 8) & 0xFF),
                    int32_t((pixel >> 16) & 0xFF), int32_t(pixel >> 24) };
                int32_t out[4];
                if (blend & 1) // [GXSetBlendMode]: the source and destination factors, added (or subtracted)
                {
                    const uint32_t dstFactor = (blend >> 5) & 7, srcFactor = (blend >> 8) & 7;
                    for (int ch = 0; ch < 4; ch++)
                    {
                        const int32_t s = source[ch] * BlendFactor(srcFactor, true, source, destination, ch),
                                      d = destination[ch] * BlendFactor(dstFactor, false, source, destination, ch);
                        out[ch] = Clamp8(((blend >> 11) & 1) ? (d - s) / 255 : (s + d) / 255);
                    }
                }
                else
                    for (int ch = 0; ch < 4; ch++)
                        out[ch] = source[ch];
                const bool colourUpdate = (blend >> 3) & 1, alphaUpdate = (blend >> 4) & 1;
                if (colourUpdate)
                    for (int ch = 0; ch < 3; ch++)
                        destination[ch] = out[ch];
                if (alphaUpdate)
                    destination[3] = out[3];
                pixel = uint32_t(destination[0]) | uint32_t(destination[1]) << 8 | uint32_t(destination[2]) << 16 |
                        uint32_t(destination[3]) << 24;
                stats.pixels++;
            }
    }

    void Rasterizer::Bp(uint8_t index, uint32_t value)
    {
        // The TEV's colour registers and konst colours share BP 0xE0-0xE7 [GXSetTevColor, GXSetTevKColor]: bit 23 says
        // which; the low word holds R and A, the high one B and G, 11 bits each (signed for the colour registers).
        if (index < 0xE0 || index > 0xE7)
            return;
        const uint32_t reg = (index - 0xE0) / 2;
        const bool high = (index & 1) != 0, isKonst = ((value >> 23) & 1) != 0;
        auto field = [&](uint32_t shift) {
            const int32_t raw = int32_t((value >> shift) & 0x7FF);
            return isKonst ? raw & 0xFF : (raw >= 1024 ? raw - 2048 : raw);
        };
        int32_t (&target)[4][4] = isKonst ? konstRegisters : tevRegisters;
        if (!high)
        {
            target[reg][0] = field(0);
            target[reg][3] = field(12);
        }
        else
        {
            target[reg][2] = field(0);
            target[reg][1] = field(12);
        }
    }

    void Rasterizer::Draw(const CommandProcessor& gp, const uint8_t* base, const DrawCall& draw, Efb& efb)
    {
        draws++;
        base_ = base;
        const uint32_t count = TriangleListSize(draw.primitive, draw.count);
        if (count == 0)
            return;
        vertices.resize(draw.count);
        if (!DecodeVertices(gp, base, draw, vertices.data()))
            return;
        screen.resize(draw.count);
        for (uint32_t i = 0; i < draw.count; i++)
            screen[i] = ToClip(gp, vertices[i]);
        indices.resize(count);
        TriangleList(draw.primitive, draw.count, 0, indices.data());
        for (uint32_t i = 0; i + 2 < count; i += 3)
            Triangle(gp, screen[indices[i]], screen[indices[i + 1]], screen[indices[i + 2]], efb);
    }
}
