#include "pack_sampling.h"
#include <cstdio>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

    void Check(bool condition, int line)
    {
        g_checks++;
        if (!condition)
        {
            fprintf(stderr, "FAIL line %d\n", line);
            g_failures++;
        }
    }

#define CHECK(condition) Check((condition), __LINE__)

    bool Same(d3d::LodRange range, float low, float high) { return range.minLod == low && range.maxLod == high; }

    void TestScale()
    {
        CHECK(d3d::PackScaleLog2(256, 512) == 1);
        CHECK(d3d::PackScaleLog2(256, 1024) == 2);
        CHECK(d3d::PackScaleLog2(256, 256) == 0);
        CHECK(d3d::PackScaleLog2(256, 128) == 0); // a smaller file adds no level
        CHECK(d3d::PackScaleLog2(0, 128) == 0);
        CHECK(d3d::PackScaleLog2(96, 192) == 1);
    }

    void TestLodRange()
    {
        // A 256 guest with its 9 levels (0-8), doubled: 10 levels. The whole chain, the last one included.
        CHECK(Same(d3d::PackLodRange(0, 8, 1, 10), 0, 9));
        // The guest's fetch constant often says 15 (no limit).
        CHECK(Same(d3d::PackLodRange(0, 15, 1, 10), 0, 9));
        // No mips (0-0): the top level and the one of the guest's size, never smaller.
        CHECK(Same(d3d::PackLodRange(0, 0, 1, 10), 0, 1));
        CHECK(Same(d3d::PackLodRange(0, 0, 2, 11), 0, 2));
        // A guest that stops at level 3 stops at the same size.
        CHECK(Same(d3d::PackLodRange(0, 3, 1, 10), 0, 4));
        // A guest that starts lower starts at the same size.
        CHECK(Same(d3d::PackLodRange(2, 8, 1, 10), 3, 9));
        // A replacement of the guest's size (a modder's file): the guest's own range.
        CHECK(Same(d3d::PackLodRange(0, 8, 0, 9), 0, 8));
        CHECK(Same(d3d::PackLodRange(2, 5, 0, 9), 2, 5));
        // A file with fewer levels than the guest asks: its own last one.
        CHECK(Same(d3d::PackLodRange(0, 8, 1, 1), 0, 0));
        CHECK(Same(d3d::PackLodRange(4, 8, 1, 3), 2, 2));
        CHECK(Same(d3d::PackLodRange(0, 0, 1, 0), 0, 0));
    }

    void TestTint()
    {
        constexpr uint16_t Magenta = 0xF81F;
        // BC1, a four-colour block (first end above the second) and a three-colour one with two transparent texels.
        std::vector<uint8_t> bc1 = { 0x00, 0xF0, 0x10, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, //
            0x10, 0x00, 0x00, 0xF0, 0x03, 0x0C, 0x00, 0x00 };
        d3d::TintLevel(dds::Format::BC1, bc1, Magenta, 0);
        const std::vector<uint8_t> bc1Expected = { 0x1F, 0xF8, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, //
            0x1F, 0xF8, 0x1F, 0xF8, 0x03, 0x0C, 0x00, 0x00 };
        CHECK(bc1 == bc1Expected); // opaque texels at colour 0, the transparent ones still at index 3

        // BC3: the alpha half kept, the colour half one colour.
        std::vector<uint8_t> bc3 = { 1, 2, 3, 4, 5, 6, 7, 8, 0x00, 0xF0, 0x10, 0x00, 0xAA, 0xAA, 0xAA, 0xAA };
        d3d::TintLevel(dds::Format::BC3, bc3, Magenta, 0);
        const std::vector<uint8_t> bc3Expected = { 1, 2, 3, 4, 5, 6, 7, 8, 0x1F, 0xF8, 0x1F, 0xF8, 0, 0, 0, 0 };
        CHECK(bc3 == bc3Expected);

        std::vector<uint8_t> rgba = { 10, 20, 30, 40, 50, 60, 70, 255 };
        d3d::TintLevel(dds::Format::RGBA8, rgba, Magenta, 0xFFFF00FFu);
        const std::vector<uint8_t> rgbaExpected = { 255, 0, 255, 40, 255, 0, 255, 255 };
        CHECK(rgba == rgbaExpected);

        std::vector<uint8_t> bc5(16, 0x5A);
        const std::vector<uint8_t> bc5Before = bc5;
        d3d::TintLevel(dds::Format::BC5, bc5, Magenta, 0);
        CHECK(bc5 == bc5Before);
    }
}

int main()
{
    TestScale();
    TestLodRange();
    TestTint();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
