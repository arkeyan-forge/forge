#include "wii/gx_raster.h"
#include "wii/memory_map.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <memory>
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
    using Colour = std::array<uint8_t, 4>;
    uint8_t* g_base = nullptr;
    constexpr uint32_t ClearDepth = 0xFFFFFF;
    constexpr Colour Background{ 3, 5, 7, 11 }, Flat{ 32, 64, 96, 128 };
    constexpr uint32_t UpdateColourAlpha = 1 << 3 | 1 << 4;
    constexpr uint32_t AlphaAlways = 7 << 16 | 7 << 19;

    constexpr uint32_t Packed(Colour colour)
    {
        return uint32_t(colour[0]) | uint32_t(colour[1]) << 8 | uint32_t(colour[2]) << 16 | uint32_t(colour[3]) << 24;
    }

    struct Stream
    {
        std::vector<uint8_t> bytes;
        void Word(uint32_t value, uint32_t size)
        {
            for (uint32_t i = size; i > 0; --i)
                bytes.push_back(uint8_t(value >> (8 * (i - 1))));
        }
        void Float(float value) { Word(std::bit_cast<uint32_t>(value), 4); }
        void Feed(CommandProcessor& gp) const { gp.Feed(g_base, bytes.data(), bytes.size()); }
    };

    void Cp(CommandProcessor& gp, uint8_t index, uint32_t value)
    {
        Stream command;
        command.Word(LoadCp, 1);
        command.Word(index, 1);
        command.Word(value, 4);
        command.Feed(gp);
    }

    void Bp(CommandProcessor& gp, uint8_t index, uint32_t value)
    {
        Stream command;
        command.Word(LoadBp, 1);
        command.Word(uint32_t(index) << 24 | value, 4);
        command.Feed(gp);
    }

    void Xf(CommandProcessor& gp, uint16_t address, std::initializer_list<uint32_t> words)
    {
        Stream command;
        command.Word(LoadXf, 1);
        command.Word(uint32_t(words.size() - 1), 2);
        command.Word(address, 2);
        for (uint32_t word : words)
            command.Word(word, 4);
        command.Feed(gp);
    }

    void XfFloats(CommandProcessor& gp, uint16_t address, std::initializer_list<float> values)
    {
        Stream command;
        command.Word(LoadXf, 1);
        command.Word(uint32_t(values.size() - 1), 2);
        command.Word(address, 2);
        for (float value : values)
            command.Float(value);
        command.Feed(gp);
    }

    void Identity(CommandProcessor& gp) { XfFloats(gp, 0, { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 }); }

    Vertex At(float x, float y, Colour colour = Flat, float z = -1)
    {
        Vertex vertex{};
        vertex.position[0] = x;
        vertex.position[1] = y;
        vertex.position[2] = z;
        for (uint32_t i = 0; i < 4; ++i)
            vertex.colour[0][i] = colour[i];
        return vertex;
    }

    void Transforms()
    {
        CommandProcessor gp;
        Identity(gp);
        // XF 0x101A-0x101F: half width, negative half height, depth range, biased x/y origin, far depth.
        // The viewport's 342 bias is removed after projection; z = far + clipZ / w * range.
        XfFloats(gp, 0x101A, { 80, -40, 4096, 442, 402, 8192 });
        XfFloats(gp, 0x1020, { 0.5f, 0.25f, 2, -0.5f, 0.5f, -0.25f });
        Xf(gp, 0x1026, { 1 });
        const auto ortho = Transform(gp, At(0.5f, 0.25f, Flat, -0.5f));
        EXPECT(ortho.x == 140);
        EXPECT(ortho.y == 60);
        EXPECT(ortho.z == 6144);
        EXPECT(ortho.inverseW == 1);

        // XF 0x1026 = 0 selects perspective: w = -viewZ; p1/p3 multiply viewZ instead of adding a translation.
        XfFloats(gp, 0x1020, { 2, 0.25f, 4, -0.5f, 1, 0.5f });
        Xf(gp, 0x1026, { 0 });
        const auto perspective = Transform(gp, At(0.5f, 0.25f, Flat, -2));
        EXPECT(perspective.x == 120);
        EXPECT(perspective.y == 20);
        EXPECT(perspective.z == 5120);
        EXPECT(perspective.inverseW == 0.5f);
        EXPECT(perspective.clip[0] == 0.5f);
        EXPECT(perspective.clip[1] == 2);
        EXPECT(perspective.clip[2] == -1.5f);
        EXPECT(perspective.clip[3] == 2);
        EXPECT(gp.Counters().unknown == 0);
    }

    void Scissor(CommandProcessor& gp, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
    {
        // BP 0x20/0x21 have y in bits 0-10 and x in 12-22, biased by 342; the maximum is inclusive.
        Bp(gp, 0x20, (top + 342) | (left + 342) << 12);
        Bp(gp, 0x21, (bottom + 342) | (right + 342) << 12);
    }

    struct Harness
    {
        CommandProcessor gp;
        Rasterizer rasterizer;
        std::unique_ptr<Efb> efb = std::make_unique<Efb>();

        Harness()
        {
            gp.onBp = [this](uint8_t index, uint32_t value) { rasterizer.Bp(index, value); };
            gp.onDraw = [this](const DrawCall& draw) { rasterizer.Draw(gp, g_base, draw, *efb); };
            Identity(gp);
            XfFloats(gp, 0x101A, { 16, -16, 4096, 358, 358, 8192 });
            XfFloats(gp, 0x1020, { 1.0f / 16, -1, -1.0f / 16, 1, 1, 0 });
            Xf(gp, 0x1026, { 1 });
            // XF colour/alpha channel 0 bit 0 selects vertex material, with lighting disabled.
            Xf(gp, 0x100E, { 1, 0, 1, 0 });
            Cp(gp, 0x50, 1 << 9 | 1 << 13);
            Cp(gp, 0x60, 0);
            // VAT A: XYZ f32 and RGBA8888 direct; no normals or texture coordinates.
            Cp(gp, 0x70, 1 | 4 << 1 | 1 << 13 | 5 << 14);
            Bp(gp, 0x00, 0);
            Bp(gp, 0x28, 0);
            // TEV stage 0: D = RASC/RASA, A = B = C = zero, add, clamp, destination PREV.
            Bp(gp, 0xC0, 15 << 12 | 15 << 8 | 15 << 4 | 10 | 1 << 19);
            Bp(gp, 0xC1, 7 << 13 | 7 << 10 | 7 << 7 | 5 << 4 | 1 << 19);
            Scissor(gp, 0, 0, EfbWidth - 1, EfbHeight - 1);
            Bp(gp, 0x40, 0);
            Bp(gp, 0x41, UpdateColourAlpha);
            Bp(gp, 0xF3, AlphaAlways);
            Clear();
        }

        void Clear(Colour colour = Background, uint32_t depth = ClearDepth)
        {
            efb->Clear(Packed(colour), depth, 0, 0, EfbWidth, EfbHeight);
            rasterizer.stats = {};
            gp.ResetCounters();
        }

        void Draw(Primitive primitive, std::initializer_list<Vertex> vertices)
        {
            Stream command;
            command.Word(wii::gx::Draw | uint32_t(primitive) << 3, 1);
            command.Word(uint32_t(vertices.size()), 2);
            for (const Vertex& vertex : vertices)
            {
                for (float component : vertex.position)
                    command.Float(component);
                for (uint8_t component : vertex.colour[0])
                    command.Word(component, 1);
            }
            constexpr uint32_t Address = wii::Mem1Cached + 0x10000;
            EXPECT(gp.VertexSize(0) == 16);
            EXPECT(wii::InMemory(Address, uint32_t(command.bytes.size())));
            memcpy(g_base + Address, command.bytes.data(), command.bytes.size());
            const uint32_t draws = gp.Counters().draws;
            gp.Feed(g_base, g_base + Address, command.bytes.size());
            EXPECT(gp.Counters().draws == draws + 1);
        }

        void Triangle(Colour colour = Flat, float z = -1, bool reverse = false)
        {
            const Vertex a = At(8, 8, colour, z), b = At(24, 8, colour, z), c = At(8, 24, colour, z);
            if (reverse)
                Draw(Primitive::Triangles, { a, c, b });
            else
                Draw(Primitive::Triangles, { a, b, c });
        }

        void Quad(Colour colour = Flat, bool reverse = false, Primitive primitive = Primitive::Quads, float offset = 0)
        {
            const Vertex a = At(8 + offset, 8 + offset, colour), b = At(24 + offset, 8 + offset, colour),
                         c = At(24 + offset, 24 + offset, colour), d = At(8 + offset, 24 + offset, colour);
            if (reverse)
                Draw(primitive, { a, d, c, b });
            else
                Draw(primitive, { a, b, c, d });
        }
    };

    struct Pixel
    {
        uint32_t colour, depth;
    };

    template <typename Expected>
    void CheckEfb(const Harness& test, Expected expected)
    {
        uint32_t mismatches = 0;
        for (uint32_t y = 0; y < EfbHeight; ++y)
            for (uint32_t x = 0; x < EfbWidth; ++x)
            {
                const Pixel want = expected(x, y);
                const size_t at = size_t(y) * EfbWidth + x;
                if (test.efb->colour[at] != want.colour || test.efb->depth[at] != want.depth)
                {
                    if (mismatches == 0)
                        fprintf(stderr, "pixel (%u,%u): colour %08X expected %08X, depth %06X expected %06X\n", x, y,
                            test.efb->colour[at], want.colour, test.efb->depth[at], want.depth);
                    ++mismatches;
                }
            }
        EXPECT(mismatches == 0);
    }

    bool InTriangle(uint32_t x, uint32_t y)
    {
        // Pixel centres are (x + 0.5, y + 0.5); top/left edges belong to the triangle, bottom/right edges do not.
        // For (8,8), (24,8), (8,24), x + y = 31 lies exactly on the excluded diagonal: 15 + ... + 1 = 120 pixels.
        return x >= 8 && y >= 8 && x + y < 31;
    }

    void CheckTriangle(
        const Harness& test, Colour colour, uint32_t depth = ClearDepth, uint32_t outsideDepth = ClearDepth)
    {
        CheckEfb(test, [=](uint32_t x, uint32_t y) {
            return InTriangle(x, y) ? Pixel{ Packed(colour), depth } : Pixel{ Packed(Background), outsideDepth };
        });
    }

    void CheckStats(const Harness& test, uint32_t triangles, uint32_t culled, uint32_t pixels)
    {
        const auto& stats = test.rasterizer.stats;
        EXPECT(stats.triangles == triangles);
        EXPECT(stats.culled == culled);
        EXPECT(stats.pixels == pixels);
        EXPECT(stats.textured == 0);
        EXPECT(stats.badTextures == 0);
        EXPECT(test.gp.Counters().unknown == 0);
        EXPECT(test.gp.Counters().badLists == 0);
    }

    void Coverage()
    {
        Harness test;
        for (bool reverse : { false, true })
        {
            test.Clear();
            test.Triangle(Flat, -1, reverse);
            CheckTriangle(test, Flat);
            CheckStats(test, 1, 0, 120);
        }

        // ONE + ONE makes every shared-edge overdraw visible without saturation; a 16 by 16 quad covers 256 pixels.
        constexpr Colour Source{ 16, 24, 32, 8 }, Destination{ 1, 2, 3, 4 }, Sum{ 17, 26, 35, 12 };
        Bp(test.gp, 0x41, UpdateColourAlpha | 1 | 1 << 8 | 1 << 5);
        for (Primitive primitive : { Primitive::Quads, Primitive::Quads2 })
            for (bool reverse : { false, true })
                // At half-pixel coordinates, horizontal/vertical edges also pass through pixel centres.
                for (float offset : { 0.0f, 0.5f })
                {
                    test.Clear(Destination);
                    test.Quad(Source, reverse, primitive, offset);
                    CheckEfb(test, [=](uint32_t x, uint32_t y) {
                        const bool inside = x >= 8 && x < 24 && y >= 8 && y < 24;
                        return Pixel{ Packed(inside ? Sum : Destination), ClearDepth };
                    });
                    CheckStats(test, 2, 0, 256);
                }
    }

    void Interpolation()
    {
        Harness test;
        test.Draw(Primitive::Triangles,
            { At(8, 8, { 255, 0, 0, 255 }), At(24, 8, { 0, 255, 0, 255 }), At(8, 24, { 0, 0, 255, 255 }) });
        EXPECT(test.efb->colour[8 * EfbWidth + 8] == Packed({ 239, 7, 7, 255 }));
        EXPECT(test.efb->colour[8 * EfbWidth + 22] == Packed({ 15, 231, 7, 255 }));
        EXPECT(test.efb->colour[22 * EfbWidth + 8] == Packed({ 15, 7, 231, 255 }));
        // At (13.5,13.5) the barycentric weights are 10/32, 11/32, 11/32, close to the centroid's thirds.
        const uint32_t centre = test.efb->colour[13 * EfbWidth + 13];
        EXPECT(centre == Packed({ 79, 87, 87, 255 }));
        for (uint32_t channel = 0; channel < 3; ++channel)
            EXPECT(std::abs(int((centre >> (8 * channel)) & 0xFF) - 85) <= 7);
        EXPECT(test.efb->colour[24 * EfbWidth + 24] == Packed(Background));
        CheckStats(test, 1, 0, 120);
    }

    void Culling()
    {
        Harness test;
        // BP GEN_MODE bits 14-15: 0 none, 1 back, 2 front, 3 both. Front is clockwise with screen y down.
        constexpr bool Visible[4][2] = { { true, true }, { true, false }, { false, true }, { false, false } };
        for (uint32_t mode = 0; mode < 4; ++mode)
            for (bool reverse : { false, true })
            {
                test.Clear();
                Bp(test.gp, 0x00, mode << 14);
                test.Triangle(Flat, -1, reverse);
                const bool visible = Visible[mode][reverse];
                CheckTriangle(test, visible ? Flat : Background);
                CheckStats(test, 1, visible ? 0 : 1, visible ? 120 : 0);
            }
    }

    void Scissoring()
    {
        Harness test;
        Scissor(test.gp, 10, 11, 13, 15);
        test.Quad();
        CheckEfb(test, [](uint32_t x, uint32_t y) {
            const bool inside = x >= 10 && x <= 13 && y >= 11 && y <= 15;
            return Pixel{ Packed(inside ? Flat : Background), ClearDepth };
        });
        CheckStats(test, 2, 0, 20);
        test.Clear();
        Scissor(test.gp, 40, 50, 43, 54);
        test.Triangle();
        CheckTriangle(test, Background);
        CheckStats(test, 1, 0, 0);
    }

    void Depth()
    {
        Harness test;
        constexpr Colour Near{ 192, 32, 16, 255 }, Far{ 16, 32, 192, 255 };
        // BP ZMODE: bit 0 compare enable, bits 1-3 function (1 LESS), bit 4 depth update.
        Bp(test.gp, 0x40, 1 | 1 << 1 | 1 << 4);
        test.Triangle(Near, -1);
        test.Triangle(Far, -0.5f);
        CheckTriangle(test, Near, 4096);
        CheckStats(test, 2, 0, 120);

        test.Clear();
        test.Triangle(Far, -0.5f);
        test.Triangle(Near, -1);
        CheckTriangle(test, Near, 4096);
        CheckStats(test, 2, 0, 240);

        test.Clear(Background, 6144);
        Bp(test.gp, 0x40, 1 | 1 << 1);
        test.Triangle(Near, -1);
        test.Triangle(Far, 0);
        CheckTriangle(test, Near, 6144, 6144);
        CheckStats(test, 2, 0, 120);

        test.Clear();
        Bp(test.gp, 0x40, 0);
        test.Triangle(Near, -1);
        test.Triangle(Far, -0.5f);
        CheckTriangle(test, Far);
        CheckStats(test, 2, 0, 240);
    }

    void NearClipping()
    {
        Harness test;
        // clipZ = -1 puts the near plane at viewZ = -1; the viewport maps the visible part to a trapezoid.
        XfFloats(test.gp, 0x1020, { 1, 0, 1, 0, 0, -1 });
        Xf(test.gp, 0x1026, { 0 });
        constexpr Colour Black{ 0, 0, 0, 0 };
        const Vertex a = At(-1, 0, Black, -2), b = At(1, 0, Black, -2), c = At(0, 1, Black, 1);
        for (uint32_t mode = 0; mode < 4; ++mode)
            for (bool reverse : { false, true })
            {
                test.Clear();
                Bp(test.gp, 0x00, mode << 14);
                test.Draw(Primitive::Triangles,
                    reverse ? std::initializer_list<Vertex>{ a, c, b } : std::initializer_list<Vertex>{ a, b, c });
                const bool visible = mode == 0 || (mode == 1 && reverse) || (mode == 2 && !reverse);
                CheckEfb(test, [=](uint32_t x, uint32_t y) {
                    const float px = float(x) + 0.5f, py = float(y) + 0.5f;
                    const bool inside = visible && py >= 32.0f / 3 && py < 16 && px >= py / 2 && px < 32 - py / 2;
                    return Pixel{ Packed(inside ? Black : Background), ClearDepth };
                });
                CheckStats(test, 1, visible ? 0 : 1, visible ? 92 : 0);
            }

        Bp(test.gp, 0x00, 0);
        // A vertex on the eye plane must also be clipped before any divide.
        test.Clear();
        test.Draw(Primitive::Triangles, { a, b, At(0, 1, Black, 0) });
        CheckEfb(test, [=](uint32_t x, uint32_t y) {
            const bool inside = x >= 8 && x < 24 && y >= 8 && y < 16;
            return Pixel{ Packed(inside ? Black : Background), ClearDepth };
        });
        CheckStats(test, 1, 0, 128);

        test.Clear();
        test.Draw(Primitive::Triangles, { At(-1, 0, Black, 1), At(1, 0, Black, 1), c });
        CheckTriangle(test, Background);
        CheckStats(test, 1, 0, 0);

        // Positive w alone is insufficient: all three vertices are in front of the eye but outside the near plane.
        test.Clear();
        test.Draw(Primitive::Triangles,
            { At(-0.25f, 0, Black, -0.5f), At(0.25f, 0, Black, -0.5f), At(0, 0.25f, Black, -0.5f) });
        CheckTriangle(test, Background);
        CheckStats(test, 1, 0, 0);

        test.Clear();
        test.Draw(Primitive::Triangles,
            { At(-1, 0, { 255, 0, 0, 255 }, -2), At(1, 0, { 0, 255, 0, 255 }, -2), At(0, 1, { 0, 0, 255, 255 }, 1) });
        // The original triangle's weights at (16.5,12.5) are 37/106, 41/106, 28/106 in clip space.
        const uint32_t colour = test.efb->colour[12 * EfbWidth + 16];
        constexpr Colour Expected{ 89, 98, 67, 255 };
        for (uint32_t channel = 0; channel < 4; ++channel)
            EXPECT(std::abs(int((colour >> (8 * channel)) & 0xFF) - Expected[channel]) <= 1);
        CheckStats(test, 1, 0, 92);

        // Generate (s,t) from position (x,y) so the original triangle's weights also locate the texel.
        constexpr uint32_t TextureAddress = wii::Mem1Cached + 0x20000;
        for (uint32_t y = 0; y < 4; ++y)
            for (uint32_t x = 0; x < 8; ++x)
                g_base[TextureAddress + y * 8 + x] = uint8_t(16 + 24 * x + 3 * y);
        Cp(test.gp, 0x30, 60 << 6);
        Xf(test.gp, 0x103F, { 1 });
        Xf(test.gp, 0x1040, { 0 });
        Bp(test.gp, 0x80, 0);
        Bp(test.gp, 0x88, 7 | 3 << 10 | 1 << 20); // one 8 by 4 I8 tile
        Bp(test.gp, 0x94, (TextureAddress & 0x1FFFFFFF) >> 5);
        Bp(test.gp, 0x28, 1 << 6);
        Bp(test.gp, 0xC0, 15 << 12 | 15 << 8 | 15 << 4 | 8 | 1 << 19);
        Bp(test.gp, 0xC1, 7 << 13 | 7 << 10 | 7 << 7 | 4 << 4 | 1 << 19);
        test.Clear();
        test.Draw(Primitive::Triangles, { a, b, c });
        EXPECT(test.efb->colour[12 * EfbWidth + 16] == Packed({ 19, 19, 19, 19 }));
        EXPECT(test.efb->colour[12 * EfbWidth + 20] == Packed({ 67, 67, 67, 67 }));
        CheckStats(test, 1, 0, 92);

        Bp(test.gp, 0x28, 0);
        Bp(test.gp, 0xC0, 15 << 12 | 15 << 8 | 15 << 4 | 10 | 1 << 19);
        Bp(test.gp, 0xC1, 7 << 13 | 7 << 10 | 7 << 7 | 5 << 4 | 1 << 19);
        // A near plane through the eye leaves w = 0 inside; the positive-w guard must still clip it.
        XfFloats(test.gp, 0x1020, { 1, 0, 1, 0, 0, 0 });
        test.Clear();
        test.Draw(Primitive::Triangles, { a, b, At(0, 1, Black, 0) });
        CheckEfb(test, [=](uint32_t x, uint32_t y) {
            const bool inside = x >= 8 && x < 24 && y < 16;
            return Pixel{ Packed(inside ? Black : Background), ClearDepth };
        });
        CheckStats(test, 1, 0, 256);

        for (float w : { 1e-7f, 1e-6f })
        {
            test.Clear();
            test.Draw(
                Primitive::Triangles, { At(-w / 4, 0, Black, -w), At(w / 4, 0, Black, -w), At(0, w / 4, Black, -w) });
            CheckTriangle(test, Background);
            CheckStats(test, 1, 0, 0);
        }
    }

    void FarDepth()
    {
        Harness test;
        constexpr Colour Black{ 0, 0, 0, 0 };
        XfFloats(test.gp, 0x101A, { 16, -16, float(ClearDepth), 358, 358, float(ClearDepth) });
        // A non-power-of-two area exposes roundoff in barycentric sums at the maximum depth.
        const auto inside = [](uint32_t x, uint32_t y) { return x >= 8 && y >= 8 && x + y < 32; };
        // LEQUAL against a cleared depth buffer must accept every covered pixel, with updates disabled or enabled.
        for (bool update : { false, true })
        {
            test.Clear();
            Bp(test.gp, 0x40, 1 | 3 << 1 | uint32_t(update) << 4);
            test.Draw(Primitive::Triangles, { At(8, 8, Black, 0), At(25, 8, Black, 0), At(8, 25, Black, 0) });
            uint32_t badColours = 0, badDepths = 0;
            for (uint32_t y = 0; y < EfbHeight; ++y)
                for (uint32_t x = 0; x < EfbWidth; ++x)
                {
                    const size_t at = size_t(y) * EfbWidth + x;
                    if (test.efb->colour[at] != Packed(inside(x, y) ? Black : Background))
                        ++badColours;
                    const uint32_t depth = test.efb->depth[at];
                    if (depth > ClearDepth || ((!update || !inside(x, y)) && depth != ClearDepth))
                        ++badDepths;
                }
            EXPECT(badColours == 0);
            EXPECT(badDepths == 0);
            CheckStats(test, 1, 0, 136);
        }
    }

    void AlphaAndBlend()
    {
        Harness test;
        // BP ALPHACOMPARE: refs in 0-7/8-15, functions in 16-18/19-21, logic in 22-23 (0 AND).
        // GREATER 128 AND ALWAYS rejects alpha 128, including its depth write, but accepts alpha 129.
        Bp(test.gp, 0xF3, 128 | 4 << 16 | 7 << 19);
        Bp(test.gp, 0x40, 1 | 1 << 1 | 1 << 4);
        test.Triangle();
        CheckTriangle(test, Background);
        CheckStats(test, 1, 0, 0);
        constexpr Colour Accepted{ 32, 64, 96, 129 };
        test.Triangle(Accepted);
        CheckTriangle(test, Accepted, 4096);
        CheckStats(test, 2, 0, 120);

        // BP BLENDMODE: enable bit 0, destination factor bits 5-7, source bits 8-10; factors 4/5 are SRCALPHA/INVSRCALPHA.
        // (source * 128 + destination * 127) / 255, including alpha, truncates to (110,70,74,191).
        constexpr Colour Source{ 200, 100, 50, 128 }, Destination{ 20, 40, 100, 255 }, Mixed{ 110, 70, 74, 191 };
        test.Clear(Destination);
        Bp(test.gp, 0x40, 0);
        Bp(test.gp, 0xF3, AlphaAlways);
        Bp(test.gp, 0x41, UpdateColourAlpha | 1 | 4 << 8 | 5 << 5);
        test.Triangle(Source);
        CheckEfb(test, [=](uint32_t x, uint32_t y) {
            return Pixel{ Packed(InTriangle(x, y) ? Mixed : Destination), ClearDepth };
        });
        CheckStats(test, 1, 0, 120);
    }

    void Konst()
    {
        Harness test;
        // TEV D = KONST; KSEL0 colour bits 4-8 select K0-K3 as 12-15, alpha bits 9-13 select their A as 28-31.
        Bp(test.gp, 0xC0, 15 << 12 | 15 << 8 | 15 << 4 | 14 | 1 << 19);
        Bp(test.gp, 0xC1, 7 << 13 | 7 << 10 | 7 << 7 | 6 << 4 | 1 << 19);
        constexpr Colour Colours[] = { { 17, 35, 53, 71 }, { 78, 92, 106, 120 }, { 139, 149, 159, 169 },
            { 200, 180, 160, 140 } };
        for (uint32_t reg = 0; reg < 4; ++reg)
        {
            test.Clear();
            const Colour colour = Colours[reg];
            const uint8_t low = uint8_t(0xE0 + 2 * reg), high = uint8_t(low + 1);
            // Bit 23 chooses the konst bank; low holds R/A and high B/G in fields 0-10 and 12-22.
            Bp(test.gp, low, 1 << 23 | uint32_t(colour[0]) | uint32_t(colour[3]) << 12);
            Bp(test.gp, high, 1 << 23 | uint32_t(colour[2]) | uint32_t(colour[1]) << 12);
            // Normal TEV writes at the same addresses must leave the konst bank intact: onBp retains both banks.
            Bp(test.gp, low, 0);
            Bp(test.gp, high, 0);
            Bp(test.gp, 0xF6, (12 + reg) << 4 | (28 + reg) << 9);
            test.Triangle();
            CheckTriangle(test, colour);
            CheckStats(test, 1, 0, 120);
        }
    }
}

int main()
{
    Transforms();
    std::string error;
    if (!wii::MapAddressSpace(g_base, {}, error))
    {
        fprintf(stderr, "cannot map the address space: %s\n", error.c_str());
        ++failures;
    }
    else
    {
        Coverage();
        Interpolation();
        Culling();
        Scissoring();
        Depth();
        NearClipping();
        FarDepth();
        AlphaAndBlend();
        Konst();
    }
    printf("gx raster test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
